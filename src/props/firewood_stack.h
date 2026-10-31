#pragma once

// FirewoodStack —— 木屋东墙下的柴堆：三层原木、劈柴墩与斧头、引火细枝篮，雪天顶上落一层白。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class MeshInstance3D;
class SkyCycle;
class FirewoodStack : public Node3D {
	GDCLASS(FirewoodStack, Node3D)
	static const int LOG_COUNT = 24;
	MeshInstance3D *snow_cap = nullptr;
	SkyCycle *sky = nullptr;
	bool snow_covered = false;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_log_count() const { return LOG_COUNT; }
	bool is_axe_embedded() const { return true; }
	bool is_snow_covered() const { return snow_covered; }
};
} // namespace godot
