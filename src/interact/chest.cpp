#include "chest.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

void Chest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_open"), &Chest::is_open);
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
			snd_open = Ref<AudioStream>();
			snd_close = Ref<AudioStream>();
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

	// 开关箱音效
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr) {
		if (rl->exists("res://assets/audio/zen_expand.mp3")) {
			Ref<Resource> r = rl->load("res://assets/audio/zen_expand.mp3");
			if (r.is_valid()) {
				snd_open = Object::cast_to<AudioStream>(r.ptr());
			}
		}
		if (rl->exists("res://assets/audio/zen_wood_drop.mp3")) {
			Ref<Resource> r = rl->load("res://assets/audio/zen_wood_drop.mp3");
			if (r.is_valid()) {
				snd_close = Object::cast_to<AudioStream>(r.ptr());
			}
		} else if (rl->exists("res://assets/audio/zen_collapse.mp3")) {
			Ref<Resource> r = rl->load("res://assets/audio/zen_collapse.mp3");
			if (r.is_valid()) {
				snd_close = Object::cast_to<AudioStream>(r.ptr());
			}
		}
	}
	audio_player = memnew(AudioStreamPlayer3D);
	audio_player->set_name("ChestAudio");
	audio_player->set_unit_size(6.0f);
	audio_player->set_max_distance(25.0f);
	add_child(audio_player);
}

void Chest::_process(double p_delta) {
	if (open_t == open_target || lid_pivot == nullptr) {
		return;
	}
	float dir = open_target > open_t ? 1.0f : -1.0f;
	open_t = Math::clamp(open_t + dir * (float)p_delta * 3.0f, 0.0f, 1.0f);
	// Hermite 缓动：起步慢、中段快、收尾慢
	float e = open_t * open_t * (3.0f - 2.0f * open_t);
	lid_pivot->set_rotation(Vector3(-1.90f * e, 0.0f, 0.0f)); // ≈109°，向后掀开
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
	if (audio_player != nullptr) {
		Ref<AudioStream> s = is_open() ? snd_open : snd_close;
		if (s.is_valid()) {
			audio_player->set_stream(s);
			audio_player->set_volume_db(-6.0f);
			audio_player->play();
		}
	}
}
