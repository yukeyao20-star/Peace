#include "path_trinkets.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void PathTrinkets::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &PathTrinkets::get_item_types);
	ClassDB::bind_method(D_METHOD("get_sway"), &PathTrinkets::get_sway);
}

void PathTrinkets::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			sway = std::sin(t * 1.3f) * 0.12f + std::sin(t * 2.9f) * 0.03f;
			if (bell != nullptr) {
				bell->set_rotation(Vector3(sway * 0.7f, 0.0f, sway));
			}
			if (feather != nullptr) {
				feather->set_rotation(Vector3(0.3f + sway * 0.3f, 0.6f, sway * 0.5f));
			}
		} break;
		default:
			break;
	}
}

void PathTrinkets::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(
				get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}

	// 一块半埋的石板，小东西摆在上面
	meshutil::add_box(this, toon, Vector3(0.80f, 0.05f, 0.55f), Vector3(0.0f, 0.025f, 0.0f), Color(0.58f, 0.56f, 0.52f), Vector3(0.0f, 0.15f, 0.0f));

	// 三颗彩色小石子
	const Color pebble_cols[3] = { Color(0.62f, 0.34f, 0.28f), Color(0.34f, 0.44f, 0.56f), Color(0.70f, 0.60f, 0.30f) };
	for (int i = 0; i < 3; i++) {
		MeshInstance3D *p = meshutil::add_sphere(this, toon, 0.065f,
				Vector3(-0.24f + 0.10f * (float)i, 0.075f, -0.10f + 0.05f * (float)i), pebble_cols[i]);
		p->set_scale(Vector3(1.2f, 0.55f, 1.0f));
		// 白色小点
		meshutil::add_sphere(this, toon, 0.012f,
				Vector3(-0.24f + 0.10f * (float)i + 0.02f, 0.095f, -0.10f + 0.05f * (float)i + 0.01f),
				Color(0.95f, 0.93f, 0.86f));
	}

	// 贝壳
	Ref<CylinderMesh> shell;
	shell.instantiate();
	shell->set_top_radius(0.075f);
	shell->set_bottom_radius(0.015f);
	shell->set_height(0.10f);
	shell->set_radial_segments(9);
	shell->set_rings(2);
	shell->set_material(meshutil::toon_material(toon, Color(0.90f, 0.80f, 0.66f), 0.3f));
	MeshInstance3D *smi = memnew(MeshInstance3D);
	smi->set_mesh(shell);
	smi->set_position(Vector3(0.16f, 0.08f, -0.10f));
	smi->set_rotation(Vector3(1.5707964f, 0.3f, 0.0f));
	add_child(smi);

	// 羽毛
	feather = memnew(Node3D);
	feather->set_name("Feather");
	feather->set_position(Vector3(-0.02f, 0.10f, 0.12f));
	add_child(feather);
	Ref<PlaneMesh> feather_mesh;
	feather_mesh.instantiate();
	feather_mesh->set_size(Vector2(0.07f, 0.24f));
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_albedo(Color(0.94f, 0.92f, 0.86f));
	fm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	feather_mesh->set_material(fm);
	MeshInstance3D *fmi = memnew(MeshInstance3D);
	fmi->set_mesh(feather_mesh);
	fmi->set_rotation(Vector3(-1.5707964f, 0.0f, 0.0f));
	fmi->set_position(Vector3(0.0f, 0.10f, 0.0f));
	feather->add_child(fmi);
	meshutil::add_box(feather, toon, Vector3(0.008f, 0.26f, 0.008f), Vector3(0.0f, 0.10f, 0.0f), Color(0.70f, 0.66f, 0.58f));

	// 细枝与小铜铃
	meshutil::add_box(this, toon, Vector3(0.018f, 0.55f, 0.018f), Vector3(0.30f, 0.28f, 0.10f), Color(0.46f, 0.34f, 0.22f), Vector3(0.0f, 0.0f, 0.12f));
	bell = memnew(Node3D);
	bell->set_name("Bell");
	bell->set_position(Vector3(0.34f, 0.55f, 0.10f));
	add_child(bell);
	Ref<CylinderMesh> bell_mesh;
	bell_mesh.instantiate();
	bell_mesh->set_top_radius(0.012f);
	bell_mesh->set_bottom_radius(0.045f);
	bell_mesh->set_height(0.08f);
	bell_mesh->set_radial_segments(8);
	bell_mesh->set_rings(1);
	bell_mesh->set_material(meshutil::toon_material(toon, Color(0.78f, 0.62f, 0.28f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(bell_mesh);
	bmi->set_position(Vector3(0.0f, -0.04f, 0.0f));
	bell->add_child(bmi);
	meshutil::add_sphere(bell, toon, 0.018f, Vector3(0.0f, -0.09f, 0.0f), Color(0.62f, 0.48f, 0.20f));

	// 小帆船
	Node3D *boat = memnew(Node3D);
	boat->set_name("ToyBoat");
	boat->set_position(Vector3(0.05f, 0.08f, 0.20f));
	boat->set_rotation(Vector3(0.0f, 0.4f, 0.0f));
	add_child(boat);
	meshutil::add_box(boat, toon, Vector3(0.22f, 0.05f, 0.09f), Vector3(0.0f, 0.0f, 0.0f), Color(0.52f, 0.34f, 0.20f));
	meshutil::add_box(boat, toon, Vector3(0.012f, 0.22f, 0.012f), Vector3(0.0f, 0.13f, 0.0f), Color(0.62f, 0.48f, 0.28f));
	Ref<PlaneMesh> sail;
	sail.instantiate();
	sail->set_size(Vector2(0.12f, 0.16f));
	Ref<StandardMaterial3D> sm;
	sm.instantiate();
	sm->set_albedo(Color(0.94f, 0.92f, 0.86f));
	sm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	sail->set_material(sm);
	MeshInstance3D *smi2 = memnew(MeshInstance3D);
	smi2->set_mesh(sail);
	smi2->set_position(Vector3(0.06f, 0.14f, 0.0f));
	boat->add_child(smi2);
}
