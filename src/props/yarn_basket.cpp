#include "yarn_basket.h"
#include <cmath>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
using namespace godot;

void YarnBasket::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &YarnBasket::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &YarnBasket::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &YarnBasket::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &YarnBasket::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &YarnBasket::interact);
	ClassDB::bind_method(D_METHOD("get_touches"), &YarnBasket::get_touches);
}
String YarnBasket::get_prompt() const { return String::utf8("按 E 摸摸毛线"); }
Vector3 YarnBasket::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.22f, 0.0f); }
void YarnBasket::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	touches++;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("毛线软软的 · 有太阳晒过的味道"), 2.6f);
	}
}
void YarnBasket::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS:
			t += (float)get_process_delta_time();
			if (needles != nullptr) needles->set_rotation(Vector3(0.0f, std::sin(t * 0.9f) * 0.04f, 0.0f));
			break;
		default: break;
	}
}
void YarnBasket::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 藤编篮
	Ref<CylinderMesh> basket;
	basket.instantiate();
	basket->set_top_radius(0.20f);
	basket->set_bottom_radius(0.16f);
	basket->set_height(0.18f);
	basket->set_radial_segments(10);
	basket->set_rings(1);
	basket->set_material(meshutil::toon_material(toon, Color(0.62f, 0.46f, 0.28f), 0.3f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(basket);
	bmi->set_position(Vector3(0.0f, 0.09f, 0.0f));
	add_child(bmi);
	// 篮口藤条
	for (int i = 0; i < 8; i++) {
		const float a = (float)i * 0.7854f;
		meshutil::add_box(this, toon, Vector3(0.045f, 0.16f, 0.012f), Vector3(std::cos(a) * 0.18f, 0.09f, std::sin(a) * 0.18f), Color(0.56f, 0.41f, 0.25f), Vector3(0.0f, -a, 0.0f));
	}
	// 三团毛线
	const Color yarn_cols[3] = { Color(0.78f, 0.30f, 0.28f), Color(0.90f, 0.72f, 0.30f), Color(0.82f, 0.80f, 0.72f) };
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.0944f;
		MeshInstance3D *y = meshutil::add_sphere(this, toon, 0.075f, Vector3(std::cos(a) * 0.07f, 0.20f, std::sin(a) * 0.07f), yarn_cols[i]);
		y->set_scale(Vector3(1.0f, 0.85f, 1.0f));
		for (int s = 0; s < 2; s++) {
			meshutil::add_box(this, toon, Vector3(0.13f, 0.012f, 0.012f), Vector3(std::cos(a) * 0.07f, 0.235f + 0.015f * (float)s, std::sin(a) * 0.07f), yarn_cols[i], Vector3(0.0f, a + (float)s, 0.4f));
		}
	}
	// 竹针
	needles = memnew(Node3D);
	needles->set_name("Needles");
	needles->set_position(Vector3(0.05f, 0.22f, 0.02f));
	add_child(needles);
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(needles, toon, Vector3(0.010f, 0.30f, 0.010f), Vector3(0.0f, 0.10f, -0.02f + 0.04f * (float)i), Color(0.78f, 0.66f, 0.46f), Vector3(0.0f, 0.0f, 0.25f + 0.1f * (float)i));
	}
	// 织了一半的围巾，搭在篮沿
	const Color scarf = Color(0.56f, 0.68f, 0.78f);
	meshutil::add_box(this, toon, Vector3(0.18f, 0.025f, 0.30f), Vector3(-0.18f, 0.22f, 0.10f), scarf, Vector3(0.15f, -0.4f, 0.0f));
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(this, toon, Vector3(0.035f, 0.030f, 0.030f), Vector3(-0.22f - 0.02f * (float)i, 0.20f - 0.02f * (float)i, 0.20f + 0.03f * (float)i), Color(0.72f, 0.82f, 0.88f), Vector3(0.15f, -0.4f, 0.0f));
	}
}
