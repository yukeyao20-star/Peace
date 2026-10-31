#pragma once

// CabinSign —— 木屋门楣上的手刻门牌「平安」，下面挂着一只铜门环，
// 按 E 叩两下门，木门里会传来闷闷的回声。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class SkyCycle;
class CabinSign : public Interactable {
	GDCLASS(CabinSign, Interactable)
	Node3D *knocker = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd;
	SkyCycle *sky = nullptr;
	float knock_t = 0.0f;
	float t = 0.0f;
	int knocks = 0;
	void build();
	void build_sound();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override { return get_global_position(); }
	float get_interact_distance() const override { return 1.9f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_knocks() const { return knocks; }
	bool is_knocking() const { return knock_t > 0.0f; }
};
} // namespace godot
