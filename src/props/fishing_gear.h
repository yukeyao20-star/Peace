#pragma once

// FishingGear —— 码头边的渔具。
//
// 两把靠在木架上的鱼竿、一只钓具箱、一张抄网、一只浮漂桶和一双涉水裤。
// 参考此前调研的钓鱼道具素材与 pixor.art 低模比例。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class FishingGear : public Node3D {
	GDCLASS(FishingGear, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};

} // namespace godot
