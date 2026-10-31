#pragma once

// WoodenToys —— 门口木箱里的木头玩具：积木、木马、弹珠、拉线小火车和布娃娃。
// 参考此前调研的低模玩具比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class WoodenToys : public Node3D {
	GDCLASS(WoodenToys, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
