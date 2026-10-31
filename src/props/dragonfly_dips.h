#pragma once

// DragonflyDips —— 湖面上的三只蜻蜓。
//
// 它们悬停、急停，偶尔用尾尖点一下水面，激起一圈小涟漪。白天天晴时出现。
// 参考此前调研的蜻蜓/点水做法。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
namespace godot {
class MeshInstance3D;
class Node3D;
class SkyCycle;
class TerrainBuilder;
class DragonflyDips : public Node3D {
	GDCLASS(DragonflyDips, Node3D)
	static const int COUNT = 3;
	Node3D *flies[COUNT] = {};
	MeshInstance3D *wings[COUNT][2] = {};
	MeshInstance3D *ripples[COUNT] = {};
	float phase[COUNT] = { 0.0f, 2.1f, 4.2f };
	float ripple_age[COUNT] = { 1.0f, 1.0f, 1.0f };
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;
	TerrainBuilder *terrain = nullptr;
	float t = 0.0f;
	int visible_count = 0;
	int dips = 0;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_visible_count() const { return visible_count; }
	int get_dips() const { return dips; }
};
} // namespace godot
