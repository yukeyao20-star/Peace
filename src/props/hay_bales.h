#pragma once

// HayBales —— 农场边的干草捆。
//
// 三个圆草捆堆在田边，缠着麻绳；一把木叉斜插在旁边，两只小雀在草捆边
// 跳着啄食，人一靠近就飞开。下雪时草捆顶上一层薄雪，脚边飘着草屑。
//
// 参考：
//   * Pixor「干草捆」CC0 低模（108 三角面）—— 圆形草捆与麻绳的构成；
//   * NatureForge: Stylized Meadow & Farm Kit —— 圆形/方形草捆与草捆座椅；
//   * Synty POLYGON Farm Pack —— 草捆、木叉等农场小道具的比例参考。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class Node3D;
class SkyCycle;

class HayBales : public Node3D {
	GDCLASS(HayBales, Node3D)

	static const int BALE_COUNT = 3;
	static const int BIRD_COUNT = 2;

	Node3D *birds[BIRD_COUNT] = {};
	MeshInstance3D *snow_caps[BALE_COUNT] = {};
	MeshInstance3D *wings[BIRD_COUNT][2] = {};
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float bird_pos[BIRD_COUNT][2] = {};
	float bird_timer[BIRD_COUNT] = {};
	bool bird_flying[BIRD_COUNT] = {};
	int birds_visible = 0;
	bool snow_covered = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_bale_count() const { return BALE_COUNT; }
	int get_birds_visible() const { return birds_visible; }
	bool get_snow_covered() const { return snow_covered; }
};

} // namespace godot
