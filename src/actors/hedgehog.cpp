#include "hedgehog.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
#include "../world/footprint_manager.h"

using namespace godot;

bool Hedgehog::is_showing() const {
	return model != nullptr && model->is_visible();
}

bool Hedgehog::get_snuffle_playing() const {
	return voice != nullptr && voice->is_playing();
}

void Hedgehog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_showing"), &Hedgehog::is_showing);
	ClassDB::bind_method(D_METHOD("get_home"), &Hedgehog::get_home);
	ClassDB::bind_method(D_METHOD("is_curled"), &Hedgehog::is_curled);
	ClassDB::bind_method(D_METHOD("get_state_value"), &Hedgehog::get_state_value);
	ClassDB::bind_method(D_METHOD("get_snuffle_ready"), &Hedgehog::get_snuffle_ready);
	ClassDB::bind_method(D_METHOD("get_snuffle_playing"), &Hedgehog::get_snuffle_playing);
}

void Hedgehog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
				TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain != nullptr) {
					Vector3 p = get_global_position();
					p.y = terrain->get_terrain_height(p.x, p.z) + 0.01f;
					set_global_position(p);
				}
			}
			home = get_global_position();
			walk_from = home;
			walk_to = home;
			build();
			make_snuffle();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float wet = (sky != nullptr) ? sky->get_rain_amount() + sky->get_snow_amount() : 0.0f;
			const bool active = night > 0.5f && wet < 0.4f;
			if (model == nullptr) {
				return;
			}
			model->set_visible(active);
			if (!active) {
				return;
			}
			const Vector3 pp = (player != nullptr) ? player->get_global_position() : Vector3(0, -999, 0);
			const float pd = pp.distance_to(model->get_global_position());

			if (state == 1) {
				state_t += delta;
				model->set_scale(Vector3(1.0f, 0.55f, 1.0f));
				if (head != nullptr) {
					head->set_rotation(Vector3(0.55f, 0.0f, 0.0f));
				}
				for (int i = 0; i < 4; i++) {
					if (feet[i] != nullptr) {
						feet[i]->set_visible(false);
					}
				}
				if (pd > 5.0f && state_t > 1.2f) {
					state = 0;
					model->set_scale(Vector3(1, 1, 1));
					if (head != nullptr) {
						head->set_rotation(Vector3(0, 0, 0));
					}
					for (int i = 0; i < 4; i++) {
						if (feet[i] != nullptr) {
							feet[i]->set_visible(true);
						}
					}
					wait_t = 1.0f;
				}
				return;
			}

			if (pd < 3.5f) {
				state = 1;
				state_t = 0.0f;
				if (voice != nullptr) {
					voice->set_global_position(model->get_global_position() + Vector3(0, 0.2f, 0));
					voice->set_pitch_scale(1.05f);
					voice->set_volume_db(-20.0f);
					voice->play(0.0f);
				}
				return;
			}

			wait_t -= delta;
			if (wait_t <= 0.0f) {
				float a = (float)(rng() % 628u) * 0.01f;
				float r = 0.6f + (float)(rng() % 200u) * 0.006f;
				walk_from = model->get_global_position();
				walk_to = home + Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r);
				TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain != nullptr) {
					walk_to.y = terrain->get_terrain_height(walk_to.x, walk_to.z) + 0.01f;
				}
				walk_dur = 1.6f + (float)(rng() % 150u) * 0.012f;
				wait_t = walk_dur + 1.2f + (float)(rng() % 300u) * 0.01f;
			}
			Vector3 target = (wait_t > 0.0f) ? walk_to : walk_from;
			Vector3 p = model->get_global_position().lerp(target, 1.0f - std::exp(-2.2f * delta));
			p.y = walk_from.y;
			model->set_global_position(p);
			Vector3 dir = walk_to - walk_from;
			dir.y = 0.0f;
			if (dir.length_squared() > 0.001f) {
				model->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), 0.0f));
			}
			{
				Vector3 pd = p - last_print;
				pd.y = 0.0f;
				if (pd.length() > 0.25f && dir.length_squared() > 0.001f && FootprintManager::get_singleton() != nullptr) {
					last_print = p;
					const float yaw = std::atan2(dir.x, dir.z);
					const Vector3 side(std::cos(yaw) * 0.045f, 0.0f, -std::sin(yaw) * 0.045f);
					FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_PAW, p + Vector3(0, 0.01f, 0) + side, yaw, 0.30f);
					FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_PAW, p + Vector3(0, 0.01f, 0) - side, yaw, 0.30f);
				}
			}
			if (head != nullptr) {
				head->set_rotation(Vector3(std::sin(t * 3.0f) * 0.10f, std::sin(t * 1.1f) * 0.2f, 0.0f));
			}
			snuffle_t -= delta;
			if (snuffle_t <= 0.0f) {
				if (voice != nullptr && pd < 8.0f) {
					voice->set_global_position(model->get_global_position() + Vector3(0, 0.2f, 0));
					voice->set_pitch_scale(0.9f + (float)(rng() % 100u) * 0.002f);
					voice->set_volume_db(-22.0f);
					voice->play(0.0f);
				}
				snuffle_t = 4.0f + (float)(rng() % 500u) * 0.01f;
			}
		} break;
		default:
			break;
	}
}

void Hedgehog::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color fur(0.42f, 0.31f, 0.20f);
	const Color spike(0.28f, 0.22f, 0.15f);
	const Color cream(0.86f, 0.76f, 0.60f);
	const Color dark(0.10f, 0.08f, 0.06f);

	model = memnew(Node3D);
	model->set_name("HedgehogModel");
	add_child(model);
	model->set_global_position(home);

	MeshInstance3D *body = memnew(MeshInstance3D);
	Ref<SphereMesh> bm;
	bm.instantiate();
	bm->set_radius(0.20f);
	bm->set_height(0.40f);
	bm->set_radial_segments(12);
	bm->set_rings(7);
	bm->set_material(meshutil::toon_material(toon, fur, 0.35f));
	body->set_mesh(bm);
	body->set_position(Vector3(0, 0.15f, 0));
	body->set_scale(Vector3(1.05f, 0.72f, 1.3f));
	model->add_child(body);

	// 背上的刺
	Ref<CylinderMesh> sp;
	sp.instantiate();
	sp->set_top_radius(0.0f);
	sp->set_bottom_radius(0.022f);
	sp->set_height(0.12f);
	sp->set_radial_segments(4);
	sp->set_rings(1);
	sp->set_material(meshutil::toon_material(toon, spike, 0.2f));
	for (int row = 0; row < 3; row++) {
		int count = 8 - row * 2;
		for (int i = 0; i < count; i++) {
			float a = 6.2831853f * (float)i / (float)count + (float)row * 0.4f;
			float r = 0.15f - (float)row * 0.02f;
			MeshInstance3D *sm = memnew(MeshInstance3D);
			sm->set_mesh(sp);
			sm->set_position(Vector3(std::cos(a) * r, 0.26f - (float)row * 0.05f, std::sin(a) * r - 0.02f));
			sm->set_rotation(Vector3(0.45f * std::sin(a), 0.0f, -0.45f * std::cos(a)));
			model->add_child(sm);
		}
	}

	head = memnew(Node3D);
	head->set_name("Head");
	head->set_position(Vector3(0.0f, 0.13f, 0.24f));
	model->add_child(head);
	meshutil::add_sphere(head, toon, 0.11f, Vector3(0, 0, 0), cream);
	meshutil::add_sphere(head, toon, 0.028f, Vector3(0, -0.01f, 0.11f), dark);
	meshutil::add_sphere(head, toon, 0.012f, Vector3(-0.045f, 0.03f, 0.075f), dark);
	meshutil::add_sphere(head, toon, 0.012f, Vector3(0.045f, 0.03f, 0.075f), dark);
	for (int i = 0; i < 2; i++) {
		meshutil::add_sphere(head, toon, 0.028f, Vector3(i == 0 ? -0.08f : 0.08f, 0.07f, -0.01f), fur);
	}

	for (int i = 0; i < 4; i++) {
		float x = (i < 2) ? -0.10f : 0.10f;
		float z = (i % 2 == 0) ? 0.16f : -0.16f;
		feet[i] = memnew(Node3D);
		feet[i]->set_position(Vector3(x, 0.03f, z));
		model->add_child(feet[i]);
		meshutil::add_box(feet[i], toon, Vector3(0.05f, 0.04f, 0.08f), Vector3(0, 0, 0), dark);
	}
}

void Hedgehog::make_snuffle() {
	snd_snuffle = synth_snuffle();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("Snuffle");
	voice->set_stream(snd_snuffle);
	voice->set_volume_db(-22.0f);
	voice->set_max_distance(10.0f);
	add_child(voice);
}

// 0.4 秒的鼻息：两下短促的软噪声
Ref<AudioStreamWAV> Hedgehog::synth_snuffle() {
	const int sr = 22050;
	const float dur = 0.42f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1664525 + 1013904223) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.10f;
		float env = 0.0f;
		for (int k = 0; k < 2; k++) {
			float bt = t - (float)k * 0.16f;
			if (bt > 0.0f && bt < 0.10f) {
				env += std::sin(3.1415926f * bt / 0.10f);
			}
		}
		float samp = (lp * 1.4f + white * 0.2f) * env;
		int16_t q = (int16_t)Math::clamp(samp * 9000.0f, -32000.0f, 32000.0f);
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
