#pragma once

// CheckersTable —— 营地边的小棋桌：木桌、棋盘、红黑两色棋子。
// 按 E 随手走一步，棋盘上会有一颗棋子轻轻挪动。
// 参考此前调研的低模桌面游戏道具。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class MeshInstance3D;
class MultiMesh;
class Node3D;
class CheckersTable : public Interactable {
	GDCLASS(CheckersTable, Interactable)
	static const int PIECE_COUNT = 12;
	Ref<MultiMesh> red_mm;
	Ref<MultiMesh> black_mm;
	Vector3 piece_pos[PIECE_COUNT];
	void update_piece(int p_index);
	int moves = 0;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_piece_count() const { return PIECE_COUNT; }
	int get_moves() const { return moves; }
};
} // namespace godot
