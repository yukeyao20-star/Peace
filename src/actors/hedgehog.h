#pragma once

// Hedgehog —— 草甸里的刺猬（夜行动物）。
//
// 白天躲着不见；入夜后在窝附近慢慢嗅探、挪动。玩家靠近 3.5m 内会
// 缩成一个刺球，等玩家走远再展开。偶尔发出轻轻的鼻息声。

#include <godot_cpp/classes/node3d.hpp>

#include <random>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class SkyCycle;
class TerrainBuilder;

class Hedgehog : public Node3D {
	GDCLASS(Hedgehog, Node3D)

private:
	Node3D *model = nullptr;
	Node3D *head = nullptr;
	Node3D *feet[4] = {nullptr, nullptr, nullptr, nullptr};
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_snuffle;
	Vector3 home;
	Vector3 walk_from;
	Vector3 walk_to;
	float t = 0.0f;
	float wait_t = 2.0f;
	float walk_dur = 2.5f;
	float state_t = 0.0f;
	float snuffle_t = 3.0f;
	int state = 0; // 0 嗅探/走动 / 1 蜷缩
	std::mt19937 rng{20270505u};

	void build();
	void make_snuffle();
	Ref<AudioStreamWAV> synth_snuffle();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool is_showing() const;
	Vector3 get_home() const { return home; }
	bool is_curled() const { return state == 1; }
	int get_state_value() const { return state; }
	bool get_snuffle_ready() const { return snd_snuffle.is_valid(); }
	bool get_snuffle_playing() const;
};

} // namespace godot
