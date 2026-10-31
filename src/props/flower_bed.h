#pragma once

// FlowerBed —— 木屋前的小花圃。
//
// 木板围出的一小块土，十二朵郁金香按三排种着；清晨花瓣挂露，白天两只蜂
// 来回，晚上花头合拢。雪天整片盖上薄雪。
//
// 参考：
//   * nature-sim flower_multimesh.gd —— 花丛用 MultiMesh 批量渲染；
//   * pixor.art「花丛」/「花坛」CC0 —— 红/白/蓝/黄低模花卉与花坛边框；
//   * dreadpon/godot_spatial_gardener —— 规则网格 + 随机旋转/缩放的种植方式。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class Node3D;
class SkyCycle;

class FlowerBed : public Node3D {
	GDCLASS(FlowerBed, Node3D)

	static const int FLOWER_COUNT = 12;
	static const int DEW_COUNT = 6;
	static const int BEE_COUNT = 2;

	Node3D *heads[FLOWER_COUNT] = {};
	MeshInstance3D *dewdrops[DEW_COUNT] = {};
	Node3D *bees[BEE_COUNT] = {};
	MeshInstance3D *wings[BEE_COUNT][2] = {};
	MeshInstance3D *snow_cover = nullptr;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float dew = 0.0f;
	float open = 1.0f;
	int dew_visible = 0;
	int bees_visible = 0;
	bool snow_covered = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_flower_count() const { return FLOWER_COUNT; }
	int get_dew_visible() const { return dew_visible; }
	int get_bees_visible() const { return bees_visible; }
	bool get_snow_covered() const { return snow_covered; }
	float get_open() const { return open; }
};

} // namespace godot
