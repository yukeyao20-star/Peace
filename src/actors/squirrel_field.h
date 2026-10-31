#pragma once

// SquirrelField —— 木屋旁草地上的两只小松鼠。
//
// 平时在窝附近跳来跳去、停下来东张西望；玩家靠近 5.5m 内会一溜烟跑回窝里
// 钻进地面躲起来，过一阵再探头出来。全部程序化低模，带一声短促的吱吱。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class TerrainBuilder;

class SquirrelField : public Node3D {
	GDCLASS(SquirrelField, Node3D)

private:
	struct Squirrel {
		Node3D *root = nullptr;
		Node3D *tail = nullptr;
		Node3D *head = nullptr;
		Vector3 home;
		Vector3 wp[3];
		int wp_index = 0;
		int state = 0; // 0 等待 / 1 跳跃 / 2 逃跑 / 3 躲藏
		float state_t = 0.0f;
		float wait_t = 1.0f;
		float hop_dur = 0.55f;
		Vector3 hop_from;
		Vector3 hop_to;
		float tail_phase = 0.0f;
		float hidden_t = 0.0f;
		bool fleeing = false;
	};

	std::vector<Squirrel> squirrels;
	TerrainBuilder *terrain = nullptr;
	Node3D *player = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_chatter;
	float t = 0.0f;
	std::mt19937 rng{20260601u};

	void build();
	void spawn(int p_index, const Vector3 &p_home, unsigned p_seed);
	void next_hop(Squirrel &p_s);
	void start_flee(Squirrel &p_s, const Vector3 &p_player_pos);
	Ref<AudioStreamWAV> make_chatter();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_squirrel_count() const { return (int)squirrels.size(); }
	int get_hidden_count() const {
		int n = 0;
		for (const Squirrel &s : squirrels) {
			if (s.state == 3) {
				n++;
			}
		}
		return n;
	}
};

} // namespace godot
