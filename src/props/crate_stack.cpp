#include "crate_stack.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
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

	// 三只板条箱
	meshutil::add_box(this, toon, Vector3(0.54f, 0.44f, 0.46f), Vector3(-0.10f, 0.22f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.46f, 0.38f, 0.40f), Vector3(-0.10f, 0.63f, 0.02f), Color(0.58f, 0.42f, 0.26f), Vector3(0.0f, 0.18f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.40f, 0.34f, 0.36f), Vector3(-0.62f, 0.17f, 0.10f), wood_dark, Vector3(0.0f, -0.12f, 0.0f));
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.56f, 0.03f, 0.02f), Vector3(-0.10f, 0.10f + 0.13f * (float)i, 0.235f), wood_dark);
	}

	// 两只木桶
	for (int b = 0; b < 2; b++) {
		const float x = 0.55f + 0.30f * (float)b;
		Ref<CylinderMesh> barrel;
		barrel.instantiate();
		barrel->set_top_radius(0.20f);
		barrel->set_bottom_radius(0.17f);
		barrel->set_height(0.46f);
		barrel->set_radial_segments(12);
		barrel->set_rings(1);
		barrel->set_material(meshutil::toon_material(toon, Color(0.50f, 0.34f, 0.20f), 0.2f));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(barrel);
		mi->set_position(Vector3(x, 0.23f, 0.15f * (float)b));
		add_child(mi);
		for (int i = 0; i < 2; i++) {
			Ref<CylinderMesh> hoop;
			hoop.instantiate();
			hoop->set_top_radius(0.205f);
			hoop->set_bottom_radius(0.205f);
			hoop->set_height(0.02f);
			hoop->set_radial_segments(12);
			hoop->set_rings(1);
			hoop->set_material(meshutil::toon_material(toon, Color(0.36f, 0.34f, 0.30f), 0.15f));
			MeshInstance3D *h = memnew(MeshInstance3D);
			h->set_mesh(hoop);
			h->set_position(Vector3(x, 0.10f + 0.24f * (float)i, 0.15f * (float)b));
			add_child(h);
		}
	}

	// 粗麻袋
	meshutil::add_box(this, toon, Vector3(0.32f, 0.38f, 0.24f), Vector3(0.22f, 0.19f, 0.48f), Color(0.72f, 0.62f, 0.44f), Vector3(0.0f, 0.3f, 0.05f));
	meshutil::add_box(this, toon, Vector3(0.24f, 0.02f, 0.02f), Vector3(0.22f, 0.34f, 0.48f), Color(0.52f, 0.42f, 0.28f), Vector3(0.0f, 0.3f, 0.0f));
}
