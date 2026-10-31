#include "garden_tools.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
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
	// 独轮车
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
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(this, toon, Vector3(0.03f, 0.03f, 0.50f), Vector3(0.0f, 0.42f, -0.36f - 0.10f * (float)i), Color(0.46f, 0.32f, 0.20f), Vector3(0.35f, 0.0f, 0.0f));
	}
	// 耙子/铁锹/锄头：靠在一起
	const Color handle(0.56f, 0.40f, 0.24f);
	const Color metal(0.52f, 0.54f, 0.56f);
	meshutil::add_box(this, toon, Vector3(0.025f, 0.85f, 0.025f), Vector3(-0.55f, 0.42f, 0.10f), handle, Vector3(0.0f, 0.0f, 0.18f));
	meshutil::add_box(this, toon, Vector3(0.26f, 0.03f, 0.03f), Vector3(-0.63f, 0.84f, 0.10f), metal);
	for (int i = 0; i < 5; i++) {
		meshutil::add_box(this, toon, Vector3(0.012f, 0.008f, 0.10f), Vector3(-0.75f + 0.06f * (float)i, 0.84f, 0.10f), metal);
	}
	meshutil::add_box(this, toon, Vector3(0.025f, 0.85f, 0.025f), Vector3(-0.30f, 0.42f, 0.18f), handle, Vector3(0.0f, 0.0f, -0.14f));
	meshutil::add_box(this, toon, Vector3(0.14f, 0.20f, 0.02f), Vector3(-0.24f, 0.06f, 0.18f), metal, Vector3(0.0f, 0.0f, -0.14f));
	meshutil::add_box(this, toon, Vector3(0.025f, 0.80f, 0.025f), Vector3(-0.05f, 0.40f, 0.24f), handle, Vector3(0.0f, 0.0f, 0.10f));
	meshutil::add_box(this, toon, Vector3(0.18f, 0.03f, 0.05f), Vector3(0.02f, 0.05f, 0.24f), metal, Vector3(0.0f, 0.0f, 0.10f));
	// 种子纸袋
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(this, toon, Vector3(0.10f, 0.14f, 0.03f), Vector3(0.42f, 0.07f, 0.18f + 0.10f * (float)i),
				Color(0.88f, 0.76f, 0.52f), Vector3(0.0f, 0.2f * (float)i, 0.0f));
	}
}
