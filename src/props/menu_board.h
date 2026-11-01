#pragma once

// MenuBoard —— 面包炉旁的立式小黑板：白粉笔写着今日出炉，按 E 看看今天写了什么。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class Label3D;
class MeshInstance3D;
class SkyCycle;
class MenuBoard : public Interactable {
	GDCLASS(MenuBoard, Interactable)
	Label3D *label = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	int reads = 0;
	String current_note;
	void build();
	void refresh_note();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.0f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_reads() const { return reads; }
	String get_note() const { return current_note; }
};
} // namespace godot
