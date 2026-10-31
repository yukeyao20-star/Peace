#include "bird_feeder.h"

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

using namespace godot;

void BirdFeeder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bird_count"), &BirdFeeder::get_bird_count);
	ClassDB::bind_method(D_METHOD("get_perched_count"), &BirdFeeder::get_perched_count);
	ClassDB::bind_method(D_METHOD("get_chirp_ready"), &BirdFeeder::get_chirp_ready);
	ClassDB::bind_method(D_METHOD("get_snow_cap_visible"), &BirdFeeder::get_snow_cap_visible);
}

int BirdFeeder::get_perched_count() const {
	int n = 0;
	for (const Bird &b : birds) {
		if (b.state == 0) {
			n++;
		}
	}
	return n;
}

bool BirdFeeder::get_snow_cap_visible() const {
	return snow_cap != nullptr && snow_cap->is_visible();
}

void BirdFeeder::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			Node *root = (get_parent() != nullptr) ? get_parent()->get_parent() : nullptr;
			if (root != nullptr) {
				sky = Object::cast_to<SkyCycle>(root->get_node_or_null("SkyCycle"));
				player = Object::cast_to<Node3D>(root->get_node_or_null("Player"));
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			if (snow_cap != nullptr) {
				snow_cap->set_visible(snow > 0.35f);
			}
			const Vector3 pp = (player != nullptr) ? player->get_global_position() : Vector3(0, -999, 0);
			const Vector3 pp_local = get_global_transform().affine_inverse().xform(pp);

			for (Bird &b : birds) {
				if (b.root == nullptr) {
					continue;
				}
				const float pd = pp_local.distance_to(b.perch);
				if (b.state == 0) {
					if (pd < 4.0f) {
						send_away(b, pp_local);
						continue;
					}
					// 停在托盘上：轻轻起伏、低头啄食
					b.wait_t -= delta;
					if (b.wait_t <= 0.0f) {
						b.peck_t = 0.42f;
						b.wait_t = 1.6f + (float)(rng() % 260u) * 0.012f;
					}
					float head_x = 0.0f;
					if (b.peck_t > 0.0f) {
						b.peck_t -= delta;
						const float u = 1.0f - b.peck_t / 0.42f;
						head_x = 0.9f * std::sin(Math::clamp(u, 0.0f, 1.0f) * 3.14159f);
					}
					if (b.head != nullptr) {
						const Vector3 hr = b.head->get_rotation();
						b.head->set_rotation(Vector3(Math::lerp(hr.x, head_x, 1.0f - std::exp(-14.0f * delta)), hr.y, hr.z));
					}
					b.root->set_position(b.perch + Vector3(0.0f, 0.008f * std::sin(t * 3.1f + b.phase), 0.0f));
					if (b.wing_l != nullptr && b.wing_r != nullptr) {
						b.wing_l->set_rotation(Vector3(0.0f, 0.0f, -0.15f));
						b.wing_r->set_rotation(Vector3(0.0f, 0.0f, 0.15f));
					}
				} else if (b.state == 1) {
					b.fly_t += delta;
					const float u = Math::clamp(b.fly_t / 1.1f, 0.0f, 1.0f);
					Vector3 p = b.fly_from.lerp(b.fly_to, u);
					p.y += std::sin(u * 3.14159f) * 1.4f;
					b.root->set_position(p);
					b.root->set_rotation(Vector3(0.0f, std::atan2(b.fly_to.x - b.fly_from.x, b.fly_to.z - b.fly_from.z), 0.0f));
					if (b.wing_l != nullptr && b.wing_r != nullptr) {
						const float flap = std::sin(t * 26.0f) * 0.9f;
						b.wing_l->set_rotation(Vector3(0.0f, 0.0f, -0.4f - flap));
						b.wing_r->set_rotation(Vector3(0.0f, 0.0f, 0.4f + flap));
					}
					if (u >= 1.0f) {
						b.root->set_visible(false);
						b.state = 2;
						b.wait_t = 4.0f + (float)(rng() % 500u) * 0.01f;
					}
				} else if (b.state == 2) {
					b.wait_t -= delta;
					if (b.wait_t <= 0.0f && pd > 5.5f) {
						b.state = 3;
						b.fly_t = 0.0f;
						b.fly_from = b.perch + Vector3((float)(rng() % 200u) * 0.03f - 3.0f, 3.2f, (float)(rng() % 200u) * 0.03f - 3.0f);
						b.fly_to = b.perch + Vector3(0.0f, 0.008f, 0.0f);
						b.root->set_visible(true);
						b.root->set_position(b.fly_from);
					}
				} else if (b.state == 3) {
					b.fly_t += delta;
					const float u = Math::clamp(b.fly_t / 1.2f, 0.0f, 1.0f);
					Vector3 p = b.fly_from.lerp(b.fly_to, u);
					p.y += std::sin(u * 3.14159f) * 0.9f;
					b.root->set_position(p);
					b.root->set_rotation(Vector3(0.0f, std::atan2(b.fly_to.x - b.fly_from.x, b.fly_to.z - b.fly_from.z), 0.0f));
					if (b.wing_l != nullptr && b.wing_r != nullptr) {
						const float flap = std::sin(t * 26.0f) * 0.9f;
						b.wing_l->set_rotation(Vector3(0.0f, 0.0f, -0.4f - flap));
						b.wing_r->set_rotation(Vector3(0.0f, 0.0f, 0.4f + flap));
					}
					if (u >= 1.0f) {
						b.state = 0;
						b.root->set_rotation(Vector3(0.0f, b.phase > 3.0f ? 0.35f : -0.35f, 0.0f));
						b.wait_t = 0.6f;
					}
				}
			}

			// 偶尔啾一声（只有鸟在托盘上才叫）
			chirp_t -= delta;
			if (chirp_t <= 0.0f) {
				chirp_t = 5.5f + (float)(rng() % 700u) * 0.01f;
				for (const Bird &b : birds) {
					if (b.state == 0 && voice != nullptr && snd_chirp.is_valid()) {
						voice->set_global_position(b.perch + Vector3(0.0f, 0.2f, 0.0f));
						voice->set_pitch_scale(0.95f + (float)(rng() % 100u) * 0.0018f);
						voice->set_volume_db(-16.0f);
						voice->play(0.0f);
						break;
					}
				}
			}
		} break;
		default:
			break;
	}
}

void BirdFeeder::send_away(Bird &p_b, const Vector3 &p_player_pos) {
	p_b.state = 1;
	p_b.fly_t = 0.0f;
	p_b.fly_from = p_b.perch;
	Vector3 away = p_b.perch - p_player_pos;
	away.y = 0.0f;
	if (away.length_squared() < 0.01f) {
		away = Vector3(1.0f, 0.0f, 0.0f);
	}
	away = away.normalized();
	p_b.fly_to = p_b.perch + away * 9.0f + Vector3(0.0f, 4.5f, 0.0f);
}

void BirdFeeder::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.46f, 0.33f, 0.20f);
	const Color wood_light(0.60f, 0.45f, 0.28f);

	meshutil::add_box(this, toon, Vector3(0.09f, 1.30f, 0.09f), Vector3(0.0f, 0.65f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.46f, 0.04f, 0.32f), Vector3(0.0f, 1.30f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(0.50f, 0.02f, 0.02f), Vector3(0.0f, 1.33f, 0.16f), wood);
	meshutil::add_box(this, toon, Vector3(0.50f, 0.02f, 0.02f), Vector3(0.0f, 1.33f, -0.16f), wood);
	meshutil::add_box(this, toon, Vector3(0.56f, 0.05f, 0.05f), Vector3(0.0f, 1.62f, 0.0f), wood);
	// 垂下来的松果
	meshutil::add_box(this, toon, Vector3(0.012f, 0.20f, 0.012f), Vector3(0.20f, 1.52f, 0.0f), Color(0.34f, 0.24f, 0.13f));
	meshutil::add_sphere(this, toon, 0.055f, Vector3(0.20f, 1.38f, 0.0f), Color(0.36f, 0.24f, 0.12f));

	// 雪帽：落雪时盖在托盘上
	snow_cap = meshutil::add_box(this, toon, Vector3(0.44f, 0.05f, 0.30f), Vector3(0.0f, 1.345f, 0.0f), Color(0.92f, 0.95f, 1.0f));
	snow_cap->set_name("SnowCap");
	snow_cap->set_visible(false);

	build_bird(0, Vector3(-0.13f, 1.325f, 0.02f), 9101u, true);
	build_bird(1, Vector3(0.13f, 1.325f, -0.02f), 9203u, false);

	snd_chirp = make_chirp();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("ChirpVoice");
	voice->set_stream(snd_chirp);
	voice->set_volume_db(-16.0f);
	voice->set_max_distance(18.0f);
	add_child(voice);
}

void BirdFeeder::build_bird(int p_index, const Vector3 &p_perch, unsigned p_seed, bool p_left) {
	std::mt19937 rng2(p_seed);
	Bird b;
	b.perch = p_perch;
	b.phase = (float)(rng2() % 628u) * 0.01f;
	b.wait_t = 0.5f + (float)(rng2() % 200u) * 0.01f;

	b.root = memnew(Node3D);
	b.root->set_name(String("Bird") + String::num_int64(p_index));
	b.root->set_position(b.perch);
	b.root->set_rotation(Vector3(0.0f, p_left ? 0.35f : -0.35f, 0.0f));
	add_child(b.root);

	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color body_col(0.42f, 0.31f, 0.21f);
	MeshInstance3D *body = meshutil::add_sphere(b.root, toon, 0.062f, Vector3(0.0f, 0.062f, 0.0f), body_col);
	body->set_scale(Vector3(1.0f, 0.92f, 1.18f));
	meshutil::add_sphere(b.root, toon, 0.040f, Vector3(0.0f, 0.058f, 0.045f), Color(0.86f, 0.76f, 0.58f)); // 浅色胸脯

	b.head = memnew(Node3D);
	b.head->set_name("Head");
	b.head->set_position(Vector3(0.0f, 0.105f, 0.048f));
	b.root->add_child(b.head);
	meshutil::add_sphere(b.head, toon, 0.042f, Vector3(0.0f, 0.0f, 0.0f), Color(0.36f, 0.27f, 0.18f));
	meshutil::add_box(b.head, toon, Vector3(0.022f, 0.016f, 0.045f), Vector3(0.0f, -0.004f, 0.055f), Color(0.86f, 0.58f, 0.20f));
	// 小黑帽与眼点
	meshutil::add_sphere(b.head, toon, 0.018f, Vector3(0.0f, 0.028f, 0.01f), Color(0.18f, 0.16f, 0.14f));
	meshutil::add_sphere(b.head, toon, 0.007f, Vector3(0.026f, 0.012f, 0.030f), Color(0.05f, 0.05f, 0.05f));
	meshutil::add_sphere(b.head, toon, 0.007f, Vector3(-0.026f, 0.012f, 0.030f), Color(0.05f, 0.05f, 0.05f));

	b.wing_l = memnew(Node3D);
	b.wing_l->set_position(Vector3(-0.055f, 0.070f, 0.0f));
	b.wing_l->set_rotation(Vector3(0.0f, 0.0f, -0.15f));
	b.root->add_child(b.wing_l);
	meshutil::add_box(b.wing_l, toon, Vector3(0.018f, 0.045f, 0.085f), Vector3(0.0f, 0.0f, 0.0f), Color(0.34f, 0.25f, 0.16f));

	b.wing_r = memnew(Node3D);
	b.wing_r->set_position(Vector3(0.055f, 0.070f, 0.0f));
	b.wing_r->set_rotation(Vector3(0.0f, 0.0f, 0.15f));
	b.root->add_child(b.wing_r);
	meshutil::add_box(b.wing_r, toon, Vector3(0.018f, 0.045f, 0.085f), Vector3(0.0f, 0.0f, 0.0f), Color(0.34f, 0.25f, 0.16f));

	meshutil::add_box(b.root, toon, Vector3(0.04f, 0.015f, 0.10f), Vector3(0.0f, 0.045f, -0.10f), Color(0.30f, 0.22f, 0.15f));

	birds.push_back(b);
}

// 两声短促的啾鸣（正弦滑音 + 一点气声），0.28 秒。
Ref<AudioStreamWAV> BirdFeeder::make_chirp() {
	const int sr = 22050;
	const int n = (int)(0.28f * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(33311u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float phase = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tsec = (float)i / (float)sr;
		const float gate = (tsec < 0.07f) ? 1.0f : ((tsec < 0.09f) ? 0.0f : (tsec < 0.20f ? 1.0f : 0.0f));
		const float freq = (tsec < 0.09f) ? (3200.0f + 1800.0f * tsec / 0.07f) : (3400.0f + 1200.0f * (tsec - 0.09f) / 0.11f);
		phase += 6.2831853f * freq / (float)sr;
		const float env = gate * std::exp(-tsec * 8.0f) * (1.0f - 0.3f * d01(rng));
		float samp = std::sin(phase) * env * 0.5f + (d01(rng) - 0.5f) * env * 0.12f;
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
