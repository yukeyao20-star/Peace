#include "wall_clock.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void WallClock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_hour_angle"), &WallClock::get_hour_angle);
	ClassDB::bind_method(D_METHOD("get_minute_angle"), &WallClock::get_minute_angle);
	ClassDB::bind_method(D_METHOD("get_pendulum_angle"), &WallClock::get_pendulum_angle);
	ClassDB::bind_method(D_METHOD("get_tick_ready"), &WallClock::get_tick_ready);
}

float WallClock::get_hour_angle() const {
	return (hour_pivot != nullptr) ? hour_pivot->get_rotation().z : 0.0f;
}

float WallClock::get_minute_angle() const {
	return (minute_pivot != nullptr) ? minute_pivot->get_rotation().z : 0.0f;
}

float WallClock::get_pendulum_angle() const {
	return (pendulum != nullptr) ? pendulum->get_rotation().z : 0.0f;
}

void WallClock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			Node *root = (get_parent() != nullptr) ? get_parent()->get_parent() : nullptr;
			if (root != nullptr) {
				sky = Object::cast_to<SkyCycle>(root->get_node_or_null("SkyCycle"));
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				if (hour_pivot != nullptr) {
					hour_pivot->set_rotation(Vector3(0.0f, 0.0f, -tod * 4.0f * 3.1415927f));
				}
				if (minute_pivot != nullptr) {
					minute_pivot->set_rotation(Vector3(0.0f, 0.0f, -tod * 48.0f * 3.1415927f));
				}
				const int q = (int)(tod * 96.0f);
				if (last_quarter >= 0 && q != last_quarter && tick_voice != nullptr && snd_tick.is_valid()) {
					tick_voice->set_pitch_scale(0.96f + 0.08f * (float)(q % 2));
					tick_voice->set_volume_db(-22.0f);
					tick_voice->play(0.0f);
				}
				last_quarter = q;
			}
			if (pendulum != nullptr) {
				pendulum->set_rotation(Vector3(0.0f, 0.0f, std::sin(t * 2.1f) * 0.16f));
			}
		} break;
		default:
			break;
	}
}

void WallClock::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.36f, 0.24f, 0.14f);
	const Color wood_light(0.52f, 0.38f, 0.23f);
	const Color cream(0.94f, 0.90f, 0.78f);
	const Color ink(0.22f, 0.17f, 0.12f);
	const float cy = 2.10f;

	// 背板与顶檐
	meshutil::add_box(this, toon, Vector3(0.62f, 0.74f, 0.05f), Vector3(0.0f, cy, 0.02f), wood);
	meshutil::add_box(this, toon, Vector3(0.68f, 0.06f, 0.08f), Vector3(0.0f, cy + 0.39f, 0.04f), wood_light);

	// 表盘
	Ref<CylinderMesh> face;
	face.instantiate();
	face->set_top_radius(0.24f);
	face->set_bottom_radius(0.24f);
	face->set_height(0.02f);
	face->set_radial_segments(32);
	face->set_rings(1);
	Ref<StandardMaterial3D> face_mat;
	face_mat.instantiate();
	face_mat->set_albedo(cream);
	face_mat->set_roughness(0.9f);
	face->set_material(face_mat);
	MeshInstance3D *face_mi = memnew(MeshInstance3D);
	face_mi->set_name("Face");
	face_mi->set_mesh(face);
	face_mi->set_position(Vector3(0.0f, cy, 0.055f));
	face_mi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	add_child(face_mi);

	// 外圈
	Ref<TorusMesh> rim;
	rim.instantiate();
	rim->set_inner_radius(0.235f);
	rim->set_outer_radius(0.275f);
	rim->set_rings(32);
	rim->set_ring_segments(6);
	rim->set_material(meshutil::toon_material(toon, wood_light, 0.4f));
	MeshInstance3D *rim_mi = memnew(MeshInstance3D);
	rim_mi->set_name("Rim");
	rim_mi->set_mesh(rim);
	rim_mi->set_position(Vector3(0.0f, cy, 0.062f));
	add_child(rim_mi);

	// 十二个刻度（12 点最长）
	for (int i = 0; i < 12; i++) {
		const float a = (float)i * 0.5235988f; // 30°
		const float r = 0.195f;
		const float h = (i % 3 == 0) ? 0.055f : 0.035f;
		meshutil::add_box(this, toon, Vector3(0.012f, h, 0.01f),
				Vector3(-std::sin(a) * r, cy + std::cos(a) * r, 0.068f), ink, Vector3(0.0f, 0.0f, -a));
	}

	// 时针（短）/ 分针（长）
	hour_pivot = memnew(Node3D);
	hour_pivot->set_name("HourHand");
	hour_pivot->set_position(Vector3(0.0f, cy, 0.072f));
	add_child(hour_pivot);
	meshutil::add_box(hour_pivot, toon, Vector3(0.026f, 0.125f, 0.012f), Vector3(0.0f, 0.062f, 0.0f), ink);
	meshutil::add_box(hour_pivot, toon, Vector3(0.048f, 0.03f, 0.012f), Vector3(0.0f, 0.02f, 0.0f), ink); // 根部配重

	minute_pivot = memnew(Node3D);
	minute_pivot->set_name("MinuteHand");
	minute_pivot->set_position(Vector3(0.0f, cy, 0.082f));
	add_child(minute_pivot);
	meshutil::add_box(minute_pivot, toon, Vector3(0.017f, 0.195f, 0.012f), Vector3(0.0f, 0.097f, 0.0f), ink);

	meshutil::add_sphere(this, toon, 0.022f, Vector3(0.0f, cy, 0.092f), Color(0.55f, 0.42f, 0.18f));

	// 钟摆
	pendulum = memnew(Node3D);
	pendulum->set_name("Pendulum");
	pendulum->set_position(Vector3(0.0f, cy - 0.36f, 0.03f));
	add_child(pendulum);
	meshutil::add_box(pendulum, toon, Vector3(0.012f, 0.34f, 0.012f), Vector3(0.0f, -0.17f, 0.0f), Color(0.42f, 0.36f, 0.28f));
	meshutil::add_sphere(pendulum, toon, 0.048f, Vector3(0.0f, -0.35f, 0.0f), Color(0.60f, 0.45f, 0.20f));

	// 刻度声
	snd_tick = make_tick();
	tick_voice = memnew(AudioStreamPlayer3D);
	tick_voice->set_name("TickVoice");
	tick_voice->set_stream(snd_tick);
	tick_voice->set_volume_db(-22.0f);
	tick_voice->set_max_distance(8.0f);
	tick_voice->set_position(Vector3(0.0f, cy, 0.08f));
	add_child(tick_voice);
}

// 一小下木头"嗒"：高频瞬态 + 一点木腔共鸣。
Ref<AudioStreamWAV> WallClock::make_tick() {
	const int sr = 22050;
	const int n = (int)(0.09f * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(5150u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < n; i++) {
		const float tsec = (float)i / (float)sr;
		const float env = std::exp(-tsec * 55.0f);
		const float noise = d01(rng) - 0.5f;
		const float ping = std::sin(tsec * 6.2831853f * 1800.0f) * 0.5f + std::sin(tsec * 6.2831853f * 950.0f) * 0.35f;
		float samp = (noise * 0.6f + ping * 0.8f) * env;
		samp = Math::clamp(samp, -1.0f, 1.0f);
		const int16_t q = (int16_t)(samp * 9000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}
