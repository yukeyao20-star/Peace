#include "checkers_table.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void CheckersTable::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &CheckersTable::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &CheckersTable::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &CheckersTable::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &CheckersTable::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &CheckersTable::interact);
	ClassDB::bind_method(D_METHOD("get_piece_count"), &CheckersTable::get_piece_count);
	ClassDB::bind_method(D_METHOD("get_moves"), &CheckersTable::get_moves);
}
String CheckersTable::get_prompt() const { return String::utf8("按 E 随手走一步棋"); }
Vector3 CheckersTable::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.62f, 0.0f); }
void CheckersTable::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	moves++;
	const int idx = (moves * 5 + 3) % PIECE_COUNT;
	piece_pos[idx].z += (idx % 2 == 0) ? 0.07f : -0.07f;
	piece_pos[idx].x += (idx % 3 == 0) ? 0.04f : 0.0f;
	update_piece(idx);
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("棋子挪了一步 · 你想接着下吗"), 2.4f);
	}
}
void CheckersTable::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); }
}
void CheckersTable::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	meshutil::add_box(this, toon, Vector3(0.90f, 0.06f, 0.70f), Vector3(0.0f, 0.50f, 0.0f), Color(0.56f, 0.40f, 0.24f));
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.36f : 0.36f;
		const float sz = (i < 2) ? -0.26f : 0.26f;
		meshutil::add_box(this, toon, Vector3(0.06f, 0.50f, 0.06f), Vector3(sx, 0.25f, sz), Color(0.46f, 0.32f, 0.19f));
	}
	// 棋盘：明暗两色分别合成 MultiMesh（64 个格子→2 次 draw）
	Ref<BoxMesh> dark_tile;
	dark_tile.instantiate();
	dark_tile->set_size(Vector3(0.062f, 0.01f, 0.062f));
	Ref<StandardMaterial3D> dm;
	dm.instantiate();
	dm->set_albedo(Color(0.26f, 0.22f, 0.18f));
	dark_tile->set_material(dm);
	Ref<BoxMesh> light_tile;
	light_tile.instantiate();
	light_tile->set_size(Vector3(0.062f, 0.01f, 0.062f));
	Ref<StandardMaterial3D> lm;
	lm.instantiate();
	lm->set_albedo(Color(0.90f, 0.86f, 0.76f));
	light_tile->set_material(lm);
	Ref<MultiMesh> dark_mm;
	dark_mm.instantiate();
	dark_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	dark_mm->set_mesh(dark_tile);
	dark_mm->set_instance_count(32);
	Ref<MultiMesh> light_mm;
	light_mm.instantiate();
	light_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	light_mm->set_mesh(light_tile);
	light_mm->set_instance_count(32);
	int di = 0;
	int li = 0;
	for (int x = 0; x < 8; x++) {
		for (int z = 0; z < 8; z++) {
			const Vector3 pos(-0.245f + 0.07f * (float)x, 0.535f, -0.245f + 0.07f * (float)z);
			if ((x + z) % 2 == 0) dark_mm->set_instance_transform(di++, Transform3D(Basis(), pos));
			else light_mm->set_instance_transform(li++, Transform3D(Basis(), pos));
		}
	}
	MultiMeshInstance3D *dmi = memnew(MultiMeshInstance3D);
	dmi->set_name("DarkSquares");
	dmi->set_multimesh(dark_mm);
	add_child(dmi);
	MultiMeshInstance3D *lmi = memnew(MultiMeshInstance3D);
	lmi->set_name("LightSquares");
	lmi->set_multimesh(light_mm);
	add_child(lmi);
	// 优化（第 170 轮）：12 颗棋子按颜色合成两个 MultiMesh。
	Ref<CylinderMesh> unit_piece;
	unit_piece.instantiate();
	unit_piece->set_top_radius(1.0f);
	unit_piece->set_bottom_radius(1.05f);
	unit_piece->set_height(1.0f);
	unit_piece->set_radial_segments(8);
	unit_piece->set_rings(1);
	red_mm.instantiate();
	red_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	red_mm->set_mesh(unit_piece);
	red_mm->set_instance_count(6);
	Ref<CylinderMesh> black_mesh = unit_piece->duplicate();
	Ref<StandardMaterial3D> bm;
	bm.instantiate();
	bm->set_albedo(Color(0.20f, 0.20f, 0.22f));
	black_mesh->set_material(bm);
	black_mm.instantiate();
	black_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	black_mm->set_mesh(black_mesh);
	black_mm->set_instance_count(6);
	Ref<StandardMaterial3D> rm;
	rm.instantiate();
	rm->set_albedo(Color(0.78f, 0.24f, 0.20f));
	unit_piece->set_material(rm);
	const Basis pb = Basis().scaled(Vector3(0.030f, 0.018f, 0.030f));
	for (int i = 0; i < PIECE_COUNT; i++) {
		const bool red = i < 6;
		const int row = i / 4;
		const int col = i % 4;
		const float z = red ? -0.21f + 0.14f * (float)row : 0.21f - 0.14f * (float)row;
		const float x = -0.21f + 0.14f * (float)col;
		piece_pos[i] = Vector3(x, 0.56f, z);
		Ref<MultiMesh> mm = red ? red_mm : black_mm;
		mm->set_instance_transform(red ? i : i - 6, Transform3D(pb, piece_pos[i]));
	}
	MeshInstance3D *rmi = nullptr;
	(void)rmi;
	MultiMeshInstance3D *red_mi = memnew(MultiMeshInstance3D);
	red_mi->set_name("RedPieces");
	red_mi->set_multimesh(red_mm);
	add_child(red_mi);
	MultiMeshInstance3D *black_mi = memnew(MultiMeshInstance3D);
	black_mi->set_name("BlackPieces");
	black_mi->set_multimesh(black_mm);
	add_child(black_mi);
}

void CheckersTable::update_piece(int p_index) {
	const bool red = p_index < 6;
	Ref<MultiMesh> mm = red ? red_mm : black_mm;
	if (!mm.is_valid()) return;
	const Basis pb = Basis().scaled(Vector3(0.030f, 0.018f, 0.030f));
	mm->set_instance_transform(red ? p_index : p_index - 6, Transform3D(pb, piece_pos[p_index]));
}