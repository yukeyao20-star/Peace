#include "winter_gear.h"
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

void WinterGear::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &WinterGear::get_item_types); }
void WinterGear::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void WinterGear::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 雪橇：两根滑板 + 横条
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(this, toon, Vector3(0.08f, 0.06f, 0.90f), Vector3((i == 0 ? -0.22f : 0.22f), 0.04f, 0.0f), Color(0.52f, 0.34f, 0.20f));
	}
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.52f, 0.04f, 0.10f), Vector3(0.0f, 0.10f, -0.28f + 0.28f * (float)i), Color(0.62f, 0.44f, 0.26f));
	}
	meshutil::add_box(this, toon, Vector3(0.02f, 0.02f, 0.50f), Vector3(0.0f, 0.08f, 0.45f), Color(0.76f, 0.68f, 0.52f), Vector3(0.2f, 0.0f, 0.0f));
	// 手套
	for (int i = 0; i < 2; i++) {
		MeshInstance3D *m = meshutil::add_sphere(this, toon, 0.07f, Vector3(0.55f, 0.06f, 0.10f + 0.15f * (float)i), i == 0 ? Color(0.78f, 0.26f, 0.24f) : Color(0.30f, 0.44f, 0.56f));
		m->set_scale(Vector3(1.0f, 0.5f, 1.4f));
		meshutil::add_box(this, toon, Vector3(0.04f, 0.03f, 0.06f), Vector3(0.61f, 0.06f, 0.04f + 0.15f * (float)i), i == 0 ? Color(0.78f, 0.26f, 0.24f) : Color(0.30f, 0.44f, 0.56f));
	}
	// 围巾搭在雪橇把上
	meshutil::add_box(this, toon, Vector3(0.16f, 0.03f, 0.55f), Vector3(-0.30f, 0.20f, 0.10f), Color(0.80f, 0.48f, 0.24f), Vector3(0.0f, 0.2f, 0.3f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.18f, 0.03f), Vector3(-0.30f, 0.10f, 0.36f), Color(0.80f, 0.48f, 0.24f));
	// 雪铲
	meshutil::add_box(this, toon, Vector3(0.035f, 0.90f, 0.035f), Vector3(-0.55f, 0.45f, -0.10f), Color(0.56f, 0.42f, 0.26f), Vector3(0.0f, 0.0f, -0.25f));
	meshutil::add_box(this, toon, Vector3(0.30f, 0.36f, 0.03f), Vector3(-0.68f, 0.10f, -0.18f), Color(0.60f, 0.64f, 0.66f), Vector3(0.0f, 0.0f, -0.25f));
	// 雪球堆
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 1.5708f;
		MeshInstance3D *s = meshutil::add_sphere(this, toon, 0.075f, Vector3(0.05f + std::cos(a) * 0.10f, 0.07f, 0.45f + std::sin(a) * 0.10f), Color(0.93f, 0.96f, 1.0f));
		(void)s;
	}
}
