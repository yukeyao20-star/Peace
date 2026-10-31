#pragma once

// DoorstepBox —— 门口的木箱：两只牛奶瓶、一份报纸、一个包裹和一小束花。
// 参考此前调研的低模道具比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class DoorstepBox : public Node3D {
	GDCLASS(DoorstepBox, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
