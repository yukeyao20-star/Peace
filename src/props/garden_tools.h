#pragma once

// GardenTools —— 菜园边的园艺工具。
//
// 一辆独轮小推车、一把耙子、一把铁锹、一把锄头和几只种子纸袋。
// 参考此前调研过的 Cozy Farm Asset Pack（Godot Asset Store）与
// pixor.art 的低模农具比例。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class GardenTools : public Node3D {
	GDCLASS(GardenTools, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};

} // namespace godot
