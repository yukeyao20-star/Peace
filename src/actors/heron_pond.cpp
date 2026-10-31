#include "heron_pond.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void HeronPond::_bind_methods() {
}

void HeronPond::_notification(int p_what) {
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
			snd_call = make_call();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("HeronVoice");
			voice->set_stream(snd_call);
			voice->set_volume_db(-10.0f);
			voice->set_max_distance(45.0f);
			add_child(voice);
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			float delta = (float)get_process_delta_time();
			anim_t += delta;
			const Vector3 ppos = player != nullptr ? player->get_global_position() : Vector3(0, -999, 0);
			for (Heron &h : herons) {
				if (h.root == nullptr) {
					continue;
				}
				if (h.hidden) {
					h.respawn_t -= delta;
					if (h.respawn_t <= 0.0f) {
						h.hidden = false;
						h.root->set_global_position(h.home);
						h.root->set_visible(true);
						if (h.wing_l) {
							h.wing_l->set_rotation(Vector3(0.0f, 0.0f, 1.20f));
						}
						if (h.wing_r) {
							h.wing_r->set_rotation(Vector3(0.0f, 0.0f, -1.20f));
						}
					}
					continue;
				}
				if (h.flying) {
					h.fly_t += delta;
					float u = Math::clamp(h.fly_t / h.fly_dur, 0.0f, 1.0f);
					Vector3 p = h.fly_from.lerp(h.fly_to, u);
					p.y += std::sin(u * 3.1415926f) * 5.0f; // 先拔高再平飞
					h.root->set_global_position(p);
					Vector3 dir = (h.fly_to - h.fly_from);
					dir.y = 0.0f;
					if (dir.length_squared() > 0.001f) {
						h.root->look_at(p + dir.normalized(), Vector3(0, 1, 0));
					}
					float flap = std::sin(anim_t * 26.0f) * 1.0f;
					if (h.wing_l) {
						h.wing_l->set_rotation(Vector3(0.0f, 0.0f, flap));
					}
					if (h.wing_r) {
						h.wing_r->set_rotation(Vector3(0.0f, 0.0f, -flap));
					}
					if (u >= 1.0f) {
						h.flying = false;
						h.hidden = true;
						h.respawn_t = 22.0f + (float)(rand() % 8);
						h.root->set_visible(false);
					}
				} else {
					// 站立：轻微呼吸 + 偶尔低头啄水
					h.peck_t -= delta;
					float peck = 0.0f;
					if (h.peck_t <= 0.0f) {
						peck = Math::clamp(0.35f - h.peck_t, 0.0f, 0.35f);
					}
					if (h.neck != nullptr) {
						float in_peck = (h.peck_t <= 0.0f && h.peck_t > -0.35f) ? 1.0f : 0.0f;
						h.neck->set_rotation(Vector3(0.25f + in_peck * 0.55f, 0.0f, 0.0f));
					}
					if (h.peck_t < -1.2f) {
						h.peck_t = 4.0f + (float)(rand() % 5);
					}
					if (ppos.distance_to(h.root->get_global_position()) < 6.0f) {
						take_off(h);
					}
					(void)peck;
					(void)h.phase;
				}
			}
		} break;
		default:
			break;
	}
}

void HeronPond::build() {
	herons.clear();
	if (terrain == nullptr) {
		return;
	}
	std::mt19937 rng(838383u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	std::vector<Vector3> spots;
	int guard = 0;
	while ((int)spots.size() < 2 && guard < 3000) {
		guard++;
		float cx = (spots.empty() || d01(rng) < 0.5f) ? -6.0f : -14.0f;
		float cz = (std::fabs(cx + 6.0f) < 0.1f) ? 26.0f : 66.0f;
		float a = d01(rng) * 6.2831853f;
		float r = 10.0f + d01(rng) * 10.0f;
		float x = cx + std::cos(a) * r;
		float z = cz + std::sin(a) * r;
		float depth = terrain->water_depth_at(x, z);
		if (depth < 0.15f || depth > 0.65f) {
			continue;
		}
		bool dup = false;
		for (const Vector3 &s : spots) {
			if (Vector2(s.x, s.z).distance_to(Vector2(x, z)) < 6.0f) {
				dup = true;
			}
		}
		if (dup) {
			continue;
		}
		spots.push_back(Vector3(x, terrain->get_terrain_height(x, z) + 0.02f, z));
	}
	for (int i = 0; i < (int)spots.size(); i++) {
		spawn_heron(i, spots[i], 838383u + (unsigned)i * 733u);
	}
}

void HeronPond::spawn_heron(int p_index, const Vector3 &p_home, unsigned p_seed) {
	std::mt19937 rng(p_seed);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color body_c(0.63f, 0.66f, 0.68f);
	const Color breast_c(0.82f, 0.84f, 0.83f);
	const Color beak_c(0.86f, 0.68f, 0.26f);
	const Color leg_c(0.35f, 0.32f, 0.26f);

	Heron h;
	h.home = p_home;
	h.phase = d01(rng) * 6.2831853f;
	h.peck_t = 3.0f + d01(rng) * 4.0f;
	h.root = memnew(Node3D);
	h.root->set_name("Heron" + String::num_int64(p_index));
	h.root->set_global_position(p_home);
	h.root->set_rotation(Vector3(0.0f, d01(rng) * 6.2831853f, 0.0f));
	add_child(h.root);

	auto sphere = [&](const Vector3 &p_pos, float p_r, const Vector3 &p_scale, const Color &p_c) {
		Ref<SphereMesh> sp;
		sp.instantiate();
		sp->set_radius(p_r);
		sp->set_height(p_r * 2.0f);
		sp->set_radial_segments(10);
		sp->set_rings(6);
		sp->set_material(meshutil::toon_material(toon, p_c));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(sp);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(0, 0, 0), p_scale);
		t.set_basis(b);
		t.set_origin(p_pos);
		mi->set_transform(t);
		h.root->add_child(mi);
	};
	auto cylinder = [&](const Vector3 &p_pos, float p_r, float p_h, const Color &p_c, float p_rot_x) {
		Ref<CylinderMesh> cy;
		cy.instantiate();
		cy->set_top_radius(p_r);
		cy->set_bottom_radius(p_r);
		cy->set_height(p_h);
		cy->set_radial_segments(8);
		cy->set_material(meshutil::toon_material(toon, p_c));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(cy);
		mi->set_position(p_pos);
		mi->set_rotation(Vector3(p_rot_x, 0.0f, 0.0f));
		h.root->add_child(mi);
		return mi;
	};

	// 身体与胸羽（站在浅水里也要高出水面）
	sphere(Vector3(0.0f, 0.95f, 0.0f), 0.17f, Vector3(0.9f, 0.75f, 1.3f), body_c);
	sphere(Vector3(0.0f, 0.90f, -0.12f), 0.12f, Vector3(0.85f, 0.75f, 0.85f), breast_c);
	// 腿
	cylinder(Vector3(0.06f, 0.42f, 0.0f), 0.014f, 0.80f, leg_c, 0.0f);
	cylinder(Vector3(-0.06f, 0.42f, 0.0f), 0.014f, 0.80f, leg_c, 0.0f);
	// 脖子（枢轴节点，便于低头）
	h.neck = memnew(Node3D);
	h.neck->set_name("Neck");
	h.neck->set_position(Vector3(0.0f, 1.05f, -0.10f));
	h.root->add_child(h.neck);
	Ref<CylinderMesh> neck_mesh;
	neck_mesh.instantiate();
	neck_mesh->set_top_radius(0.032f);
	neck_mesh->set_bottom_radius(0.045f);
	neck_mesh->set_height(0.56f);
	neck_mesh->set_radial_segments(8);
	neck_mesh->set_material(meshutil::toon_material(toon, body_c));
	MeshInstance3D *nmi = memnew(MeshInstance3D);
	nmi->set_mesh(neck_mesh);
	nmi->set_position(Vector3(0.0f, 0.26f, -0.03f));
	nmi->set_rotation(Vector3(0.22f, 0.0f, 0.0f));
	h.neck->add_child(nmi);
	Ref<SphereMesh> head_mesh;
	head_mesh.instantiate();
	head_mesh->set_radius(0.07f);
	head_mesh->set_height(0.14f);
	head_mesh->set_radial_segments(10);
	head_mesh->set_rings(6);
	head_mesh->set_material(meshutil::toon_material(toon, breast_c));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(head_mesh);
	hmi->set_position(Vector3(0.0f, 0.58f, -0.08f));
	h.neck->add_child(hmi);
	// 长喙
	Ref<CylinderMesh> beak;
	beak.instantiate();
	beak->set_top_radius(0.002f);
	beak->set_bottom_radius(0.024f);
	beak->set_height(0.30f);
	beak->set_radial_segments(6);
	beak->set_material(meshutil::toon_material(toon, beak_c));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(beak);
	bmi->set_position(Vector3(0.0f, 0.57f, -0.25f));
	bmi->set_rotation(Vector3(-1.5707964f, 0.0f, 0.0f));
	h.neck->add_child(bmi);

	// 翅膀（枢轴在体侧）
	Ref<BoxMesh> wing;
	wing.instantiate();
	wing->set_size(Vector3(0.36f, 0.02f, 0.34f));
	wing->set_material(meshutil::toon_material(toon, body_c * 0.9f));
	auto make_wing = [&](bool p_left) {
		Node3D *node = memnew(Node3D);
		node->set_name(p_left ? "WingL" : "WingR");
		node->set_position(Vector3(p_left ? -0.15f : 0.15f, 1.02f, 0.0f));
		h.root->add_child(node);
		MeshInstance3D *wmi = memnew(MeshInstance3D);
		wmi->set_mesh(wing);
		wmi->set_position(Vector3(p_left ? -0.18f : 0.18f, 0.0f, 0.0f));
		node->add_child(wmi);
		node->set_rotation(Vector3(0.0f, 0.0f, p_left ? 1.20f : -1.20f)); // 收翅
		return node;
	};
	h.wing_l = make_wing(true);
	h.wing_r = make_wing(false);

	herons.push_back(h);
}

void HeronPond::take_off(Heron &p_heron) {
	if (p_heron.flying || p_heron.hidden) {
		return;
	}
	p_heron.flying = true;
	p_heron.fly_t = 0.0f;
	p_heron.fly_from = p_heron.root->get_global_position();
	// 背向玩家飞 20m，落在另一侧水面/岸边
	Vector3 away = p_heron.fly_from - (player != nullptr ? player->get_global_position() : p_heron.fly_from);
	away.y = 0.0f;
	if (away.length_squared() < 0.01f) {
		away = Vector3(1, 0, 0);
	}
	away = away.normalized();
	p_heron.fly_to = p_heron.fly_from + away * 20.0f;
	p_heron.fly_to.y = p_heron.fly_from.y + 2.0f;
	if (voice != nullptr && snd_call.is_valid()) {
		voice->set_global_position(p_heron.fly_from + Vector3(0, 0.8f, 0));
		voice->set_pitch_scale(0.95f + (float)(rand() % 10) * 0.01f);
		voice->play();
	}
}

// 苍鹭起飞：短促粗哑的一声
Ref<AudioStreamWAV> HeronPond::make_call() {
	const int sr = 22050;
	const float dur = 0.45f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(73737u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float f0 = 680.0f - 260.0f * (t / dur);
		float env = Math::clamp(t / 0.01f, 0.0f, 1.0f) * Math::exp(-t * 7.0f);
		float v = std::sin(6.2831853f * f0 * t) * 0.45f
				+ std::sin(6.2831853f * f0 * 2.0f * t) * 0.25f
				+ std::sin(6.2831853f * f0 * 3.1f * t) * 0.12f;
		v = v * 0.7f + (d01(rng) - 0.5f) * 0.35f;
		int16_t q = (int16_t)Math::clamp(v * env * 12000.0f, -32000.0f, 32000.0f);
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
