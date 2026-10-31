#pragma once

// EntrywayShelf —— 门边墙上的小搁板：钥匙、信件托盘、手套、帽子和一面小镜子。
// 参考此前调研的室内小物比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class EntrywayShelf : public Node3D {
	GDCLASS(EntrywayShelf, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
