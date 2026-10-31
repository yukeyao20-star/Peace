#include "garden_tools.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void GardenTools::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &GardenTools::get_item_types); }

void GardenTools::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}

void GardenTools::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}
	// 独轮车车身与轮子
	meshutil::add_box(this, toon, Vector3(0.50f, 0.06f, 0.34f), Vector3(0.0f, 0.42f, 0.0f), Color(0.54f, 0.38f, 0.24f));
	meshutil::add_box(this, toon, Vector3(0.46f, 0.22f, 0.30f), Vector3(0.0f, 0.55f, 0.0f), Color(0.58f, 0.42f, 0.28f));
	Ref<CylinderMesh> wheel;
	wheel.instantiate();
	wheel->set_top_radius(0.16f);
	wheel->set_bottom_radius(0.16f);
	wheel->set_height(0.06f);
	wheel->set_radial_segments(10);
	wheel->set_rings(1);
	wheel->set_material(meshutil::toon_material(toon, Color(0.26f, 0.24f, 0.22f), 0.2f));
	MeshInstance3D *wmi = memnew(MeshInstance3D);
	wmi->set_mesh(wheel);
	wmi->set_position(Vector3(0.0f, 0.17f, 0.22f));
	wmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(wmi);

	// 优化（第 150 轮）：木柄、金属头、种子袋各合成 MultiMesh。
	Ref<BoxMesh> unit_box;
	unit_box.instantiate();
	unit_box->set_size(Vector3(1.0f, 1.0f, 1.0f));
	unit_box->set_material(meshutil::toon_material(toon, Color(0.56f, 0.40f, 0.24f), 0.2f));
	Ref<MultiMesh> handle_mm;
	handle_mm.instantiate();
	handle_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	handle_mm->set_mesh(unit_box);
	handle_mm->set_instance_count(5);
	{
		Basis h0; h0.set_euler_scale(Vector3(0.35f, 0.0f, 0.0f), Vector3(0.03f, 0.03f, 0.50f));
		handle_mm->set_instance_transform(0, Transform3D(h0, Vector3(0.0f, 0.42f, -0.36f)));
		handle_mm->set_instance_transform(1, Transform3D(h0, Vector3(0.0f, 0.42f, -0.46f)));
		Basis h2; h2.set_euler_scale(Vector3(0.0f, 0.0f, 0.18f), Vector3(0.025f, 0.85f, 0.025f));
		handle_mm->set_instance_transform(2, Transform3D(h2, Vector3(-0.55f, 0.42f, 0.10f)));
		Basis h3; h3.set_euler_scale(Vector3(0.0f, 0.0f, -0.14f), Vector3(0.025f, 0.85f, 0.025f));
		handle_mm->set_instance_transform(3, Transform3D(h3, Vector3(-0.30f, 0.42f, 0.18f)));
		Basis h4; h4.set_euler_scale(Vector3(0.0f, 0.0f, 0.10f), Vector3(0.025f, 0.80f, 0.025f));
		handle_mm->set_instance_transform(4, Transform3D(h4, Vector3(-0.05f, 0.40f, 0.24f)));
	}
	MultiMeshInstance3D *handle_mi = memnew(MultiMeshInstance3D);
	handle_mi->set_name("Handles");
	handle_mi->set_multimesh(handle_mm);
	add_child(handle_mi);

	Ref<BoxMesh> metal_box;
	metal_box.instantiate();
	metal_box->set_size(Vector3(1.0f, 1.0f, 1.0f));
	metal_box->set_material(meshutil::toon_material(toon, Color(0.52f, 0.54f, 0.56f), 0.2f));
	Ref<MultiMesh> metal_mm;
	metal_mm.instantiate();
	metal_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	metal_mm->set_mesh(metal_box);
	metal_mm->set_instance_count(8);
	// 耙头 + 5 根耙齿
	metal_mm->set_instance_transform(0, Transform3D(Basis().scaled(Vector3(0.26f, 0.03f, 0.03f)), Vector3(-0.63f, 0.84f, 0.10f)));
	for (int i = 0; i < 5; i++) {
		metal_mm->set_instance_transform(1 + i, Transform3D(Basis().scaled(Vector3(0.012f, 0.008f, 0.10f)),
				Vector3(-0.75f + 0.06f * (float)i, 0.84f, 0.10f)));
	}
	Basis sb; sb.set_euler_scale(Vector3(0.0f, 0.0f, -0.14f), Vector3(0.14f, 0.20f, 0.02f));
	metal_mm->set_instance_transform(6, Transform3D(sb, Vector3(-0.24f, 0.06f, 0.18f)));
	Basis hb; hb.set_euler_scale(Vector3(0.0f, 0.0f, 0.10f), Vector3(0.18f, 0.03f, 0.05f));
	metal_mm->set_instance_transform(7, Transform3D(hb, Vector3(0.02f, 0.05f, 0.24f)));
	MultiMeshInstance3D *metal_mi = memnew(MultiMeshInstance3D);
	metal_mi->set_name("MetalHeads");
	metal_mi->set_multimesh(metal_mm);
	add_child(metal_mi);

	Ref<MultiMesh> seed_mm;
	seed_mm.instantiate();
	seed_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	seed_mm->set_mesh(unit_box);
	seed_mm->set_instance_count(4);
	Ref<BoxMesh> seed_box;
	seed_box.instantiate();
	seed_box->set_size(Vector3(0.10f, 0.14f, 0.03f));
	seed_box->set_material(meshutil::toon_material(toon, Color(0.88f, 0.76f, 0.52f), 0.2f));
	seed_mm->set_mesh(seed_box);
	for (int i = 0; i < 4; i++) {
		Basis b; b.set_euler_scale(Vector3(0.0f, 0.2f * (float)i, 0.0f), Vector3(1, 1, 1));
		seed_mm->set_instance_transform(i, Transform3D(b, Vector3(0.42f, 0.07f, 0.18f + 0.10f * (float)i)));
	}
	MultiMeshInstance3D *seed_mi = memnew(MultiMeshInstance3D);
	seed_mi->set_name("SeedPackets");
	seed_mi->set_multimesh(seed_mm);
	add_child(seed_mi);
}
