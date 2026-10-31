#include "squirrel_field.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void SquirrelField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_squirrel_count"), &SquirrelField::get_squirrel_count);
	ClassDB::bind_method(D_METHOD("get_hidden_count"), &SquirrelField::get_hidden_count);
}

void SquirrelField::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			snd_chatter = make_chatter();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("SquirrelVoice");
			voice->set_stream(snd_chatter);
			voice->set_volume_db(-14.0f);
			voice->set_max_distance(30.0f);
			add_child(voice);
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const Vector3 ppos = player != nullptr ? player->get_global_position() : Vector3(0, -999, 0);
			for (Squirrel &s : squirrels) {
				if (s.root == nullptr) {
					continue;
				}
				if (s.state == 3) {
					// 躲在地洞里：玩家走远后才会重新探头
					s.hidden_t -= delta;
					if (s.hidden_t <= 0.0f) {
						if (ppos.distance_to(s.home) < 7.0f) {
							s.hidden_t = 1.2f;
						} else {
							s.state = 0;
							s.wait_t = 0.8f;
							s.root->set_global_position(s.home);
							s.root->set_scale(Vector3(1, 1, 1));
							s.root->set_visible(true);
						}
					}
					continue;
				}

				bool flee = s.state != 2 && ppos.distance_to(s.root->get_global_position()) < 5.5f;
				if (flee) {
					start_flee(s, ppos);
				}

				if (s.state == 2) {
					s.state_t += delta;
					float u = Math::clamp(s.state_t / s.hop_dur, 0.0f, 1.0f);
					Vector3 p = s.hop_from.lerp(s.hop_to, u);
					p.y += std::sin(u * 3.1415926f) * 0.16f;
					s.root->set_global_position(p);
					Vector3 dir = s.hop_to - s.hop_from;
					dir.y = 0.0f;
					if (dir.length_squared() > 0.001f) {
						s.root->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), 0.0f));
					}
					if (u > 0.72f) {
						float k = (u - 0.72f) / 0.28f;
						s.root->set_scale(Vector3(1.0f, Math::max(0.15f, 1.0f - k), 1.0f));
					}
					if (u >= 1.0f) {
						s.state = 3;
						s.hidden_t = 6.0f + (float)(rng() % 500u) * 0.01f;
						s.root->set_visible(false);
						s.root->set_scale(Vector3(1, 1, 1));
					}
				} else if (s.state == 1) {
					s.state_t += delta;
					float u = Math::clamp(s.state_t / s.hop_dur, 0.0f, 1.0f);
					Vector3 p = s.hop_from.lerp(s.hop_to, u);
					p.y += std::sin(u * 3.1415926f) * 0.22f;
					s.root->set_global_position(p);
					Vector3 dir = s.hop_to - s.hop_from;
					dir.y = 0.0f;
					if (dir.length_squared() > 0.001f) {
						s.root->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), 0.0f));
					}
					if (u >= 1.0f) {
						s.state = 0;
						s.wait_t = 1.2f + (float)(rng() % 200u) * 0.009f;
					}
				} else {
					s.wait_t -= delta;
					if (s.wait_t <= 0.0f) {
						next_hop(s);
					}
				}

				// 尾巴：等待时慢慢左右摆，跳跃时甩得更快
				if (s.tail != nullptr) {
					float speed = (s.state == 1 || s.state == 2) ? 9.0f : 3.2f;
					s.tail->set_rotation(Vector3(
							0.05f + std::sin(t * speed + s.tail_phase) * 0.18f,
							0.0f,
							std::sin(t * (speed * 0.7f) + s.tail_phase) * 0.22f));
				}
				if (s.head != nullptr && s.state == 0) {
					s.head->set_rotation(Vector3(
							std::sin(t * 0.9f + s.tail_phase) * 0.08f,
							std::sin(t * 0.63f + s.tail_phase * 1.7f) * 0.30f,
							0.0f));
				}
			}
		} break;
		default:
			break;
	}
}

void SquirrelField::build() {
	squirrels.clear();
	const Vector3 homes[2] = {Vector3(-2.6f, 0.0f, -18.5f), Vector3(6.2f, 0.0f, -16.0f)};
	spawn(0, homes[0], 111u);
	spawn(1, homes[1], 222u);
}

void SquirrelField::spawn(int p_index, const Vector3 &p_home, unsigned p_seed) {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color fur(0.52f, 0.33f, 0.17f);
	const Color fur_light(0.72f, 0.51f, 0.30f);
	const Color fur_dark(0.30f, 0.18f, 0.09f);
	const Color cream(0.86f, 0.74f, 0.55f);

	Vector3 home = p_home;
	if (terrain != nullptr) {
		home.y = terrain->get_terrain_height(home.x, home.z) + 0.01f;
	}

	Squirrel s;
	s.home = home;
	s.tail_phase = (float)(p_seed % 100u) * 0.06f;
	s.wait_t = 1.2f + (float)(p_seed % 50u) * 0.02f;

	Node3D *root = memnew(Node3D);
	root->set_name(String("Squirrel") + String::num_int64(p_index));
	add_child(root);
	root->set_global_position(home);
	s.root = root;

	// 身体与头（朝 +Z）
	MeshInstance3D *body = memnew(MeshInstance3D);
	Ref<SphereMesh> body_mesh;
	body_mesh.instantiate();
	body_mesh->set_radius(0.16f);
	body_mesh->set_height(0.32f);
	body_mesh->set_radial_segments(12);
	body_mesh->set_rings(8);
	body_mesh->set_material(meshutil::toon_material(toon, fur, 0.45f));
	body->set_mesh(body_mesh);
	body->set_position(Vector3(0.0f, 0.18f, 0.0f));
	body->set_scale(Vector3(1.0f, 0.82f, 1.5f));
	root->add_child(body);

	meshutil::add_sphere(root, toon, 0.075f, Vector3(0.0f, 0.16f, 0.10f), cream);

	Node3D *head = memnew(Node3D);
	head->set_position(Vector3(0.0f, 0.30f, 0.17f));
	root->add_child(head);
	s.head = head;
	meshutil::add_sphere(head, toon, 0.105f, Vector3(0.0f, 0.0f, 0.0f), fur);
	meshutil::add_sphere(head, toon, 0.070f, Vector3(0.0f, -0.01f, 0.075f), cream);
	meshutil::add_sphere(head, toon, 0.016f, Vector3(-0.048f, 0.035f, 0.082f), Color(0.06f, 0.05f, 0.04f));
	meshutil::add_sphere(head, toon, 0.016f, Vector3(0.048f, 0.035f, 0.082f), Color(0.06f, 0.05f, 0.04f));
	meshutil::add_sphere(head, toon, 0.013f, Vector3(0.0f, -0.012f, 0.118f), Color(0.18f, 0.10f, 0.07f));

	// 三角耳
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> ear;
		ear.instantiate();
		ear->set_top_radius(0.0f);
		ear->set_bottom_radius(0.040f);
		ear->set_height(0.095f);
		ear->set_radial_segments(6);
		ear->set_rings(1);
		ear->set_material(meshutil::toon_material(toon, fur_dark, 0.3f));
		MeshInstance3D *em = memnew(MeshInstance3D);
		em->set_mesh(ear);
		em->set_position(Vector3(i == 0 ? -0.06f : 0.06f, 0.10f, -0.01f));
		em->set_rotation(Vector3(0.0f, 0.0f, i == 0 ? 0.20f : -0.20f));
		head->add_child(em);
	}

	// 四只小脚
	for (int i = 0; i < 4; i++) {
		float x = (i < 2) ? 0.075f : -0.075f;
		float z = (i % 2 == 0) ? 0.09f : -0.09f;
		meshutil::add_box(root, toon, Vector3(0.055f, 0.045f, 0.10f), Vector3(x, 0.035f, z), fur_dark);
	}

	// 蓬松大尾巴：三团球从尾根向上卷
	Node3D *tail = memnew(Node3D);
	tail->set_position(Vector3(0.0f, 0.20f, -0.19f));
	root->add_child(tail);
	s.tail = tail;
	MeshInstance3D *t1 = memnew(MeshInstance3D);
	Ref<SphereMesh> tm;
	tm.instantiate();
	tm->set_radius(0.09f);
	tm->set_height(0.18f);
	tm->set_radial_segments(10);
	tm->set_rings(6);
	tm->set_material(meshutil::toon_material(toon, fur_light, 0.5f));
	t1->set_mesh(tm);
	t1->set_position(Vector3(0.0f, 0.02f, -0.06f));
	t1->set_scale(Vector3(1.0f, 1.1f, 1.0f));
	tail->add_child(t1);

	MeshInstance3D *t2 = memnew(MeshInstance3D);
	t2->set_mesh(tm);
	t2->set_material_override(meshutil::toon_material(toon, fur_light, 0.55f));
	t2->set_position(Vector3(0.0f, 0.16f, -0.09f));
	t2->set_scale(Vector3(1.15f, 1.35f, 1.15f));
	tail->add_child(t2);

	MeshInstance3D *t3 = memnew(MeshInstance3D);
	t3->set_mesh(tm);
	t3->set_material_override(meshutil::toon_material(toon, fur, 0.55f));
	t3->set_position(Vector3(0.0f, 0.30f, -0.05f));
	t3->set_scale(Vector3(1.0f, 1.25f, 1.0f));
	tail->add_child(t3);

	// 窝附近的三个落脚点
	for (int i = 0; i < 3; i++) {
		float a = 6.2831853f * (float)i / 3.0f + (float)(p_seed % 30u) * 0.12f;
		float r = 1.4f + (float)((p_seed + i * 17u) % 140u) * 0.01f;
		Vector3 w = home + Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r);
		if (terrain != nullptr) {
			w.y = terrain->get_terrain_height(w.x, w.z) + 0.01f;
		}
		s.wp[i] = w;
	}

	squirrels.push_back(s);
}

void SquirrelField::next_hop(Squirrel &p_s) {
	p_s.wp_index = (p_s.wp_index + 1) % 3;
	p_s.hop_from = p_s.root->get_global_position();
	p_s.hop_to = p_s.wp[p_s.wp_index];
	p_s.hop_dur = 0.45f + (float)(rng() % 200u) * 0.0012f;
	p_s.state = 1;
	p_s.state_t = 0.0f;
}

void SquirrelField::start_flee(Squirrel &p_s, const Vector3 &p_player_pos) {
	(void)p_player_pos;
	p_s.hop_from = p_s.root->get_global_position();
	p_s.hop_to = p_s.home;
	p_s.hop_dur = 0.80f;
	p_s.state = 2;
	p_s.state_t = 0.0f;
	// 逃跑时吱一声
	if (voice != nullptr && snd_chatter.is_valid()) {
		voice->set_global_position(p_s.home + Vector3(0.0f, 0.4f, 0.0f));
		voice->set_pitch_scale(1.0f + (float)(rng() % 100u) * 0.0018f);
		voice->set_volume_db(-12.0f);
		voice->play(0.0f);
	}
}

// 0.32 秒三声急促的吱吱：高频短音 + 一点气声
Ref<AudioStreamWAV> SquirrelField::make_chatter() {
	const int sr = 22050;
	const float dur = 0.34f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int b = 0; b < 3; b++) {
			float bt = t - b * 0.095f;
			if (bt < 0.0f || bt > 0.055f) {
				continue;
			}
			float u = bt / 0.055f;
			float f0 = 2850.0f + 420.0f * b + 300.0f * u;
			float env = std::sin(3.1415926f * u);
			samp += std::sin(6.2831853f * f0 * bt) * env * 0.45f;
		}
		samp += (d01(rng) - 0.5f) * 0.02f;
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
