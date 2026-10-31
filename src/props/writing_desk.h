#pragma once

// WritingDesk —— 木屋角落的写字台。
//
// 桌上有墨水瓶与羽毛笔、摊开的信纸、红蜡封、一本打开的书和一截跳动的
// 蜡烛。椅子轻轻歪着，像刚有人起身离开。
//
// 参考：
//   * CGTrader「Scribes Illuminated Manuscript Desk Pack」—— 羽毛笔/墨水瓶/
//     蜡封/羊皮纸的组合；
//   * CGTrader「Lamplit Stationery Shop Pack」—— 墨渍桌面与黄铜笔架；
//   * pixor.art「木桌」CC0 —— 桌腿与桌面比例。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;

class WritingDesk : public Node3D {
	GDCLASS(WritingDesk, Node3D)

	static const int ITEM_TYPES = 5;

	MeshInstance3D *flame = nullptr;
	float t = 0.0f;
	bool flame_on = true;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_item_types() const { return ITEM_TYPES; }
	bool get_flame_on() const { return flame_on; }
};

} // namespace godot
