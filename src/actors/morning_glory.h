#pragma once

// MorningGlory —— 爬在前院篱笆上的牵牛花。
//
// 清晨开花、午后合拢、夜里垂下头；花瓣上带着露珠，白天有两只小蜂来转。
// 下雨时花会提前闭合，雪天整片藤蔓安静下来。
//
// 参考：
//   * Reddit r/godot「Overgrowth taking over building」—— 先在表面铺藤蔓
//     骨架再烘焙成 MultiMesh 茎/叶，本作用同样的"骨架 + 实例化"思路；
//   * godot-visual-effects vines_3d_demo —— 用 growth 参数驱动藤蔓生长；
//   * My Little Pollinator —— Line2D 逐帧追加点、沿路径铺藤蔓的轻量做法；
//   * The_Dark_Side_of_Earth/vine.gd —— 藤蔓末端生成花苞的结构参考。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class MultiMesh;
class MultiMeshInstance3D;
class StandardMaterial3D;
class ShaderMaterial;
class SkyCycle;

class MorningGlory : public Node3D {
	GDCLASS(MorningGlory, Node3D)

	static const int VINE_COUNT = 5;
	static const int FLOWER_COUNT = 6;
	static const int BEE_COUNT = 2;

	Node3D *flowers[FLOWER_COUNT] = {};
	Node3D *bees[BEE_COUNT] = {};
	Ref<ShaderMaterial> flower_mats[FLOWER_COUNT];
	Ref<ShaderMaterial> leaf_mat;
	MultiMeshInstance3D *stem_mm = nullptr;
	MultiMeshInstance3D *leaf_mm = nullptr;
	Ref<MultiMesh> stem_multimesh;
	Ref<MultiMesh> leaf_multimesh;
	MeshInstance3D *dewdrops[FLOWER_COUNT] = {};
	Ref<StandardMaterial3D> dew_mat;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float flower_open = 0.0f;
	float dew = 0.0f;
	int open_count = 0;
	int dew_visible = 0;
	int bees_visible = 0;
	float sway_amp = 0.05f;
	float phase[FLOWER_COUNT] = {};

	void build();
	float open_target() const;
	void update_bees(float delta, bool visible);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_flower_count() const { return FLOWER_COUNT; }
	int get_open_flowers() const { return open_count; }
	int get_dew_visible() const { return dew_visible; }
	int get_bees_visible() const { return bees_visible; }
	float get_flower_open() const { return flower_open; }
	float get_sway_amp() const { return sway_amp; }
};

} // namespace godot
