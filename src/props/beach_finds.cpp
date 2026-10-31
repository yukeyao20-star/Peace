#include "beach_finds.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void BeachFinds::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &BeachFinds::get_item_types); }
void BeachFinds::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void BeachFinds::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 漂流木
	meshutil::add_box(this, toon, Vector3(0.50f, 0.08f, 0.09f), Vector3(-0.05f, 0.05f, 0.0f), Color(0.72f, 0.64f, 0.52f), Vector3(0.0f, 0.2f, 0.05f));
	meshutil::add_box(this, toon, Vector3(0.12f, 0.06f, 0.07f), Vector3(0.24f, 0.06f, 0.03f), Color(0.66f, 0.58f, 0.46f), Vector3(0.0f, 0.6f, 0.1f));
	// 海玻璃罐
	Ref<CylinderMesh> jar;
	jar.instantiate();
	jar->set_top_radius(0.06f);
	jar->set_bottom_radius(0.055f);
	jar->set_height(0.14f);
	jar->set_radial_segments(9);
	jar->set_rings(1);
	Ref<StandardMaterial3D> jm;
	jm.instantiate();
	jm->set_albedo(Color(0.70f, 0.86f, 0.90f, 0.4f));
	jm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	jm->set_roughness(0.15f);
	jar->set_material(jm);
	MeshInstance3D *jmi = memnew(MeshInstance3D);
	jmi->set_mesh(jar);
	jmi->set_position(Vector3(0.28f, 0.08f, -0.10f));
	add_child(jmi);
	const Color glass[3] = { Color(0.40f, 0.66f, 0.62f), Color(0.56f, 0.50f, 0.74f), Color(0.74f, 0.82f, 0.56f) };
	for (int i = 0; i < 3; i++) {
		meshutil::add_sphere(this, toon, 0.022f, Vector3(0.28f + 0.03f * (float)(i - 1), 0.04f, -0.10f + 0.02f * (float)i), glass[i]);
	}
	// 漂流瓶（带纸条）
	Ref<CylinderMesh> bottle;
	bottle.instantiate();
	bottle->set_top_radius(0.032f);
	bottle->set_bottom_radius(0.045f);
	bottle->set_height(0.18f);
	bottle->set_radial_segments(8);
	bottle->set_rings(1);
	Ref<StandardMaterial3D> bm;
	bm.instantiate();
	bm->set_albedo(Color(0.42f, 0.58f, 0.46f, 0.7f));
	bm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	bm->set_roughness(0.2f);
	bottle->set_material(bm);
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(bottle);
	bmi->set_position(Vector3(-0.30f, 0.10f, -0.16f));
	bmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(bmi);
	meshutil::add_box(this, toon, Vector3(0.12f, 0.03f, 0.03f), Vector3(-0.30f, 0.10f, -0.16f), Color(0.92f, 0.88f, 0.76f), Vector3(0.0f, 0.4f, 0.0f));
	// 海星
	Node3D *star = memnew(Node3D);
	star->set_name("Starfish");
	star->set_position(Vector3(0.08f, 0.03f, 0.18f));
	add_child(star);
	for (int i = 0; i < 5; i++) {
		const float a = (float)i * 1.2566f;
		meshutil::add_box(star, toon, Vector3(0.13f, 0.025f, 0.045f),
				Vector3(std::cos(a) * 0.055f, 0.0f, std::sin(a) * 0.055f), Color(0.84f, 0.48f, 0.34f),
				Vector3(0.0f, -a, 0.0f));
	}
	// 羽毛
	Ref<PlaneMesh> feather;
	feather.instantiate();
	feather->set_size(Vector2(0.06f, 0.18f));
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_albedo(Color(0.94f, 0.92f, 0.86f));
	fm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	feather->set_material(fm);
	MeshInstance3D *fmi = memnew(MeshInstance3D);
	fmi->set_mesh(feather);
	fmi->set_position(Vector3(-0.12f, 0.04f, 0.24f));
	fmi->set_rotation(Vector3(-1.5707964f, 0.5f, 0.0f));
	add_child(fmi);
	meshutil::add_box(this, toon, Vector3(0.006f, 0.006f, 0.20f), Vector3(-0.12f, 0.045f, 0.24f), Color(0.72f, 0.68f, 0.60f), Vector3(0.0f, 0.5f, 0.0f));
}
