#include "swallow_nest.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void SwallowNest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_chick_count"), &SwallowNest::get_chick_count);
	ClassDB::bind_method(D_METHOD("get_swallows_visible"), &SwallowNest::get_swallows_visible);
	ClassDB::bind_method(D_METHOD("get_visits"), &SwallowNest::get_visits);
	ClassDB::bind_method(D_METHOD("get_rain_hide"), &SwallowNest::get_rain_hide);
}

void SwallowNest::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			}
			bool active = true;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				rain_hide = sky->get_rain_amount() > 0.45f || sky->get_snow_amount() > 0.3f;
				active = tod > 0.26f && tod < 0.74f && !rain_hide;
			}
			swallows_visible = active ? SWALLOW_COUNT : 0;

			// 绕檐飞行 + 定时俯冲喂食
			visit_timer -= delta;
			if (active && visit_timer <= 0.0f && feeding_index < 0) {
				feeding_index = (visits % SWALLOW_COUNT);
				feeding_t = 0.0f;
				visit_timer = 6.0f + (float)((visits * 7919 + 31) % 3000) / 1000.0f;
			}
			if (feeding_index >= 0) {
				feeding_t += delta;
				if (feeding_t > 1.4f) {
					feeding_index = -1;
					visits++;
					if (chirp_voice != nullptr && snd_chirp.is_valid()) {
						chirp_voice->set_pitch_scale(0.96f + 0.05f * (float)(visits % 5));
						chirp_voice->play(0.0f);
					}
				}
			}

			for (int i = 0; i < SWALLOW_COUNT; i++) {
				if (swallows[i] == nullptr) {
					continue;
				}
				swallows[i]->set_visible(active);
				if (!active) {
					continue;
				}
				const bool feeding = (feeding_index == i);
				if (feeding) {
					// 贴着巢边悬停
					const float w = std::sin(t * 10.0f) * 0.04f;
					swallows[i]->set_position(Vector3(0.0f + w, 0.28f, 0.55f));
					swallows[i]->set_rotation(Vector3(0.0f, 0.0f, std::sin(t * 6.0f) * 0.15f));
				} else {
					const float a = t * 1.1f + (float)i * 3.14159f;
					const float r = 2.6f + 0.3f * std::sin(t * 0.6f + (float)i);
					swallows[i]->set_position(Vector3(std::cos(a) * r, 1.2f + 0.4f * std::sin(t * 1.3f + (float)i), 1.2f + std::sin(a) * r));
					swallows[i]->set_rotation(Vector3(0.0f, -a + 1.5708f, 0.0f));
				}
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = std::sin(t * 16.0f + (float)i * 1.4f) * 0.7f;
						wings[i][w]->set_rotation(Vector3(0.0f, 0.0f, (w == 0 ? 1.0f : -1.0f) * (0.2f + flap)));
					}
				}
			}

			// 雏鸟：喂食时探头张望
			for (int i = 0; i < CHICK_COUNT; i++) {
				if (chicks[i] == nullptr) {
					continue;
				}
				const float bob = (feeding_index >= 0)
						? (0.035f * std::max(0.0f, std::sin(t * 9.0f + (float)i)))
						: (0.008f * std::sin(t * 1.7f + (float)i));
				chicks[i]->set_position(Vector3(-0.06f + 0.06f * (float)i, 0.12f + bob, 0.02f + 0.02f * (float)i));
				chicks[i]->set_rotation(Vector3((feeding_index >= 0 ? -0.35f : 0.0f), 0.0f, 0.0f));
			}
		} break;
		default:
			break;
	}
}

void SwallowNest::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color mud(0.52f, 0.42f, 0.30f);

	// 泥碗
	Ref<CylinderMesh> bowl;
	bowl.instantiate();
	bowl->set_top_radius(0.17f);
	bowl->set_bottom_radius(0.10f);
	bowl->set_height(0.13f);
	bowl->set_radial_segments(14);
	bowl->set_rings(1);
	bowl->set_material(meshutil::toon_material(toon, mud, 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(bowl);
	bmi->set_position(Vector3(0.0f, 0.0f, 0.0f));
	add_child(bmi);
	Ref<TorusMesh> rim;
	rim.instantiate();
	rim->set_inner_radius(0.155f);
	rim->set_outer_radius(0.19f);
	rim->set_rings(16);
	rim->set_ring_segments(4);
	rim->set_material(meshutil::toon_material(toon, Color(0.60f, 0.50f, 0.34f), 0.15f));
	MeshInstance3D *rmi = memnew(MeshInstance3D);
	rmi->set_mesh(rim);
	rmi->set_position(Vector3(0.0f, 0.065f, 0.0f));
	add_child(rmi);
	// 巢里的干草与暗影
	meshutil::add_box(this, toon, Vector3(0.24f, 0.02f, 0.24f), Vector3(0.0f, 0.075f, 0.0f), Color(0.72f, 0.60f, 0.38f));

	// 三只雏鸟
	for (int i = 0; i < CHICK_COUNT; i++) {
		Node3D *c = memnew(Node3D);
		c->set_name(String("Chick") + String::num_int64(i));
		c->set_position(Vector3(-0.06f + 0.06f * (float)i, 0.12f, 0.02f + 0.02f * (float)i));
		add_child(c);
		chicks[i] = c;
		meshutil::add_sphere(c, toon, 0.038f, Vector3(0.0f, 0.0f, 0.0f), Color(0.80f, 0.76f, 0.60f));
		meshutil::add_box(c, toon, Vector3(0.022f, 0.026f, 0.045f), Vector3(0.0f, 0.005f, 0.045f), Color(0.90f, 0.62f, 0.24f), Vector3(0.2f, 0.0f, 0.0f));
		meshutil::add_sphere(c, toon, 0.006f, Vector3(-0.015f, 0.022f, 0.026f), Color(0.10f, 0.09f, 0.08f));
		meshutil::add_sphere(c, toon, 0.006f, Vector3(0.015f, 0.022f, 0.026f), Color(0.10f, 0.09f, 0.08f));
	}

	// 两只成燕：深蓝背、白肚、剪刀尾
	for (int i = 0; i < SWALLOW_COUNT; i++) {
		Node3D *s = memnew(Node3D);
		s->set_name(String("Swallow") + String::num_int64(i));
		s->set_scale(Vector3(1.0f, 1.0f, 1.0f));
		add_child(s);
		swallows[i] = s;
		MeshInstance3D *body = meshutil::add_sphere(s, toon, 0.050f, Vector3(0.0f, 0.0f, 0.0f), Color(0.16f, 0.20f, 0.30f));
		body->set_scale(Vector3(0.85f, 0.85f, 1.5f));
		meshutil::add_sphere(s, toon, 0.033f, Vector3(0.0f, 0.02f, 0.075f), Color(0.18f, 0.22f, 0.34f));
		meshutil::add_box(s, toon, Vector3(0.018f, 0.012f, 0.030f), Vector3(0.0f, 0.015f, 0.115f), Color(0.16f, 0.14f, 0.12f));
		meshutil::add_box(s, toon, Vector3(0.07f, 0.012f, 0.11f), Vector3(-0.045f, 0.0f, -0.10f), Color(0.14f, 0.18f, 0.28f), Vector3(0.0f, -0.5f, 0.0f));
		meshutil::add_box(s, toon, Vector3(0.07f, 0.012f, 0.11f), Vector3(0.045f, 0.0f, -0.10f), Color(0.14f, 0.18f, 0.28f), Vector3(0.0f, 0.5f, 0.0f));
		for (int w = 0; w < 2; w++) {
			MeshInstance3D *wing = meshutil::add_box(s, toon, Vector3(0.16f, 0.012f, 0.07f),
					Vector3((w == 0 ? -1.0f : 1.0f) * 0.09f, 0.02f, 0.0f), Color(0.14f, 0.18f, 0.28f));
			wings[i][w] = wing;
		}
	}

	build_chirp();
	chirp_voice = memnew(AudioStreamPlayer3D);
	chirp_voice->set_name("ChirpVoice");
	chirp_voice->set_stream(snd_chirp);
	chirp_voice->set_volume_db(-10.0f);
	chirp_voice->set_max_distance(24.0f);
	add_child(chirp_voice);
}

void SwallowNest::build_chirp() {
	const int sr = 22050;
	const int n = (int)(0.35f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(8842u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float ph = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float f = 2600.0f + 900.0f * std::sin(tt * 70.0f);
		ph += 6.2831853f * f / (float)sr;
		const float env = std::sin(3.14159f * std::min(1.0f, tt / 0.35f)) * std::exp(-tt * 3.0f);
		const float noise = d01(rng) - 0.5f;
		buf[i] = (std::sin(ph) * 0.5f + noise * 0.22f) * env;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 24000.0f, -24000.0f, 24000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	snd_chirp = wav;
}
