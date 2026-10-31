#pragma once

// PlantStand —— 门廊边的小花架。
//
// 三层木架，摆着蕨类、垂藤、开花的陶盆和一排多肉。垂下来的藤会随风轻轻摆。
//
// 参考：
//   * CGTrader「Wooden Planter Box Pack」里的 Tiered Pyramid Plant Stand；
//   * pixor.art / Kenney 低模盆栽（花盆 + 叶簇）；
//   * Cozy Low-Poly Plants & Pots —— 陶盆与叶片的配色。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Node3D;

class PlantStand : public Node3D {
	GDCLASS(PlantStand, Node3D)

	static const int ITEM_TYPES = 5;

	Node3D *vine = nullptr;
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
