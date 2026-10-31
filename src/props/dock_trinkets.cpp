#include "dock_trinkets.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void DockTrinkets::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &DockTrinkets::get_item_types);
	ClassDB::bind_method(D_METHOD("get_lamp_on"), &DockTrinkets::get_lamp_on);
}

void DockTrinkets::_notification(int p_what) {
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
			if (sky == nullptr && get_tree() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			}
			lamp_on = sky != nullptr && (sky->get_time_of_day() < 0.26f || sky->get_time_of_day() > 0.74f);
			if (flame != nullptr) {
				flame->set_visible(lamp_on);
				const float f = 0.85f + 0.15f * std::sin(t * 6.0f) + 0.07f * std::sin(t * 11.0f);
				flame->set_scale(Vector3(0.8f * f, f, 0.8f * f));
			}
		} break;
		default:
			break;
	}
}

void DockTrinkets::build() {
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
	// 脚下的一块旧木板
	meshutil::add_box(this, toon, Vector3(1.4f, 0.04f, 1.0f), Vector3(0.0f, 0.02f, 0.0f), Color(0.52f, 0.40f, 0.26f));

	// 盘好的缆绳
	Ref<TorusMesh> coil;
	coil.instantiate();
	coil->set_inner_radius(0.16f);
	coil->set_outer_radius(0.20f);
	coil->set_rings(18);
	coil->set_ring_segments(4);
	coil->set_material(meshutil::toon_material(toon, Color(0.72f, 0.62f, 0.44f), 0.2f));
	for (int i = 0; i < 3; i++) {
		MeshInstance3D *c = memnew(MeshInstance3D);
		c->set_mesh(coil);
		c->set_position(Vector3(-0.42f, 0.055f + 0.018f * (float)i, -0.18f));
		c->set_scale(Vector3(1.0f - 0.08f * (float)i, 1.0f, 1.0f - 0.08f * (float)i));
		c->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
		add_child(c);
	}

	// 救生圈
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.16f);
	ring->set_outer_radius(0.25f);
	ring->set_rings(18);
	ring->set_ring_segments(5);
	ring->set_material(meshutil::toon_material(toon, Color(0.90f, 0.46f, 0.22f), 0.2f));
	MeshInstance3D *rmi = memnew(MeshInstance3D);
	rmi->set_mesh(ring);
	rmi->set_position(Vector3(0.15f, 0.20f, -0.15f));
	rmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	add_child(rmi);
	// 白条纹
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 1.5708f;
		meshutil::add_box(this, toon, Vector3(0.09f, 0.03f, 0.05f),
				Vector3(0.15f + std::cos(a) * 0.20f, 0.20f, -0.15f + std::sin(a) * 0.20f),
				Color(0.94f, 0.92f, 0.86f), Vector3(0.0f, -a, 0.0f));
	}

	// 油灯
	meshutil::add_box(this, toon, Vector3(0.09f, 0.07f, 0.09f), Vector3(0.42f, 0.075f, 0.20f), Color(0.28f, 0.26f, 0.24f));
	Ref<CylinderMesh> glass;
	glass.instantiate();
	glass->set_top_radius(0.045f);
	glass->set_bottom_radius(0.055f);
	glass->set_height(0.13f);
	glass->set_radial_segments(8);
	glass->set_rings(1);
	Ref<StandardMaterial3D> gm;
	gm.instantiate();
	gm->set_albedo(Color(0.90f, 0.80f, 0.56f, 0.45f));
	gm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	gm->set_roughness(0.15f);
	glass->set_material(gm);
	MeshInstance3D *gmi = memnew(MeshInstance3D);
	gmi->set_mesh(glass);
	gmi->set_position(Vector3(0.42f, 0.17f, 0.20f));
	add_child(gmi);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.02f, 0.10f), Vector3(0.42f, 0.245f, 0.20f), Color(0.30f, 0.28f, 0.26f));
	Ref<SphereMesh> flame_mesh;
	flame_mesh.instantiate();
	flame_mesh->set_radius(0.022f);
	flame_mesh->set_height(0.05f);
	flame_mesh->set_radial_segments(5);
	flame_mesh->set_rings(3);
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fm->set_albedo(Color(1.0f, 0.74f, 0.30f));
	fm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	flame_mesh->set_material(fm);
	flame = memnew(MeshInstance3D);
	flame->set_mesh(flame_mesh);
	flame->set_position(Vector3(0.42f, 0.16f, 0.20f));
	flame->set_visible(false);
	flame->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(flame);

	// 木箱
	meshutil::add_box(this, toon, Vector3(0.34f, 0.30f, 0.28f), Vector3(-0.05f, 0.19f, 0.30f), Color(0.56f, 0.40f, 0.24f));
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.36f, 0.03f, 0.02f), Vector3(-0.05f, 0.08f + 0.10f * (float)i, 0.445f), Color(0.42f, 0.30f, 0.18f));
	}

	// 蟹笼：木框 + 交叉网线
	meshutil::add_box(this, toon, Vector3(0.36f, 0.03f, 0.30f), Vector3(0.38f, 0.05f, -0.28f), Color(0.46f, 0.34f, 0.22f));
	meshutil::add_box(this, toon, Vector3(0.36f, 0.03f, 0.30f), Vector3(0.38f, 0.26f, -0.28f), Color(0.46f, 0.34f, 0.22f));
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 0.7854f;
		meshutil::add_box(this, toon, Vector3(0.38f, 0.008f, 0.008f),
				Vector3(0.38f, 0.155f, -0.28f), Color(0.70f, 0.66f, 0.56f),
				Vector3(0.0f, a, 0.4f * std::sin(a)));
	}
}
