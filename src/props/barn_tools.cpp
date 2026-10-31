#include "barn_tools.h"

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

void BarnTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &BarnTools::get_item_types);
}

void BarnTools::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
		} break;
		default:
			break;
	}
}

void BarnTools::build() {
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

	// 木水桶
	Ref<CylinderMesh> bucket;
	bucket.instantiate();
	bucket->set_top_radius(0.16f);
	bucket->set_bottom_radius(0.13f);
	bucket->set_height(0.28f);
	bucket->set_radial_segments(12);
	bucket->set_rings(1);
	bucket->set_material(meshutil::toon_material(toon, Color(0.56f, 0.40f, 0.24f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(bucket);
	bmi->set_position(Vector3(-0.45f, 0.14f, 0.05f));
	add_child(bmi);
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> hoop;
		hoop.instantiate();
		hoop->set_top_radius(0.158f);
		hoop->set_bottom_radius(0.158f);
		hoop->set_height(0.02f);
		hoop->set_radial_segments(12);
		hoop->set_rings(1);
		hoop->set_material(meshutil::toon_material(toon, Color(0.36f, 0.34f, 0.30f), 0.15f));
		MeshInstance3D *h = memnew(MeshInstance3D);
		h->set_mesh(hoop);
		h->set_position(Vector3(-0.45f, 0.08f + 0.14f * (float)i, 0.05f));
		add_child(h);
	}
	meshutil::add_box(this, toon, Vector3(0.30f, 0.02f, 0.02f), Vector3(-0.45f, 0.30f, 0.05f), Color(0.44f, 0.32f, 0.20f));

	// 矮木凳
	meshutil::add_box(this, toon, Vector3(0.40f, 0.05f, 0.34f), Vector3(-0.02f, 0.42f, -0.30f), Color(0.58f, 0.44f, 0.28f));
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.15f : 0.11f;
		const float sz = (i < 2) ? -0.44f : -0.18f;
		meshutil::add_box(this, toon, Vector3(0.04f, 0.42f, 0.04f), Vector3(sx, 0.21f, sz), Color(0.48f, 0.34f, 0.20f));
	}

	// 铁皮奶罐
	Ref<CylinderMesh> can;
	can.instantiate();
	can->set_top_radius(0.11f);
	can->set_bottom_radius(0.13f);
	can->set_height(0.40f);
	can->set_radial_segments(12);
	can->set_rings(1);
	can->set_material(meshutil::toon_material(toon, Color(0.66f, 0.68f, 0.70f), 0.25f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(can);
	cmi->set_position(Vector3(0.38f, 0.20f, 0.06f));
	add_child(cmi);
	Ref<CylinderMesh> neck;
	neck.instantiate();
	neck->set_top_radius(0.07f);
	neck->set_bottom_radius(0.10f);
	neck->set_height(0.10f);
	neck->set_radial_segments(10);
	neck->set_rings(1);
	neck->set_material(meshutil::toon_material(toon, Color(0.62f, 0.64f, 0.66f), 0.25f));
	MeshInstance3D *nmi = memnew(MeshInstance3D);
	nmi->set_mesh(neck);
	nmi->set_position(Vector3(0.38f, 0.45f, 0.06f));
	add_child(nmi);
	meshutil::add_box(this, toon, Vector3(0.13f, 0.02f, 0.13f), Vector3(0.38f, 0.505f, 0.06f), Color(0.56f, 0.58f, 0.60f));
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(this, toon, Vector3(0.02f, 0.14f, 0.03f), Vector3(0.38f + (i == 0 ? -0.13f : 0.13f), 0.34f, 0.06f), Color(0.56f, 0.58f, 0.60f));
	}

	// 饲料袋
	Ref<BoxMesh> sack;
	sack.instantiate();
	sack->set_size(Vector3(0.30f, 0.36f, 0.22f));
	sack->set_material(meshutil::toon_material(toon, Color(0.74f, 0.64f, 0.46f), 0.2f));
	MeshInstance3D *smi = memnew(MeshInstance3D);
	smi->set_mesh(sack);
	smi->set_position(Vector3(0.02f, 0.18f, 0.34f));
	smi->set_rotation(Vector3(0.0f, 0.3f, 0.06f));
	add_child(smi);
	meshutil::add_box(this, toon, Vector3(0.22f, 0.02f, 0.02f), Vector3(0.02f, 0.31f, 0.34f), Color(0.52f, 0.42f, 0.28f), Vector3(0.0f, 0.3f, 0.0f));

	// 旧马蹄铁
	Ref<TorusMesh> shoe;
	shoe.instantiate();
	shoe->set_inner_radius(0.09f);
	shoe->set_outer_radius(0.12f);
	shoe->set_rings(14);
	shoe->set_ring_segments(4);
	shoe->set_material(meshutil::toon_material(toon, Color(0.42f, 0.42f, 0.44f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(shoe);
	hmi->set_position(Vector3(-0.42f, 0.045f, 0.40f));
	hmi->set_rotation(Vector3(1.5707964f, 0.2f, 0.0f));
	add_child(hmi);
}
