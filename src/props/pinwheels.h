#pragma once

// Pinwheels —— 花圃边的三支小风车。
//
// 三根木签插在草里，四瓣彩纸风车迎着风呼噜噜地转；雨大风急时转得更快，
// 下雪时纸叶被雪压住，慢慢停下来。
//
// 参考：
//   * gamedev.stackexchange「spin the wheel」—— 用角速度与阻尼驱动转轮；
//   * Godot Essentials Rotator Component —— 恒定角速度旋转的环境小道具；
//   * godotshaders「Radial Motion Blur for Propellers and Wheels」—— 高速
//     旋转时的径向模糊思路（本作只在转速高时加快叶片旋转）。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Node3D;
class SkyCycle;

class Pinwheels : public Node3D {
	GDCLASS(Pinwheels, Node3D)

	static const int COUNT = 3;

	Node3D *wheels[COUNT] = {};
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float spin = 1.0f;
	bool snow_stopped = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_count() const { return COUNT; }
	float get_spin() const { return spin; }
	bool get_snow_stopped() const { return snow_stopped; }
};

} // namespace godot
