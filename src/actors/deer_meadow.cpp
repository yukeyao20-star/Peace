#include "deer_meadow.h"

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

using namespace godot;

float DeerMeadow::get_distance_from_home() const {
	return root != nullptr ? root->get_global_position().distance_to(home) : 0.0f;
}

void DeerMeadow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_state"), &DeerMeadow::get_state);
	ClassDB::bind_method(D_METHOD("is_fleeing"), &DeerMeadow::is_fleeing);
	ClassDB::bind_method(D_METHOD("get_snort_ready"), &DeerMeadow::get_snort_ready);
	ClassDB::bind_method(D_METHOD("get_distance_from_home"), &DeerMeadow::get_distance_from_home);
}

void DeerMeadow::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
				TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
				Vector3 p = get_global_position();
				if (terrain != nullptr) {
					p.y = terrain->get_terrain_height(p.x, p.z) + 0.01f;
				}
				set_global_position(p);
			}
			home = get_global_position();
			build();
			snd_snort = make_snort();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("DeerVoice");
			voice->set_stream(snd_snort);
			voice->set_volume_db(-16.0f);
			voice->set_max_distance(40.0f);
			add_child(voice);
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float wet = (sky != nullptr) ? sky->get_rain_amount() + sky->get_snow_amount() : 0.0f;
			const bool visible = night < 0.65f && wet < 0.5f;
			if (root == nullptr) {
				return;
			}
			root->set_visible(visible);
			if (!visible) {
				return;
			}
			const Vector3 pp = (player != nullptr) ? player->get_global_position() : Vector3(0, -999, 0);

			if (state == 2) {
				state_t += delta;
				float u = Math::clamp(state_t / walk_dur, 0.0f, 1.0f);
				Vector3 p = walk_from.lerp(walk_to, u);
				root->set_global_position(p);
				Vector3 dir = walk_to - walk_from;
				dir.y = 0.0f;
				if (dir.length_squared() > 0.001f) {
					root->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), 0.0f));
				}
				for (int i = 0; i < 4; i++) {
					if (leg[i] != nullptr) {
						leg[i]->set_rotation(Vector3(std::sin(t * 16.0f + (float)i * 1.7f) * 0.55f, 0.0f, 0.0f));
					}
				}
				if (u >= 1.0f) {
					state = 0;
					wait_t = 2.0f;
					return_t = 8.0f;
				}
				return;
			}

			if (return_t > 0.0f) {
				return_t -= delta;
				if (return_t <= 0.0f && pp.distance_to(home) > 12.0f) {
					walk_from = root->get_global_position();
					walk_to = home + Vector3((float)(rng() % 60u) * 0.03f - 0.9f, 0.0f, (float)(rng() % 60u) * 0.03f - 0.9f);
					walk_dur = 3.2f;
					state = 1;
					flee_home = true;
					return;
				}
			}

			if (pp.distance_to(root->get_global_position()) < 14.0f && get_parent() != nullptr) {
				Node *gd = get_parent()->get_node_or_null("GameDirector");
				if (gd != nullptr) {
					gd->call("on_deer_seen");
				}
			}
			if (pp.distance_to(root->get_global_position()) < 7.0f) {
				start_flee(pp);
				return;
			}

			if (state == 1) {
				state_t += delta;
				float u = Math::clamp(state_t / walk_dur, 0.0f, 1.0f);
				Vector3 p = walk_from.lerp(walk_to, u);
				root->set_global_position(p);
				Vector3 dir = walk_to - walk_from;
				dir.y = 0.0f;
				if (dir.length_squared() > 0.001f) {
					root->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), 0.0f));
				}
				for (int i = 0; i < 4; i++) {
					if (leg[i] != nullptr) {
						leg[i]->set_rotation(Vector3(std::sin(t * 7.0f + (float)i * 1.6f) * 0.30f, 0.0f, 0.0f));
					}
				}
				if (u >= 1.0f) {
					state = 0;
					wait_t = 2.5f + (float)(rng() % 300u) * 0.01f;
					flee_home = false;
				}
			} else {
				wait_t -= delta;
				if (head != nullptr) {
					float graze = 0.35f + 0.10f * std::sin(t * 0.8f);
					head->set_rotation(Vector3(Math::lerp(head->get_rotation().x, graze, 1.0f - std::exp(-2.0f * delta)), 0.0f, 0.0f));
				}
				if (wait_t <= 0.0f) {
					choose_walk();
				}
			}
			snort_t -= delta;
		} break;
		default:
			break;
	}
}

void DeerMeadow::choose_walk() {
	Vector3 base = flee_home ? home : root->get_global_position();
	float a = (float)(rng() % 628u) * 0.01f;
	float r = 2.0f + (float)(rng() % 200u) * 0.012f;
	target = base + Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r);
	TerrainBuilder *terrain = nullptr;
	if (get_parent() != nullptr) {
		terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
	}
	if (terrain != nullptr) {
		target.y = terrain->get_terrain_height(target.x, target.z) + 0.01f;
	}
	walk_from = root->get_global_position();
	walk_to = target;
	walk_dur = 2.0f + (float)(rng() % 200u) * 0.012f;
	state = 1;
	state_t = 0.0f;
}

void DeerMeadow::start_flee(const Vector3 &p_player_pos) {
	Vector3 away = root->get_global_position() - p_player_pos;
	away.y = 0.0f;
	if (away.length_squared() < 0.01f) {
		away = Vector3(0.0f, 0.0f, 1.0f);
	}
	away = away.normalized();
	walk_from = root->get_global_position();
	walk_to = walk_from + away * 22.0f;
	TerrainBuilder *terrain = nullptr;
	if (get_parent() != nullptr) {
		terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
	}
	if (terrain != nullptr) {
		walk_to.y = terrain->get_terrain_height(walk_to.x, walk_to.z) + 0.01f;
	}
	walk_dur = 2.2f;
	state = 2;
	state_t = 0.0f;
	return_t = 0.0f;
	if (voice != nullptr) {
		voice->set_global_position(root->get_global_position() + Vector3(0, 0.8f, 0));
		voice->set_pitch_scale(0.95f + (float)(rng() % 100u) * 0.0015f);
		voice->set_volume_db(-14.0f);
		voice->play(0.0f);
	}
}

void DeerMeadow::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color fur(0.66f, 0.46f, 0.28f);
	const Color fur_light(0.80f, 0.64f, 0.44f);
	const Color cream(0.92f, 0.86f, 0.74f);
	const Color hoof(0.24f, 0.18f, 0.12f);

	root = memnew(Node3D);
	root->set_name("DeerModel");
	add_child(root);
	root->set_global_position(home);

	// 身体
	MeshInstance3D *body = memnew(MeshInstance3D);
	Ref<SphereMesh> bm;
	bm.instantiate();
	bm->set_radius(0.36f);
	bm->set_height(0.72f);
	bm->set_radial_segments(14);
	bm->set_rings(8);
	bm->set_material(meshutil::toon_material(toon, fur, 0.35f));
	body->set_mesh(bm);
	body->set_position(Vector3(0.0f, 1.02f, 0.0f));
	body->set_scale(Vector3(0.78f, 0.78f, 1.35f));
	root->add_child(body);
	meshutil::add_sphere(root, toon, 0.22f, Vector3(0.0f, 1.02f, 0.38f), fur_light);

	// 脖子与头
	Node3D *neck = memnew(Node3D);
	neck->set_position(Vector3(0.0f, 1.20f, 0.48f));
	root->add_child(neck);
	meshutil::add_box(neck, toon, Vector3(0.18f, 0.62f, 0.18f), Vector3(0.0f, 0.28f, 0.06f), fur_light, Vector3(-0.35f, 0.0f, 0.0f));
	head = memnew(Node3D);
	head->set_name("Head");
	head->set_position(Vector3(0.0f, 0.58f, 0.22f));
	neck->add_child(head);
	meshutil::add_sphere(head, toon, 0.16f, Vector3(0, 0, 0), fur);
	meshutil::add_sphere(head, toon, 0.09f, Vector3(0, -0.03f, 0.14f), cream);
	meshutil::add_sphere(head, toon, 0.014f, Vector3(-0.05f, 0.04f, 0.13f), Color(0.08f, 0.06f, 0.05f));
	meshutil::add_sphere(head, toon, 0.014f, Vector3(0.05f, 0.04f, 0.13f), Color(0.08f, 0.06f, 0.05f));
	// 耳朵与鹿角
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> ear;
		ear.instantiate();
		ear->set_top_radius(0.0f);
		ear->set_bottom_radius(0.045f);
		ear->set_height(0.13f);
		ear->set_radial_segments(5);
		ear->set_rings(1);
		ear->set_material(meshutil::toon_material(toon, fur_light));
		MeshInstance3D *em = memnew(MeshInstance3D);
		em->set_mesh(ear);
		em->set_position(Vector3(i == 0 ? -0.09f : 0.09f, 0.15f, 0.0f));
		em->set_rotation(Vector3(0.2f, 0.0f, i == 0 ? 0.45f : -0.45f));
		head->add_child(em);
		for (int k = 0; k < 2; k++) {
			meshutil::add_box(head, toon, Vector3(0.035f, 0.22f, 0.035f),
					Vector3(i == 0 ? -0.08f : 0.08f, 0.24f + (float)k * 0.10f, -0.02f + (float)k * 0.06f),
					Color(0.72f, 0.62f, 0.42f), Vector3(k == 0 ? 0.15f : -0.2f, 0.0f, i == 0 ? 0.18f : -0.18f));
		}
	}

	// 四条腿
	for (int i = 0; i < 4; i++) {
		float x = (i < 2) ? -0.22f : 0.22f;
		float z = (i % 2 == 0) ? 0.42f : -0.42f;
		leg[i] = memnew(Node3D);
		leg[i]->set_name(String("Leg") + String::num_int64(i));
		leg[i]->set_position(Vector3(x, 0.82f, z));
		root->add_child(leg[i]);
		meshutil::add_box(leg[i], toon, Vector3(0.09f, 0.78f, 0.09f), Vector3(0.0f, -0.39f, 0.0f), fur);
		meshutil::add_box(leg[i], toon, Vector3(0.10f, 0.07f, 0.13f), Vector3(0.0f, -0.79f, 0.02f), hoof);
	}
	// 小尾巴
	meshutil::add_sphere(root, toon, 0.07f, Vector3(0.0f, 1.15f, -0.50f), cream);
}

// 0.45 秒的鹿喷气/短鸣：一声带气流的“噗”
Ref<AudioStreamWAV> DeerMeadow::make_snort() {
	const int sr = 22050;
	const float dur = 0.45f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1664525 + 1013904223) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.10f;
		float tone = std::sin(6.2831853f * 220.0f * t) * 0.18f;
		float env = Math::clamp(t / 0.02f, 0.0f, 1.0f) * std::exp(-5.5f * t);
		float samp = (white - lp) * env * 0.7f + tone * env;
		int16_t q = (int16_t)Math::clamp(samp * 12000.0f, -32000.0f, 32000.0f);
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
