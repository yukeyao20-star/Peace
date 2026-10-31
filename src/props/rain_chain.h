#pragma once

// RainChain —— 门廊屋檐下的雨链（鎖樋 / kusari-doi）。
//
// 晴天只是一串铜色小杯；下雨时水珠顺着杯沿一级级滑下，落进下面的石钵，
// 泛起一圈圈涟漪，同时响起细碎的流水声。雪天会挂上一层白霜，水珠停走。
//
// 参考：
//   * Rain chain（Wikipedia / KCP International）—— 一串带孔金属杯，
//     雨水沿杯沿层层下滑，替代落水管；
//   * Ralane/godot4-catenary —— 装饰性链条/绳索的悬链线与风摆思路（MIT）；
//   * Tshmofen/verlet-rope-4 —— Godot 4 的 Verlet 绳索（本作用更轻的
//     单摆轻摆，链条短且绷直，不需要逐节点积分）；
//   * godot-demo-projects compute/texture —— 雨滴落在水面产生涟漪高度场。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class StandardMaterial3D;

class RainChain : public Node3D {
	GDCLASS(RainChain, Node3D)

	static const int CUP_COUNT = 10;
	static const int BEAD_COUNT = 8;
	static const int RIPPLE_COUNT = 3;

	Node3D *chain_root = nullptr;
	MeshInstance3D *cups[CUP_COUNT] = {};
	MeshInstance3D *links[CUP_COUNT] = {};
	MeshInstance3D *beads[BEAD_COUNT] = {};
	MeshInstance3D *frost_caps[CUP_COUNT] = {};
	MeshInstance3D *ripples[RIPPLE_COUNT] = {};

	Ref<StandardMaterial3D> bead_mat;
	Ref<StandardMaterial3D> copper_mat;
	Ref<StandardMaterial3D> frost_mat;
	Ref<StandardMaterial3D> ripple_mats[RIPPLE_COUNT];
	Ref<StandardMaterial3D> basin_water_mat;

	AudioStreamPlayer3D *trickle = nullptr;
	Ref<AudioStreamWAV> snd_trickle;

	float t = 0.0f;
	float wet = 0.0f;
	float frost_lvl = 0.0f;
	float sway_amp = 0.0f;
	float bead_phase[BEAD_COUNT] = {};
	float ripple_age[RIPPLE_COUNT] = {};
	int ripple_next = 0;
	int bead_visible = 0;
	int active_ripples = 0;

	void build();
	Ref<AudioStreamWAV> make_trickle();
	void spawn_ripple();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_cup_count() const { return CUP_COUNT; }
	int get_bead_visible() const { return bead_visible; }
	float get_wet_level() const { return wet; }
	float get_frost_level() const { return frost_lvl; }
	float get_sway_amp() const { return sway_amp; }
	int get_active_ripples() const { return active_ripples; }
	bool get_trickle_playing() const;
};

} // namespace godot
