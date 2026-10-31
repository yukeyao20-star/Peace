#include "duck_flock.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void DuckFlock::_bind_methods() {
}

void DuckFlock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			if (get_parent() != nullptr) {
				sky_cycle = get_parent()->get_node_or_null("SkyCycle");
			}
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();
			anim_t += (float)delta;
			// 夜晚鸭子放慢巡游（贴着水面歇着漂），白天正常速度
			float night_mul = 1.0f;
			if (sky_cycle != nullptr) {
				SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);
				if (sky != nullptr) {
					float td = sky->get_time_of_day();
					bool night = td < 0.22f || td > 0.78f;
					night_mul = night ? 0.35f : 1.0f;
				}
			}
			for (Duck &d : ducks) {
				if (d.root == nullptr) {
					continue;
				}
				d.angle += d.speed * night_mul * (float)delta;
				float bob = std::sin(anim_t * d.bob_w + d.bob_phase) * 0.012f;
				Vector3 pos(lake_center.x + std::cos(d.angle) * d.radius,
						water_level + 0.02f + bob,
						lake_center.y + std::sin(d.angle) * d.radius);
				d.root->set_position(pos);
				// 朝向 = 切线方向（角速度符号决定顺/逆时针），再叠一个固定微偏
				float sgn = d.speed >= 0.0f ? 1.0f : -1.0f;
				Vector3 tangent(-std::sin(d.angle) * sgn, 0.0f, std::cos(d.angle) * sgn);
				float yaw = std::atan2(tangent.x, tangent.z) + d.yaw_offset;
				Vector3 dir(std::sin(yaw), 0.0f, std::cos(yaw));
				Vector3 gpos = d.root->get_global_position();
				d.root->look_at(gpos + dir, Vector3(0.0f, 1.0f, 0.0f));
				// 轻微左右摇晃（像踩水）
				d.root->rotate_object_local(Vector3(0.0f, 0.0f, 1.0f),
						std::sin(anim_t * 1.6f + d.bob_phase) * 0.045f);
				if (d.wake != nullptr) {
					float ws = 1.0f + 0.08f * std::sin(anim_t * 2.1f + d.bob_phase);
					d.wake->set_scale(Vector3(ws, 1.0f, ws));
				}
			}
		} break;
		default:
			break;
	}
}

void DuckFlock::build() {
	ducks.clear();
	spawn_duck(0, 20261031u);
	for (int i = 1; i < duck_count; i++) {
		spawn_duck(i, 20261031u + (unsigned)i * 7919u);
	}
}

void DuckFlock::spawn_duck(int p_index, unsigned p_seed) {
	std::mt19937 rng(p_seed);
	auto rnd = [&rng](float lo, float hi) {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};

	Duck d;
	d.angle = rnd(0.0f, 6.2831853f);
	d.radius = rnd(6.2f, 9.4f);
	d.speed = rnd(0.022f, 0.052f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
	d.bob_w = rnd(0.9f, 1.7f);
	d.bob_phase = rnd(0.0f, 6.2831853f);
	d.yaw_offset = (rnd(0.0f, 1.0f) - 0.5f) * 0.35f;

	d.root = memnew(Node3D);
	d.root->set_name("Duck" + String::num_int64(p_index));
	add_child(d.root);

	Ref<Shader> toon = meshutil::load_toon_shader();
	const bool male = rnd(0.0f, 1.0f) < 0.55f;
	const Color body_c = male ? Color(0.64f, 0.62f, 0.57f) : Color(0.56f, 0.47f, 0.35f);
	const Color head_c = male ? Color(0.10f, 0.36f, 0.22f) : Color(0.44f, 0.35f, 0.25f);
	const Color beak_c(0.93f, 0.58f, 0.12f);

	auto add_sphere = [&](const Vector3 &p_pos, float p_radius, float p_height,
			const Vector3 &p_scale, const Color &p_color) {
		Ref<SphereMesh> sp;
		sp.instantiate();
		sp->set_radius(p_radius);
		sp->set_height(p_height);
		sp->set_radial_segments(16);
		sp->set_rings(8);
		sp->set_material(meshutil::toon_material(toon, p_color));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(sp);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(0.0f, 0.0f, 0.0f), p_scale);
		t.set_basis(b);
		t.set_origin(p_pos);
		mi->set_transform(t);
		d.root->add_child(mi);
	};

	auto add_cone = [&](const Vector3 &p_pos, float p_bottom, float p_top, float p_height,
			float p_rot_x, const Color &p_color) {
		Ref<CylinderMesh> cy;
		cy.instantiate();
		cy->set_bottom_radius(p_bottom);
		cy->set_top_radius(p_top);
		cy->set_height(p_height);
		cy->set_radial_segments(12);
		cy->set_material(meshutil::toon_material(toon, p_color));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(cy);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(p_rot_x, 0.0f, 0.0f), Vector3(1, 1, 1));
		t.set_basis(b);
		t.set_origin(p_pos);
		mi->set_transform(t);
		d.root->add_child(mi);
	};

	// 身体：压扁的椭球（沿 -Z 朝前）
	add_sphere(Vector3(0.0f, 0.10f, 0.0f), 0.15f, 0.30f,
			Vector3(1.05f, 0.72f, 1.5f), body_c);
	// 颈
	Ref<CylinderMesh> neck;
	neck.instantiate();
	neck->set_bottom_radius(0.045f);
	neck->set_top_radius(0.034f);
	neck->set_height(0.16f);
	neck->set_radial_segments(10);
	neck->set_material(meshutil::toon_material(toon, head_c));
	MeshInstance3D *neck_mi = memnew(MeshInstance3D);
	neck_mi->set_mesh(neck);
	neck_mi->set_position(Vector3(0.0f, 0.19f, -0.09f));
	neck_mi->set_rotation(Vector3(0.25f, 0.0f, 0.0f));
	d.root->add_child(neck_mi);
	// 头
	add_sphere(Vector3(0.0f, 0.275f, -0.15f), 0.062f, 0.124f,
			Vector3(1.0f, 1.0f, 1.12f), head_c);
	// 喙（圆锥，朝 -Z）
	add_cone(Vector3(0.0f, 0.262f, -0.225f), 0.024f, 0.004f, 0.075f,
			-1.5707964f, beak_c);
	// 尾（圆锥，朝 +Z 略上翘）
	add_cone(Vector3(0.0f, 0.135f, 0.195f), 0.055f, 0.012f, 0.13f,
			1.35f, body_c);
	// 眼睛
	add_sphere(Vector3(0.040f, 0.292f, -0.185f), 0.010f, 0.020f, Vector3(1, 1, 1),
			Color(0.06f, 0.06f, 0.06f));
	add_sphere(Vector3(-0.040f, 0.292f, -0.185f), 0.010f, 0.020f, Vector3(1, 1, 1),
			Color(0.06f, 0.06f, 0.06f));

	// 尾迹水圈：贴在吃水线附近的一圈淡白圆环（随浮动轻微缩放）
	Ref<TorusMesh> torus;
	torus.instantiate();
	torus->set_inner_radius(0.17f);
	torus->set_outer_radius(0.21f);
	torus->set_rings(6);
	torus->set_ring_segments(20);
	Ref<StandardMaterial3D> wake_mat;
	wake_mat.instantiate();
	wake_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	wake_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wake_mat->set_albedo(Color(0.92f, 0.96f, 1.0f, 0.22f));
	wake_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	torus->set_material(wake_mat);
	MeshInstance3D *wake_mi = memnew(MeshInstance3D);
	wake_mi->set_mesh(torus);
	wake_mi->set_position(Vector3(0.0f, -0.018f, 0.02f));
	d.root->add_child(wake_mi);
	d.wake = wake_mi;

	ducks.push_back(d);
}
