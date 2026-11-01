#pragma once

// Workbench —— 谷仓边的木工台：台钳、手刨、靠着的锯子和一卷卷刨花，
// 按 E 推两下刨子，木香和刨花一起落下来。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class SkyCycle;
class Workbench : public Interactable {
	GDCLASS(Workbench, Interactable)
	Node3D *plane = nullptr;
	Node3D *shavings = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd;
	SkyCycle *sky = nullptr;
	float work_t = 0.0f;
	float t = 0.0f;
	int uses = 0;
	void build();
	void build_sound();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.1f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_uses() const { return uses; }
	bool is_working() const { return work_t > 0.0f; }
};
} // namespace godot
