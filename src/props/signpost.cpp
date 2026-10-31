#include "signpost.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void Signpost::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Signpost::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Signpost::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Signpost::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Signpost::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Signpost::interact);
	ClassDB::bind_method(D_METHOD("get_reads"), &Signpost::get_reads);
}
String Signpost::get_prompt() const { return String::utf8("按 E 看看路牌"); }
Vector3 Signpost::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 1.2f, 0.0f); }
void Signpost::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	reads++;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("木屋在西北 · 湖边往南 · 树林往东"), 3.2f);
	}
}
void Signpost::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); }
}
void Signpost::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	meshutil::add_box(this, toon, Vector3(0.10f, 1.60f, 0.10f), Vector3(0.0f, 0.80f, 0.0f), Color(0.50f, 0.35f, 0.21f));
	arrows = memnew(Node3D);
	arrows->set_name("Arrows");
	add_child(arrows);
	const Color board(0.66f, 0.50f, 0.30f);
	// 三块箭头板，各偏一点角度
	meshutil::add_box(arrows, toon, Vector3(0.52f, 0.14f, 0.04f), Vector3(0.20f, 1.28f, 0.02f), board, Vector3(0.0f, 0.3f, 0.0f));
	meshutil::add_box(arrows, toon, Vector3(0.52f, 0.14f, 0.04f), Vector3(0.18f, 1.08f, 0.01f), board, Vector3(0.0f, -0.4f, 0.0f));
	meshutil::add_box(arrows, toon, Vector3(0.52f, 0.14f, 0.04f), Vector3(0.16f, 0.88f, 0.02f), board, Vector3(0.0f, 0.1f, 0.0f));
	// 箭头尖
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(arrows, toon, Vector3(0.12f, 0.14f, 0.04f),
				Vector3(0.48f, 1.28f - 0.20f * (float)i, 0.02f), board,
				Vector3(0.0f, i == 1 ? -0.4f : (i == 0 ? 0.3f : 0.1f), 0.785f));
	}
	// 手写路牌
	Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	Label3D *label = memnew(Label3D);
	label->set_name("SignText");
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_DISABLED);
	label->set_pixel_size(0.0035);
	label->set_font_size(44);
	label->set_text(String::utf8("木屋\n湖边\n树林"));
	label->set_modulate(Color(0.24f, 0.18f, 0.12f));
	label->set_position(Vector3(0.18f, 1.08f, 0.05f));
	label->set_rotation(Vector3(0.0f, 0.0f, 0.0f));
	if (cjk.is_valid()) label->set_font(cjk);
	add_child(label);
}
