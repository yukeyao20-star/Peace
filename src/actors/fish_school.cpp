#include "fish_school.h"

#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

void FishSchool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_fish_position", "index"), &FishSchool::get_fish_position);
	ClassDB::bind_method(D_METHOD("get_neighbor_checks"), &FishSchool::get_neighbor_checks);
	ClassDB::bind_method(D_METHOD("get_school_size"), &FishSchool::get_school_size);
}

Vector2 FishSchool::get_fish_position(int p_index) const {
	if (p_index < 0 || p_index >= (int)fish.size()) {
		return Vector2();
	}
	return fish[p_index].pos;
}

void FishSchool::_notification(int p_what) {
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
			Vector3 ppos(0.0f, -999.0f, 0.0f);
			bool near_water = true;
			if (player != nullptr) {
				ppos = player->get_global_position();
				near_water = ppos.distance_to(Vector3(lake_center.x, water_level, lake_center.y)) < 60.0f
						|| ppos.distance_to(Vector3(-14.0f, water_level, 66.0f)) < 60.0f;
				set_visible(near_water);
			}
			if (!near_water) {
				return; // 玩家很远：整群冻结，不做邻居查询与转向，省算力
			}
			if (sky == nullptr && get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			if (terrain == nullptr && get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
			}
			const float snow_now = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			ice_mix = Math::lerp(ice_mix, snow_now > 0.82f ? 1.0f : 0.0f, 1.0f - std::exp(-0.8f * delta));

			// 邻居表：每帧只算一次，半径截断（参考 sergioabreu-g/boids 的 _detectNeighbors）
			neighbors.assign(fish.size(), {});
			neighbor_checks = 0;
			const float visual_r2 = 4.0f * 4.0f;
			for (size_t i = 0; i < fish.size(); i++) {
				for (size_t j = i + 1; j < fish.size(); j++) {
					neighbor_checks++;
					const Vector2 d = fish[i].pos - fish[j].pos;
					if (d.length_squared() < visual_r2) {
						neighbors[i].push_back((int)j);
						neighbors[j].push_back((int)i);
					}
				}
			}

			const Vector2 ppos2(ppos.x, ppos.z);
			const float max_speed = 1.55f * (1.0f - 0.45f * ice_mix);
			for (size_t i = 0; i < fish.size(); i++) {
				Fish &f = fish[i];
				if (f.root == nullptr) {
					continue;
				}
				// —— 分离 / 对齐 / 聚合（同种） + 异种避让 ——
				Vector2 sep(0.0f, 0.0f);
				Vector2 cross(0.0f, 0.0f);
				Vector2 avg_pos(0.0f, 0.0f);
				Vector2 avg_vel(0.0f, 0.0f);
				int same = 0;
				for (int j : neighbors[i]) {
					const Fish &o = fish[j];
					const Vector2 d = f.pos - o.pos;
					const float dist = d.length();
					if (dist < 0.001f) {
						continue;
					}
					if (dist < 0.9f) {
						sep += d / dist * (1.0f - dist / 0.9f);
					}
					if (o.species == f.species) {
						avg_pos += o.pos;
						avg_vel += o.vel;
						same++;
					} else if (dist < 1.6f) {
						cross += d / dist * (1.0f - dist / 1.6f);
					}
				}
				Vector2 acc(0.0f, 0.0f);
				acc += sep * 6.0f;
				acc += cross * 2.4f;
				if (same > 0) {
					const Vector2 coh = avg_pos / (float)same - f.pos;
					acc += avg_vel / (float)same * 0.55f + coh * 0.6f;
				}
				// 玩家靠近：sqrt 衰减的斥力（参考 Flock._escapePredator）
				const Vector2 away = f.pos - ppos2;
				const float pd = away.length();
				if (pd < 4.5f && pd > 0.01f) {
					acc += away / pd * std::sqrt(1.0f - pd / 4.5f) * 9.0f;
				}
				// 湖岸 / 浅水 / 离群：回到自家水域中心（参考 Flock._borders）
				const Vector2 ahead = f.pos + f.vel * 0.6f;
				const float depth_ahead = (terrain != nullptr) ? terrain->water_depth_at(ahead.x, ahead.y) : 3.0f;
				const Vector2 to_center = f.center - f.pos;
				const float dc = to_center.length();
				if (dc > 13.0f || depth_ahead < 0.9f) {
					acc += to_center.normalized() * ((depth_ahead < 0.9f) ? 7.0f : 3.0f);
				}

				f.vel += acc * delta;
				float sp = f.vel.length();
				if (sp > max_speed) {
					f.vel = f.vel / sp * max_speed;
					sp = max_speed;
				} else if (sp < 0.25f) {
					f.vel = (sp > 0.001f ? f.vel / sp : Vector2(1.0f, 0.0f)) * 0.25f;
					sp = 0.25f;
				}
				f.pos += f.vel * delta;

				const float bob = std::sin(anim_t * f.bob_w + f.bob_phase) * 0.04f;
				const Vector3 wpos(f.pos.x, water_level - Math::lerp(f.depth, 0.45f, ice_mix) + bob, f.pos.y);
				f.root->set_global_position(wpos);
				const float yaw = std::atan2(f.vel.x, f.vel.y) + f.yaw_offset;
				const Vector3 dir(std::sin(yaw), 0.0f, std::cos(yaw));
				f.root->look_at(wpos + dir, Vector3(0.0f, 1.0f, 0.0f));
				if (f.tail != nullptr) {
					const float wag = 4.0f + sp * 6.0f;
					f.tail->set_rotation(Vector3(0.0f, std::sin(anim_t * wag + f.bob_phase) * 0.55f, 0.0f));
				}
			}
		} break;
		default:
			break;
	}
}

void FishSchool::build() {
	fish.clear();
	for (int i = 0; i < fish_count; i++) {
		spawn_fish(i, 424242u + (unsigned)i * 6151u);
	}
}

void FishSchool::spawn_fish(int p_index, unsigned p_seed) {
	std::mt19937 rng(p_seed);
	auto rnd = [&rng](float lo, float hi) {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};

	Fish f;
	f.center = (p_index < 10) ? lake_center : Vector2(-14.0f, 66.0f);
	f.species = p_index % 3;
	const float a0 = rnd(0.0f, 6.2831853f);
	const float radius = (p_index < 10) ? rnd(2.8f, 8.0f) : rnd(4.0f, 14.0f);
	f.pos = f.center + Vector2(std::cos(a0), std::sin(a0)) * radius;
	const float course = rnd(0.0f, 6.2831853f);
	f.vel = Vector2(std::cos(course), std::sin(course)) * rnd(0.35f, 0.65f);
	f.depth = rnd(0.8f, 2.3f); // 湖底 -5.2，留足净空
	f.bob_w = rnd(0.9f, 1.8f);
	f.bob_phase = rnd(0.0f, 6.2831853f);
	f.yaw_offset = (rnd(0.0f, 1.0f) - 0.5f) * 0.18f;

	Ref<Shader> toon = meshutil::load_toon_shader();
	static const Color palette[4] = {
		Color(0.62f, 0.70f, 0.76f),
		Color(0.76f, 0.68f, 0.50f),
		Color(0.44f, 0.58f, 0.55f),
		Color(0.70f, 0.55f, 0.48f),
	};
	const Color body_c = palette[p_index % 4];
	Ref<ShaderMaterial> mat = meshutil::toon_material(toon, body_c);

	f.root = memnew(Node3D);
	f.root->set_name("Fish" + String::num_int64(p_index));
	add_child(f.root);

	Ref<CapsuleMesh> body;
	body.instantiate();
	body->set_radius(0.032f);
	body->set_height(0.24f);
	body->set_radial_segments(8);
	body->set_rings(3);
	body->set_material(mat);
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(body);
	bmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f)); // 胶囊沿 Z（前进方向）
	f.root->add_child(bmi);

	f.tail = memnew(Node3D);
	f.tail->set_name("Tail");
	f.tail->set_position(Vector3(0.0f, 0.0f, 0.125f));
	f.root->add_child(f.tail);
	Ref<CylinderMesh> tail_mesh;
	tail_mesh.instantiate();
	tail_mesh->set_bottom_radius(0.030f);
	tail_mesh->set_top_radius(0.003f);
	tail_mesh->set_height(0.07f);
	tail_mesh->set_radial_segments(6);
	tail_mesh->set_material(meshutil::toon_material(toon, body_c * 0.88f));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(tail_mesh);
	tmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	f.tail->add_child(tmi);

	// 首帧就位（避免建成瞬间堆在原点闪一帧）
	f.root->set_global_position(Vector3(f.pos.x, water_level - f.depth, f.pos.y));
	fish.push_back(f);
}
