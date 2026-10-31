#include "wooden_toys.h"
#include <cmath>
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
#include "../world/terrain_builder.h"
using namespace godot;

void WoodenToys::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &WoodenToys::get_item_types); }
void WoodenToys::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void WoodenToys::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 玩具木箱
	meshutil::add_box(this, toon, Vector3(0.56f, 0.28f, 0.40f), Vector3(0.0f, 0.14f, 0.0f), Color(0.58f, 0.42f, 0.26f));
	// 积木
	const Color block_cols[4] = { Color(0.80f, 0.30f, 0.26f), Color(0.32f, 0.52f, 0.76f), Color(0.86f, 0.70f, 0.28f), Color(0.36f, 0.58f, 0.38f) };
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(this, toon, Vector3(0.09f, 0.09f, 0.09f), Vector3(-0.16f + 0.09f * (float)i, 0.18f + 0.09f * (float)(i % 2), 0.05f), block_cols[i], Vector3(0.0f, 0.3f * (float)i, 0.0f));
	}
	// 木马
	Node3D *horse = memnew(Node3D);
	horse->set_name("ToyHorse");
	horse->set_position(Vector3(0.25f, 0.17f, 0.05f));
	horse->set_rotation(Vector3(0.0f, 0.3f, 0.0f));
	add_child(horse);
	meshutil::add_box(horse, toon, Vector3(0.20f, 0.10f, 0.08f), Vector3(0.0f, 0.04f, 0.0f), Color(0.66f, 0.46f, 0.28f));
	meshutil::add_box(horse, toon, Vector3(0.06f, 0.10f, 0.06f), Vector3(0.11f, 0.10f, 0.0f), Color(0.72f, 0.52f, 0.32f));
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.07f : 0.07f;
		const float sz = (i < 2) ? -0.03f : 0.03f;
		meshutil::add_box(horse, toon, Vector3(0.025f, 0.10f, 0.025f), Vector3(sx, -0.04f, sz), Color(0.60f, 0.42f, 0.26f));
	}
	// 弹珠
	for (int i = 0; i < 4; i++) {
		MeshInstance3D *m = meshutil::add_sphere(this, toon, 0.025f, Vector3(0.55f + 0.06f * (float)i, 0.03f, 0.10f + 0.04f * (float)(i % 2)), block_cols[i]);
		(void)m;
	}
	// 拉线小火车
	Node3D *train = memnew(Node3D);
	train->set_name("ToyTrain");
	train->set_position(Vector3(-0.05f, 0.03f, 0.30f));
	add_child(train);
	meshutil::add_box(train, toon, Vector3(0.24f, 0.24f, 0.24f), Vector3(0.0f, 0.06f, 0.0f), Color(0.42f, 0.52f, 0.34f));
	meshutil::add_box(train, toon, Vector3(0.10f, 0.12f, 0.10f), Vector3(0.06f, 0.16f, 0.0f), Color(0.36f, 0.46f, 0.30f));
	for (int i = 0; i < 4; i++) {
		Ref<CylinderMesh> wheel;
		wheel.instantiate();
		wheel->set_top_radius(0.025f);
		wheel->set_bottom_radius(0.025f);
		wheel->set_height(0.02f);
		wheel->set_radial_segments(8);
		wheel->set_rings(1);
		wheel->set_material(meshutil::toon_material(toon, Color(0.26f, 0.22f, 0.18f), 0.2f));
		MeshInstance3D *w = memnew(MeshInstance3D);
		w->set_mesh(wheel);
		w->set_position(Vector3((i % 2 == 0 ? -0.07f : 0.07f), 0.02f, (i < 2 ? -0.06f : 0.06f)));
		w->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
		train->add_child(w);
	}
	// 布娃娃
	Node3D *doll = memnew(Node3D);
	doll->set_name("Doll");
	doll->set_position(Vector3(-0.35f, 0.05f, 0.15f));
	add_child(doll);
	meshutil::add_sphere(doll, toon, 0.055f, Vector3(0.0f, 0.14f, 0.0f), Color(0.94f, 0.86f, 0.74f));
	meshutil::add_box(doll, toon, Vector3(0.10f, 0.14f, 0.08f), Vector3(0.0f, 0.03f, 0.0f), Color(0.80f, 0.42f, 0.42f));
	meshutil::add_box(doll, toon, Vector3(0.03f, 0.10f, 0.03f), Vector3(-0.06f, 0.03f, 0.0f), Color(0.94f, 0.86f, 0.74f));
	meshutil::add_box(doll, toon, Vector3(0.03f, 0.10f, 0.03f), Vector3(0.06f, 0.03f, 0.0f), Color(0.94f, 0.86f, 0.74f));
}
