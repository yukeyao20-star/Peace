#pragma once

// WinterGear —— 门口的冬日小物：雪橇、手套、围巾、雪铲和一堆雪球。
// 参考此前调研的冬日道具比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class WinterGear : public Node3D {
	GDCLASS(WinterGear, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
