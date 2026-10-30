#include "berry_bush.h"

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "mesh_util.h"
#include "sky_cycle.h" // get_snow_amount（雪毯淡入，与树冠雪毯/夜光联动同套路）

#include <cstdint>
#include <random>

using namespace godot;

void BerryBush::_bind_methods() {
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &BerryBush::interact);
	ClassDB::bind_method(D_METHOD("get_prompt"), &BerryBush::get_prompt);
	ADD_SIGNAL(MethodInfo("berry_picked")); // 采到一颗时发出（GameDirector 计数用）
}

namespace {
constexpr float REGROW_SECONDS = 45.0f; // 采摘后重新长出的时间
constexpr float POP_SECONDS = 0.5f;     // 长出弹出动画时长
constexpr int BERRY_COUNT = 7;          // 每丛浆果数
} // namespace

void BerryBush::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成，保持场景干净
			}
			build();
			add_to_group("interactables");
			add_to_group("berry_bushes"); // GameDirector 用它接采摘信号
			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			// 采摘音效正在播放时退出，会泄 AudioStreamWAV/AudioStreamPlaybackWAV
			// 各 2 个 ObjectDB 实例（qa_report P2#1）——先停声源再放掉流引用。
			if (voice != nullptr) {
				voice->stop();
			}
			snd_pop = Ref<AudioStreamWAV>();
		} break;
		default:
			break;
	}
}

void BerryBush::build() {
	Ref<Shader> shader = meshutil::load_toon_shader();

	// —— 灌丛：3 团压扁球体错落堆叠（中心大球 + 两侧小球），压 y 出圆墩感 ——
	const Color leaf(0.22f, 0.38f, 0.16f);
	const Color leaf_dark(0.17f, 0.30f, 0.12f);
	const Vector3 squash(1.0f, 0.8f, 1.0f);
	const Vector3 blob_pos[3] = {
		Vector3(0.00f, 0.42f, 0.00f),
		Vector3(-0.34f, 0.30f, 0.10f),
		Vector3(0.30f, 0.34f, -0.12f),
	};
	const float blob_r[3] = { 0.52f, 0.38f, 0.42f };
	for (int i = 0; i < 3; i++) {
		MeshInstance3D *blob = meshutil::add_sphere(this, shader, blob_r[i], blob_pos[i],
				i == 0 ? leaf : leaf_dark);
		blob->set_scale(squash);
	}

	// —— 浆果：撒在主灌丛球面附近，世界坐标哈希播种（各丛不雷同、整图可复现）——
	berries = memnew(Node3D);
	berries->set_name("Berries");
	add_child(berries);

	const Vector3 gp = get_global_position();
	uint32_t seed = (uint32_t)((int64_t)(gp.x * 7.0) * 73856093) ^
			(uint32_t)((int64_t)(gp.z * 7.0) * 19349663);
	seed ^= 0x9E3779B9;
	std::mt19937 rng(seed);

	const Color berry(0.85f, 0.20f, 0.14f);
	std::uniform_real_distribution<float> ang(0.0f, float(Math::TAU));
	std::uniform_real_distribution<float> rad(0.34f, 0.50f);
	std::uniform_real_distribution<float> hei(0.18f, 0.62f);
	for (int i = 0; i < BERRY_COUNT; i++) {
		float a = ang(rng);
		float r = rad(rng);
		float y = hei(rng);
		meshutil::add_sphere(berries, shader, 0.065f,
				Vector3(Math::cos(a) * r, y, Math::sin(a) * r), berry);
	}

	// —— 雪毯：每团灌丛顶搭一片压扁白球（略宽出悬檐），雪量驱动淡入 ——
	// 几何由 blob_pos/blob_r 确定性导出；共用一个 StandardMaterial3D，每帧只调透明度。
	snow_caps = memnew(Node3D);
	snow_caps->set_name("SnowCaps");
	add_child(snow_caps);
	snow_mat.instantiate();
	snow_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	snow_mat->set_albedo(Color(0.96f, 0.97f, 1.0f, 0.0f)); // 初始全透明
	snow_mat->set_roughness(0.85f);
	for (int i = 0; i < 3; i++) {
		const float top_y = blob_pos[i].y + blob_r[i] * 0.8f; // 压扁后球顶
		Ref<SphereMesh> cap;
		cap.instantiate();
		cap->set_radius(blob_r[i] + 0.04f); // 略宽出灌丛，做出雪檐悬边
		cap->set_height((blob_r[i] + 0.04f) * 2.0f);
		cap->set_radial_segments(16);
		cap->set_rings(8);
		cap->set_material(snow_mat);
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(cap);
		mi->set_position(Vector3(blob_pos[i].x, top_y - 0.06f, blob_pos[i].z));
		mi->set_scale(Vector3(1.0f, 0.42f, 1.0f)); // 压扁成雪层
		mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		snow_caps->add_child(mi);
	}
	snow_caps->set_visible(false); // 无雪隐形，_process 每帧接管

	// —— 采摘音效：3D 声源（靠近哪丛采哪丛，距离衰减自然）——
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("Voice");
	voice->set_volume_db(-6.0f);
	voice->set_max_distance(24.0f);
	voice->set_unit_size(8.0f);
	voice->set_bus(StringName("SFX"));
	add_child(voice);
	snd_pop = make_pop();
	voice->set_stream(snd_pop); // 之前漏挂音源，play() 静默无效——测试脚本抓出
}

void BerryBush::_process(double p_delta) {
	update_snow(); // 雪毯与果组状态互不干扰，重长等待期间也要更新
	if (berries == nullptr) {
		return;
	}
	if (regrow_t > 0.0f) {
		regrow_t -= (float)p_delta;
		if (regrow_t <= 0.0f) {
			regrow_t = 0.0f;
			pop_t = 0.0f;
			berries->set_visible(true);
		}
		return;
	}
	if (pop_t < 1.0f) {
		pop_t = Math::min(pop_t + (float)p_delta / POP_SECONDS, 1.0f);
		// Hermite 缓动弹出：从 5% 缩到 100%
		float e = pop_t * pop_t * (3.0f - 2.0f * pop_t);
		float s = 0.05f + 0.95f * e;
		berries->set_scale(Vector3(s, s, s));
	}
}

// 雪毯显隐/透明度随雪量：懒查找 SkyCycle，找不到视为无雪（行为不变）。
// 注意本类不直接挂在 World 根（在 Homestead 下），要沿祖先链向上找。
void BerryBush::update_snow() {
	if (snow_caps == nullptr || snow_mat.is_null()) {
		return;
	}
	if (cycle == nullptr) {
		for (Node *n = this; n != nullptr && cycle == nullptr; n = n->get_parent()) {
			cycle = Object::cast_to<SkyCycle>(n->get_node_or_null(NodePath("SkyCycle")));
		}
	}
	const float snow = (cycle != nullptr) ? cycle->get_snow_amount() : 0.0f;
	snow_caps->set_visible(snow > 0.01f);
	if (snow > 0.01f) {
		snow_mat->set_albedo(Color(0.96f, 0.97f, 1.0f, snow));
	}
}

String BerryBush::get_prompt() const {
	return (regrow_t > 0.0f) ? String() : String::utf8("按 E 采浆果");
}

Vector3 BerryBush::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.5f, 0.0f);
}

void BerryBush::interact(const Vector3 &p_player_pos) {
	if (berries == nullptr || regrow_t > 0.0f) {
		return; // 还没长出来
	}
	berries->set_visible(false);
	berries->set_scale(Vector3(1, 1, 1));
	regrow_t = REGROW_SECONDS;
	if (voice != nullptr) {
		voice->play(); // 采摘"啵"一声
	}
	emit_signal("berry_picked"); // 通知计分（GameDirector）
}

// 程序化合成采摘音"啵"（22050Hz 16-bit 单声道，无外部资源）：
// 下滑啁啾（520→240Hz 相位积分）× 快指数衰减，短促圆润的果摘手感
Ref<AudioStreamWAV> BerryBush::make_pop() {
	const int sr = 22050;
	const float dur = 0.09f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	float phase = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float f = 520.0f - 2800.0f * t;      // 频率快速下滑
		if (f < 120.0f) {
			f = 120.0f;
		}
		phase += 6.2831853f * f / (float)sr; // 相位积分（变频正弦）
		float env = Math::clamp(t * 600.0f, 0.0f, 1.0f); // ~1.7ms 起音防爆音
		float s = std::sin(phase) * env * std::exp(-t * 38.0f) * 0.5f;

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
