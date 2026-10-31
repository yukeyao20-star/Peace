#include "hearth_tools.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
using namespace godot;

void HearthTools::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &HearthTools::get_item_types); }
void HearthTools::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void HearthTools::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color iron(0.34f, 0.32f, 0.30f);
	const Color wood(0.48f, 0.33f, 0.20f);
	// 工具架
	meshutil::add_box(this, toon, Vector3(0.34f, 0.04f, 0.20f), Vector3(0.0f, 0.02f, 0.0f), iron);
	meshutil::add_box(this, toon, Vector3(0.04f, 0.70f, 0.04f), Vector3(-0.14f, 0.37f, 0.0f), iron);
	meshutil::add_box(this, toon, Vector3(0.30f, 0.03f, 0.03f), Vector3(0.0f, 0.62f, 0.0f), iron);
	// 火钳
	meshutil::add_box(this, toon, Vector3(0.025f, 0.55f, 0.025f), Vector3(-0.04f, 0.35f, 0.05f), iron);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.03f, 0.03f), Vector3(0.02f, 0.08f, 0.05f), iron, Vector3(0.0f, 0.0f, 0.4f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.03f, 0.03f), Vector3(-0.02f, 0.08f, 0.05f), iron, Vector3(0.0f, 0.0f, -0.4f));
	// 小铲
	meshutil::add_box(this, toon, Vector3(0.025f, 0.50f, 0.025f), Vector3(0.08f, 0.33f, 0.05f), wood);
	meshutil::add_box(this, toon, Vector3(0.12f, 0.14f, 0.02f), Vector3(0.08f, 0.06f, 0.05f), iron);
	// 风箱
	meshutil::add_box(this, toon, Vector3(0.22f, 0.05f, 0.10f), Vector3(0.26f, 0.12f, 0.10f), wood, Vector3(0.0f, 0.4f, 0.15f));
	meshutil::add_box(this, toon, Vector3(0.20f, 0.05f, 0.09f), Vector3(0.27f, 0.17f, 0.10f), wood, Vector3(0.0f, 0.4f, -0.15f));
	meshutil::add_box(this, toon, Vector3(0.025f, 0.025f, 0.16f), Vector3(0.37f, 0.14f, 0.12f), iron, Vector3(0.0f, 0.5f, 0.0f));
	// 柴篮与柴
	Ref<CylinderMesh> basket;
	basket.instantiate();
	basket->set_top_radius(0.16f);
	basket->set_bottom_radius(0.12f);
	basket->set_height(0.20f);
	basket->set_radial_segments(10);
	basket->set_rings(1);
	basket->set_material(meshutil::toon_material(toon, Color(0.58f, 0.44f, 0.28f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(basket);
	bmi->set_position(Vector3(-0.38f, 0.10f, 0.15f));
	add_child(bmi);
	for (int i = 0; i < 3; i++) {
		Ref<CylinderMesh> log;
		log.instantiate();
		log->set_top_radius(0.035f);
		log->set_bottom_radius(0.035f);
		log->set_height(0.26f);
		log->set_radial_segments(6);
		log->set_rings(1);
		log->set_material(meshutil::toon_material(toon, Color(0.44f, 0.30f, 0.18f), 0.2f));
		MeshInstance3D *lmi = memnew(MeshInstance3D);
		lmi->set_mesh(log);
		lmi->set_position(Vector3(-0.38f + 0.04f * (float)i, 0.22f + 0.05f * (float)i, 0.15f));
		lmi->set_rotation(Vector3(0.4f, 0.3f * (float)i, 1.5707964f));
		add_child(lmi);
	}
	// 火柴盒
	meshutil::add_box(this, toon, Vector3(0.12f, 0.04f, 0.08f), Vector3(-0.05f, 0.03f, 0.24f), Color(0.82f, 0.70f, 0.48f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.01f, 0.06f), Vector3(-0.05f, 0.055f, 0.24f), Color(0.56f, 0.24f, 0.20f));
}
