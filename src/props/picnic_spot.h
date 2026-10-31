#pragma once

// PicnicSpot —— 草地上的野餐垫。
//
// 红白格子毯、藤篮、面包与两只苹果；两只小蝴蝶在篮子边上绕，风来时
// 毯角的两块餐巾轻轻掀起。没有交互，只是湖边一个可以坐下来的地方。
//
// 参考：
//   * Tiny Treats「Pleasant Picnic」CC0 资产包（Godot 4.1）—— 格子毯、
//     藤篮、餐盘的配色与比例；
//   * godotengine.org/asset-library/asset/3433 —— 低多边形野餐套件的构成；
//   * gdquest-demos/godot-4-how-tos —— 程序化摆放小道具的常用做法。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class Node3D;

class PicnicSpot : public Node3D {
	GDCLASS(PicnicSpot, Node3D)

	static const int BUTTERFLY_COUNT = 2;

	Node3D *butterflies[BUTTERFLY_COUNT] = {};
	MeshInstance3D *wings[BUTTERFLY_COUNT][2] = {};
	Node3D *napkins[2] = {};
	MeshInstance3D *basket = nullptr;

	float t = 0.0f;
	int butterflies_visible = 0;
	int item_count = 0;
	float breeze_amp = 0.0f;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_butterflies_visible() const { return butterflies_visible; }
	int get_item_count() const { return item_count; }
	float get_breeze_amp() const { return breeze_amp; }
};

} // namespace godot
