#pragma once

// Sunflowers —— 农场边的一排向日葵。
//
// 花盘白天跟着太阳从东转到西，夜里慢慢垂头；两根茎轻轻摇，几只蜂在
// 花盘边绕。没有交互，只是给田野一个会随着时间变化的朝向。
//
// 参考：
//   * blog.quizalize.com「Birdsong and sunflowers」—— 把太阳方向投影到
//     水平面作为目标方向、每帧旋转花盘的向日逻辑；
//   * ElonGame/godot-Stylized-Sky-project —— 按时间推算太阳方位；
//   * godotshaders.com「3D Sun Shader」—— 花盘中心的暖色放射结构参考。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class SkyCycle;

class Sunflowers : public Node3D {
	GDCLASS(Sunflowers, Node3D)

	static const int FLOWER_COUNT = 6;
	static const int BEE_COUNT = 2;

	Node3D *heads[FLOWER_COUNT] = {};
	MeshInstance3D *wings[BEE_COUNT][2] = {};
	Node3D *bees[BEE_COUNT] = {};

	float t = 0.0f;
	float head_yaw = 0.0f;
	float head_pitch = 0.0f;
	int bees_visible = 0;
	bool sun_up = true;
	SkyCycle *sky = nullptr;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_flower_count() const { return FLOWER_COUNT; }
	int get_bees_visible() const { return bees_visible; }
	bool get_sun_up() const { return sun_up; }
	float get_head_yaw() const { return head_yaw; }
	float get_head_pitch() const { return head_pitch; }
};

} // namespace godot
