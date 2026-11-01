#pragma once

// Otter —— 湖湾里的小水獭：沿着水面游一圈，偶尔翻个身仰泳，再一头扎进水里。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class MeshInstance3D;
class Node3D;
class SkyCycle;
class TerrainBuilder;
class Otter : public Node3D {
	GDCLASS(Otter, Node3D)
	Node3D *body = nullptr;
	MeshInstance3D *ripple = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	float dive_t = 0.0f;
	float next_dive = 14.0f;
	float water_level = -0.55f;
	bool diving = false;
	bool on_back = false;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	bool is_diving() const { return diving; }
	bool is_on_back() const { return on_back; }
	bool is_visible_creature() const { return is_visible(); }
};
} // namespace godot
