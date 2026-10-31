#include "kitchen_pantry.h"
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

void KitchenPantry::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &KitchenPantry::get_item_types); }
void KitchenPantry::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}

void KitchenPantry::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	set_rotation(Vector3(0.0f, -1.5708f, 0.0f));
	// 面粉袋
	meshutil::add_box(this, toon, Vector3(0.28f, 0.40f, 0.22f), Vector3(0.30f, 0.20f, 0.0f), Color(0.90f, 0.86f, 0.74f), Vector3(0.0f, 0.2f, 0.05f));
	meshutil::add_box(this, toon, Vector3(0.20f, 0.02f, 0.02f), Vector3(0.30f, 0.36f, 0.0f), Color(0.66f, 0.54f, 0.38f), Vector3(0.0f, 0.2f, 0.0f));

	// 香料小架 + 四个小罐
	meshutil::add_box(this, toon, Vector3(0.52f, 0.04f, 0.18f), Vector3(0.02f, 0.78f, -0.12f), Color(0.52f, 0.36f, 0.22f));
	meshutil::add_box(this, toon, Vector3(0.52f, 0.03f, 0.16f), Vector3(0.02f, 0.55f, -0.13f), Color(0.52f, 0.36f, 0.22f));
	for (int i = 0; i < 4; i++) {
		Ref<CylinderMesh> jar;
		jar.instantiate();
		jar->set_top_radius(0.035f);
		jar->set_bottom_radius(0.032f);
		jar->set_height(0.075f);
		jar->set_radial_segments(8);
		jar->set_rings(1);
		const Color cols[4] = { Color(0.72f, 0.42f, 0.18f), Color(0.56f, 0.30f, 0.20f), Color(0.40f, 0.50f, 0.24f), Color(0.78f, 0.68f, 0.36f) };
		jar->set_material(meshutil::toon_material(toon, cols[i], 0.2f));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(jar);
		mi->set_position(Vector3(-0.16f + 0.12f * (float)i, 0.84f, -0.12f));
		add_child(mi);
	}

	// 蜂蜜罐
	Ref<CylinderMesh> honey;
	honey.instantiate();
	honey->set_top_radius(0.065f);
	honey->set_bottom_radius(0.055f);
	honey->set_height(0.13f);
	honey->set_radial_segments(9);
	honey->set_rings(1);
	Ref<StandardMaterial3D> hm;
	hm.instantiate();
	hm->set_albedo(Color(0.86f, 0.62f, 0.16f, 0.85f));
	hm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hm->set_roughness(0.2f);
	honey->set_material(hm);
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(honey);
	hmi->set_position(Vector3(-0.30f, 0.09f, 0.18f));
	add_child(hmi);
	meshutil::add_box(this, toon, Vector3(0.13f, 0.025f, 0.13f), Vector3(-0.30f, 0.17f, 0.18f), Color(0.86f, 0.68f, 0.36f));

	// 黄油搅拌桶
	Ref<CylinderMesh> churn;
	churn.instantiate();
	churn->set_top_radius(0.13f);
	churn->set_bottom_radius(0.15f);
	churn->set_height(0.42f);
	churn->set_radial_segments(12);
	churn->set_rings(1);
	churn->set_material(meshutil::toon_material(toon, Color(0.54f, 0.38f, 0.24f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(churn);
	cmi->set_position(Vector3(0.32f, 0.21f, 0.30f));
	add_child(cmi);
	meshutil::add_box(this, toon, Vector3(0.03f, 0.30f, 0.03f), Vector3(0.32f, 0.52f, 0.30f), Color(0.60f, 0.48f, 0.30f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.03f, 0.03f), Vector3(0.32f, 0.68f, 0.30f), Color(0.60f, 0.48f, 0.30f));

	// 擀面杖
	Ref<CylinderMesh> pin;
	pin.instantiate();
	pin->set_top_radius(0.035f);
	pin->set_bottom_radius(0.035f);
	pin->set_height(0.42f);
	pin->set_radial_segments(8);
	pin->set_rings(1);
	pin->set_material(meshutil::toon_material(toon, Color(0.78f, 0.62f, 0.38f), 0.2f));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_mesh(pin);
	pmi->set_position(Vector3(-0.05f, 0.05f, 0.38f));
	pmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(pmi);
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> handle;
		handle.instantiate();
		handle->set_top_radius(0.015f);
		handle->set_bottom_radius(0.015f);
		handle->set_height(0.10f);
		handle->set_radial_segments(6);
		handle->set_rings(1);
		handle->set_material(meshutil::toon_material(toon, Color(0.64f, 0.48f, 0.28f), 0.2f));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(handle);
		mi->set_position(Vector3(-0.05f + (i == 0 ? -0.26f : 0.26f), 0.05f, 0.38f));
		mi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
		add_child(mi);
	}
}
