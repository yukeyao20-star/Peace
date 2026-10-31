#pragma once

// TurtleLog —— 荷塘里的一根浮木和晒太阳的小乌龟。
//
// 白天乌龟趴在木头上；人一靠近就"扑通"滑进水里，过一会儿再浮上来。
// 参考此前调研的水边小动物做法。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class Node3D;
class SkyCycle;
class TerrainBuilder;
class TurtleLog : public Node3D {
	GDCLASS(TurtleLog, Node3D)
	Node3D *turtle = nullptr;
	SkyCycle *sky = nullptr;
	TerrainBuilder *terrain = nullptr;
	Node3D *player = nullptr;
	float t = 0.0f;
	float resurface_t = 0.0f;
	bool in_water = false;
	int plops = 0;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	bool get_in_water() const { return in_water; }
	int get_plops() const { return plops; }
};
} // namespace godot
