#pragma once

// JarLights —— 花园小径边的三只玻璃罐灯。
//
// 木桩上放着玻璃罐，里面绕着几颗小灯珠；天黑后亮起暖光，风里轻轻晃。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class MeshInstance3D;
class OmniLight3D;
class SkyCycle;
class JarLights : public Node3D {
	GDCLASS(JarLights, Node3D)
	static const int COUNT = 3;
	Node3D *jars[COUNT] = {};
	MeshInstance3D *bulbs[COUNT][2] = {};
	OmniLight3D *lamps[COUNT] = {};
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	bool lit = false;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_count() const { return COUNT; }
	bool get_lit() const { return lit; }
};
} // namespace godot
