#pragma once

// StewBowl —— 汤锅旁的小木凳与一叠木碗。
//
// 靠近按 E 盛一碗热汤：碗层减少，手里多出一只冒热气的碗，跟着你走。
// 再按 E 喝掉：暖意提示、轻轻的啜汤声，并记入每日待办。
// 走远了汤会洒掉（碗放回），碗空了过一阵锅边会重新补满。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class Node3D;

class StewBowl : public Interactable {
	GDCLASS(StewBowl, Interactable)

private:
	Node3D *bowls[3] = { nullptr, nullptr, nullptr };
	Node3D *held = nullptr;
	CPUParticles3D *held_steam = nullptr;
	CPUParticles3D *sip_puff = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_sip;
	Node3D *player = nullptr;
	bool holding = false;
	int bowls_left = 3;
	float held_t = 0.0f;
	float refill_t = 0.0f;
	float t = 0.0f;

	void build();
	Ref<AudioStreamWAV> make_sip();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_bowls_left() const { return bowls_left; }
	bool get_holding() const { return holding; }
	bool get_held_visible() const;
	bool get_sip_ready() const { return snd_sip.is_valid(); }
};

} // namespace godot
