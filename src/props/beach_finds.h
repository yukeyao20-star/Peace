#pragma once

// BeachFinds —— 湖岸捡来的小东西：漂流木、海玻璃罐、漂流瓶、海星和羽毛。
// 参考此前调研的海岸道具比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class BeachFinds : public Node3D {
	GDCLASS(BeachFinds, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
