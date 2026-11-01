#include "painter_easel.h"
#include <cmath>
#include <godot_cpp/classes/engine.hpp>
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

void PainterEasel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &PainterEasel::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &PainterEasel::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &PainterEasel::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &PainterEasel::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &PainterEasel::interact);
	ClassDB::bind_method(D_METHOD("get_looks"), &PainterEasel::get_looks);
	ClassDB::bind_method(D_METHOD("is_snowy"), &PainterEasel::is_snowy);
}
bool PainterEasel::is_snowy() const { return sky != nullptr && sky->get_snow_amount() > 0.35f; }
String PainterEasel::get_prompt() const { return String::utf8("按 E 看看画"); }
Vector3 PainterEasel::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.95f, 0.0f); }
void PainterEasel::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	looks++;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) {
			const char *lines[4] = {
				"画布上是湖、雪山和一小片芦苇 · 颜料还没干",
				"湖心那一点蓝调得真准 · 像刚下过雨",
				"画角签着一个小小的「叶」字 · 笔触很轻",
				"你把画往日光里偏了偏 · 水面像真的在动",
			};
			gd->call("flash_hint", String::utf8(lines[looks % 4]), 2.8f);
		}
	}
	update_look();
}
void PainterEasel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (canvas != nullptr) canvas->set_rotation(Vector3(-0.22f + std::sin(t * 0.6f) * 0.008f, std::sin(t * 0.43f) * 0.01f, 0.0f));
			if (snow_cap != nullptr) snow_cap->set_visible(is_snowy());
		} break;
		default: break;
	}
}
void PainterEasel::update_look() {
	if (canvas == nullptr) return;
	canvas->set_scale(Vector3(1.0f, 1.0f, 1.0f));
}
void PainterEasel::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 2.35f, 0.0f));
	// 三脚架
	meshutil::add_box(this, toon, Vector3(0.045f, 1.42f, 0.045f), Vector3(-0.26f, 0.70f, -0.16f), Color(0.52f, 0.38f, 0.23f), Vector3(0.20f, 0.0f, 0.16f));
	meshutil::add_box(this, toon, Vector3(0.045f, 1.42f, 0.045f), Vector3(0.26f, 0.70f, -0.16f), Color(0.52f, 0.38f, 0.23f), Vector3(-0.20f, 0.0f, 0.16f));
	meshutil::add_box(this, toon, Vector3(0.045f, 1.36f, 0.045f), Vector3(0.0f, 0.68f, -0.40f), Color(0.50f, 0.36f, 0.22f), Vector3(0.30f, 0.0f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.62f, 0.035f, 0.10f), Vector3(0.0f, 0.52f, 0.10f), Color(0.48f, 0.34f, 0.21f));
	// 画布与木框
	canvas = memnew(Node3D);
	canvas->set_name("Canvas");
	canvas->set_position(Vector3(0.0f, 1.02f, 0.10f));
	canvas->set_rotation(Vector3(-0.22f, 0.0f, 0.0f));
	add_child(canvas);
	meshutil::add_box(canvas, toon, Vector3(0.72f, 0.56f, 0.035f), Vector3(0.0f, 0.0f, 0.0f), Color(0.58f, 0.44f, 0.28f));
	// 画面：天空 / 雪山 / 湖 / 草地 / 太阳
	meshutil::add_box(canvas, toon, Vector3(0.64f, 0.20f, 0.006f), Vector3(0.0f, 0.14f, 0.022f), Color(0.62f, 0.74f, 0.86f));
	meshutil::add_box(canvas, toon, Vector3(0.20f, 0.13f, 0.006f), Vector3(-0.10f, 0.12f, 0.026f), Color(0.70f, 0.74f, 0.78f));
	meshutil::add_box(canvas, toon, Vector3(0.64f, 0.03f, 0.006f), Vector3(0.0f, 0.02f, 0.022f), Color(0.50f, 0.66f, 0.74f));
	meshutil::add_box(canvas, toon, Vector3(0.44f, 0.04f, 0.006f), Vector3(0.06f, -0.005f, 0.022f), Color(0.42f, 0.60f, 0.76f));
	meshutil::add_box(canvas, toon, Vector3(0.64f, 0.08f, 0.006f), Vector3(0.0f, -0.14f, 0.022f), Color(0.46f, 0.58f, 0.34f));
	meshutil::add_sphere(canvas, toon, 0.022f, Vector3(0.20f, 0.21f, 0.028f), Color(0.96f, 0.86f, 0.52f))->set_scale(Vector3(1.0f, 1.0f, 0.2f));
	// 芦苇
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(canvas, toon, Vector3(0.006f, 0.10f, 0.004f), Vector3(-0.22f + 0.03f * (float)i, -0.045f, 0.026f), Color(0.34f, 0.42f, 0.26f), Vector3(0.0f, 0.0f, 0.15f * (float)(i - 2)));
	}
	// 颜料盒与画笔
	meshutil::add_box(this, toon, Vector3(0.30f, 0.05f, 0.16f), Vector3(0.34f, 0.05f, 0.30f), Color(0.46f, 0.32f, 0.20f));
	const Color paints[4] = { Color(0.80f, 0.20f, 0.18f), Color(0.20f, 0.36f, 0.70f), Color(0.90f, 0.76f, 0.24f), Color(0.92f, 0.92f, 0.88f) };
	for (int i = 0; i < 4; i++) {
		meshutil::add_sphere(this, toon, 0.022f, Vector3(0.27f + 0.05f * (float)i, 0.085f, 0.30f), paints[i]);
	}
	meshutil::add_box(this, toon, Vector3(0.012f, 0.26f, 0.012f), Vector3(0.22f, 0.18f, 0.38f), Color(0.62f, 0.46f, 0.28f), Vector3(0.45f, 0.0f, 0.35f));
	meshutil::add_box(this, toon, Vector3(0.02f, 0.05f, 0.02f), Vector3(0.32f, 0.30f, 0.40f), Color(0.32f, 0.30f, 0.28f), Vector3(0.45f, 0.0f, 0.35f));
	// 雪帽
	snow_cap = meshutil::add_box(this, toon, Vector3(0.74f, 0.025f, 0.06f), Vector3(0.0f, 1.315f, 0.10f), Color(0.94f, 0.96f, 0.98f));
	snow_cap->set_visible(false);
}
