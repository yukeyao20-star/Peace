#include "tabletop_games.h"
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
#include "../world/terrain_builder.h"
using namespace godot;

void TabletopGames::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &TabletopGames::get_item_types); }
void TabletopGames::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void TabletopGames::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 小木托盘
	meshutil::add_box(this, toon, Vector3(0.70f, 0.04f, 0.46f), Vector3(0.0f, 0.02f, 0.0f), Color(0.56f, 0.40f, 0.24f));
	// 多米诺（一排倒下的骨牌）
	for (int i = 0; i < 5; i++) {
		meshutil::add_box(this, toon, Vector3(0.06f, 0.05f, 0.12f), Vector3(-0.26f + 0.08f * (float)i, 0.065f, -0.10f),
				i % 2 == 0 ? Color(0.92f, 0.90f, 0.84f) : Color(0.84f, 0.82f, 0.76f), Vector3(0.0f, 0.25f * (float)i, 0.0f));
		meshutil::add_box(this, toon, Vector3(0.01f, 0.051f, 0.06f), Vector3(-0.26f + 0.08f * (float)i, 0.065f, -0.10f), Color(0.30f, 0.28f, 0.26f));
	}
	// 一叠纸牌
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(this, toon, Vector3(0.16f, 0.012f, 0.11f), Vector3(0.12f, 0.05f + 0.012f * (float)i, -0.10f),
				i == 3 ? Color(0.84f, 0.26f, 0.24f) : Color(0.94f, 0.92f, 0.86f), Vector3(0.0f, 0.15f * (float)i, 0.0f));
	}
	// 骰盅与骰子
	Ref<CylinderMesh> cup;
	cup.instantiate();
	cup->set_top_radius(0.075f);
	cup->set_bottom_radius(0.065f);
	cup->set_height(0.12f);
	cup->set_radial_segments(9);
	cup->set_rings(1);
	cup->set_material(meshutil::toon_material(toon, Color(0.44f, 0.30f, 0.18f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(cup);
	cmi->set_position(Vector3(-0.22f, 0.10f, 0.14f));
	add_child(cmi);
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.032f, 0.032f, 0.032f),
				Vector3(-0.02f + 0.05f * (float)i, 0.055f, 0.14f + 0.03f * (float)(i % 2)), Color(0.94f, 0.92f, 0.86f),
				Vector3(0.0f, 0.6f * (float)i, 0.0f));
	}
	// 木盒拼图
	meshutil::add_box(this, toon, Vector3(0.16f, 0.08f, 0.16f), Vector3(0.26f, 0.08f, 0.14f), Color(0.58f, 0.42f, 0.26f));
	meshutil::add_box(this, toon, Vector3(0.17f, 0.01f, 0.17f), Vector3(0.26f, 0.125f, 0.14f), Color(0.46f, 0.32f, 0.20f));
	// 陀螺
	Ref<CylinderMesh> top;
	top.instantiate();
	top->set_top_radius(0.045f);
	top->set_bottom_radius(0.005f);
	top->set_height(0.09f);
	top->set_radial_segments(8);
	top->set_rings(1);
	top->set_material(meshutil::toon_material(toon, Color(0.80f, 0.36f, 0.24f), 0.2f));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(top);
	tmi->set_position(Vector3(0.10f, 0.09f, 0.16f));
	tmi->set_rotation(Vector3(0.0f, 0.0f, 0.1f));
	add_child(tmi);
	meshutil::add_box(this, toon, Vector3(0.012f, 0.06f, 0.012f), Vector3(0.10f, 0.15f, 0.16f), Color(0.60f, 0.46f, 0.28f));
}
