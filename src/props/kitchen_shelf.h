#pragma once

// KitchenShelf —— 木屋厨房墙上的小搁板。
//
// 一只陶茶壶、两只茶杯、两罐果酱、一篮面包、一截蜡烛，板下还挂着一束
// 干香草。早晨壶嘴冒一点热气，烛火轻轻跳，香草在风里慢慢摆。
//
// 参考：
//   * Tiny Treats「Charming Kitchen」CC0（Godot 4.1）—— 茶杯/陶壶/罐子的比例；
//   * Godot 4 全流程厨房教程（小红书/B 站）—— 厨房小物的摆放与层级；
//   * CGTrader「Low Poly Kitchen Pack」—— 上层搁板的尺寸参考。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CPUParticles3D;
class MeshInstance3D;
class Node3D;
class SkyCycle;

class KitchenShelf : public Node3D {
	GDCLASS(KitchenShelf, Node3D)

	static const int ITEM_TYPES = 5;

	Node3D *herbs = nullptr;
	MeshInstance3D *flame = nullptr;
	CPUParticles3D *steam = nullptr;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	bool flame_on = true;
	bool steam_on = false;
	float herbs_sway = 0.0f;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_item_types() const { return ITEM_TYPES; }
	bool get_flame_on() const { return flame_on; }
	bool get_steam_on() const { return steam_on; }
	float get_herbs_sway() const { return herbs_sway; }
};

} // namespace godot
