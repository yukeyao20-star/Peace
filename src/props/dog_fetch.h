#pragma once

// DogFetch —— 院子里的小狗与红皮球。
//
// 走近按 E 把球扔出去：小狗会先叫一声，追着球跑，叼起来送回你脚边放下，
// 然后摇着尾巴等你再扔。跑得越快，尾巴摇得越欢。
//
// 参考：
//   * skooter500/GE2-Test-2021-Starter —— 狗捡球：投掷 → 叫 → 寻球 → 叼回
//     玩家前方 10 单位；尾巴速度随移动速度变化；
//   * KnightwingJ/GE2-Assignment —— 球停下后狗 seek，叼到后回 wander；
//   * kidscancode「Pet Following」（Godot 4）—— 宠物跟随的移动/停止判据。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class TerrainBuilder;

class DogFetch : public Interactable {
	GDCLASS(DogFetch, Interactable)

	enum State { IDLE = 0, THROWN = 1, CHASE = 2, RETURN = 3, DROP = 4 };

	Node3D *dog = nullptr;
	Node3D *tail = nullptr;
	MeshInstance3D *ball = nullptr;
	Node3D *player = nullptr;
	TerrainBuilder *terrain = nullptr;
	AudioStreamPlayer3D *bark_voice = nullptr;
	Ref<AudioStreamWAV> snd_bark;

	State state = IDLE;
	Vector3 dog_pos;
	Vector3 ball_pos;
	Vector3 ball_vel;
	Vector3 dog_vel;
	float heading = 0.0f;
	float state_t = 0.0f;
	float wag = 0.0f;
	int fetch_count = 0;

	void build();
	void build_bark();
	void start_throw(const Vector3 &p_player_pos);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 3.0f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_state() const { return (int)state; }
	int get_fetch_count() const { return fetch_count; }
	Vector3 get_dog_pos() const { return dog_pos; }
	Vector3 get_ball_pos() const { return ball_pos; }
	float get_wag() const { return wag; }
};

} // namespace godot
