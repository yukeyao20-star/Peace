#pragma once

// HoneyStand —— 蜂箱旁的小蜂蜜摊：三罐琥珀色蜂蜜、一块蜂巢、木勺，
// 按 E 尝一口，小蜂在罐口打转。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class Node3D;
class SkyCycle;
class HoneyStand : public Interactable {
	GDCLASS(HoneyStand, Interactable)
	Node3D *jar = nullptr;
	Node3D *bees[2] = {};
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	float tilt = 0.0f;
	int tastes = 0;
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
	int get_tastes() const { return tastes; }
	bool get_bees_visible() const { return bees[0] != nullptr && bees[0]->is_visible(); }
};
} // namespace godot
