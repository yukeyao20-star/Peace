#include "garden_windmill.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void GardenWindmill::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &GardenWindmill::get_item_types);
	ClassDB::bind_method(D_METHOD("get_spin"), &GardenWindmill::get_spin);
	ClassDB::bind_method(D_METHOD("get_snowed"), &GardenWindmill::get_snowed);
}
void GardenWindmill::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			float target = 1.2f;
			snowed = false;
			if (sky != nullptr) {
				target += sky->get_rain_amount() * 4.0f;
				if (sky->get_snow_amount() > 0.45f) { target = 0.0f; snowed = true; }
			}
			spin = Math::lerp(spin, target, Math::min(1.0f, delta * 0.7f));
			if (blades != nullptr) blades->set_rotation(Vector3(0.0f, 0.0f, blades->get_rotation().z + spin * delta));
			if (snow_cap != nullptr) snow_cap->set_visible(snowed);
		} break;
		default: break;
	}
}
void GardenWindmill::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	const Color wood(0.58f, 0.42f, 0.26f);
	// 塔身
	Ref<CylinderMesh> tower;
	tower.instantiate();
	tower->set_top_radius(0.42f);
	tower->set_bottom_radius(0.58f);
	tower->set_height(2.0f);
	tower->set_radial_segments(10);
	tower->set_rings(1);
	tower->set_material(meshutil::toon_material(toon, wood, 0.2f));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(tower);
	tmi->set_position(Vector3(0.0f, 1.0f, 0.0f));
	add_child(tmi);
	// 顶帽
	Ref<CylinderMesh> cap;
	cap.instantiate();
	cap->set_top_radius(0.02f);
	cap->set_bottom_radius(0.48f);
	cap->set_height(0.55f);
	cap->set_radial_segments(10);
	cap->set_rings(1);
	cap->set_material(meshutil::toon_material(toon, Color(0.52f, 0.28f, 0.18f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(cap);
	cmi->set_position(Vector3(0.0f, 2.27f, 0.0f));
	add_child(cmi);
	// 小门
	meshutil::add_box(this, toon, Vector3(0.22f, 0.36f, 0.03f), Vector3(0.0f, 0.20f, 0.56f), Color(0.24f, 0.18f, 0.12f));
	// 风叶
	blades = memnew(Node3D);
	blades->set_name("Blades");
	blades->set_position(Vector3(0.0f, 2.05f, 0.55f));
	add_child(blades);
	meshutil::add_sphere(blades, toon, 0.06f, Vector3(0.0f, 0.0f, 0.0f), Color(0.36f, 0.30f, 0.24f));
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 1.5707964f;
		meshutil::add_box(blades, toon, Vector3(0.70f, 0.10f, 0.03f),
				Vector3(std::cos(a) * 0.36f, std::sin(a) * 0.36f, 0.0f), Color(0.90f, 0.86f, 0.74f),
				Vector3(0.0f, 0.0f, a));
		meshutil::add_box(blades, toon, Vector3(0.72f, 0.02f, 0.02f),
				Vector3(std::cos(a) * 0.36f, std::sin(a) * 0.36f, 0.02f), wood,
				Vector3(0.0f, 0.0f, a));
	}
	// 雪帽
	Ref<SphereMesh> snow;
	snow.instantiate();
	snow->set_radius(0.42f);
	snow->set_height(0.16f);
	snow->set_radial_segments(10);
	snow->set_rings(4);
	snow->set_material(meshutil::toon_material(toon, Color(0.92f, 0.96f, 1.0f), 0.2f));
	snow_cap = memnew(MeshInstance3D);
	snow_cap->set_mesh(snow);
	snow_cap->set_position(Vector3(0.0f, 2.50f, 0.0f));
	snow_cap->set_visible(false);
	snow_cap->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(snow_cap);
}
