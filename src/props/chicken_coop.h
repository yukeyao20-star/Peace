#pragma once

// ChickenCoop —— 院角的小鸡舍与三只母鸡。
//
// 木窝、斜坡、草窝；三只鸡在窝边慢慢踱步、低头啄食，人一靠近就扑棱着跑开，
// 过一会儿再回来。按 E 可以收走草窝里的鸡蛋，过一阵母鸡会再下一个。
//
// 参考：
//   * kyrick/godot-boids —— 鸡群使用视野/避让距离/最大速度的 steering 参数；
//   * konbel/steering-behaviors-godot-4 —— Wander + Flee 的基本行为；
//   * dusoleil/chicken-chaser-gd —— 鸡的 mov_length / peck_chance / flap_chance
//     这类"移动-啄食-扑翅"概率状态；
//   * CSDN《Godot4.3 小鸡 NPC》—— 行走/空闲状态由计时器驱动。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;

class ChickenCoop : public Interactable {
	GDCLASS(ChickenCoop, Interactable)

	static const int CHICKEN_COUNT = 3;

	struct Chicken {
		Node3D *root = nullptr;
		Node3D *head = nullptr;
		MeshInstance3D *wings[2] = {};
		Vector3 pos;
		float heading = 0.0f;
		float mode_t = 0.0f;
		int mode = 0; // 0 走 1 啄食 2 受惊跑
		float scared = 0.0f;
		float phase = 0.0f;
	};

	Chicken chickens[CHICKEN_COUNT];
	Node3D *egg = nullptr;
	AudioStreamPlayer3D *cluck_voice = nullptr;
	Ref<AudioStreamWAV> snd_cluck;
	Node3D *player = nullptr;

	float t = 0.0f;
	float lay_t = 0.0f;
	float cluck_t = 0.0f;
	float next_cluck = 6.0f;
	bool egg_ready = true;
	int egg_count = 0;
	int scared_count = 0;

	void build();
	void build_cluck();
	void update_chicken(int p_index, float delta, const Vector3 &p_player);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.6f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_chicken_count() const { return CHICKEN_COUNT; }
	bool get_egg_ready() const { return egg_ready; }
	int get_egg_count() const { return egg_count; }
	int get_scared_count() const { return scared_count; }
};

} // namespace godot
