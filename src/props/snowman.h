#pragma once

// Snowman —— 雪天草甸上的雪堆，可以按 E 堆雪人。
//
// 下雪时雪堆可见；按 E 会分三段滚出雪球、叠成雪人（胡萝卜鼻子、
// 煤球眼睛、树枝手臂、红围巾），再按 E 拍一拍会扬起一团雪粉。
// 雪化后雪人随之消失，下次下雪可以重新堆。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class Node3D;
class SkyCycle;
class TerrainBuilder;

class Snowman : public Interactable {
	GDCLASS(Snowman, Interactable)

private:
	TerrainBuilder *terrain = nullptr;
	SkyCycle *sky = nullptr;
	Node3D *pile = nullptr;
	Node3D *body = nullptr;
	MeshInstance3D *parts[3] = {nullptr, nullptr, nullptr};
	Node3D *face = nullptr;
	CPUParticles3D *puff = nullptr;
	AudioStreamPlayer3D *crunch = nullptr;
	Ref<AudioStreamWAV> snd_crunch;
	float t = 0.0f;
	float build_t = 0.0f;
	bool built = false;
	bool building = false;

	void build();
	void make_crunch();
	Ref<AudioStreamWAV> synth_crunch();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool get_built() const { return built; }
	bool get_building() const { return building; }
	bool get_crunch_ready() const { return snd_crunch.is_valid(); }
	bool get_puff_emitting() const;
};

} // namespace godot
