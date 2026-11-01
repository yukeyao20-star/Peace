#pragma once

// YarnBasket —— 门廊摇椅边的毛线篮：三团彩色毛线、两根竹针和织了一半的围巾。
// 按 E 摸一摸毛线，软软的。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class Node3D;
class YarnBasket : public Interactable {
	GDCLASS(YarnBasket, Interactable)
	Node3D *needles = nullptr;
	float t = 0.0f;
	int touches = 0;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 1.8f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_touches() const { return touches; }
};
} // namespace godot
