#include "crate_stack.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void CrateStack::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &CrateStack::get_item_types); }

void CrateStack::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}

void CrateStack::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}
	const Color wood(0.54f, 0.38f, 0.23f);
	const Color wood_dark(0.42f, 0.29f, 0.18f);

	Ref<BoxMesh> unit_box;
	unit_box.instantiate();
	unit_box->set_size(Vector3(1.0f, 1.0f, 1.0f));
	unit_box->set_material(meshutil::toon_material(toon, wood, 0.2f));
	Ref<MultiMesh> crate_mm;
	crate_mm.instantiate();
	crate_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	crate_mm->set_mesh(unit_box);
	crate_mm->set_instance_count(3);
	{
		Basis b0 = Basis().scaled(Vector3(0.54f, 0.44f, 0.46f));
		crate_mm->set_instance_transform(0, Transform3D(b0, Vector3(-0.10f, 0.22f, 0.0f)));
		Basis b1; b1.set_euler_scale(Vector3(0.0f, 0.18f, 0.0f), Vector3(0.46f, 0.38f, 0.40f));
		crate_mm->set_instance_transform(1, Transform3D(b1, Vector3(-0.10f, 0.63f, 0.02f)));
		Basis b2; b2.set_euler_scale(Vector3(0.0f, -0.12f, 0.0f), Vector3(0.40f, 0.34f, 0.36f));
		crate_mm->set_instance_transform(2, Transform3D(b2, Vector3(-0.62f, 0.17f, 0.10f)));
	}
	MultiMeshInstance3D *crate_mi = memnew(MultiMeshInstance3D);
	crate_mi->set_name("Crates");
	crate_mi->set_multimesh(crate_mm);
	add_child(crate_mi);

	Ref<BoxMesh> slat_box;
	slat_box.instantiate();
	slat_box->set_size(Vector3(0.56f, 0.03f, 0.02f));
	slat_box->set_material(meshutil::toon_material(toon, wood_dark, 0.2f));
	Ref<MultiMesh> slat_mm;
	slat_mm.instantiate();
	slat_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	slat_mm->set_mesh(slat_box);
	slat_mm->set_instance_count(3);
	for (int i = 0; i < 3; i++) {
		slat_mm->set_instance_transform(i, Transform3D(Basis(), Vector3(-0.10f, 0.10f + 0.13f * (float)i, 0.235f)));
	}
	MultiMeshInstance3D *slat_mi = memnew(MultiMeshInstance3D);
	slat_mi->set_name("Slats");
	slat_mi->set_multimesh(slat_mm);
	add_child(slat_mi);

	Ref<CylinderMesh> barrel_unit;
	barrel_unit.instantiate();
	barrel_unit->set_top_radius(1.0f);
	barrel_unit->set_bottom_radius(0.85f);
	barrel_unit->set_height(1.0f);
	barrel_unit->set_radial_segments(12);
	barrel_unit->set_rings(1);
	barrel_unit->set_material(meshutil::toon_material(toon, Color(0.50f, 0.34f, 0.20f), 0.2f));
	Ref<CylinderMesh> hoop_unit;
	hoop_unit.instantiate();
	hoop_unit->set_top_radius(1.0f);
	hoop_unit->set_bottom_radius(1.0f);
	hoop_unit->set_height(1.0f);
	hoop_unit->set_radial_segments(12);
	hoop_unit->set_rings(1);
	hoop_unit->set_material(meshutil::toon_material(toon, Color(0.36f, 0.34f, 0.30f), 0.15f));
	Ref<MultiMesh> barrel_mm;
	barrel_mm.instantiate();
	barrel_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	barrel_mm->set_mesh(barrel_unit);
	barrel_mm->set_instance_count(2);
	Ref<MultiMesh> hoop_mm;
	hoop_mm.instantiate();
	hoop_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	hoop_mm->set_mesh(hoop_unit);
	hoop_mm->set_instance_count(4);
	for (int b = 0; b < 2; b++) {
		const float x = 0.55f + 0.30f * (float)b;
		barrel_mm->set_instance_transform(b, Transform3D(Basis().scaled(Vector3(0.20f, 0.46f, 0.20f)), Vector3(x, 0.23f, 0.15f * (float)b)));
		for (int i = 0; i < 2; i++) {
			hoop_mm->set_instance_transform(b * 2 + i, Transform3D(Basis().scaled(Vector3(0.205f, 0.02f, 0.205f)), Vector3(x, 0.10f + 0.24f * (float)i, 0.15f * (float)b)));
		}
	}
	MultiMeshInstance3D *barrel_mi = memnew(MultiMeshInstance3D);
	barrel_mi->set_name("Barrels");
	barrel_mi->set_multimesh(barrel_mm);
	add_child(barrel_mi);
	MultiMeshInstance3D *hoop_mi = memnew(MultiMeshInstance3D);
	hoop_mi->set_name("Hoops");
	hoop_mi->set_multimesh(hoop_mm);
	add_child(hoop_mi);

	meshutil::add_box(this, toon, Vector3(0.32f, 0.38f, 0.24f), Vector3(0.22f, 0.19f, 0.48f), Color(0.72f, 0.62f, 0.44f), Vector3(0.0f, 0.3f, 0.05f));
	meshutil::add_box(this, toon, Vector3(0.24f, 0.02f, 0.02f), Vector3(0.22f, 0.34f, 0.48f), Color(0.52f, 0.42f, 0.28f), Vector3(0.0f, 0.3f, 0.0f));
}
