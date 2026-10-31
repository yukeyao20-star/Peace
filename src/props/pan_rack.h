#pragma once

// PanRack —— 厨房墙上的挂锅架：三口锅、一把汤勺和一只隔热垫。
// 参考此前调研的 Tiny Treats 厨房套件比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class PanRack : public Node3D {
	GDCLASS(PanRack, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
