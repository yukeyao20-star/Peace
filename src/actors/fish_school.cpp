#include "fish_school.h"

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
			if (player != nullptr) {
				// 玩家不在湖边时整组隐藏（鱼在水下，远处看不见也白画）
				set_visible(player->get_global_position().distance_to(
						Vector3(lake_center.x, water_level, lake_center.y)) < 60.0f);
			}
			for (Fish &f : fish) {
				if (f.root == nullptr) {
					continue;
				}
				f.angle += f.speed * delta;
				float bob = std::sin(anim_t * f.bob_w + f.bob_phase) * 0.04f;
				Vector3 pos(lake_center.x + std::cos(f.angle) * f.radius,
						water_level - f.depth + bob,
						lake_center.y + std::sin(f.angle) * f.radius);
				f.root->set_global_position(pos);
				float sgn = f.speed >= 0.0f ? 1.0f : -1.0f;
				Vector3 tangent(-std::sin(f.angle) * sgn, 0.0f, std::cos(f.angle) * sgn);
				float yaw = std::atan2(tangent.x, tangent.z) + f.yaw_offset;
				Vector3 dir(std::sin(yaw), 0.0f, std::cos(yaw));
				f.root->look_at(pos + dir, Vector3(0.0f, 1.0f, 0.0f));
				if (f.tail != nullptr) {
					f.tail->set_rotation(Vector3(0.0f, std::sin(anim_t * 7.5f + f.bob_phase) * 0.55f, 0.0f));
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
	f.angle = rnd(0.0f, 6.2831853f);
	f.radius = rnd(2.8f, 8.0f);
	f.speed = rnd(0.05f, 0.13f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
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
	f.root->set_global_position(Vector3(lake_center.x + std::cos(f.angle) * f.radius,
			water_level - f.depth, lake_center.y + std::sin(f.angle) * f.radius));
	fish.push_back(f);
}
