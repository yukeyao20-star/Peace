#pragma once

// TabletopGames —— 棋桌旁的桌面小游戏：多米诺、纸牌、骰盅、木盒拼图、陀螺。
// 参考此前调研的低模桌面道具比例。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class TabletopGames : public Node3D {
	GDCLASS(TabletopGames, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};
} // namespace godot
