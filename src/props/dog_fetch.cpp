#include "dog_fetch.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void DogFetch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &DogFetch::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &DogFetch::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &DogFetch::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &DogFetch::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &DogFetch::interact);
	ClassDB::bind_method(D_METHOD("get_state"), &DogFetch::get_state);
	ClassDB::bind_method(D_METHOD("get_fetch_count"), &DogFetch::get_fetch_count);
	ClassDB::bind_method(D_METHOD("get_dog_pos"), &DogFetch::get_dog_pos);
	ClassDB::bind_method(D_METHOD("get_ball_pos"), &DogFetch::get_ball_pos);
	ClassDB::bind_method(D_METHOD("get_wag"), &DogFetch::get_wag);
}

String DogFetch::get_prompt() const {
	return state == IDLE ? String::utf8("按 E 把球扔出去") : String::utf8("小狗正追着球跑…");
}

Vector3 DogFetch::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.4f, 0.0f);
}

void DogFetch::interact(const Vector3 &p_player_pos) {
	if (state != IDLE) {
		return;
	}
	start_throw(p_player_pos);
}

void DogFetch::start_throw(const Vector3 &p_player_pos) {
	Vector3 f(0.0f, 0.0f, 1.0f);
	if (player != nullptr && Object::cast_to<Player>(player) != nullptr) {
		f = Object::cast_to<Player>(player)->get_facing();
		f.y = 0.0f;
		if (f.length_squared() < 0.01f) {
			f = Vector3(0.0f, 0.0f, 1.0f);
		}
		f = f.normalized();
	}
	ball_pos = p_player_pos + f * 0.8f + Vector3(0.0f, 1.1f, 0.0f);
	ball_vel = f * 5.5f + Vector3(0.0f, 3.2f, 0.0f);
	if (ball != nullptr) {
		ball->set_visible(true);
		ball->set_global_position(ball_pos);
	}
	state = THROWN;
	state_t = 0.0f;
	if (bark_voice != nullptr && snd_bark.is_valid()) {
		bark_voice->play(0.0f);
	}
}

void DogFetch::_notification(int p_what) {
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
			state_t += delta;
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}
			if (terrain == nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(root->find_child("Terrain", true, false));
			}
			auto ground = [&](float x, float z) {
				return (terrain != nullptr) ? terrain->get_terrain_height(x, z) : 0.0f;
			};

			const Vector3 base = get_global_position();
			const float speed = dog_vel.length();
			if (state == THROWN) {
				ball_vel.y -= 12.0f * delta;
				ball_pos += ball_vel * delta;
				const float gy = ground(ball_pos.x, ball_pos.z) + 0.12f;
				if (ball_pos.y <= gy) {
					ball_pos.y = gy;
					ball_vel = Vector3();
					state = CHASE;
					state_t = 0.0f;
				}
				if (state_t > 3.0f) {
					state = CHASE;
					state_t = 0.0f;
				}
				if (ball != nullptr) {
					ball->set_global_position(ball_pos);
				}
			} else if (state == CHASE) {
				const Vector3 target(ball_pos.x, 0.0f, ball_pos.z);
				const Vector3 dog_world = base + dog_pos;
				Vector3 to = target - Vector3(dog_world.x, 0.0f, dog_world.z);
				const float d = to.length();
				if (d < 0.45f) {
					if (ball != nullptr) {
						ball->set_visible(false);
					}
					state = RETURN;
					state_t = 0.0f;
				} else if (d > 0.01f) {
					to = to.normalized();
					dog_vel = to * 2.4f;
					dog_pos += dog_vel * delta;
					heading = std::atan2(to.x, to.z);
				}
			} else if (state == RETURN) {
				Vector3 target = dog_pos;
				if (player != nullptr) {
					target = player->get_global_position() - base + Vector3(0.0f, 0.0f, 0.0f);
				}
				Vector3 to = Vector3(target.x, 0.0f, target.z) - Vector3(dog_pos.x, 0.0f, dog_pos.z);
				const float d = to.length();
				if (d < 0.9f) {
					state = DROP;
					state_t = 0.0f;
					fetch_count++;
					ball_pos = dog_pos + Vector3(0.25f, 0.10f, 0.25f);
					ball_pos.y = ground(base.x + ball_pos.x, base.z + ball_pos.z) + 0.12f;
					if (ball != nullptr) {
						ball->set_visible(true);
						ball->set_global_position(Vector3(base.x + ball_pos.x, ball_pos.y, base.z + ball_pos.z));
					}
					dog_vel = Vector3();
					if (bark_voice != nullptr && snd_bark.is_valid()) {
						bark_voice->set_pitch_scale(1.06f);
						bark_voice->play(0.0f);
					}
				} else if (d > 0.01f) {
					to = to.normalized();
					dog_vel = to * 2.6f;
					dog_pos += dog_vel * delta;
					heading = std::atan2(to.x, to.z);
				}
			} else if (state == DROP) {
				dog_vel = Vector3();
				if (state_t > 1.0f) {
					state = IDLE;
					state_t = 0.0f;
				}
			} else {
				dog_vel = Vector3();
				// 无事时慢慢摇尾巴
				dog_pos += Vector3(0.0f, 0.0f, 0.0f);
			}

			// 落地高度
			dog_pos.y = ground(base.x + dog_pos.x, base.z + dog_pos.z);
			if (dog != nullptr) {
				dog->set_position(dog_pos);
				dog->set_rotation(Vector3(0.0f, heading, 0.0f));
			}
			// 尾巴摇速随移动速度
			wag = (state == IDLE || state == DROP) ? 1.5f : (2.0f + speed * 3.0f);
			if (tail != nullptr) {
				tail->set_rotation(Vector3(0.0f, 0.0f, std::sin(state_t * wag * 4.0f) * 0.5f));
			}
		} break;
		default:
			break;
	}
}

void DogFetch::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	dog_pos = Vector3(0.0f, 0.0f, 0.0f);

	dog = memnew(Node3D);
	dog->set_name("Dog");
	add_child(dog);
	const Color fur(0.64f, 0.46f, 0.28f);
	MeshInstance3D *body = meshutil::add_sphere(dog, toon, 0.20f, Vector3(0.0f, 0.40f, 0.0f), fur);
	body->set_scale(Vector3(0.9f, 0.9f, 1.6f));
	meshutil::add_sphere(dog, toon, 0.13f, Vector3(0.0f, 0.58f, 0.30f), Color(0.70f, 0.52f, 0.32f));
	meshutil::add_box(dog, toon, Vector3(0.13f, 0.10f, 0.16f), Vector3(0.0f, 0.54f, 0.44f), Color(0.60f, 0.44f, 0.28f));
	// 耳朵
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(dog, toon, Vector3(0.07f, 0.14f, 0.05f),
				Vector3((i == 0 ? -1.0f : 1.0f) * 0.10f, 0.66f, 0.27f), Color(0.50f, 0.34f, 0.20f), Vector3(0.0f, 0.0f, (i == 0 ? 0.2f : -0.2f)));
	}
	// 腿
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.10f : 0.10f;
		const float sz = (i < 2) ? 0.22f : -0.22f;
		meshutil::add_box(dog, toon, Vector3(0.06f, 0.28f, 0.06f), Vector3(sx, 0.14f, sz), Color(0.56f, 0.40f, 0.24f));
	}
	// 尾巴（单独节点，摇）
	tail = memnew(Node3D);
	tail->set_name("Tail");
	tail->set_position(Vector3(0.0f, 0.48f, -0.34f));
	dog->add_child(tail);
	meshutil::add_box(tail, toon, Vector3(0.05f, 0.05f, 0.22f), Vector3(0.0f, 0.08f, -0.11f), Color(0.58f, 0.42f, 0.26f), Vector3(-0.6f, 0.0f, 0.0f));

	// 红皮球
	Ref<SphereMesh> bm;
	bm.instantiate();
	bm->set_radius(0.10f);
	bm->set_height(0.20f);
	bm->set_radial_segments(10);
	bm->set_rings(5);
	bm->set_material(meshutil::toon_material(toon, Color(0.82f, 0.22f, 0.18f), 0.3f));
	ball = memnew(MeshInstance3D);
	ball->set_mesh(bm);
	ball_pos = Vector3(0.35f, 0.0f, 0.40f);
	ball_pos.y = 0.12f;
	ball->set_position(ball_pos);
	add_child(ball);

	build_bark();
	bark_voice = memnew(AudioStreamPlayer3D);
	bark_voice->set_name("BarkVoice");
	bark_voice->set_stream(snd_bark);
	bark_voice->set_volume_db(-8.0f);
	bark_voice->set_max_distance(24.0f);
	add_child(bark_voice);
}

void DogFetch::build_bark() {
	const int sr = 22050;
	const int n = (int)(0.22f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(9911u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float ph = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float env = std::exp(-tt * 18.0f) * Math::clamp(tt / 0.01f, 0.0f, 1.0f);
		const float f = 420.0f + 180.0f * std::exp(-tt * 8.0f);
		ph += 6.2831853f * f / (float)sr;
		const float noise = d01(rng) - 0.5f;
		buf[i] = (std::sin(ph) * 0.8f + noise * 0.3f) * env;
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
	snd_bark = wav;
}
