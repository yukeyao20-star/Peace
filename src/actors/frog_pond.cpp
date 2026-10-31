#include "frog_pond.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

namespace {
std::mt19937 rng_pick(919191u);
} // namespace

void FrogPond::_bind_methods() {
}

void FrogPond::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			if (get_parent() != nullptr) {
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			float delta = (float)get_process_delta_time();
			anim_t += delta;

			// 偶尔有青蛙叫一声
			croak_timer -= delta;
			if (croak_timer <= 0.0f) {
				std::uniform_real_distribution<float> pick_f(0.0f, 1.0f);
				for (const Frog &f : frogs) {
					if (!f.hidden && !f.jumping && f.root != nullptr && pick_f(rng_pick) < 0.5f) {
						if (voice != nullptr && snd_croak.is_valid()) {
							voice->set_global_position(f.root->get_global_position() + Vector3(0, 0.2f, 0));
							voice->set_stream(snd_croak);
							voice->set_pitch_scale(0.9f + pick_f(rng_pick) * 0.25f);
							voice->play();
						}
						break;
					}
				}
				croak_timer = 4.0f + pick_f(rng_pick) * 5.0f;
			}

			const Vector3 ppos = player != nullptr ? player->get_global_position() : Vector3(0, -999, 0);
			for (Frog &f : frogs) {
				if (f.root == nullptr) {
					continue;
				}
				if (f.hidden) {
					f.respawn_t -= delta;
					if (f.respawn_t <= 0.0f) {
						f.hidden = false;
						f.root->set_global_position(f.home);
						f.root->set_visible(true);
					} else {
						continue;
					}
				}
				if (f.jumping) {
					f.jump_t += delta;
					float u = Math::clamp(f.jump_t / f.jump_dur, 0.0f, 1.0f);
					Vector3 p = f.jump_start.lerp(f.jump_end, u);
					p.y += 4.0f * 0.42f * u * (1.0f - u);
					f.root->set_global_position(p);
					f.root->look_at(p + (f.jump_end - f.jump_start), Vector3(0, 1, 0));
					if (u >= 1.0f) {
						f.jumping = false;
						f.hidden = true;
						f.respawn_t = 10.0f;
						f.root->set_visible(false);
						start_ring(Vector3(f.jump_end.x, water_level + 0.012f, f.jump_end.z));
						if (voice != nullptr && snd_plop.is_valid()) {
							voice->set_global_position(f.jump_end);
							voice->set_stream(snd_plop);
							voice->set_pitch_scale(0.95f + (float)(rand() % 10) * 0.008f);
							voice->play();
						}
					}
				} else {
					// 缓慢鼓腮/呼吸
					float s = 1.0f + std::sin(anim_t * 2.2f + f.croak_phase) * 0.03f;
					f.root->set_scale(Vector3(s, 1.0f, s));
					if (!f.hidden && ppos.distance_to(f.root->get_global_position()) < 4.5f) {
						jump(f);
					}
				}
			}

			// 落水涟漪
			if (ring_active && ring != nullptr) {
				ring_t += delta;
				float u = Math::clamp(ring_t / ring_dur, 0.0f, 1.0f);
				float s = 0.5f + 1.6f * u;
				ring->set_scale(Vector3(s, 1.0f, s));
				Ref<StandardMaterial3D> mat = Ref<StandardMaterial3D>(
						Object::cast_to<StandardMaterial3D>(ring_mat.ptr()));
				if (mat.is_valid()) {
					Color c = mat->get_albedo();
					c.a = 0.55f * (1.0f - u);
					mat->set_albedo(c);
				}
				if (u >= 1.0f) {
					ring_active = false;
					ring->set_visible(false);
				}
			}
		} break;
		default:
			break;
	}
}

void FrogPond::build() {
	frogs.clear();
	// 湖面固定的荷花/荷叶点位（与 terrain_builder.cpp 的 lotus 位置一致）
	// 稍稍偏离荷花中心，蹲在旁边叶面上，避免花瓣穿进身体
	const Vector3 spots[] = {
		Vector3(-4.15f, water_level + 0.16f, 30.85f),
		Vector3(-7.15f, water_level + 0.16f, 21.85f),
		Vector3(-13.65f, water_level + 0.16f, 12.35f),
	};
	for (int i = 0; i < 3; i++) {
		spawn_frog(i, spots[i], 610001u + (unsigned)i * 131u);
	}

	// 落水涟漪（共用一只圆环）
	Ref<TorusMesh> torus;
	torus.instantiate();
	torus->set_inner_radius(0.05f);
	torus->set_outer_radius(0.075f);
	torus->set_rings(6);
	torus->set_ring_segments(24);
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_albedo(Color(0.94f, 0.98f, 1.0f, 0.55f));
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	torus->set_material(mat);
	ring_mat = mat;
	ring = memnew(MeshInstance3D);
	ring->set_name("FrogRing");
	ring->set_mesh(torus);
	ring->set_visible(false);
	add_child(ring);

	// 声音
	snd_croak = make_croak();
	snd_plop = make_plop();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("FrogVoice");
	voice->set_volume_db(-8.0f);
	voice->set_max_distance(35.0f);
	add_child(voice);
}

void FrogPond::spawn_frog(int p_index, const Vector3 &p_home, unsigned p_seed) {
	std::mt19937 rng(p_seed);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);

	Frog f;
	f.home = p_home;
	f.croak_phase = d01(rng) * 6.2831853f;

	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color body_c(0.24f, 0.46f, 0.22f);
	const Color belly_c(0.68f, 0.73f, 0.52f);
	const Color eye_c(0.86f, 0.87f, 0.62f);
	const Color pupil_c(0.05f, 0.06f, 0.04f);

	f.root = memnew(Node3D);
	f.root->set_name("Frog" + String::num_int64(p_index));
	f.root->set_global_position(p_home);
	float yaw = d01(rng) * 6.2831853f;
	f.root->set_rotation(Vector3(0.0f, yaw, 0.0f));
	add_child(f.root);

	auto add_sphere = [&](const Vector3 &p_pos, float p_radius, const Vector3 &p_scale,
			const Color &p_color) {
		Ref<SphereMesh> sp;
		sp.instantiate();
		sp->set_radius(p_radius);
		sp->set_height(p_radius * 2.0f);
		sp->set_radial_segments(10);
		sp->set_rings(6);
		sp->set_material(meshutil::toon_material(toon, p_color));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(sp);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(0, 0, 0), p_scale);
		t.set_basis(b);
		t.set_origin(p_pos);
		mi->set_transform(t);
		f.root->add_child(mi);
	};

	// 身体：压扁的绿球
	add_sphere(Vector3(0.0f, 0.055f, 0.0f), 0.075f, Vector3(1.25f, 0.70f, 1.35f), body_c);
	// 肚皮
	add_sphere(Vector3(0.0f, 0.035f, -0.035f), 0.055f, Vector3(1.05f, 0.45f, 1.05f), belly_c);
	// 两只凸起的眼睛
	for (int s = 0; s < 2; s++) {
		float sx = (s == 0 ? 1.0f : -1.0f) * 0.042f;
		add_sphere(Vector3(sx, 0.115f, -0.035f), 0.026f, Vector3(1, 1, 1), eye_c);
		add_sphere(Vector3(sx, 0.118f, -0.056f), 0.012f, Vector3(1, 1, 1), pupil_c);
	}
	// 后腿
	add_sphere(Vector3(0.070f, 0.035f, 0.055f), 0.040f, Vector3(0.55f, 0.5f, 0.9f), body_c);
	add_sphere(Vector3(-0.070f, 0.035f, 0.055f), 0.040f, Vector3(0.55f, 0.5f, 0.9f), body_c);

	frogs.push_back(f);
}

void FrogPond::jump(Frog &p_frog) {
	if (p_frog.jumping || p_frog.hidden) {
		return;
	}
	p_frog.jumping = true;
	p_frog.jump_t = 0.0f;
	p_frog.jump_start = p_frog.root->get_global_position();
	// 朝远离玩家的方向跳 2~3m
	Vector3 away = p_frog.jump_start - (player != nullptr ? player->get_global_position() : p_frog.jump_start);
	away.y = 0.0f;
	if (away.length_squared() < 0.01f) {
		away = Vector3(1.0f, 0.0f, 0.0f);
	}
	away = away.normalized();
	std::uniform_real_distribution<float> d(2.0f, 3.0f);
	std::uniform_real_distribution<float> a(-0.6f, 0.6f);
	float ang = a(rng_pick);
	Vector3 dir(std::cos(ang) * away.x - std::sin(ang) * away.z, 0.0f,
			std::sin(ang) * away.x + std::cos(ang) * away.z);
	p_frog.jump_end = p_frog.jump_start + dir * d(rng_pick);
	p_frog.jump_end.y = p_frog.jump_start.y;
}

void FrogPond::start_ring(const Vector3 &p_pos) {
	if (ring == nullptr) {
		return;
	}
	ring->set_global_position(p_pos);
	ring->set_scale(Vector3(0.5f, 1.0f, 0.5f));
	ring->set_visible(true);
	ring_active = true;
	ring_t = 0.0f;
}

// —— 程序化青蛙叫「呱—呱」 ——
Ref<AudioStreamWAV> FrogPond::make_croak() {
	const int sr = 22050;
	const float dur = 0.42f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(818181u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int b = 0; b < 2; b++) {
			float bt = t - b * 0.20f;
			if (bt < 0.0f || bt > 0.16f) {
				continue;
			}
			float env = Math::clamp(bt / 0.008f, 0.0f, 1.0f) * Math::exp(-bt * 18.0f);
			// 低频基音 + 快速幅度调制（青蛙的"呱"）
			float f0 = 150.0f * (1.0f - 0.25f * bt / 0.16f);
			float am = 0.55f + 0.45f * std::sin(6.2831853f * 33.0f * bt);
			float v = std::sin(6.2831853f * f0 * bt)
					+ 0.45f * std::sin(6.2831853f * f0 * 2.0f * bt)
					+ 0.18f * std::sin(6.2831853f * f0 * 3.0f * bt);
			v *= am * env;
			v += (d01(rng) - 0.5f) * 0.08f * env;
			samp += v;
		}
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

// —— 落水「噗通」 ——
Ref<AudioStreamWAV> FrogPond::make_plop() {
	const int sr = 22050;
	const float dur = 0.22f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(282828u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float env = Math::clamp(t / 0.006f, 0.0f, 1.0f) * Math::exp(-t * 22.0f);
		lp += 0.18f * ((d01(rng) - 0.5f) - lp);
		float v = std::sin(6.2831853f * (180.0f - 300.0f * t) * t) * 0.8f + lp * 1.6f;
		int16_t q = (int16_t)Math::clamp(v * env * 16000.0f, -32000.0f, 32000.0f);
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
