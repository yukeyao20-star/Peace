#pragma once

// PathTrinkets —— 石径边散落的小玩意。
//
// 三颗涂了颜色的小石子、一枚贝壳、一根羽毛、一只挂在细枝上的小铜铃，
// 还有一艘拇指大的小帆船。风吹过时铜铃和羽毛会轻轻动。
//
// 参考：
//   * OpenGameArt「more handpainted rocks」—— 手绘小石子的配色；
//   * Pixor 低模贝壳/羽毛道具 —— 小物件比例；
//   * 常见风铃结构 —— 细枝 + 线 + 小铜铃。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Node3D;

class PathTrinkets : public Node3D {
	GDCLASS(PathTrinkets, Node3D)

	static const int ITEM_TYPES = 5;

	Node3D *bell = nullptr;
	Node3D *feather = nullptr;
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
