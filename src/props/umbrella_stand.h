#pragma once

// UmbrellaStand —— 木屋门边的伞架。
//
// 门口一只旧木桶改的伞架，插着两把收拢的伞（红、蓝）；雨天伞尖会滴水，
// 雪天桶沿积一小圈雪。没有交互，只是让门口更像有人住。
//
// 参考：
//   * pixor.art「伞架」低模（740 三角面）—— 伞架/伞柄的比例；
//   * godotshaders「Raindrops glass」与 Godot 计算纹理 Demo —— 雨滴/水滴
//     的粒子做法；本作只用最简单的下落小水滴；
//   * godot-demo-projects compute/texture —— 滴水落地的涟漪思路。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CPUParticles3D;
class MeshInstance3D;
class Node3D;
class SkyCycle;

class UmbrellaStand : public Node3D {
	GDCLASS(UmbrellaStand, Node3D)

	static const int UMBRELLA_COUNT = 2;

	Node3D *umbrellas[UMBRELLA_COUNT] = {};
	CPUParticles3D *drips = nullptr;
	MeshInstance3D *snow_ring = nullptr;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float wet = 0.0f;
	float snow_lvl = 0.0f;
	bool drips_on = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_umbrella_count() const { return UMBRELLA_COUNT; }
	bool get_drips_on() const { return drips_on; }
	float get_wet_level() const { return wet; }
	float get_snow_level() const { return snow_lvl; }
};

} // namespace godot
