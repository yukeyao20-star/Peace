#include "bench.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Bench::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Bench::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Bench::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Bench::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Bench::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Bench::interact);
}

void Bench::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			TerrainBuilder *terrain = nullptr;
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
			}
			if (terrain != nullptr) {
				Vector3 p = get_global_position();
				p.y = terrain->get_terrain_height(p.x, p.z) + 0.02f;
				set_global_position(p);
			}
			add_to_group("interactables");
			seat_yaw = 1.5707964f; // 朝 +X（火堆方向）
			seat_pos = get_global_position() + Vector3(0.0f, 0.36f, 0.0f);
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			if (seated == nullptr) {
				return;
			}
			if (!seated->get_is_sitting()) {
				seated = nullptr;
				e_prev = Input::get_singleton()->is_key_pressed(KEY_E);
				return;
			}
			bool e = Input::get_singleton()->is_key_pressed(KEY_E);
			if (e && !e_prev) {
				stand_up();
			}
			e_prev = e;
		} break;
		default:
			break;
	}
}

String Bench::get_prompt() const {
	return seated != nullptr ? String() : String::utf8("按 E 坐下");
}

Vector3 Bench::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.5f, 0.0f);
}

float Bench::get_interact_distance() const {
	return 3.0f;
}

void Bench::interact(const Vector3 &p_player_pos) {
	if (seated != nullptr || get_parent() == nullptr) {
		return;
	}
	Player *p = Object::cast_to<Player>(get_parent()->get_node_or_null("Player"));
	if (p == nullptr || p->get_is_boating()) {
		return;
	}
	seated = p;
	e_prev = true;
	p->sit_down(seat_pos, seat_yaw);
	Node *gd = get_parent()->get_node_or_null("GameDirector");
	if (gd != nullptr) {
		gd->call("flash_hint", String::utf8("按 E 起身 · 看看火堆和湖"), 3.5f);
	}
}

void Bench::stand_up() {
	if (seated == nullptr) {
		return;
	}
	Player *p = seated;
	Vector3 out = seat_pos + Vector3(std::cos(seat_yaw), 0.0f, -std::sin(seat_yaw)) * 0.75f;
	out.y = get_global_position().y + 0.02f;
	p->set_global_position(out);
	p->stand_up();
	p->suppress_interaction(0.4f); // 起身这一下不要把"按 E"再当成坐下
	seated = nullptr;
}

void Bench::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.45f, 0.31f, 0.18f);
	const Color wood_light(0.58f, 0.42f, 0.25f);
	const Color wood_dark(0.28f, 0.19f, 0.10f);

	// 坐板与靠背
	meshutil::add_box(this, toon, Vector3(0.52f, 0.08f, 1.65f), Vector3(0.0f, 0.45f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(0.08f, 0.52f, 1.65f), Vector3(-0.30f, 0.80f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.06f, 1.65f), Vector3(-0.30f, 1.06f, 0.0f), wood_light);
	// 扶手
	meshutil::add_box(this, toon, Vector3(0.56f, 0.06f, 0.08f), Vector3(0.0f, 0.70f, 0.80f), wood_dark);
	meshutil::add_box(this, toon, Vector3(0.56f, 0.06f, 0.08f), Vector3(0.0f, 0.70f, -0.80f), wood_dark);
	meshutil::add_box(this, toon, Vector3(0.06f, 0.24f, 0.06f), Vector3(0.24f, 0.58f, 0.80f), wood_dark);
	meshutil::add_box(this, toon, Vector3(0.06f, 0.24f, 0.06f), Vector3(0.24f, 0.58f, -0.80f), wood_dark);
	// 四条腿
	for (int i = 0; i < 4; i++) {
		float x = (i < 2) ? -0.22f : 0.22f;
		float z = (i % 2 == 0) ? -0.70f : 0.70f;
		meshutil::add_box(this, toon, Vector3(0.09f, 0.45f, 0.09f), Vector3(x, 0.22f, z), wood_dark);
	}
}
