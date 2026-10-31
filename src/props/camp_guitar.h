#pragma once

// CampGuitar —— 靠在营地圆木边的一把旧吉他。
//
// 按 E 弹一段四和弦的分解，琴弦震动，几点金色音符从音孔边升起。
// 参考此前调研的八音盒程序化旋律与和弦分解做法。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class Node3D;

class CampGuitar : public Interactable {
	GDCLASS(CampGuitar, Interactable)
	Node3D *strings = nullptr;
	CPUParticles3D *notes = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> tune;
	bool playing = false;
	int times = 0;
	float t = 0.0f;
	void build();
	Ref<AudioStreamWAV> compose();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	bool get_playing() const { return playing; }
	int get_times() const { return times; }
	bool get_tune_ready() const { return tune.is_valid(); }
};

} // namespace godot
