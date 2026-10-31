#pragma once

// Signpost —— 岔路口的木路牌：三块手写箭头指向木屋、湖边和树林。
// 按 E 可以仔细看看路牌。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class Node3D;
class Signpost : public Interactable {
	GDCLASS(Signpost, Interactable)
	Node3D *arrows = nullptr;
	int reads = 0;
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
	int get_reads() const { return reads; }
};
} // namespace godot
