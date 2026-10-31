#pragma once

// Sprinkler —— 花圃边的旋转洒水器。
//
// 按 E 打开：喷头转起来，水珠成扇形洒出去，带一点细细的水声；过一阵子自动关。
// 参考此前调研的灌溉/喷头道具。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class Node3D;
class Sprinkler : public Interactable {
	GDCLASS(Sprinkler, Interactable)
	Node3D *head = nullptr;
	CPUParticles3D *spray = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd;
	bool on = false;
	float on_t = 0.0f;
	int uses = 0;
	void build();
	void build_sound();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	bool get_on() const { return on; }
	int get_uses() const { return uses; }
};
} // namespace godot
