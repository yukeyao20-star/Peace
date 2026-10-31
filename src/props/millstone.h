#pragma once

// Millstone —— 农场边的小石磨。
//
// 立着的石轮装在木架上，旁边一只木槽；按 E 推磨，石轮慢慢转，槽里的
// 面粉一点点多起来。参考此前调研的农场工具包比例。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;

class Millstone : public Interactable {
	GDCLASS(Millstone, Interactable)
	Node3D *wheel = nullptr;
	Node3D *crank = nullptr;
	MeshInstance3D *flour = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd;
	bool grinding = false;
	float grind_t = 0.0f;
	float flour_level = 0.0f;
	int grinds = 0;
	void build();
	void build_sound();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	bool get_grinding() const { return grinding; }
	float get_flour_level() const { return flour_level; }
	int get_grinds() const { return grinds; }
};

} // namespace godot
