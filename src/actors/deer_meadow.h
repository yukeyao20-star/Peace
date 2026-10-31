#pragma once

// DeerMeadow —— 林缘草地上的一只小鹿。
//
// 平时低头吃草、在几个点之间慢慢走；玩家靠近 7m 会喷一口气撒腿跑远，
// 过一阵再慢慢回到自己的草地。程序化低模与合成鹿鸣/喷气声。

#include <godot_cpp/classes/node3d.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class SkyCycle;
class TerrainBuilder;

class DeerMeadow : public Node3D {
	GDCLASS(DeerMeadow, Node3D)

private:
	Node3D *root = nullptr;
	Node3D *head = nullptr;
	Node3D *leg[4] = {nullptr, nullptr, nullptr, nullptr};
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_snort;
	Vector3 home;
	Vector3 target;
	Vector3 walk_from;
	Vector3 walk_to;
	float t = 0.0f;
	float state_t = 0.0f;
	float wait_t = 2.0f;
	float walk_dur = 3.0f;
	float return_t = 0.0f;
	float snort_t = 5.0f;
	int state = 0; // 0 吃草/等待 / 1 走 / 2 逃跑
	bool flee_home = false;
	Vector3 last_print;
	bool print_side = false;
	std::mt19937 rng{20270202u};

	void build();
	void choose_walk();
	void start_flee(const Vector3 &p_player_pos);
	Ref<AudioStreamWAV> make_snort();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_state() const { return state; }
	bool is_fleeing() const { return state == 2; }
	bool get_snort_ready() const { return snd_snort.is_valid(); }
	float get_distance_from_home() const;
};

} // namespace godot
