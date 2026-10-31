#include "dock_gull.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void DockGull::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_perched"), &DockGull::is_perched);
	ClassDB::bind_method(D_METHOD("is_flying"), &DockGull::is_flying);
	ClassDB::bind_method(D_METHOD("is_call_ready"), &DockGull::is_call_ready);
}

void DockGull::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			perch = Vector3(-6.0f, 1.08f, -11.55f);
			build();
			snd_call = make_call();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("GullVoice");
			voice->set_stream(snd_call);
			voice->set_volume_db(-14.0f);
			voice->set_max_distance(48.0f);
			add_child(voice);
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float wet = (sky != nullptr) ? sky->get_rain_amount() + sky->get_snow_amount() : 0.0f;
			const bool show = night < 0.35f && wet < 0.35f;
			const Vector3 pp = (player != nullptr) ? player->get_global_position() : Vector3(0, -999, 0);
			if (root == nullptr) {
				return;
			}
			if (state == 0) {
				root->set_visible(show);
				if (!show) {
					return;
				}
				root->set_position(perch + Vector3(0.0f, std::sin(t * 1.3f) * 0.01f, 0.0f));
				call_t -= delta;
				if (call_t <= 0.0f) {
					if (voice != nullptr) {
						voice->set_global_position(perch + Vector3(0, 0.3f, 0));
						voice->set_pitch_scale(0.95f + (float)(rng() % 100u) * 0.0015f);
						voice->set_volume_db(-15.0f);
						voice->play(0.0f);
					}
					call_t = 6.0f + (float)(rng() % 600u) * 0.01f;
				}
				if (pp.distance_to(perch) < 5.0f) {
					take_off(pp);
				}
			} else if (state == 1) {
				fly_t += delta;
				float u = Math::clamp(fly_t / fly_dur, 0.0f, 1.0f);
				Vector3 p = fly_from.lerp(fly_to, u);
				p.y += std::sin(u * 3.1415926f) * 4.5f;
				root->set_global_position(p);
				Vector3 dir = fly_to - fly_from;
				dir.y = 0.0f;
				if (dir.length_squared() > 0.001f) {
					root->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), 0.0f));
				}
				float flap = std::sin(t * 22.0f) * 0.9f;
				if (wing_l) {
					wing_l->set_rotation(Vector3(0.0f, 0.0f, flap));
				}
				if (wing_r) {
					wing_r->set_rotation(Vector3(0.0f, 0.0f, -flap));
				}
				if (u >= 1.0f) {
					state = 2;
					root->set_visible(false);
					respawn_t = 9.0f + (float)(rng() % 500u) * 0.01f;
				}
			} else {
				respawn_t -= delta;
				if (respawn_t <= 0.0f) {
					if (show && pp.distance_to(perch) > 8.0f) {
						state = 0;
						root->set_global_position(perch);
						root->set_rotation(Vector3(0.0f, 0.0f, 0.0f));
						root->set_visible(true);
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

void DockGull::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color white(0.94f, 0.95f, 0.93f);
	const Color gray(0.62f, 0.66f, 0.70f);
	const Color dark(0.20f, 0.22f, 0.24f);
	const Color beak(0.95f, 0.72f, 0.20f);
	const Color foot(0.90f, 0.55f, 0.22f);

	root = memnew(Node3D);
	root->set_name("GullModel");
	add_child(root);
	root->set_global_position(perch);

	// 身体、头、尾
	MeshInstance3D *body = memnew(MeshInstance3D);
	Ref<SphereMesh> bm;
	bm.instantiate();
	bm->set_radius(0.16f);
	bm->set_height(0.32f);
	bm->set_radial_segments(14);
	bm->set_rings(8);
	bm->set_material(meshutil::toon_material(toon, white, 0.35f));
	body->set_mesh(bm);
	body->set_position(Vector3(0.0f, 0.17f, 0.0f));
	body->set_scale(Vector3(0.85f, 1.0f, 1.45f));
	root->add_child(body);
	meshutil::add_sphere(root, toon, 0.10f, Vector3(0.0f, 0.33f, 0.17f), white);
	meshutil::add_sphere(root, toon, 0.045f, Vector3(0.0f, 0.31f, 0.27f), white);
	// 喙
	Ref<CylinderMesh> bk;
	bk.instantiate();
	bk->set_top_radius(0.0f);
	bk->set_bottom_radius(0.025f);
	bk->set_height(0.11f);
	bk->set_radial_segments(5);
	bk->set_rings(1);
	bk->set_material(meshutil::toon_material(toon, beak, 0.2f));
	MeshInstance3D *bkm = memnew(MeshInstance3D);
	bkm->set_mesh(bk);
	bkm->set_position(Vector3(0.0f, 0.30f, 0.35f));
	bkm->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	root->add_child(bkm);
	// 眼睛
	meshutil::add_sphere(root, toon, 0.016f, Vector3(-0.06f, 0.35f, 0.22f), dark);
	meshutil::add_sphere(root, toon, 0.016f, Vector3(0.06f, 0.35f, 0.22f), dark);
	// 尾羽
	meshutil::add_box(root, toon, Vector3(0.22f, 0.03f, 0.20f), Vector3(0.0f, 0.20f, -0.23f), white);

	// 收起的翅膀
	for (int i = 0; i < 2; i++) {
		Node3D *wing = memnew(Node3D);
		wing->set_name(i == 0 ? "WingL" : "WingR");
		wing->set_position(Vector3(i == 0 ? -0.12f : 0.12f, 0.22f, -0.02f));
		root->add_child(wing);
		meshutil::add_box(wing, toon, Vector3(0.04f, 0.16f, 0.34f), Vector3(0.0f, 0.03f, -0.03f), gray,
				Vector3(0.0f, 0.0f, i == 0 ? -0.18f : 0.18f));
		if (i == 0) {
			wing_l = wing;
		} else {
			wing_r = wing;
		}
	}
	// 橙色的脚
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(root, toon, Vector3(0.03f, 0.12f, 0.03f), Vector3(i == 0 ? -0.05f : 0.05f, 0.04f, 0.04f), foot);
	}
}

void DockGull::take_off(const Vector3 &p_player_pos) {
	state = 1;
	fly_t = 0.0f;
	fly_from = perch;
	Vector3 away = perch - p_player_pos;
	away.y = 0.0f;
	if (away.length_squared() < 0.01f) {
		away = Vector3(0.0f, 0.0f, -1.0f);
	}
	away = away.normalized();
	fly_to = perch + away * 22.0f + Vector3(0.0f, 7.0f, 0.0f);
	if (voice != nullptr) {
		voice->set_global_position(perch + Vector3(0, 0.3f, 0));
		voice->set_pitch_scale(1.08f);
		voice->set_volume_db(-12.0f);
		voice->play(0.0f);
	}
}

// 0.8 秒两声清亮的海鸥叫
Ref<AudioStreamWAV> DockGull::make_call() {
	const int sr = 22050;
	const float dur = 0.8f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int b = 0; b < 2; b++) {
			float bt = t - b * 0.34f;
			if (bt < 0.0f || bt > 0.23f) {
				continue;
			}
			float u = bt / 0.23f;
			float f = 1120.0f - 240.0f * u;
			float env = Math::clamp(bt / 0.02f, 0.0f, 1.0f) * Math::clamp((0.23f - bt) / 0.10f, 0.0f, 1.0f);
			samp += (std::sin(6.2831853f * f * bt) + 0.3f * std::sin(6.2831853f * f * 2.0f * bt)) * env * 0.4f;
		}
		int16_t q = (int16_t)Math::clamp(samp * 13000.0f, -32000.0f, 32000.0f);
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
