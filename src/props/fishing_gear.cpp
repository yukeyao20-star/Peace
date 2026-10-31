#include "fishing_gear.h"
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

void FishingGear::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &FishingGear::get_item_types); }
void FishingGear::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}

void FishingGear::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}
	// 鱼竿架
	meshutil::add_box(this, toon, Vector3(0.06f, 1.20f, 0.06f), Vector3(-0.40f, 0.60f, 0.0f), Color(0.46f, 0.32f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.50f, 0.04f, 0.06f), Vector3(-0.40f, 0.95f, 0.0f), Color(0.46f, 0.32f, 0.20f));
	// 两把鱼竿
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> rod;
		rod.instantiate();
		rod->set_top_radius(0.006f);
		rod->set_bottom_radius(0.012f);
		rod->set_height(1.45f);
		rod->set_radial_segments(5);
		rod->set_rings(1);
		rod->set_material(meshutil::toon_material(toon, i == 0 ? Color(0.30f, 0.26f, 0.22f) : Color(0.52f, 0.36f, 0.22f), 0.2f));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(rod);
		mi->set_position(Vector3(-0.34f + 0.08f * (float)i, 0.75f, 0.06f + 0.03f * (float)i));
		mi->set_rotation(Vector3(0.06f, 0.0f, 0.10f * (float)i));
		add_child(mi);
	}
	// 钓具箱
	meshutil::add_box(this, toon, Vector3(0.42f, 0.22f, 0.28f), Vector3(0.12f, 0.11f, -0.05f), Color(0.40f, 0.44f, 0.38f));
	meshutil::add_box(this, toon, Vector3(0.44f, 0.04f, 0.30f), Vector3(0.12f, 0.24f, -0.05f), Color(0.32f, 0.36f, 0.32f));
	meshutil::add_box(this, toon, Vector3(0.06f, 0.03f, 0.02f), Vector3(0.12f, 0.19f, 0.10f), Color(0.80f, 0.70f, 0.34f));
	// 抄网：圆环 + 网兜
	Ref<TorusMesh> hoop;
	hoop.instantiate();
	hoop->set_inner_radius(0.16f);
	hoop->set_outer_radius(0.20f);
	hoop->set_rings(16);
	hoop->set_ring_segments(4);
	hoop->set_material(meshutil::toon_material(toon, Color(0.62f, 0.56f, 0.38f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(hoop);
	hmi->set_position(Vector3(0.42f, 0.55f, 0.10f));
	hmi->set_rotation(Vector3(1.2f, 0.0f, 0.0f));
	add_child(hmi);
	Ref<CylinderMesh> net;
	net.instantiate();
	net->set_top_radius(0.17f);
	net->set_bottom_radius(0.05f);
	net->set_height(0.26f);
	net->set_radial_segments(10);
	net->set_rings(1);
	Ref<StandardMaterial3D> nm;
	nm.instantiate();
	nm->set_albedo(Color(0.86f, 0.88f, 0.82f, 0.45f));
	nm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	nm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	net->set_material(nm);
	MeshInstance3D *nmi = memnew(MeshInstance3D);
	nmi->set_mesh(net);
	nmi->set_position(Vector3(0.42f, 0.42f, 0.18f));
	nmi->set_rotation(Vector3(1.2f, 0.0f, 0.0f));
	add_child(nmi);
	meshutil::add_box(this, toon, Vector3(0.03f, 0.55f, 0.03f), Vector3(0.42f, 0.25f, 0.0f), Color(0.56f, 0.42f, 0.26f));
	// 浮漂桶
	Ref<CylinderMesh> bucket;
	bucket.instantiate();
	bucket->set_top_radius(0.12f);
	bucket->set_bottom_radius(0.10f);
	bucket->set_height(0.22f);
	bucket->set_radial_segments(10);
	bucket->set_rings(1);
	bucket->set_material(meshutil::toon_material(toon, Color(0.44f, 0.52f, 0.54f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(bucket);
	bmi->set_position(Vector3(-0.25f, 0.11f, -0.35f));
	add_child(bmi);
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.1f;
		meshutil::add_sphere(this, toon, 0.028f,
				Vector3(-0.25f + std::cos(a) * 0.04f, 0.24f, -0.35f + std::sin(a) * 0.04f),
				i == 0 ? Color(0.86f, 0.30f, 0.24f) : Color(0.92f, 0.90f, 0.84f));
	}
	// 涉水裤
	meshutil::add_box(this, toon, Vector3(0.24f, 0.36f, 0.12f), Vector3(0.30f, 0.24f, 0.38f), Color(0.34f, 0.42f, 0.36f), Vector3(0.0f, 0.4f, 0.0f));
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(this, toon, Vector3(0.10f, 0.28f, 0.10f), Vector3(0.24f + 0.12f * (float)i, 0.06f, 0.40f), Color(0.30f, 0.38f, 0.32f), Vector3(0.0f, 0.4f, 0.0f));
	}
}
