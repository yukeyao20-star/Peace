#pragma once

// BeanTrellis —— 菜园里的豆角架：三座竹竿三脚架，藤蔓爬上去，开小白花、结豆荚，
// 白天有小蜂绕花，风一来整架轻轻摆。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class MeshInstance3D;
class SkyCycle;
class BeanTrellis : public Node3D {
	GDCLASS(BeanTrellis, Node3D)
	static const int TEEPEE_COUNT = 3;
	Node3D *frames[TEEPEE_COUNT] = {};
	Node3D *bees[2] = {};
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_teepee_count() const { return TEEPEE_COUNT; }
	bool get_bees_visible() const { return bees[0] != nullptr && bees[0]->is_visible(); }
};
} // namespace godot
