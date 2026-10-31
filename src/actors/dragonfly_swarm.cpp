#include "dragonfly_swarm.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void DragonflySwarm::_bind_methods() {
}

void DragonflySwarm::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			if (get_parent() != nullptr) {
				sky_cycle = get_parent()->get_node_or_null("SkyCycle");
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			float delta = (float)get_process_delta_time();
			anim_t += delta;
			// 白天 + 无雨雪才出现；玩家太远整组隐藏
			bool active = true;
			if (sky_cycle != nullptr) {
				SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);
				if (sky != nullptr) {
					active = sky->get_night_factor() < 0.35f
							&& sky->get_rain_amount() < 0.30f
							&& sky->get_snow_amount() < 0.30f;
				}
			}
			if (player != nullptr) {
				active = active && player->get_global_position().distance_to(
						Vector3(lake_center.x, water_level, lake_center.y)) < 70.0f;
			}
			set_visible(active);
			if (!active) {
				return;
			}
			for (Dragonfly &b : bugs) {
				if (b.root == nullptr) {
					continue;
				}
				b.retarget_t -= delta;
				if (b.retarget_t <= 0.0f) {
					static std::mt19937 rng(818181u);
					std::uniform_real_distribution<float> d01(0.0f, 1.0f);
					b.target = b.home + Vector3((d01(rng) - 0.5f) * 3.4f,
							(d01(rng) - 0.5f) * 0.9f, (d01(rng) - 0.5f) * 3.4f);
					b.retarget_t = 0.55f + d01(rng) * 1.25f;
				}
				Vector3 to = b.target - b.pos;
				// 到达附近开始悬停（轻微抖动），等下一次换点
				if (to.length_squared() > 0.03f) {
					Vector3 dir = to.normalized();
					b.pos += dir * b.speed * delta;
					b.root->look_at(b.pos + dir, Vector3(0.0f, 1.0f, 0.0f));
				} else {
					b.pos.y += std::sin(anim_t * 7.0f + b.wing_phase) * 0.0025f;
				}
				b.root->set_global_position(b.pos);
				// 四翅高频扑动：左右反相
				float flap = std::sin(anim_t * 42.0f + b.wing_phase) * 0.55f;
				if (b.wing_l != nullptr) {
					b.wing_l->set_rotation(Vector3(0.0f, 0.0f, flap));
				}
				if (b.wing_r != nullptr) {
					b.wing_r->set_rotation(Vector3(0.0f, 0.0f, -flap));
				}
			}
		} break;
		default:
			break;
	}
}

void DragonflySwarm::build() {
	bugs.clear();
	for (int i = 0; i < count; i++) {
		spawn_dragonfly(i, 717171u + (unsigned)i * 5309u);
	}
}

void DragonflySwarm::spawn_dragonfly(int p_index, unsigned p_seed) {
	std::mt19937 rng(p_seed);
	auto rnd = [&rng](float lo, float hi) {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};

	Dragonfly b;
	float ang = rnd(1.9f, 3.5f);           // 南岸/西南岸芦苇带
	float rad = rnd(13.5f, 16.5f);
	float by = water_level + rnd(0.55f, 1.45f);
	b.home = Vector3(lake_center.x + std::cos(ang) * rad, by, lake_center.y + std::sin(ang) * rad);
	b.pos = b.home;
	b.target = b.home;
	b.retarget_t = rnd(0.1f, 1.2f);
	b.speed = rnd(2.2f, 3.6f);
	b.wing_phase = rnd(0.0f, 6.2831853f);

	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color body_c(0.10f, 0.30f, 0.34f);

	b.root = memnew(Node3D);
	b.root->set_name("Dragonfly" + String::num_int64(p_index));
	b.root->set_global_position(b.pos);
	add_child(b.root);

	Ref<CylinderMesh> body;
	body.instantiate();
	body->set_bottom_radius(0.011f);
	body->set_top_radius(0.016f);
	body->set_height(0.16f);
	body->set_radial_segments(6);
	body->set_material(meshutil::toon_material(toon, body_c));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(body);
	bmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f)); // 沿 Z
	b.root->add_child(bmi);

	// 细长尾
	Ref<CylinderMesh> tail;
	tail.instantiate();
	tail->set_bottom_radius(0.004f);
	tail->set_top_radius(0.010f);
	tail->set_height(0.14f);
	tail->set_radial_segments(6);
	tail->set_material(meshutil::toon_material(toon, body_c * 1.1f));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(tail);
	tmi->set_position(Vector3(0.0f, 0.0f, 0.14f));
	tmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	b.root->add_child(tmi);

	// 复眼（前端 -Z）
	Ref<SphereMesh> eye_mesh;
	eye_mesh.instantiate();
	eye_mesh->set_radius(0.016f);
	eye_mesh->set_height(0.032f);
	eye_mesh->set_radial_segments(8);
	eye_mesh->set_rings(4);
	eye_mesh->set_material(meshutil::toon_material(toon, Color(0.05f, 0.10f, 0.12f)));
	for (int s = 0; s < 2; s++) {
		MeshInstance3D *emi = memnew(MeshInstance3D);
		emi->set_mesh(eye_mesh);
		emi->set_position(Vector3((s == 0 ? 1.0f : -1.0f) * 0.014f, 0.008f, -0.085f));
		b.root->add_child(emi);
	}

	// 半透明薄翅：左右各两片
	Ref<StandardMaterial3D> wing_mat;
	wing_mat.instantiate();
	wing_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	wing_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wing_mat->set_albedo(Color(0.82f, 0.93f, 0.95f, 0.38f));
	wing_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	Ref<BoxMesh> wing_mesh;
	wing_mesh.instantiate();
	wing_mesh->set_size(Vector3(0.15f, 0.003f, 0.030f));
	wing_mesh->set_material(wing_mat);

	auto make_side = [&](bool p_left) {
		Node3D *side = memnew(Node3D);
		side->set_name(p_left ? "WingL" : "WingR");
		b.root->add_child(side);
		for (int k = 0; k < 2; k++) {
			MeshInstance3D *wmi = memnew(MeshInstance3D);
			wmi->set_mesh(wing_mesh);
			float sx = p_left ? -0.085f : 0.085f;
			wmi->set_position(Vector3(sx, 0.018f, k == 0 ? -0.028f : 0.030f));
			side->add_child(wmi);
		}
		return side;
	};
	b.wing_l = make_side(true);
	b.wing_r = make_side(false);

	bugs.push_back(b);
}
