#pragma once

// HerbBundles —— 门廊檐下倒挂的香草束、蒜辫和一串红辣椒，风里慢慢打转。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class MeshInstance3D;
class SkyCycle;
class HerbBundles : public Node3D {
	GDCLASS(HerbBundles, Node3D)
	static const int HANGING = 4;
	Node3D *hanging[HANGING] = {};
	MeshInstance3D *snow_caps[HANGING] = {};
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_bundle_count() const { return HANGING; }
	bool is_snowy() const;
};
} // namespace godot
