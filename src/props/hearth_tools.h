#pragma once

// HearthTools —— 壁炉旁的铁架与工具：火钳、小铲、风箱、柴篮和一盒火柴。
// 参考此前调研的室内陈设与壁炉配件比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class HearthTools : public Node3D {
	GDCLASS(HearthTools, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
