#pragma once

// FireflyJar —— 门廊上的萤火虫玻璃罐。
//
// 夜里罐子里有几点会呼吸的暖光；按 E 打开盖子放它们出来，再按 E 捉回来。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class MeshInstance3D;
class Node3D;
class SkyCycle;
class FireflyJar : public Interactable {
	GDCLASS(FireflyJar, Interactable)
	static const int COUNT = 5;
	MeshInstance3D *motes[COUNT] = {};
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	bool sealed = true;
	bool night = false;
	int toggles = 0;
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
	bool get_sealed() const { return sealed; }
	bool get_night() const { return night; }
	int get_toggles() const { return toggles; }
};
} // namespace godot
