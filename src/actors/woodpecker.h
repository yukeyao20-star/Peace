#pragma once

// Woodpecker —— 小径旁枯树上的啄木鸟。
//
// 白天趴在树干上，隔几秒快速啄一阵，发出"笃笃笃"的敲击声；
// 玩家靠近 6m 内就飞走，过一阵再回来。夜里不出现。

#include <godot_cpp/classes/node3d.hpp>

#include <random>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class SkyCycle;

class Woodpecker : public Node3D {
	GDCLASS(Woodpecker, Node3D)

private:
	Node3D *bird = nullptr;
	Node3D *head = nullptr;
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_knock;
	Vector3 home;
	float t = 0.0f;
	float peck_t = 0.0f;
	float knock_t = 2.0f;
	float fly_t = 0.0f;
	float fly_dur = 1.6f;
	float respawn_t = 0.0f;
	Vector3 fly_from;
	Vector3 fly_to;
	int state = 0; // 0 停驻 / 1 飞走 / 2 等待
	std::mt19937 rng{20270105u};

	void build();
	void take_off(const Vector3 &p_player_pos);
	Ref<AudioStreamWAV> make_knock();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool is_perched() const { return state == 0; }
	bool get_knock_ready() const { return snd_knock.is_valid(); }
	float get_peck_offset() const;
};

} // namespace godot
