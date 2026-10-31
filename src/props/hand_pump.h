#pragma once

// HandPump —— 菜园边的老式手压水泵：按 E 压几下，铁柄吱呀，水哗哗流进木槽。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class HandPump : public Interactable {
	GDCLASS(HandPump, Interactable)
	Node3D *handle = nullptr;
	MeshInstance3D *stream = nullptr;
	MeshInstance3D *water = nullptr;
	MeshInstance3D *ripple = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd;
	float flow_t = 0.0f;
	float t = 0.0f;
	float water_level = 0.35f;
	int pumps = 0;
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
	int get_pumps() const { return pumps; }
	bool is_flowing() const { return flow_t > 0.0f; }
	float get_water_level() const { return water_level; }
};
} // namespace godot
