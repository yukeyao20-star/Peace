#pragma once

// DockTrinkets —— 码头边的零碎。
//
// 盘好的缆绳、橙白救生圈、一盏油灯、一只木箱和一只蟹笼。天黑后油灯亮起，
// 小小一团暖光落在木板上。
//
// 参考：
//   * CGTrader「Stylized Fishing Village Docks Pack」—— 缆绳/油灯/救生圈组合；
//   * 「Rowboat and Dock Pack」—— 黄铜船灯与缆绳比例；
//   * OpenGameArt「Modular Wooden Docks」—— 码头木板与小物摆放。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class SkyCycle;

class DockTrinkets : public Node3D {
	GDCLASS(DockTrinkets, Node3D)

	static const int ITEM_TYPES = 5;

	MeshInstance3D *flame = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	bool lamp_on = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_item_types() const { return ITEM_TYPES; }
	bool get_lamp_on() const { return lamp_on; }
};

} // namespace godot
