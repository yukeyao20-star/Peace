#include "stone_cairn.h"
#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void StoneCairn::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &StoneCairn::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &StoneCairn::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &StoneCairn::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &StoneCairn::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &StoneCairn::interact);
	ClassDB::bind_method(D_METHOD("get_stone_count"), &StoneCairn::get_stone_count);
	ClassDB::bind_method(D_METHOD("get_additions"), &StoneCairn::get_additions);
}
String StoneCairn::get_prompt() const { return count < MAX_STONES ? String::utf8("按 E 往上放一块石头") : String::utf8("石堆已经很高了"); }
Vector3 StoneCairn::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.5f, 0.0f); }
void StoneCairn::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (count >= MAX_STONES) return;
	stones[count]->set_visible(true);
	count++;
	top = stones[count - 1];
	additions++;
	sway_t = 1.6f;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("石头轻轻晃了一下 · 又稳住了"), 2.2f);
	}
}
void StoneCairn::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sway_t > 0.0f) sway_t -= delta;
			if (top != nullptr) {
				top->set_rotation(Vector3(0.0f, 0.0f, (sway_t > 0.0f ? std::sin((1.6f - sway_t) * 18.0f) * sway_t * 0.08f : 0.0f)));
			}
		} break;
		default: break;
	}
}
void StoneCairn::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	float y = 0.0f;
	const Color cols[4] = { Color(0.56f, 0.54f, 0.50f), Color(0.50f, 0.48f, 0.44f), Color(0.60f, 0.58f, 0.54f), Color(0.46f, 0.45f, 0.42f) };
	for (int i = 0; i < MAX_STONES; i++) {
		const float r = 0.16f - 0.012f * (float)i;
		MeshInstance3D *m = meshutil::add_sphere(this, toon, r, Vector3(0.01f * (float)(i % 3), y + r * 0.5f, 0.01f * (float)(i % 2)), cols[i % 4]);
		m->set_scale(Vector3(1.25f, 0.55f, 1.0f));
		m->set_visible(i < count);
		stones[i] = m;
		y += r * 0.78f;
		if (i == count - 1) top = nullptr;
	}
	top = stones[count - 1];
	// 苔藓
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 1.57f;
		meshutil::add_sphere(this, toon, 0.03f, Vector3(std::cos(a) * 0.18f, 0.02f, std::sin(a) * 0.18f), Color(0.32f, 0.46f, 0.24f));
	}
}
