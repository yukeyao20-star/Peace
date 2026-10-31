#include "birdhouse.h"
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
#include <godot_cpp/classes/window.hpp>

#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void Birdhouse::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Birdhouse::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Birdhouse::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Birdhouse::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Birdhouse::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Birdhouse::interact);
	ClassDB::bind_method(D_METHOD("get_birds_visible"), &Birdhouse::get_birds_visible);
	ClassDB::bind_method(D_METHOD("get_peeks"), &Birdhouse::get_peeks);
}

String Birdhouse::get_prompt() const { return String::utf8("按 E 探头看看鸟屋"); }
Vector3 Birdhouse::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 1.5f, 0.0f); }
void Birdhouse::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	peeks++;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("鸟屋里铺着干草 · 三枚淡蓝色的小蛋"), 3.0f);
	}
}

void Birdhouse::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) break;
			if (player == nullptr) player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			if (sky == nullptr) sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			bool active = true;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				active = tod > 0.27f && tod < 0.73f && sky->get_snow_amount() < 0.4f && sky->get_rain_amount() < 0.5f;
			}
			float pd = 999.0f;
			if (player != nullptr) pd = player->get_global_position().distance_to(get_global_position() + Vector3(0, 1.4f, 0));
			birds_visible = 0;
			for (int i = 0; i < BIRD_COUNT; i++) {
				if (birds[i] == nullptr) continue;
				timers[i] -= delta;
				if (!active || pd < 2.0f) { if (!inside[i]) timers[i] = 1.5f; }
				if (timers[i] <= 0.0f) {
					inside[i] = !inside[i];
					timers[i] = 4.0f + (float)(i * 3) + (inside[i] ? 3.0f : 1.0f);
					if (chirp != nullptr && snd_chirp.is_valid()) chirp->play(0.0f);
				}
				const bool vis = active && pd >= 2.0f;
				birds[i]->set_visible(vis);
				if (!vis) continue;
				birds_visible++;
				if (inside[i]) {
					birds[i]->set_position(Vector3(0.0f, 1.28f, -0.04f));
				} else {
					birds[i]->set_position(Vector3((i == 0 ? -1.0f : 1.0f) * 0.10f, 1.22f + 0.02f * std::sin(t * 2.0f + (float)i), 0.22f));
					birds[i]->set_rotation(Vector3(0.0f, (i == 0 ? -1.0f : 1.0f) * 0.5f, 0.0f));
				}
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = std::sin(t * (inside[i] ? 4.0f : 14.0f) + (float)i) * (inside[i] ? 0.15f : 0.6f);
						wings[i][w]->set_rotation(Vector3(0.0f, 0.0f, (w == 0 ? 1.0f : -1.0f) * (0.2f + flap)));
					}
				}
			}
		} break;
		default: break;
	}
}

void Birdhouse::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 木柱
	meshutil::add_box(this, toon, Vector3(0.09f, 1.15f, 0.09f), Vector3(0.0f, 0.575f, 0.0f), Color(0.48f, 0.33f, 0.19f));
	// 鸟屋
	meshutil::add_box(this, toon, Vector3(0.36f, 0.32f, 0.32f), Vector3(0.0f, 1.30f, 0.0f), Color(0.62f, 0.44f, 0.26f));
	meshutil::add_box(this, toon, Vector3(0.46f, 0.06f, 0.40f), Vector3(-0.10f, 1.50f, 0.0f), Color(0.52f, 0.26f, 0.18f), Vector3(0.0f, 0.0f, 0.5f));
	meshutil::add_box(this, toon, Vector3(0.46f, 0.06f, 0.40f), Vector3(0.10f, 1.50f, 0.0f), Color(0.52f, 0.26f, 0.18f), Vector3(0.0f, 0.0f, -0.5f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.10f, 0.02f), Vector3(0.0f, 1.30f, 0.165f), Color(0.12f, 0.09f, 0.07f));
	meshutil::add_box(this, toon, Vector3(0.03f, 0.03f, 0.14f), Vector3(0.0f, 1.20f, 0.24f), Color(0.46f, 0.32f, 0.20f));
	// 两只蓝鸲
	for (int i = 0; i < BIRD_COUNT; i++) {
		Node3D *b = memnew(Node3D);
		b->set_name(String("Bluebird") + String::num_int64(i));
		add_child(b);
		birds[i] = b;
		meshutil::add_sphere(b, toon, 0.045f, Vector3(0.0f, 0.04f, 0.0f), Color(0.26f, 0.42f, 0.62f));
		meshutil::add_sphere(b, toon, 0.032f, Vector3(0.0f, 0.085f, 0.035f), Color(0.36f, 0.50f, 0.66f));
		meshutil::add_box(b, toon, Vector3(0.016f, 0.010f, 0.026f), Vector3(0.0f, 0.085f, 0.068f), Color(0.84f, 0.62f, 0.24f));
		for (int w = 0; w < 2; w++) {
			MeshInstance3D *wing = meshutil::add_box(b, toon, Vector3(0.075f, 0.008f, 0.045f),
					Vector3((w == 0 ? -1.0f : 1.0f) * 0.045f, 0.05f, 0.0f), Color(0.22f, 0.36f, 0.54f));
			wings[i][w] = wing;
		}
	}
	build_chirp();
	chirp = memnew(AudioStreamPlayer3D);
	chirp->set_name("ChirpVoice");
	chirp->set_stream(snd_chirp);
	chirp->set_volume_db(-11.0f);
	chirp->set_max_distance(18.0f);
	add_child(chirp);
}

void Birdhouse::build_chirp() {
	const int sr = 22050;
	const int n = (int)(0.22f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(3344u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float ph = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float f = 3000.0f + 700.0f * std::sin(tt * 55.0f);
		ph += 6.2831853f * f / (float)sr;
		const float env = std::sin(3.14159f * Math::min(1.0f, tt / 0.22f)) * std::exp(-tt * 4.0f);
		buf[i] = (std::sin(ph) * 0.5f + (d01(rng) - 0.5f) * 0.2f) * env;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 22000.0f, -22000.0f, 22000.0f);
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
