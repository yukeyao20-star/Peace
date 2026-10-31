#pragma once

// KitchenPantry —— 厨房墙角的储物角。
//
// 一袋面粉、一个小香料架、一罐蜂蜜、一只黄油搅拌桶和一根擀面杖。
// 参考此前调研的 Tiny Treats 厨房套件与低模烘焙道具比例。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class KitchenPantry : public Node3D {
	GDCLASS(KitchenPantry, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};

} // namespace godot
