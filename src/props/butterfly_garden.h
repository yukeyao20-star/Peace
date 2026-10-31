#pragma once

// ButterflyGarden —— 花圃边的三只蝴蝶。
//
// 它们绕着花丛飞，偶尔停在一朵花上合起翅膀；人一靠近就飞走。
// 参考此前调研的蝴蝶/花卉行为。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
namespace godot {
class MeshInstance3D;
class Node3D;
class ButterflyGarden : public Node3D {
	GDCLASS(ButterflyGarden, Node3D)
	static const int COUNT = 3;
	Node3D *butterflies[COUNT] = {};
	MeshInstance3D *wings[COUNT][2] = {};
	Vector3 pos[COUNT];
	Vector3 target[COUNT];
	float state_t[COUNT] = {};
	bool landed[COUNT] = {};
	Node3D *player = nullptr;
	float t = 0.0f;
	int visible_count = 0;
	int landed_count = 0;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_visible_count() const { return visible_count; }
	int get_landed_count() const { return landed_count; }
	int get_count() const { return COUNT; }
};
} // namespace godot
