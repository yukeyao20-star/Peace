#pragma once

// RockingChair —— 木屋门廊上的摇椅。
//
// 靠近按 E 坐下，椅子会自己轻轻前后摇；再按 E 起身。
// 座位跟着椅子的摆动走，木头偶尔吱呀一声。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

class Player;

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;

class RockingChair : public Interactable {
	GDCLASS(RockingChair, Interactable)

private:
	Player *rider = nullptr;
	Node3D *rocker = nullptr;
	bool e_prev = false;
	float angle = 0.0f;
	float t = 0.0f;
	float creak_t = 2.0f;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_creak;

	void build();
	void stand_up();
	void make_creak();
	Ref<AudioStreamWAV> synth_creak();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool is_ridden() const { return rider != nullptr; }
	float get_angle() const { return angle; }
	bool get_creak_ready() const { return snd_creak.is_valid(); }
};

} // namespace godot
