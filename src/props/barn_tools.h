#pragma once

// BarnTools —— 谷仓墙角的一堆农具。
//
// 一只木水桶、一张矮木凳、一只铁皮奶罐、一袋饲料和一只旧马蹄铁。
//
// 参考：
//   * pixor.art「牛奶罐」CC0（536 三角面）—— 高身奶罐与把手；
//   * Kenney「Bucket」CC0 —— 木桶比例；
//   * Cozy Farm Asset Pack（Godot Asset Store）—— 谷仓工具的搭配。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class BarnTools : public Node3D {
	GDCLASS(BarnTools, Node3D)

	static const int ITEM_TYPES = 5;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_item_types() const { return ITEM_TYPES; }
};

} // namespace godot
