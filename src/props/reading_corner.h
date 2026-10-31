#pragma once

// ReadingCorner —— 屋里靠墙的阅读角。
//
// 一张扶手椅、一条搭在扶手上的毛毯、一张小边桌；桌上热茶冒着气，旁边的
// 落地灯天黑后亮起暖黄的光。
//
// 参考：
//   * Modern Japanese Home Props Lite（Godot Asset Store）—— 扶手椅/边桌比例；
//   * Tiny Treats「Charming Kitchen」CC0 —— 茶杯与桌面的尺度；
//   * 常见落地灯低模结构 —— 细杆 + 布灯罩 + 暖光。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CPUParticles3D;
class MeshInstance3D;
class SkyCycle;

class ReadingCorner : public Node3D {
	GDCLASS(ReadingCorner, Node3D)

	static const int ITEM_TYPES = 5;

	MeshInstance3D *bulb = nullptr;
	CPUParticles3D *steam = nullptr;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	bool lamp_on = false;
	bool steam_on = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_item_types() const { return ITEM_TYPES; }
	bool get_lamp_on() const { return lamp_on; }
	bool get_steam_on() const { return steam_on; }
};

} // namespace godot
