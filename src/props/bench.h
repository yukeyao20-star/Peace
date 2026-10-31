#pragma once

// Bench —— 火堆旁的长椅。靠近按 E 坐下，再按 E 起身。
// 坐下时角色模型压扁成坐姿、视角留在原地；起身落回长椅前方。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

class Player;

namespace godot {

class OmniLight3D;
class SkyCycle;
class StandardMaterial3D;

class Bench : public Interactable {
	GDCLASS(Bench, Interactable)

private:
	Player *seated = nullptr;
	Vector3 seat_pos;
	float seat_yaw = 0.0f;
	bool e_prev = false;
	OmniLight3D *lantern_light = nullptr;
	StandardMaterial3D *lantern_glass = nullptr;
	SkyCycle *sky = nullptr;
	float lamp_t = 0.0f;

	void build();
	void stand_up();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
};

} // namespace godot
