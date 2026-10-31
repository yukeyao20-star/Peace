#pragma once

// CompostBin —— 菜园堆肥箱：板条箱里堆着厨余，按 E 翻一翻，温热的白汽从缝里冒出来。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class CPUParticles3D;
class MeshInstance3D;
class Node3D;
class SkyCycle;
class CompostBin : public Interactable {
	GDCLASS(CompostBin, Interactable)
	Node3D *pile = nullptr;
	Node3D *flip = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	CPUParticles3D *steam = nullptr;
	Node3D *flies[2] = {};
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	float turn_t = 0.0f;
	float flip_angle = -0.5f;
	int turns = 0;
	void build();
	void build_steam();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.1f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_turns() const { return turns; }
	bool is_steaming() const { return turn_t > 0.0f; }
	bool is_snow_covered() const;
};
} // namespace godot
