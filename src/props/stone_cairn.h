#pragma once

// StoneCairn —— 草地上的祈愿石堆。按 E 往上放一块小石头，它会轻轻晃一下再稳住。
// 参考此前调研的石堆/路标结构。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class MeshInstance3D;
class Node3D;
class StoneCairn : public Interactable {
	GDCLASS(StoneCairn, Interactable)
	static const int MAX_STONES = 8;
	MeshInstance3D *stones[MAX_STONES] = {};
	Node3D *top = nullptr;
	float t = 0.0f;
	float sway_t = 0.0f;
	int count = 6;
	int additions = 0;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.0f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_stone_count() const { return count; }
	int get_additions() const { return additions; }
};
} // namespace godot
