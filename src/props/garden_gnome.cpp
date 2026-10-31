#include "garden_gnome.h"
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
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void GardenGnome::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &GardenGnome::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &GardenGnome::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &GardenGnome::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &GardenGnome::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &GardenGnome::interact);
	ClassDB::bind_method(D_METHOD("get_greets"), &GardenGnome::get_greets);
	ClassDB::bind_method(D_METHOD("get_lamp_on"), &GardenGnome::get_lamp_on);
}
String GardenGnome::get_prompt() const { return String::utf8("按 E 跟地精打个招呼"); }
Vector3 GardenGnome::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.35f, 0.0f); }
void GardenGnome::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	greets++;
	static const char *lines[4] = {
		"地精抬眼看了看你，胡子上还沾着露水。",
		"“今天的风不错。”他小声说。",
		"地精把灯笼往你这边挪了挪。",
		"“花圃那边，替我看一眼。”",
	};
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8(lines[greets % 4]), 3.2f);
	}
}
void GardenGnome::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			lamp_on = sky != nullptr && (sky->get_time_of_day() < 0.26f || sky->get_time_of_day() > 0.74f);
			if (lamp != nullptr) {
				lamp->set_visible(lamp_on);
				const float f = 0.9f + 0.1f * std::sin(t * 5.0f);
				lamp->set_scale(Vector3(f, f, f));
			}
		} break;
		default: break;
	}
}
void GardenGnome::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 靴子与身子
	meshutil::add_box(this, toon, Vector3(0.20f, 0.08f, 0.28f), Vector3(0.0f, 0.04f, 0.02f), Color(0.30f, 0.26f, 0.22f));
	Ref<CylinderMesh> body;
	body.instantiate();
	body->set_top_radius(0.10f);
	body->set_bottom_radius(0.16f);
	body->set_height(0.32f);
	body->set_radial_segments(9);
	body->set_rings(1);
	body->set_material(meshutil::toon_material(toon, Color(0.36f, 0.44f, 0.58f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(body);
	bmi->set_position(Vector3(0.0f, 0.24f, 0.0f));
	add_child(bmi);
	// 胡子与脸
	MeshInstance3D *beard = meshutil::add_sphere(this, toon, 0.10f, Vector3(0.0f, 0.40f, 0.06f), Color(0.94f, 0.93f, 0.88f));
	beard->set_scale(Vector3(1.0f, 1.15f, 0.7f));
	meshutil::add_sphere(this, toon, 0.055f, Vector3(0.0f, 0.50f, 0.07f), Color(0.88f, 0.70f, 0.58f));
	// 红尖帽
	Ref<CylinderMesh> hat;
	hat.instantiate();
	hat->set_top_radius(0.01f);
	hat->set_bottom_radius(0.12f);
	hat->set_height(0.30f);
	hat->set_radial_segments(9);
	hat->set_rings(1);
	hat->set_material(meshutil::toon_material(toon, Color(0.78f, 0.26f, 0.22f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(hat);
	hmi->set_position(Vector3(0.0f, 0.66f, 0.0f));
	hmi->set_rotation(Vector3(0.0f, 0.0f, 0.12f));
	add_child(hmi);
	// 手里的灯笼
	meshutil::add_box(this, toon, Vector3(0.03f, 0.18f, 0.03f), Vector3(0.20f, 0.30f, 0.10f), Color(0.36f, 0.30f, 0.24f), Vector3(0.0f, 0.0f, -0.5f));
	Ref<SphereMesh> lm;
	lm.instantiate();
	lm->set_radius(0.045f);
	lm->set_height(0.09f);
	lm->set_radial_segments(6);
	lm->set_rings(3);
	Ref<StandardMaterial3D> lmat;
	lmat.instantiate();
	lmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	lmat->set_albedo(Color(1.0f, 0.78f, 0.36f));
	lmat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	lm->set_material(lmat);
	lamp = memnew(MeshInstance3D);
	lamp->set_mesh(lm);
	lamp->set_position(Vector3(0.28f, 0.22f, 0.10f));
	lamp->set_visible(false);
	lamp->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(lamp);
	// 脚边的小蘑菇
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.1f;
		const float x = std::cos(a) * 0.30f;
		const float z = std::sin(a) * 0.30f;
		meshutil::add_box(this, toon, Vector3(0.02f, 0.08f, 0.02f), Vector3(x, 0.04f, z), Color(0.90f, 0.86f, 0.72f));
		meshutil::add_sphere(this, toon, 0.06f, Vector3(x, 0.10f, z), Color(0.66f, 0.30f, 0.24f));
	}
}
