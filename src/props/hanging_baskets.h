#pragma once

// HangingBaskets —— 门廊下吊着的三只花篮。
//
// 铁钩与细链吊着藤编花篮，篮边垂下的藤蔓随风轻摆，篮里开着几朵小花。
// 参考此前调研的吊篮/垂藤结构。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class Node3D;
class HangingBaskets : public Node3D {
	GDCLASS(HangingBaskets, Node3D)
	static const int ITEM_TYPES = 5;
	Node3D *vines[3] = {};
	float t = 0.0f;
	float sway = 0.0f;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
	float get_sway() const { return sway; }
};
} // namespace godot
