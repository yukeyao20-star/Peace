#include "hammock.h"

#include <cmath>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/shader.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Hammock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Hammock::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Hammock::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Hammock::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Hammock::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Hammock::interact);
	ClassDB::bind_method(D_METHOD("is_ridden"), &Hammock::is_ridden);
	ClassDB::bind_method(D_METHOD("get_fabric_pos"), &Hammock::get_fabric_pos);
}

Vector3 Hammock::get_fabric_pos() const {
	return fabric != nullptr ? fabric->get_global_position() : get_global_position();
}

void Hammock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
			}
			if (terrain != nullptr) {
				Vector3 p = get_global_position();
				p.y = terrain->get_terrain_height(p.x, p.z) + 0.02f;
				set_global_position(p);
			}
			add_to_group("interactables");
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (fabric != nullptr) {
				fabric->set_position(Vector3(0.0f, 0.92f + std::sin(t * 1.3f) * 0.035f, 0.0f));
				fabric->set_rotation(Vector3(std::sin(t * 0.9f) * 0.02f, 0.0f, std::sin(t * 1.1f) * 0.015f));
			}
			if (rider != nullptr && !rider->get_is_lying()) {
				rider = nullptr;
				e_prev = Input::get_singleton()->is_key_pressed(KEY_E);
				return;
			}
			if (rider == nullptr) {
				return;
			}
			bool e = Input::get_singleton()->is_key_pressed(KEY_E);
			if (e && !e_prev) {
				stand_up();
				e_prev = e;
				return;
			}
			e_prev = e;
			if (fabric != nullptr) {
				rider->lie_down(fabric->get_global_position() + Vector3(0.0f, 0.10f, 0.0f), 1.5707964f);
			}
		} break;
		default:
			break;
	}
}

String Hammock::get_prompt() const {
	return rider != nullptr ? String() : String::utf8("按 E 躺进吊床");
}

Vector3 Hammock::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.0f, 0.0f);
}

float Hammock::get_interact_distance() const {
	return 2.8f;
}

void Hammock::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (rider != nullptr || get_parent() == nullptr) {
		return;
	}
	Player *p = Object::cast_to<Player>(get_parent()->get_node_or_null("Player"));
	if (p == nullptr || p->get_is_boating() || p->get_photo_mode() || p->get_is_sitting()) {
		return;
	}
	rider = p;
	e_prev = true;
	if (fabric != nullptr) {
		p->lie_down(fabric->get_global_position() + Vector3(0.0f, 0.10f, 0.0f), 1.5707964f);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("on_hammock_lay");
			gd->call("flash_hint", String::utf8("吊床轻轻晃 · 再按 E 下来"), 3.0f);
		}
	}
}

void Hammock::stand_up() {
	if (rider == nullptr) {
		return;
	}
	Player *p = rider;
	Vector3 out = get_global_position() + Vector3(0.0f, 0.02f, 1.4f);
	p->set_global_position(out);
	p->stand_up();
	p->suppress_interaction(0.4f);
	rider = nullptr;
}

void Hammock::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.42f, 0.29f, 0.15f);
	const Color wood_dark(0.32f, 0.21f, 0.11f);
	const Color cloth(0.78f, 0.68f, 0.50f);
	const Color rope(0.80f, 0.72f, 0.54f);

	// 两根树桩
	for (int i = 0; i < 2; i++) {
		float x = (i == 0) ? -1.35f : 1.35f;
		meshutil::add_box(this, toon, Vector3(0.16f, 1.75f, 0.16f), Vector3(x, 0.87f, 0.0f), wood_dark);
		meshutil::add_box(this, toon, Vector3(0.30f, 0.10f, 0.30f), Vector3(x, 1.76f, 0.0f), wood);
	}

	// 网面：一排微微下垂的木条
	fabric = memnew(Node3D);
	fabric->set_name("Fabric");
	fabric->set_position(Vector3(0.0f, 0.92f, 0.0f));
	add_child(fabric);
	for (int i = 0; i < 9; i++) {
		float u = (float)i / 8.0f;
		float x = -1.05f + u * 2.1f;
		float sag = -std::sin(u * 3.1415926f) * 0.10f;
		float slope = -0.10f * 3.1415926f * std::cos(u * 3.1415926f) / 2.1f;
		meshutil::add_box(fabric, toon, Vector3(0.23f, 0.03f, 0.58f), Vector3(x, sag, 0.0f), cloth,
				Vector3(0.0f, 0.0f, slope));
	}
	// 两端把网面连到树桩上的绳
	for (int i = 0; i < 2; i++) {
		float side = (i == 0) ? -1.0f : 1.0f;
		Vector3 a(side * 1.35f, 1.55f, 0.0f);
		Vector3 b(side * 1.05f, 0.92f, 0.0f);
		Vector3 mid = (a + b) * 0.5f;
		Vector3 d = b - a;
		float len = d.length();
		float ang = std::atan2(d.y, d.x);
		meshutil::add_box(this, toon, Vector3(len, 0.022f, 0.022f), mid, rope, Vector3(0.0f, 0.0f, ang));
	}
}
