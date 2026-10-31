#include "picnic_food.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void PicnicFood::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &PicnicFood::get_item_types); }
void PicnicFood::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void PicnicFood::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 野餐篮（敞口）
	meshutil::add_box(this, toon, Vector3(0.44f, 0.18f, 0.30f), Vector3(0.0f, 0.09f, 0.0f), Color(0.64f, 0.46f, 0.26f));
	meshutil::add_box(this, toon, Vector3(0.46f, 0.03f, 0.32f), Vector3(0.0f, 0.20f, 0.0f), Color(0.54f, 0.38f, 0.22f));
	// 西瓜（半月形：红球 + 绿边）
	for (int i = 0; i < 2; i++) {
		MeshInstance3D *melon = meshutil::add_sphere(this, toon, 0.12f, Vector3(-0.12f + 0.22f * (float)i, 0.10f, 0.26f), Color(0.84f, 0.26f, 0.24f));
		melon->set_scale(Vector3(1.0f, 0.5f, 0.9f));
		meshutil::add_box(this, toon, Vector3(0.26f, 0.02f, 0.20f), Vector3(-0.12f + 0.22f * (float)i, 0.02f, 0.26f), Color(0.34f, 0.52f, 0.26f));
	}
	// 柠檬水罐
	Ref<CylinderMesh> jug;
	jug.instantiate();
	jug->set_top_radius(0.08f);
	jug->set_bottom_radius(0.09f);
	jug->set_height(0.20f);
	jug->set_radial_segments(10);
	jug->set_rings(1);
	Ref<StandardMaterial3D> jm;
	jm.instantiate();
	jm->set_albedo(Color(0.92f, 0.84f, 0.46f, 0.85f));
	jm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	jm->set_roughness(0.2f);
	jug->set_material(jm);
	MeshInstance3D *jmi = memnew(MeshInstance3D);
	jmi->set_mesh(jug);
	jmi->set_position(Vector3(0.34f, 0.10f, 0.0f));
	add_child(jmi);
	Ref<TorusMesh> handle;
	handle.instantiate();
	handle->set_inner_radius(0.05f);
	handle->set_outer_radius(0.062f);
	handle->set_rings(10);
	handle->set_ring_segments(3);
	handle->set_material(meshutil::toon_material(toon, Color(0.86f, 0.78f, 0.44f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(handle);
	hmi->set_position(Vector3(0.43f, 0.12f, 0.0f));
	hmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(hmi);
	// 馅饼
	Ref<CylinderMesh> pie;
	pie.instantiate();
	pie->set_top_radius(0.13f);
	pie->set_bottom_radius(0.11f);
	pie->set_height(0.05f);
	pie->set_radial_segments(10);
	pie->set_rings(1);
	pie->set_material(meshutil::toon_material(toon, Color(0.82f, 0.62f, 0.32f), 0.2f));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_mesh(pie);
	pmi->set_position(Vector3(-0.30f, 0.03f, -0.22f));
	add_child(pmi);
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 1.5708f;
		meshutil::add_box(this, toon, Vector3(0.02f, 0.02f, 0.16f), Vector3(-0.30f + std::cos(a) * 0.05f, 0.06f, -0.22f + std::sin(a) * 0.05f), Color(0.92f, 0.74f, 0.40f), Vector3(0.0f, -a, 0.0f));
	}
	// 三明治
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(this, toon, Vector3(0.16f, 0.07f, 0.12f), Vector3(0.12f, 0.04f, -0.24f + 0.15f * (float)i), Color(0.92f, 0.86f, 0.68f), Vector3(0.0f, 0.3f, 0.0f));
		meshutil::add_box(this, toon, Vector3(0.15f, 0.01f, 0.11f), Vector3(0.12f, 0.055f, -0.24f + 0.15f * (float)i), Color(0.40f, 0.58f, 0.30f), Vector3(0.0f, 0.3f, 0.0f));
	}
	// 草莓
	for (int i = 0; i < 5; i++) {
		const float a = (float)i * 1.2566f;
		meshutil::add_sphere(this, toon, 0.03f, Vector3(0.30f + std::cos(a) * 0.06f, 0.03f, 0.26f + std::sin(a) * 0.06f), Color(0.82f, 0.18f, 0.22f));
		meshutil::add_box(this, toon, Vector3(0.02f, 0.01f, 0.02f), Vector3(0.30f + std::cos(a) * 0.06f, 0.055f, 0.26f + std::sin(a) * 0.06f), Color(0.36f, 0.54f, 0.24f));
	}
}
