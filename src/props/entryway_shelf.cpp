#include "entryway_shelf.h"
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
using namespace godot;

void EntrywayShelf::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &EntrywayShelf::get_item_types); }
void EntrywayShelf::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void EntrywayShelf::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 搁板与挂钩
	meshutil::add_box(this, toon, Vector3(0.90f, 0.05f, 0.24f), Vector3(0.0f, 0.0f, 0.0f), Color(0.56f, 0.40f, 0.24f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.12f, 0.16f), Vector3(-0.38f, -0.07f, -0.05f), Color(0.46f, 0.32f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.12f, 0.16f), Vector3(0.38f, -0.07f, -0.05f), Color(0.46f, 0.32f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.9f, 0.04f, 0.04f), Vector3(0.0f, 0.50f, -0.09f), Color(0.46f, 0.32f, 0.20f));
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.03f, 0.08f, 0.03f), Vector3(-0.25f + 0.25f * (float)i, 0.45f, -0.08f), Color(0.38f, 0.34f, 0.30f));
	}
	// 钥匙（挂在中间钩上）
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.035f);
	ring->set_outer_radius(0.043f);
	ring->set_rings(12);
	ring->set_ring_segments(3);
	ring->set_material(meshutil::toon_material(toon, Color(0.76f, 0.66f, 0.34f), 0.2f));
	MeshInstance3D *rmi = memnew(MeshInstance3D);
	rmi->set_mesh(ring);
	rmi->set_position(Vector3(0.0f, 0.40f, -0.08f));
	rmi->set_rotation(Vector3(0.0f, 1.5707964f, 0.0f));
	add_child(rmi);
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(this, toon, Vector3(0.02f, 0.10f, 0.02f), Vector3(-0.03f + 0.06f * (float)i, 0.33f, -0.08f), Color(0.78f, 0.68f, 0.36f));
		meshutil::add_box(this, toon, Vector3(0.05f, 0.02f, 0.02f), Vector3(-0.03f + 0.06f * (float)i, 0.28f, -0.08f), Color(0.78f, 0.68f, 0.36f));
	}
	// 信件托盘
	meshutil::add_box(this, toon, Vector3(0.26f, 0.03f, 0.16f), Vector3(-0.26f, 0.03f, 0.02f), Color(0.50f, 0.34f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.22f, 0.02f, 0.13f), Vector3(-0.26f, 0.055f, 0.02f), Color(0.94f, 0.92f, 0.84f), Vector3(0.0f, 0.15f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.20f, 0.02f, 0.12f), Vector3(-0.25f, 0.075f, 0.02f), Color(0.92f, 0.90f, 0.82f), Vector3(0.0f, -0.1f, 0.0f));
	// 手套
	for (int i = 0; i < 2; i++) {
		MeshInstance3D *g = meshutil::add_sphere(this, toon, 0.05f, Vector3(0.30f + 0.08f * (float)i, 0.05f, 0.0f), Color(0.60f, 0.38f, 0.32f));
		g->set_scale(Vector3(1.0f, 0.6f, 1.3f));
	}
	// 帽子（挂右钩）
	Ref<CylinderMesh> cap;
	cap.instantiate();
	cap->set_top_radius(0.08f);
	cap->set_bottom_radius(0.10f);
	cap->set_height(0.10f);
	cap->set_radial_segments(10);
	cap->set_rings(1);
	cap->set_material(meshutil::toon_material(toon, Color(0.36f, 0.42f, 0.36f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(cap);
	cmi->set_position(Vector3(0.25f, 0.44f, -0.06f));
	add_child(cmi);
	meshutil::add_box(this, toon, Vector3(0.20f, 0.02f, 0.12f), Vector3(0.25f, 0.39f, -0.02f), Color(0.32f, 0.38f, 0.32f));
	// 小镜子
	meshutil::add_box(this, toon, Vector3(0.22f, 0.30f, 0.02f), Vector3(-0.45f, 0.28f, -0.08f), Color(0.52f, 0.42f, 0.28f));
	Ref<BoxMesh> mirror;
	mirror.instantiate();
	mirror->set_size(Vector3(0.18f, 0.26f, 0.005f));
	Ref<StandardMaterial3D> mm;
	mm.instantiate();
	mm->set_albedo(Color(0.82f, 0.86f, 0.88f));
	mm->set_metallic(0.8f);
	mm->set_roughness(0.08f);
	mirror->set_material(mm);
	MeshInstance3D *mmi = memnew(MeshInstance3D);
	mmi->set_mesh(mirror);
	mmi->set_position(Vector3(-0.45f, 0.28f, -0.065f));
	add_child(mmi);
}
