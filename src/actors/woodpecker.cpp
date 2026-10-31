#include "woodpecker.h"

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

float Woodpecker::get_peck_offset() const {
	return bird != nullptr ? bird->get_position().x : 0.0f;
}

void Woodpecker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_perched"), &Woodpecker::is_perched);
	ClassDB::bind_method(D_METHOD("get_knock_ready"), &Woodpecker::get_knock_ready);
	ClassDB::bind_method(D_METHOD("get_peck_offset"), &Woodpecker::get_peck_offset);
}

void Woodpecker::_notification(int p_what) {
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
				if (terrain != nullptr) {
					Vector3 p = get_global_position();
					p.y = terrain->get_terrain_height(p.x, p.z) + 0.01f;
					set_global_position(p);
				}
			}
			home = get_global_position();
			build();
			snd_knock = make_knock();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("WoodpeckerVoice");
			voice->set_stream(snd_knock);
			voice->set_volume_db(-14.0f);
			voice->set_max_distance(38.0f);
			add_child(voice);
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float wet = (sky != nullptr) ? sky->get_rain_amount() + sky->get_snow_amount() : 0.0f;
			const bool show = night < 0.35f && wet < 0.4f;
			const Vector3 pp = (player != nullptr) ? player->get_global_position() : Vector3(0, -999, 0);
			if (bird == nullptr) {
				return;
			}
			if (state == 0) {
				bird->set_visible(show);
				if (!show) {
					return;
				}
				peck_t -= delta;
				if (peck_t <= 0.0f) {
					peck_t = 0.55f;
					if (voice != nullptr) {
						voice->set_global_position(home + Vector3(0.3f, 1.6f, 0.0f));
						voice->set_pitch_scale(0.95f + (float)(rng() % 100u) * 0.0015f);
						voice->set_volume_db(-13.0f);
						voice->play(0.0f);
					}
					knock_t = 5.0f + (float)(rng() % 600u) * 0.01f;
					if (pp.distance_to(home) < 26.0f && get_parent() != nullptr) {
						Node *gd = get_parent()->get_node_or_null("GameDirector");
						if (gd != nullptr) {
							gd->call("on_woodpecker_heard");
						}
					}
				}
				float amp = (peck_t > 0.0f) ? std::fabs(std::sin(t * 26.0f)) * 0.07f : 0.0f;
				bird->set_position(Vector3(0.30f - amp, 1.55f, 0.0f));
				if (head != nullptr) {
					head->set_rotation(Vector3(0.0f, 0.0f, amp * 6.0f));
				}
				if (pp.distance_to(home) < 6.0f) {
					take_off(pp);
				}
			} else if (state == 1) {
				fly_t += delta;
				float u = Math::clamp(fly_t / fly_dur, 0.0f, 1.0f);
				Vector3 p = fly_from.lerp(fly_to, u);
				p.y += std::sin(u * 3.1415926f) * 4.0f;
				bird->set_global_position(p);
				Vector3 dir = fly_to - fly_from;
				dir.y = 0.0f;
				if (dir.length_squared() > 0.001f) {
					bird->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), 0.0f));
				}
				if (u >= 1.0f) {
					state = 2;
					bird->set_visible(false);
					respawn_t = 12.0f + (float)(rng() % 500u) * 0.01f;
				}
			} else {
				respawn_t -= delta;
				if (respawn_t <= 0.0f) {
					if (show && pp.distance_to(home) > 8.0f) {
						state = 0;
						bird->set_global_position(home);
						bird->set_rotation(Vector3(0.0f, 0.0f, 0.0f));
						bird->set_visible(true);
					} else {
						respawn_t = 1.5f;
					}
				}
			}
		} break;
		default:
			break;
	}
}

void Woodpecker::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color bark(0.33f, 0.24f, 0.16f);
	const Color bark_light(0.44f, 0.33f, 0.22f);

	// 枯树干与几根断枝
	Ref<CylinderMesh> trunk;
	trunk.instantiate();
	trunk->set_top_radius(0.14f);
	trunk->set_bottom_radius(0.20f);
	trunk->set_height(3.2f);
	trunk->set_radial_segments(10);
	trunk->set_rings(2);
	trunk->set_material(meshutil::toon_material(toon, bark, 0.25f));
	MeshInstance3D *trunk_mi = memnew(MeshInstance3D);
	trunk_mi->set_name("Snag");
	trunk_mi->set_mesh(trunk);
	trunk_mi->set_position(Vector3(0.0f, 1.6f, 0.0f));
	add_child(trunk_mi);
	meshutil::add_box(this, toon, Vector3(0.09f, 0.7f, 0.09f), Vector3(-0.32f, 2.2f, 0.1f), bark_light, Vector3(0.0f, 0.0f, 0.65f));
	meshutil::add_box(this, toon, Vector3(0.09f, 0.5f, 0.09f), Vector3(0.26f, 1.5f, -0.12f), bark_light, Vector3(0.2f, 0.0f, -0.55f));

	// 鸟：黑白羽 + 红顶
	bird = memnew(Node3D);
	bird->set_name("WoodpeckerBird");
	add_child(bird);
	bird->set_position(Vector3(0.30f, 1.55f, 0.0f));
	MeshInstance3D *body = memnew(MeshInstance3D);
	Ref<SphereMesh> bm;
	bm.instantiate();
	bm->set_radius(0.075f);
	bm->set_height(0.15f);
	bm->set_radial_segments(10);
	bm->set_rings(6);
	bm->set_material(meshutil::toon_material(toon, Color(0.16f, 0.16f, 0.18f), 0.35f));
	body->set_mesh(bm);
	body->set_scale(Vector3(0.85f, 1.5f, 0.75f));
	body->set_position(Vector3(0.0f, 0.0f, 0.0f));
	bird->add_child(body);
	meshutil::add_sphere(bird, toon, 0.045f, Vector3(-0.02f, 0.02f, 0.05f), Color(0.92f, 0.90f, 0.84f));
	head = memnew(Node3D);
	head->set_name("Head");
	head->set_position(Vector3(0.0f, 0.12f, 0.0f));
	bird->add_child(head);
	meshutil::add_sphere(head, toon, 0.055f, Vector3(0.0f, 0.0f, 0.0f), Color(0.16f, 0.16f, 0.18f));
	meshutil::add_sphere(head, toon, 0.038f, Vector3(0.0f, 0.04f, 0.01f), Color(0.86f, 0.18f, 0.14f));
	meshutil::add_box(head, toon, Vector3(0.10f, 0.028f, 0.028f), Vector3(-0.075f, 0.005f, 0.0f), Color(0.28f, 0.24f, 0.18f));
	meshutil::add_sphere(head, toon, 0.012f, Vector3(-0.02f, 0.015f, 0.045f), Color(0.06f, 0.05f, 0.05f));
}

void Woodpecker::take_off(const Vector3 &p_player_pos) {
	state = 1;
	fly_t = 0.0f;
	fly_from = home;
	Vector3 away = home - p_player_pos;
	away.y = 0.0f;
	if (away.length_squared() < 0.01f) {
		away = Vector3(1.0f, 0.0f, 0.0f);
	}
	away = away.normalized();
	fly_to = home + away * 18.0f + Vector3(0.0f, 6.0f, 0.0f);
}

// 0.6 秒的连续啄木声：短促、干硬的"笃笃笃"
Ref<AudioStreamWAV> Woodpecker::make_knock() {
	const int sr = 22050;
	const float dur = 0.62f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int k = 0; k < 9; k++) {
			float bt = t - (float)k * 0.062f;
			if (bt < 0.0f || bt > 0.03f) {
				continue;
			}
			float env = std::exp(-180.0f * bt);
			float f = 900.0f + (float)(k % 3) * 120.0f;
			samp += (std::sin(6.2831853f * f * bt) * 0.6f + std::sin(6.2831853f * f * 1.7f * bt) * 0.25f) * env;
		}
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
