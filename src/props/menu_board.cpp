#include "menu_board.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void MenuBoard::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &MenuBoard::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &MenuBoard::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &MenuBoard::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &MenuBoard::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &MenuBoard::interact);
	ClassDB::bind_method(D_METHOD("get_reads"), &MenuBoard::get_reads);
	ClassDB::bind_method(D_METHOD("get_note"), &MenuBoard::get_note);
}
String MenuBoard::get_prompt() const { return String::utf8("按 E 看看今日黑板"); }
Vector3 MenuBoard::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.85f, 0.0f); }
void MenuBoard::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	reads++;
	refresh_note();
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", current_note.replace("\n", " · "), 3.0f);
	}
}
void MenuBoard::refresh_note() {
	String note;
	if (sky != nullptr) {
		const float rain = sky->get_rain_amount();
		const float snow = sky->get_snow_amount();
		const float tod = sky->get_time_of_day();
		if (snow > 0.35f) {
			note = String::utf8("落雪了\n炉边留一杯热茶");
		} else if (rain > 0.35f) {
			note = String::utf8("今日有雨\n面包炉照常生火");
		} else if (tod < 0.33f) {
			note = String::utf8("早安\n第一炉面包刚出炉");
		} else if (tod > 0.70f) {
			note = String::utf8("晚炉\n给晚归的人留一块");
		} else {
			note = String::utf8("今日出炉\n蜂蜜面包与热牛奶");
		}
	} else {
		note = String::utf8("今日出炉\n蜂蜜面包");
	}
	current_note = note;
	if (label != nullptr) label->set_text(note);
}
void MenuBoard::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (label != nullptr) label->set_rotation(Vector3(0.0f, std::sin(t * 0.8f) * 0.006f, 0.0f));
			if (snow_cap != nullptr) snow_cap->set_visible(sky != nullptr && sky->get_snow_amount() > 0.35f);
			if (sky != nullptr) refresh_note();
		} break;
		default: break;
	}
}
void MenuBoard::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 2.7f, 0.0f));
	// A 字支架
	meshutil::add_box(this, toon, Vector3(0.06f, 1.15f, 0.06f), Vector3(-0.36f, 0.57f, -0.12f), Color(0.52f, 0.38f, 0.23f), Vector3(0.20f, 0.0f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.06f, 1.15f, 0.06f), Vector3(0.36f, 0.57f, -0.12f), Color(0.52f, 0.38f, 0.23f), Vector3(0.20f, 0.0f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.06f, 1.10f, 0.06f), Vector3(-0.32f, 0.55f, 0.16f), Color(0.50f, 0.36f, 0.22f), Vector3(-0.25f, 0.0f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.06f, 1.10f, 0.06f), Vector3(0.32f, 0.55f, 0.16f), Color(0.50f, 0.36f, 0.22f), Vector3(-0.25f, 0.0f, 0.0f));
	// 黑板
	meshutil::add_box(this, toon, Vector3(0.86f, 0.70f, 0.05f), Vector3(0.0f, 0.82f, 0.10f), Color(0.50f, 0.38f, 0.25f), Vector3(-0.12f, 0.0f, 0.0f));
	Ref<StandardMaterial3D> slate;
	slate.instantiate();
	slate->set_albedo(Color(0.16f, 0.22f, 0.20f));
	slate->set_roughness(0.9f);
	MeshInstance3D *surface = memnew(MeshInstance3D);
	Ref<BoxMesh> board;
	board.instantiate();
	board->set_size(Vector3(0.78f, 0.62f, 0.012f));
	board->set_material(slate);
	surface->set_mesh(board);
	surface->set_position(Vector3(0.0f, 0.82f, 0.128f));
	surface->set_rotation(Vector3(-0.12f, 0.0f, 0.0f));
	add_child(surface);
	// 粉笔字
	Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	label = memnew(Label3D);
	label->set_name("ChalkText");
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_DISABLED);
	label->set_pixel_size(0.0030f);
	label->set_font_size(24);
	label->set_text(String::utf8("今日出炉\n蜂蜜面包"));
	label->set_modulate(Color(0.94f, 0.93f, 0.86f));
	label->set_outline_size(0);
	label->set_position(Vector3(0.0f, 0.88f, 0.158f));
	label->set_rotation(Vector3(-0.12f, 0.0f, 0.0f));
	if (cjk.is_valid()) label->set_font(cjk);
	add_child(label);
	// 粉笔槽：粉笔 + 抹布
	meshutil::add_box(this, toon, Vector3(0.82f, 0.04f, 0.12f), Vector3(0.0f, 0.46f, 0.19f), Color(0.46f, 0.33f, 0.21f), Vector3(-0.12f, 0.0f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.09f, 0.025f, 0.025f), Vector3(-0.16f, 0.49f, 0.19f), Color(0.92f, 0.90f, 0.82f), Vector3(0.0f, 0.3f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.07f, 0.022f, 0.022f), Vector3(-0.02f, 0.49f, 0.19f), Color(0.86f, 0.74f, 0.44f), Vector3(0.0f, -0.2f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.03f, 0.08f), Vector3(0.20f, 0.49f, 0.19f), Color(0.62f, 0.66f, 0.66f), Vector3(0.0f, 0.5f, 0.0f));
	// 雪帽
	snow_cap = meshutil::add_box(this, toon, Vector3(0.88f, 0.03f, 0.10f), Vector3(0.0f, 1.19f, 0.09f), Color(0.94f, 0.96f, 0.98f), Vector3(-0.12f, 0.0f, 0.0f));
	snow_cap->set_visible(false);
}
