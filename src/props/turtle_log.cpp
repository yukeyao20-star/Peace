#include "turtle_log.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void TurtleLog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_in_water"), &TurtleLog::get_in_water);
	ClassDB::bind_method(D_METHOD("get_plops"), &TurtleLog::get_plops);
}
void TurtleLog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) break;
			if (player == nullptr) player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			if (sky == nullptr) sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			if (terrain == nullptr) terrain = Object::cast_to<TerrainBuilder>(root->find_child("Terrain", true, false));
			bool day = true;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				day = tod > 0.27f && tod < 0.73f && sky->get_snow_amount() < 0.4f;
			}
			float pd = 999.0f;
			if (player != nullptr) pd = player->get_global_position().distance_to(get_global_position() + Vector3(0, 0.25f, 0));
			if (!in_water && pd < 2.2f) {
				in_water = true;
				resurface_t = 8.0f;
				plops++;
			}
			if (in_water) {
				resurface_t -= delta;
				if (resurface_t <= 0.0f && pd > 3.0f) in_water = false;
			}
			if (turtle != nullptr) {
				turtle->set_visible(day && !in_water);
				if (!in_water) turtle->set_position(Vector3(0.05f, 0.22f, 0.0f));
			}
		} break;
		default: break;
	}
}
void TurtleLog::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
	}
	// 浮木
	Ref<CylinderMesh> log;
	log.instantiate();
	log->set_top_radius(0.20f);
	log->set_bottom_radius(0.20f);
	log->set_height(1.80f);
	log->set_radial_segments(10);
	log->set_rings(1);
	log->set_material(meshutil::toon_material(toon, Color(0.46f, 0.32f, 0.20f), 0.2f));
	MeshInstance3D *lmi = memnew(MeshInstance3D);
	lmi->set_mesh(log);
	lmi->set_position(Vector3(0.0f, 0.10f, 0.0f));
	lmi->set_rotation(Vector3(0.0f, 0.5f, 1.5707964f));
	add_child(lmi);
	// 小乌龟
	turtle = memnew(Node3D);
	turtle->set_name("Turtle");
	turtle->set_position(Vector3(0.05f, 0.22f, 0.0f));
	add_child(turtle);
	MeshInstance3D *shell = meshutil::add_sphere(turtle, toon, 0.16f, Vector3(0.0f, 0.0f, 0.0f), Color(0.40f, 0.46f, 0.26f));
	shell->set_scale(Vector3(1.2f, 0.55f, 1.0f));
	meshutil::add_sphere(turtle, toon, 0.06f, Vector3(0.19f, 0.0f, 0.0f), Color(0.50f, 0.54f, 0.30f));
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.12f : 0.12f;
		const float sz = (i < 2) ? -0.12f : 0.12f;
		meshutil::add_box(turtle, toon, Vector3(0.06f, 0.03f, 0.08f), Vector3(sx, -0.06f, sz), Color(0.44f, 0.48f, 0.26f));
	}
	meshutil::add_box(turtle, toon, Vector3(0.06f, 0.03f, 0.06f), Vector3(-0.18f, 0.0f, 0.0f), Color(0.44f, 0.48f, 0.26f));
}
