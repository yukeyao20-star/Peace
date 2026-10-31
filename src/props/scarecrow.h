#pragma once

// Scarecrow —— 院角的稻草人。
//
// 麻布上衣、草帽、木十字架；风一来肩膀和袖子轻轻晃。两只乌鸦绕着它转圈，
// 不敢落得太近，偶尔"哑——"叫一声。走到跟前按 E 可以把歪掉的草帽扶正。
//
// 参考：
//   * Scare the Crow!（WesHack 2024，Godot）—— 稻草人驱赶乌鸦的题材与行为关系；
//   * gmtk-2023（Godot）—— 提交里有 "tweak crow ai"、"Add dancing crow"，
//     乌鸦用简单航点/环绕状态；
//   * Summer Engine「How to make AI in Godot」—— FSM + 感知范围的基本结构，
//     本作只保留"绕飞 + 保持距离"这一层；
//   * Reddit r/godot 程序化稻草人动画 —— 关节用代码摆动而不是骨骼动画。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class Node3D;
class SkyCycle;

class Scarecrow : public Interactable {
	GDCLASS(Scarecrow, Interactable)

	static const int CROW_COUNT = 2;
	static const int WING_COUNT = 2;

	Node3D *upper = nullptr;
	Node3D *hat = nullptr;
	MeshInstance3D *sleeves[2] = {};
	Node3D *crows[CROW_COUNT] = {};
	MeshInstance3D *wings[CROW_COUNT][WING_COUNT] = {};
	CPUParticles3D *straw = nullptr;
	AudioStreamPlayer3D *caw_voice = nullptr;
	Ref<AudioStreamWAV> snd_caw;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float last_caw = -10.0f;
	float next_caw = 5.0f;
	int adjust_count = 0;
	int crows_visible = 0;
	float crow_angle = 0.0f;

	void build();
	void build_caw();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.6f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_adjust_count() const { return adjust_count; }
	int get_crows_visible() const { return crows_visible; }
	float get_hat_tilt() const;
	float get_crow_angle() const { return crow_angle; }
};

} // namespace godot
