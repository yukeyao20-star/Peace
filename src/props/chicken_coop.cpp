#include "chicken_coop.h"

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
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void ChickenCoop::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &ChickenCoop::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &ChickenCoop::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &ChickenCoop::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &ChickenCoop::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &ChickenCoop::interact);
	ClassDB::bind_method(D_METHOD("get_chicken_count"), &ChickenCoop::get_chicken_count);
	ClassDB::bind_method(D_METHOD("get_egg_ready"), &ChickenCoop::get_egg_ready);
	ClassDB::bind_method(D_METHOD("get_egg_count"), &ChickenCoop::get_egg_count);
	ClassDB::bind_method(D_METHOD("get_scared_count"), &ChickenCoop::get_scared_count);
}

String ChickenCoop::get_prompt() const {
	return egg_ready ? String::utf8("按 E 收走草窝里的鸡蛋") : String::utf8("按 E 看看鸡窝");
}

Vector3 ChickenCoop::get_interact_pos() const {
	return get_global_position() + Vector3(0.6f, 0.6f, 0.8f);
}

void ChickenCoop::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (egg_ready) {
		egg_ready = false;
		egg_count++;
		lay_t = 0.0f;
		if (egg != nullptr) {
			egg->set_visible(false);
		}
		if (cluck_voice != nullptr && snd_cluck.is_valid()) {
			cluck_voice->set_pitch_scale(1.06f);
			cluck_voice->play(0.0f);
		}
		if (get_tree() != nullptr) {
			Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
			if (gd != nullptr) {
				gd->call("flash_hint", String::utf8("收了一枚温热的鸡蛋 · 母鸡咕咕地应了一声"), 3.0f);
			}
		}
	} else if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("草窝空着 · 母鸡还在一旁刨食"), 2.6f);
		}
	}
}

void ChickenCoop::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}

			// 定时下蛋
			if (!egg_ready) {
				lay_t += delta;
				if (lay_t > 18.0f) {
					egg_ready = true;
					lay_t = 0.0f;
					if (egg != nullptr) {
						egg->set_visible(true);
					}
				}
			}

			// 鸡群
			scared_count = 0;
			const Vector3 coop_pos = get_global_position();
			for (int i = 0; i < CHICKEN_COUNT; i++) {
				update_chicken(i, delta, player != nullptr ? player->get_global_position() : coop_pos + Vector3(0, 0, -999));
				if (chickens[i].scared > 0.0f) {
					scared_count++;
				}
			}

			// 偶尔咕咕叫
			if (t > next_cluck) {
				next_cluck = t + 6.0f + (float)((egg_count * 131 + 7) % 4000) / 1000.0f;
				if (cluck_voice != nullptr && snd_cluck.is_valid()) {
					cluck_voice->set_pitch_scale(0.94f + 0.04f * (float)(egg_count % 4));
					cluck_voice->play(0.0f);
				}
			}
		} break;
		default:
			break;
	}
}

void ChickenCoop::update_chicken(int p_index, float delta, const Vector3 &p_player) {
	Chicken &c = chickens[p_index];
	if (c.root == nullptr) {
		return;
	}
	const Vector3 home_pts[CHICKEN_COUNT] = {
		Vector3(1.0f, 0.0f, 1.2f),
		Vector3(-1.1f, 0.0f, 1.0f),
		Vector3(0.2f, 0.0f, 1.8f),
	};
	const Vector3 hp = home_pts[p_index];

	// 人靠近：扑棱着逃开
	const Vector3 world_pos = get_global_position() + c.pos;
	if (c.scared <= 0.0f && world_pos.distance_to(p_player) < 1.8f) {
		c.scared = 2.2f;
		const Vector3 away = c.pos - (p_player - get_global_position());
		c.heading = std::atan2(away.x, away.z);
	}
	if (c.scared > 0.0f) {
		c.scared -= delta;
		c.mode = 2;
		if (c.scared <= 0.0f) {
			c.mode = 0;
			c.mode_t = 1.5f;
		}
	} else {
		c.mode_t -= delta;
		if (c.mode_t <= 0.0f) {
			c.mode = (c.mode == 0) ? 1 : 0;
			c.mode_t = 1.6f + 1.8f * (0.5f + 0.5f * std::sin(t * 0.7f + c.phase));
		}
	}

	float speed = 0.30f;
	if (c.mode == 1) {
		speed = 0.0f;
	} else if (c.mode == 2) {
		speed = 1.7f;
	}

	// 远离窝太远就往回走
	const float dhome = Vector2(c.pos.x - hp.x, c.pos.z - hp.z).length();
	if (dhome > 3.0f && c.mode != 2) {
		const float want = std::atan2(hp.x - c.pos.x, hp.z - c.pos.z);
		float diff = want - c.heading;
		while (diff > 3.14159f) {
			diff -= 6.28318f;
		}
		while (diff < -3.14159f) {
			diff += 6.28318f;
		}
		c.heading += Math::clamp(diff, -2.0f * delta, 2.0f * delta);
	} else if (c.mode == 0) {
		c.heading += std::sin(t * 1.7f + c.phase) * 0.8f * delta;
	}

	if (speed > 0.0f) {
		c.pos.x += std::sin(c.heading) * speed * delta;
		c.pos.z += std::cos(c.heading) * speed * delta;
	}
	c.root->set_position(c.pos);
	c.root->set_rotation(Vector3(0.0f, c.heading, 0.0f));

	// 啄食：头一点一点；受惊：翅膀扑棱
	if (c.head != nullptr) {
		const float peck = (c.mode == 1) ? (0.38f + 0.26f * std::sin(t * 7.0f + c.phase)) : 0.0f;
		c.head->set_rotation(Vector3(peck, 0.0f, 0.0f));
	}
	for (int w = 0; w < 2; w++) {
		if (c.wings[w] != nullptr) {
			const float flap = (c.mode == 2) ? (0.7f + 0.5f * std::sin(t * 22.0f + c.phase)) : 0.0f;
			c.wings[w]->set_rotation(Vector3(0.0f, 0.0f, (w == 0 ? 1.0f : -1.0f) * flap));
		}
	}
}

void ChickenCoop::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(
				get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}
	const Color wood(0.46f, 0.31f, 0.18f);
	const Color roof(0.52f, 0.26f, 0.18f);

	// 鸡舍主体
	meshutil::add_box(this, toon, Vector3(1.20f, 1.00f, 0.95f), Vector3(0.0f, 0.55f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(1.34f, 0.06f, 1.08f), Vector3(-0.36f, 1.10f, 0.0f), roof, Vector3(0.0f, 0.0f, 0.42f));
	meshutil::add_box(this, toon, Vector3(1.34f, 0.06f, 1.08f), Vector3(0.36f, 1.10f, 0.0f), roof, Vector3(0.0f, 0.0f, -0.42f));
	// 门洞
	meshutil::add_box(this, toon, Vector3(0.36f, 0.44f, 0.03f), Vector3(0.0f, 0.30f, 0.49f), Color(0.14f, 0.11f, 0.09f));
	// 斜坡
	meshutil::add_box(this, toon, Vector3(0.44f, 0.04f, 0.62f), Vector3(0.0f, 0.16f, 0.76f), Color(0.56f, 0.40f, 0.24f), Vector3(0.38f, 0.0f, 0.0f));
	// 草窝
	meshutil::add_box(this, toon, Vector3(0.54f, 0.12f, 0.44f), Vector3(0.82f, 0.06f, 0.55f), Color(0.82f, 0.70f, 0.42f));

	// 鸡蛋模型
	Ref<SphereMesh> egg_mesh;
	egg_mesh.instantiate();
	egg_mesh->set_radius(0.07f);
	egg_mesh->set_height(0.17f);
	egg_mesh->set_radial_segments(8);
	egg_mesh->set_rings(5);
	egg_mesh->set_material(meshutil::toon_material(toon, Color(0.96f, 0.92f, 0.82f), 0.3f));
	egg = memnew(Node3D);
	egg->set_name("Egg");
	egg->set_position(Vector3(0.82f, 0.19f, 0.55f));
	MeshInstance3D *emi = memnew(MeshInstance3D);
	emi->set_mesh(egg_mesh);
	egg->add_child(emi);
	add_child(egg);

	// 三只母鸡
	const Color body_cols[CHICKEN_COUNT] = {
		Color(0.94f, 0.91f, 0.84f),
		Color(0.82f, 0.62f, 0.36f),
		Color(0.90f, 0.88f, 0.80f),
	};
	const Vector3 starts[CHICKEN_COUNT] = {
		Vector3(1.0f, 0.0f, 1.2f),
		Vector3(-1.1f, 0.0f, 1.0f),
		Vector3(0.2f, 0.0f, 1.8f),
	};
	for (int i = 0; i < CHICKEN_COUNT; i++) {
		Chicken &c = chickens[i];
		c.pos = starts[i];
		c.heading = (float)i * 2.1f;
		c.phase = (float)i * 1.7f;
		c.mode = 0;
		c.mode_t = 1.5f + 1.2f * (float)i;

		c.root = memnew(Node3D);
		c.root->set_name(String("Chicken") + String::num_int64(i));
		c.root->set_position(c.pos);
		c.root->set_scale(Vector3(0.85f, 0.85f, 0.85f));
		add_child(c.root);

		MeshInstance3D *body = meshutil::add_sphere(c.root, toon, 0.18f, Vector3(0.0f, 0.19f, 0.0f), body_cols[i]);
		body->set_scale(Vector3(1.15f, 0.92f, 1.0f));

		c.head = memnew(Node3D);
		c.head->set_name("Head");
		c.head->set_position(Vector3(0.0f, 0.36f, 0.16f));
		c.root->add_child(c.head);
		meshutil::add_sphere(c.head, toon, 0.095f, Vector3(0.0f, 0.02f, 0.0f), body_cols[i]);
		meshutil::add_box(c.head, toon, Vector3(0.030f, 0.060f, 0.030f), Vector3(0.0f, 0.09f, 0.02f), Color(0.82f, 0.24f, 0.18f));
		meshutil::add_box(c.head, toon, Vector3(0.045f, 0.022f, 0.030f), Vector3(0.0f, 0.02f, 0.10f), Color(0.92f, 0.66f, 0.22f));
		meshutil::add_sphere(c.head, toon, 0.018f, Vector3(0.0f, -0.03f, 0.09f), Color(0.78f, 0.22f, 0.18f));

		// 尾巴与两只翅膀
		meshutil::add_box(c.root, toon, Vector3(0.12f, 0.16f, 0.06f), Vector3(0.0f, 0.26f, -0.19f), body_cols[i], Vector3(-0.5f, 0.0f, 0.0f));
		for (int w = 0; w < 2; w++) {
			MeshInstance3D *wing = meshutil::add_box(c.root, toon, Vector3(0.035f, 0.16f, 0.19f),
					Vector3((w == 0 ? -1.0f : 1.0f) * 0.19f, 0.22f, 0.0f), body_cols[i]);
			c.wings[w] = wing;
		}
		// 腿
		meshutil::add_box(c.root, toon, Vector3(0.018f, 0.10f, 0.018f), Vector3(-0.055f, 0.05f, 0.02f), Color(0.86f, 0.62f, 0.22f));
		meshutil::add_box(c.root, toon, Vector3(0.018f, 0.10f, 0.018f), Vector3(0.055f, 0.05f, 0.02f), Color(0.86f, 0.62f, 0.22f));
	}

	build_cluck();
	cluck_voice = memnew(AudioStreamPlayer3D);
	cluck_voice->set_name("CluckVoice");
	cluck_voice->set_stream(snd_cluck);
	cluck_voice->set_volume_db(-9.0f);
	cluck_voice->set_max_distance(20.0f);
	add_child(cluck_voice);
}

void ChickenCoop::build_cluck() {
	const int sr = 22050;
	const int n = (int)(0.30f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(7311u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float ph = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float f = 900.0f - 420.0f * (tt / 0.30f) + 60.0f * std::sin(tt * 70.0f);
		ph += 6.2831853f * f / (float)sr;
		const float env = std::sin(3.14159f * Math::min(1.0f, tt / 0.30f)) * std::exp(-tt * 4.0f);
		const float noise = d01(rng) - 0.5f;
		buf[i] = (std::sin(ph) * 0.55f + noise * 0.30f) * env;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 28000.0f, -28000.0f, 28000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	snd_cluck = wav;
}
