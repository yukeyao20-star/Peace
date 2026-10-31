#pragma once

// GardenWindmill —— 花园尽头的小风车。
//
// 木塔顶着一圈风叶，雨天转得快，雪天慢慢停；塔顶会积一层雪。
// 参考此前调研的风车/风车塔结构。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class MeshInstance3D;
class Node3D;
class SkyCycle;
class GardenWindmill : public Node3D {
	GDCLASS(GardenWindmill, Node3D)
	static const int ITEM_TYPES = 5;
	Node3D *blades = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	SkyCycle *sky = nullptr;
	float spin = 1.0f;
	bool snowed = false;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
	float get_spin() const { return spin; }
	bool get_snowed() const { return snowed; }
};
} // namespace godot
