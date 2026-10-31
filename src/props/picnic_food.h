#pragma once

// PicnicFood —— 野餐垫旁的吃食：西瓜、柠檬水、馅饼、三明治和草莓。
// 参考此前调研的野餐套件比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class PicnicFood : public Node3D {
	GDCLASS(PicnicFood, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
