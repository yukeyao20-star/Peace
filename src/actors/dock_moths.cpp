#include "dock_moths.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void DockMoths::_bind_methods() {
}

void DockMoths::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky_cycle = get_parent()->get_node_or_null("SkyCycle");
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
				Node *lamp = get_parent()->get_node_or_null(NodePath("Dock/DockLantern"));
				if (lamp != nullptr) {
					lamp_pos = Object::cast_to<Node3D>(lamp)->get_global_position();
				}
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			float delta = (float)get_process_delta_time();
			anim_t += delta;
			bool active = true;
			if (sky_cycle != nullptr) {
				SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);
				if (sky != nullptr) {
					active = sky->get_night_factor() > 0.45f
							&& sky->get_rain_amount() < 0.3f
							&& sky->get_snow_amount() < 0.3f;
				}
			}
			if (player != nullptr && player->get_global_position().distance_to(lamp_pos) > 45.0f) {
				active = false;
			}
			set_visible(active);
			if (!active) {
				return;
			}
			for (Moth &m : moths) {
				if (m.root == nullptr) {
					continue;
				}
				m.angle += m.speed * delta;
				float r = m.radius + std::sin(anim_t * 0.7f + m.bob_phase) * 0.10f;
				float bob = std::sin(anim_t * m.bob_w + m.bob_phase) * 0.12f;
				Vector3 pos = lamp_pos + Vector3(std::cos(m.angle) * r, bob, std::sin(m.angle) * r);
				m.root->set_global_position(pos);
				Vector3 fwd(-std::sin(m.angle), 0.0f, std::cos(m.angle));
				m.root->look_at(pos + fwd, Vector3(0.0f, 1.0f, 0.0f));
				float flap = std::sin(anim_t * 46.0f + m.wing_phase) * 0.75f;
				if (m.wing_l != nullptr) {
					m.wing_l->set_rotation(Vector3(0.0f, 0.0f, flap));
				}
				if (m.wing_r != nullptr) {
					m.wing_r->set_rotation(Vector3(0.0f, 0.0f, -flap));
				}
			}
		} break;
		default:
			break;
	}
}

void DockMoths::build() {
	moths.clear();
	for (int i = 0; i < count; i++) {
		spawn_moth(i, 515151u + (unsigned)i * 977u);
	}
}

void DockMoths::spawn_moth(int p_index, unsigned p_seed) {
	std::mt19937 rng(p_seed);
	auto rnd = [&rng](float lo, float hi) {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};

	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color body_c(0.32f, 0.28f, 0.22f);
	const Color wing_c(0.82f, 0.76f, 0.62f);

	Moth m;
	m.angle = rnd(0.0f, 6.2831853f);
	m.radius = rnd(0.28f, 0.72f);
	m.speed = rnd(0.55f, 1.35f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
	m.bob_w = rnd(1.2f, 2.4f);
	m.bob_phase = rnd(0.0f, 6.2831853f);
	m.wing_phase = rnd(0.0f, 6.2831853f);

	m.root = memnew(Node3D);
	m.root->set_name("Moth" + String::num_int64(p_index));
	add_child(m.root);

	Ref<SphereMesh> body;
	body.instantiate();
	body->set_radius(0.016f);
	body->set_height(0.045f);
	body->set_radial_segments(6);
	body->set_rings(3);
	body->set_material(meshutil::toon_material(toon, body_c));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(body);
	bmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	m.root->add_child(bmi);

	Ref<BoxMesh> wing;
	wing.instantiate();
	wing->set_size(Vector3(0.075f, 0.003f, 0.055f));
	wing->set_material(meshutil::toon_material(toon, wing_c));

	auto side = [&](bool p_left) {
		Node3D *node = memnew(Node3D);
		node->set_name(p_left ? "WingL" : "WingR");
		node->set_position(Vector3(p_left ? -0.012f : 0.012f, 0.01f, 0.0f));
		m.root->add_child(node);
		MeshInstance3D *wmi = memnew(MeshInstance3D);
		wmi->set_mesh(wing);
		wmi->set_position(Vector3(p_left ? -0.045f : 0.045f, 0.0f, 0.0f));
		node->add_child(wmi);
		return node;
	};
	m.wing_l = side(true);
	m.wing_r = side(false);

	moths.push_back(m);
}
