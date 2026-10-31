#include "doorstep_box.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
using namespace godot;

void DoorstepBox::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &DoorstepBox::get_item_types); }
void DoorstepBox::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void DoorstepBox::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 木箱
	meshutil::add_box(this, toon, Vector3(0.46f, 0.26f, 0.34f), Vector3(0.0f, 0.13f, 0.0f), Color(0.56f, 0.40f, 0.24f));
	meshutil::add_box(this, toon, Vector3(0.48f, 0.03f, 0.36f), Vector3(0.0f, 0.27f, 0.0f), Color(0.44f, 0.31f, 0.19f));
	// 两只牛奶瓶
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> bottle;
		bottle.instantiate();
		bottle->set_top_radius(0.045f);
		bottle->set_bottom_radius(0.055f);
		bottle->set_height(0.20f);
		bottle->set_radial_segments(9);
		bottle->set_rings(1);
		bottle->set_material(meshutil::toon_material(toon, Color(0.94f, 0.94f, 0.90f), 0.25f));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(bottle);
		mi->set_position(Vector3(-0.12f + 0.13f * (float)i, 0.38f, -0.04f));
		add_child(mi);
		meshutil::add_sphere(this, toon, 0.045f, Vector3(-0.12f + 0.13f * (float)i, 0.29f, -0.04f), Color(0.90f, 0.90f, 0.86f));
	}
	// 报纸
	meshutil::add_box(this, toon, Vector3(0.22f, 0.04f, 0.16f), Vector3(0.16f, 0.30f, 0.08f), Color(0.90f, 0.88f, 0.82f), Vector3(0.0f, 0.2f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.20f, 0.01f, 0.14f), Vector3(0.16f, 0.325f, 0.08f), Color(0.70f, 0.70f, 0.66f), Vector3(0.0f, 0.2f, 0.0f));
	// 包裹
	meshutil::add_box(this, toon, Vector3(0.26f, 0.18f, 0.22f), Vector3(-0.10f, 0.03f, 0.04f), Color(0.72f, 0.58f, 0.40f), Vector3(0.0f, 0.3f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.27f, 0.01f, 0.03f), Vector3(-0.10f, 0.12f, 0.04f), Color(0.62f, 0.54f, 0.40f), Vector3(0.0f, 0.3f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.03f, 0.01f, 0.23f), Vector3(-0.10f, 0.12f, 0.04f), Color(0.62f, 0.54f, 0.40f), Vector3(0.0f, 0.3f, 0.0f));
	// 一小束花
	meshutil::add_box(this, toon, Vector3(0.02f, 0.16f, 0.02f), Vector3(0.18f, 0.38f, -0.10f), Color(0.32f, 0.48f, 0.22f), Vector3(0.0f, 0.0f, 0.15f));
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.1f;
		meshutil::add_sphere(this, toon, 0.035f, Vector3(0.18f + std::cos(a) * 0.03f, 0.47f, -0.10f + std::sin(a) * 0.03f),
				i == 0 ? Color(0.86f, 0.44f, 0.48f) : Color(0.92f, 0.82f, 0.52f));
	}
}
