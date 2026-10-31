#pragma once

// WindowSill —— 木屋窗台上的小陈设。
//
// 三只陶盆里种着香草，一只玻璃罐装着海边捡来的碎玻璃，旁边一截蜡烛
// 慢慢烧着。夜里烛火在窗玻璃上映出一点暖色。
//
// 参考：
//   * pixor.art「盆栽」CC0 —— 陶盆与叶簇比例；
//   * Habr「窗台小花园」Godot 4 教程 —— 多个陶盆并排的窗台布置；
//   * Flockbay / Kenney「Potted Plant」CC0 —— 低模盆栽尺寸。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;

class WindowSill : public Node3D {
	GDCLASS(WindowSill, Node3D)

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
