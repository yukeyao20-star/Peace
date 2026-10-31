#pragma once

// Bicycle —— 靠在院前木篱上的旧自行车：车筐里插着野花，按 E 拨一下车铃。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class SkyCycle;
class Bicycle : public Interactable {
	GDCLASS(Bicycle, Interactable)
	Node3D *bell_pivot = nullptr;
	MeshInstance3D *snow_seat = nullptr;
	MeshInstance3D *snow_basket = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd;
	SkyCycle *sky = nullptr;
	float ring_t = 0.0f;
	float t = 0.0f;
	int rings = 0;
	void build();
	void build_sound();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.0f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_rings() const { return rings; }
	bool is_ringing() const { return ring_t > 0.0f; }
};
} // namespace godot
