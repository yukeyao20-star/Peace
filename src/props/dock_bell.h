#pragma once

// DockBell —— 码头木柱上的小铜铃，按 E 摇一下，声音能传过水面。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class AudioStreamPlayer3D;
class AudioStreamWAV;
class Node3D;
class DockBell : public Interactable {
	GDCLASS(DockBell, Interactable)
	Node3D *bell = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd;
	float ring_t = 0.0f;
	int rings = 0;
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
	int get_rings() const { return rings; }
	bool get_ringing() const { return ring_t > 0.0f; }
};
} // namespace godot
