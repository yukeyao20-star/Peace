#pragma once

// NetRack —— 码头边的晒网架：ㄇ 形木架上摊着一张旧渔网，网口挂着一排软木浮子；
// 按 E 抖一抖网，水珠会顺着网眼滑下来。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class Node3D;
class SkyCycle;
class NetRack : public Interactable {
	GDCLASS(NetRack, Interactable)
	Node3D *net = nullptr;
	Node3D *floats = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	float shake = 0.0f;
	int shakes = 0;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_floats() const { return 6; }
	int get_shakes() const { return shakes; }
	bool is_swinging() const { return shake > 0.0f; }
};
} // namespace godot
