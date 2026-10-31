#pragma once

// RoseArch —— 花园入口的月季拱门。
//
// 木拱上爬满月季，红粉花朵点缀其间；风过时花瓣偶尔飘落。
// 参考此前调研的藤蔓/花架结构。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class CPUParticles3D;
class Node3D;
class RoseArch : public Node3D {
	GDCLASS(RoseArch, Node3D)
	static const int ITEM_TYPES = 5;
	Node3D *arch = nullptr;
	CPUParticles3D *petals = nullptr;
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
