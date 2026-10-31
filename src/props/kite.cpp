#include "kite.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

Vector3 Kite::get_kite_pos() const {
	return kite != nullptr ? kite->get_global_position() : get_global_position();
}

void Kite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Kite::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Kite::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Kite::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Kite::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Kite::interact);
	ClassDB::bind_method(D_METHOD("get_flying"), &Kite::get_flying);
	ClassDB::bind_method(D_METHOD("get_kite_pos"), &Kite::get_kite_pos);
	ClassDB::bind_method(D_METHOD("get_flutter_ready"), &Kite::get_flutter_ready);
}

void Kite::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
				if (terrain != nullptr) {
					Vector3 p = get_global_position();
					p.y = terrain->get_terrain_height(p.x, p.z) + 0.01f;
					set_global_position(p);
				}
			}
			home = get_global_position();
			add_to_group("interactables");
			build();
			make_flutter();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (kite == nullptr) {
				return;
			}
			if (!flying) {
				Vector3 p = home + Vector3(0.0f, 0.25f, 0.0f);
				kite->set_global_position(p);
				kite->set_rotation(Vector3(1.35f, 0.0f, 0.0f));
				if (string != nullptr) {
					string->set_visible(false);
				}
				if (flut != nullptr && flut->is_playing()) {
					flut->stop();
				}
				return;
			}
			// 风筝在半空画八字，风大时摆得更明显
			float wind = 0.4f;
			if (player != nullptr) {
				float d = player->get_global_position().distance_to(home);
				if (d > 18.0f) {
					flying = false;
					return;
				}
			}
			Vector3 p = home + Vector3(
					std::sin(t * 0.55f) * (2.2f + wind),
					8.5f + std::sin(t * 1.3f) * 0.45f,
					std::cos(t * 0.37f) * 2.4f - 1.0f);
			kite->set_global_position(p);
			kite->set_rotation(Vector3(
					0.15f + std::sin(t * 1.7f) * 0.12f,
					std::sin(t * 0.5f) * 0.4f,
					std::sin(t * 1.1f) * 0.22f));
			if (tail != nullptr) {
				tail->set_rotation(Vector3(std::sin(t * 4.5f) * 0.25f, 0.0f, std::sin(t * 3.2f) * 0.35f));
			}
			if (string != nullptr) {
				Vector3 hand = home + Vector3(0.0f, 1.1f, 0.0f);
				if (player != nullptr) {
					hand = player->get_global_position() + Vector3(0.0f, 1.2f, 0.0f);
				}
				Vector3 mid = (hand + p) * 0.5f;
				float len = hand.distance_to(p);
				string->set_visible(true);
				string->set_global_position(mid);
				string->set_scale(Vector3(1.0f, 1.0f, len));
				string->look_at(p, Vector3(0.0f, 1.0f, 0.0f));
			}
			if (flut != nullptr && !flut->is_playing()) {
				flut->play(0.0f);
			}
		} break;
		default:
			break;
	}
}

String Kite::get_prompt() const {
	return flying ? String::utf8("按 E 收线") : String::utf8("按 E 放风筝");
}

Vector3 Kite::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.8f, 0.0f);
}

float Kite::get_interact_distance() const {
	return 2.6f;
}

void Kite::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	flying = !flying;
	if (flying && get_parent() != nullptr) {
		Node *gdk = get_parent()->get_node_or_null("GameDirector");
		if (gdk != nullptr) {
			gdk->call("on_kite_launched");
		}
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("flash_hint", flying ? String::utf8("风筝升起来了 · 再按 E 收线") : String::utf8("风筝收回草地"), 2.8f);
		}
	}
}

void Kite::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color red(0.86f, 0.30f, 0.22f);
	const Color cream(0.94f, 0.88f, 0.70f);
	const Color stick(0.55f, 0.42f, 0.24f);

	// 地面上的线轴
	meshutil::add_box(this, toon, Vector3(0.16f, 0.10f, 0.16f), Vector3(0.0f, 0.05f, 0.0f), stick);

	kite = memnew(Node3D);
	kite->set_name("KiteBody");
	add_child(kite);
	// 菱形风筝面：两块旋转 45° 的薄板
	MeshInstance3D *panel = memnew(MeshInstance3D);
	Ref<BoxMesh> pm;
	pm.instantiate();
	pm->set_size(Vector3(0.72f, 0.72f, 0.02f));
	pm->set_material(meshutil::toon_material(toon, red, 0.4f));
	panel->set_mesh(pm);
	panel->set_rotation(Vector3(0.0f, 0.0f, 0.7853982f));
	kite->add_child(panel);
	MeshInstance3D *trim = memnew(MeshInstance3D);
	Ref<BoxMesh> tm;
	tm.instantiate();
	tm->set_size(Vector3(0.50f, 0.50f, 0.021f));
	tm->set_material(meshutil::toon_material(toon, cream, 0.3f));
	trim->set_mesh(tm);
	trim->set_rotation(Vector3(0.0f, 0.0f, 0.7853982f));
	trim->set_position(Vector3(0.0f, 0.0f, 0.002f));
	kite->add_child(trim);
	// 竹骨
	meshutil::add_box(kite, toon, Vector3(0.86f, 0.025f, 0.025f), Vector3(0, 0, 0.02f), stick);
	meshutil::add_box(kite, toon, Vector3(0.025f, 0.86f, 0.025f), Vector3(0, 0, 0.02f), stick);

	// 尾巴
	tail = memnew(Node3D);
	tail->set_name("KiteTail");
	tail->set_position(Vector3(0.0f, -0.42f, 0.02f));
	kite->add_child(tail);
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(tail, toon, Vector3(0.10f, 0.22f, 0.015f), Vector3(std::sin((float)i) * 0.04f, -0.13f - (float)i * 0.20f, 0.0f),
				(i % 2 == 0) ? red : cream, Vector3(0.0f, 0.0f, std::sin((float)i * 1.3f) * 0.3f));
	}

	// 风筝线：单位长度沿 Z 的细盒，逐帧缩放/朝向
	Ref<BoxMesh> sm;
	sm.instantiate();
	sm->set_size(Vector3(0.012f, 0.012f, 1.0f));
	sm->set_material(meshutil::toon_material(toon, Color(0.82f, 0.78f, 0.62f)));
	string = memnew(MeshInstance3D);
	string->set_name("KiteString");
	string->set_mesh(sm);
	string->set_visible(false);
	add_child(string);
}

void Kite::make_flutter() {
	snd_flutter = synth_flutter();
	flut = memnew(AudioStreamPlayer3D);
	flut->set_name("KiteFlutter");
	flut->set_stream(snd_flutter);
	flut->set_volume_db(-24.0f);
	flut->set_max_distance(22.0f);
	add_child(flut);
}

// 2 秒循环的纸面风声：柔和的带通噪声，偶尔一阵抖动
Ref<AudioStreamWAV> Kite::synth_flutter() {
	const int sr = 22050;
	const float dur = 2.0f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1664525 + 1013904223) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.05f;
		float gust = 0.55f + 0.45f * std::sin(6.2831853f * 0.7f * t + std::sin(6.2831853f * 0.13f * t) * 2.0f);
		float fade = 1.0f;
		if (i < 2200) {
			fade = (float)i / 2200.0f;
		}
		if (i > n - 2200) {
			fade = (float)(n - i) / 2200.0f;
		}
		float samp = lp * gust * fade * 0.9f;
		int16_t q = (int16_t)Math::clamp(samp * 9000.0f, -32000.0f, 32000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
	wav->set_loop_end(n);
	wav->set_data(bytes);
	return wav;
}
