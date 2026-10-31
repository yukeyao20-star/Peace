#pragma once

// CampGear —— 营地边的露营零碎。
//
// 一段当凳子的圆木、放在木头上的搪瓷杯、一盏铁皮提灯、摊开的折地图和
// 一只小指南针。天黑后提灯亮起，火苗轻轻跳。
//
// 参考：
//   * CGTrader「Wayfarers Low-Poly Campsite Pack」—— 铁艺提灯与地图指示牌；
//   * edwardgreen.dev「Camping prototype」（Godot 4）—— 露营场景道具组织；
//   * gamedevacademy「Lighting in Godot」—— 提灯 + OmniLight/发光罩做法。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class SkyCycle;

class CampGear : public Node3D {
	GDCLASS(CampGear, Node3D)

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
