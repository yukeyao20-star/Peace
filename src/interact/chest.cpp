#include "chest.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

void Chest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_open"), &Chest::is_open);
	ClassDB::bind_method(D_METHOD("is_looted"), &Chest::is_looted);
	ClassDB::bind_method(D_METHOD("set_looted"), &Chest::set_looted);

	ADD_SIGNAL(MethodInfo("chest_opened")); // 掀开箱盖时发出（GameDirector 每日待办计数用）
	ADD_SIGNAL(MethodInfo("chest_looted")); // 首次开箱翻出野餐毯时发出（GameDirector 公告用）
}

void Chest::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成，保持场景干净
			}
			build(meshutil::load_toon_shader());
			add_to_group("interactables");
			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (audio_player != nullptr) {
				audio_player->stop();
				audio_player->set_stream(Ref<AudioStream>());
			}
			if (chime_player != nullptr) {
				chime_player->stop();
				chime_player->set_stream(Ref<AudioStream>());
			}
			snd_open = Ref<AudioStream>();
			snd_close = Ref<AudioStream>();
			snd_discover = Ref<AudioStream>();
			blanket = nullptr; // 子节点随树拆除，只清指针
		} break;
		default:
			break;
	}
}

void Chest::build(Ref<Shader> p_shader) {
	const Color wood(0.45f, 0.29f, 0.17f);   // 箱体：橡木
	const Color trim(0.28f, 0.18f, 0.10f);   // 包边/加固条：深木
	const Color inner(0.14f, 0.09f, 0.05f);  // 内衬：近黑深褐
	const Color brass(0.85f, 0.70f, 0.35f);  // 锁扣：黄铜

	// —— 箱体：静态 + 碰撞 ——
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("Body");
	add_child(body);

	meshutil::add_box(body, p_shader, Vector3(0.90f, 0.50f, 0.55f), Vector3(0.0f, 0.25f, 0.0f), wood);
	// 顶部内衬：盖板打开后露出的深色内腔
	meshutil::add_box(body, p_shader, Vector3(0.80f, 0.06f, 0.45f), Vector3(0.0f, 0.48f, 0.02f), inner);
	// 锁扣座（箱体正面中部）
	meshutil::add_box(body, p_shader, Vector3(0.10f, 0.14f, 0.03f), Vector3(0.0f, 0.40f, 0.285f), brass);
	// 四角包边（MC 储物箱的深色边角观感）
	for (int sx = 0; sx < 2; sx++) {
		for (int sz = 0; sz < 2; sz++) {
			Vector3 pos((sx == 0 ? -0.42f : 0.42f), 0.26f, (sz == 0 ? -0.24f : 0.24f));
			meshutil::add_box(body, p_shader, Vector3(0.07f, 0.52f, 0.08f), pos, trim);
		}
	}

	Ref<BoxShape3D> body_shape;
	body_shape.instantiate();
	body_shape->set_size(Vector3(0.90f, 0.50f, 0.55f));
	CollisionShape3D *body_col = memnew(CollisionShape3D);
	body_col->set_name("Collision");
	body_col->set_shape(body_shape);
	body_col->set_position(Vector3(0.0f, 0.25f, 0.0f));
	body->add_child(body_col);

	// —— 盖板：合页枢轴在后上边缘，绕 X 轴向后旋开 ——
	lid_pivot = memnew(Node3D);
	lid_pivot->set_name("LidPivot");
	lid_pivot->set_position(Vector3(0.0f, 0.50f, -0.275f));
	add_child(lid_pivot);

	StaticBody3D *lid = memnew(StaticBody3D);
	lid->set_name("Lid");
	lid->set_position(Vector3(0.0f, 0.06f, 0.275f)); // 盖板中心相对铰链：上抬 + 前移
	lid_pivot->add_child(lid);

	meshutil::add_box(lid, p_shader, Vector3(0.94f, 0.12f, 0.59f), Vector3(0.0f, 0.0f, 0.0f), wood);
	// 两道横向加固条（压在盖板上表面）
	meshutil::add_box(lid, p_shader, Vector3(0.94f, 0.03f, 0.08f), Vector3(0.0f, 0.075f, -0.18f), trim);
	meshutil::add_box(lid, p_shader, Vector3(0.94f, 0.03f, 0.08f), Vector3(0.0f, 0.075f, 0.18f), trim);
	// 黄铜锁鼻（垂在盖板前缘，关上时与箱体锁扣座咬合）
	meshutil::add_box(lid, p_shader, Vector3(0.09f, 0.08f, 0.03f), Vector3(0.0f, -0.09f, 0.295f), brass);

	Ref<BoxShape3D> lid_shape;
	lid_shape.instantiate();
	lid_shape->set_size(Vector3(0.94f, 0.12f, 0.59f));
	CollisionShape3D *lid_col = memnew(CollisionShape3D);
	lid_col->set_name("Collision");
	lid_col->set_shape(lid_shape);
	lid->add_child(lid_col);

	// 开关箱木质铰链咯吱音效（直接复用开门声音）
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr) {
		if (rl->exists("res://assets/audio/door_open_creak.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/door_open_creak.wav");
			if (r.is_valid()) {
				snd_open = Object::cast_to<AudioStream>(r.ptr());
			}
		} else if (rl->exists("res://assets/audio/wood_creak_open.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/wood_creak_open.wav");
			if (r.is_valid()) {
				snd_open = Object::cast_to<AudioStream>(r.ptr());
			}
		}
		if (rl->exists("res://assets/audio/wood_creak_close.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/wood_creak_close.wav");
			if (r.is_valid()) {
				snd_close = Object::cast_to<AudioStream>(r.ptr());
			}
		}
	}
	audio_player = memnew(AudioStreamPlayer3D);
	audio_player->set_name("ChestAudio");
	audio_player->set_unit_size(6.0f);
	audio_player->set_max_distance(25.0f);
	audio_player->set_bus(StringName("SFX"));
	add_child(audio_player);

	// —— 发现"叮咚"提示音（程序合成，走 SFX 总线） ——
	snd_discover = make_discover_chime();
	chime_player = memnew(AudioStreamPlayer3D);
	chime_player->set_name("DiscoverChime");
	chime_player->set_unit_size(6.0f);
	chime_player->set_max_distance(25.0f);
	chime_player->set_bus(StringName("SFX"));
	add_child(chime_player);
}

void Chest::_process(double p_delta) {
	if (open_t == open_target || lid_pivot == nullptr) {
		// 空闲期只剩叮咚倒计时，照常推进
	} else {
		float dir = open_target > open_t ? 1.0f : -1.0f;
		open_t = Math::clamp(open_t + dir * (float)p_delta * 3.0f, 0.0f, 1.0f);
		// Hermite 缓动：起步慢、中段快、收尾慢
		float e = open_t * open_t * (3.0f - 2.0f * open_t);
		lid_pivot->set_rotation(Vector3(-1.45f * e, 0.0f, 0.0f)); // ≈83°，向上方掀开不穿透后墙木板
	}
	if (chime_t > 0.0f) {
		chime_t -= (float)p_delta;
		if (chime_t <= 0.0f && chime_player != nullptr && snd_discover.is_valid()) {
			chime_player->set_stream(snd_discover);
			chime_player->set_volume_db(-4.0f);
			chime_player->play(); // 等开箱吱呀过半再响，两个声源不抢
		}
	}
}

String Chest::get_prompt() const {
	return is_open() ? String::utf8("按 E 关闭木箱") : String::utf8("按 E 打开木箱");
}

Vector3 Chest::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.6f, 0.0f);
}

void Chest::interact(const Vector3 &p_player_pos) {
	if (open_t != open_target) {
		return; // 动画进行中不接受新指令
	}
	open_target = is_open() ? 0.0f : 1.0f;
	if (open_target > 0.5f) {
		// 一次性彩蛋先于计分信号发出：GameDirector 的发现公告先上屏，
		// 若这一箱正好补完当日清单，结算横幅随后覆盖它（优先级正确）
		if (!looted) {
			emit_signal("chest_looted");
			set_looted(true); // 置位 + 摆毯（SaveSystem 下轮自动存收编）
			chime_t = 0.55f;
		}
		emit_signal("chest_opened"); // 这次是"开箱"方向，通知每日待办计分
	}
	if (audio_player != nullptr) {
		Ref<AudioStream> s = is_open() ? snd_open : snd_close;
		if (s.is_valid()) {
			audio_player->set_stream(s);
			audio_player->set_volume_db(-6.0f);
			audio_player->play();
		}
	}
}

void Chest::set_looted(bool p_looted) {
	if (looted == p_looted) {
		return;
	}
	looted = p_looted;
	if (looted) {
		spawn_blanket(meshutil::load_toon_shader()); // 读档回放：只补摆毯子，不重播音效
	}
}

// 野餐毯 + 藤篮摆在箱旁（纯视觉、无碰撞：布毯贴地可踩过；摆位截图核验过）
void Chest::spawn_blanket(Ref<Shader> p_shader) {
	if (blanket != nullptr) {
		return;
	}
	const Color cloth(0.70f, 0.24f, 0.20f);        // 砖红毯面
	const Color stripe(0.92f, 0.88f, 0.80f);       // 奶白条纹
	const Color basket(0.55f, 0.38f, 0.20f);       // 藤篮
	const Color basket_dark(0.45f, 0.30f, 0.16f);  // 篮沿/提梁

	blanket = memnew(Node3D);
	blanket->set_name("PicnicBlanket");
	add_child(blanket);
	// 木箱在木屋内后右角（屋内壁 x≈3.4/z≈-2.9）；小屋地板顶面=木屋局部 y0.2=箱局部 y0，
	// 毯子贴地板摆在箱西侧（摆位截图核验：首版曾 +X 穿东墙、y-0.18 埋地板）
	blanket->set_position(Vector3(-0.95f, 0.0175f, 0.30f));
	blanket->set_rotation(Vector3(0.0f, -0.4f, 0.0f));

	meshutil::add_box(blanket, p_shader, Vector3(1.05f, 0.035f, 0.72f), Vector3(0.0f, 0.0175f, 0.0f), cloth);
	// 两条奶白条纹略高出毯面 7mm，避免共面闪烁
	meshutil::add_box(blanket, p_shader, Vector3(1.05f, 0.042f, 0.10f), Vector3(0.0f, 0.021f, -0.20f), stripe);
	meshutil::add_box(blanket, p_shader, Vector3(1.05f, 0.042f, 0.10f), Vector3(0.0f, 0.021f, 0.20f), stripe);
	// 毯角上的藤篮：篮身 + 篮沿 + 提梁
	meshutil::add_box(blanket, p_shader, Vector3(0.30f, 0.16f, 0.22f), Vector3(0.28f, 0.08f, -0.28f), basket);
	meshutil::add_box(blanket, p_shader, Vector3(0.32f, 0.03f, 0.24f), Vector3(0.28f, 0.17f, -0.28f), basket_dark);
	meshutil::add_box(blanket, p_shader, Vector3(0.34f, 0.025f, 0.05f), Vector3(0.28f, 0.205f, -0.28f), basket_dark);
}

// 程序化合成发现音"叮咚"（22050Hz 16-bit 单声道，无外部资源）：
// 两音上行（660→880Hz 正弦+泛音），柔软短促，像翻出小物件的惊喜
Ref<AudioStreamWAV> Chest::make_discover_chime() {
	const int sr = 22050;
	const float dur = 0.5f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	struct Note {
		float t0;
		float f;
		float decay;
	};
	const Note notes[2] = { { 0.02f, 660.0f, 15.0f }, { 0.20f, 880.0f, 11.0f } };

	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float s = 0.0f;
		for (const Note &nt : notes) {
			float lt = t - nt.t0;
			if (lt < 0.0f) {
				continue;
			}
			float ph = 6.2831853f * nt.f * lt;
			s += (std::sin(ph) + 0.30f * std::sin(2.0f * ph)) * std::exp(-lt * nt.decay) * 0.36f;
		}
		s = Math::clamp(s, -0.8f, 0.8f);
		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF); // 小端 16-bit
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}
