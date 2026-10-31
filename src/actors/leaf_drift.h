#pragma once

// LeafDrift —— 秋日落叶。
//
// 玩家周围一直飘着慢慢打转的叶子（雪天停），路边堆着几小堆干叶；
// 快步走过叶堆会踢起一小蓬叶子，带一声干脆的沙沙响；雨天叶子颜色变深，
// 雪天叶堆被雪盖住。
//
// 参考：
//   * godotshaders「Wind Driven Falling Particles (leaves, petals, feathers)」——
//     风向/风速随阵风变化、叶片三轴自转、阻尼；
//   * godotshaders「Modular Environmental Particles」—— 落叶边飘边转、
//     落地前减速淡出；
//   * godotshaders「Falling leaf shader」—— 用随机值与矩阵旋转叶片网格；
//   * Go3DGoGodot/Godot3D_Waving_tree_and_falling_leaves —— 3D 落叶的
//     发射范围与密度参考；
//   * dante-signal31/SakuraDemo —— 粒子 scale/自转随机、preprocess 预热的做法。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class Mesh;
class MultiMesh;
class MultiMeshInstance3D;
class StandardMaterial3D;
class SkyCycle;

class LeafDrift : public Node3D {
	GDCLASS(LeafDrift, Node3D)

	static const int PILE_COUNT = 3;
	static const int PILE_LEAVES = 22;

	CPUParticles3D *fall_fx = nullptr;
	CPUParticles3D *kick_fx = nullptr;
	MultiMeshInstance3D *pile_mm = nullptr;
	Ref<MultiMesh> pile_mesh;
	Ref<Mesh> leaf_mesh;
	Ref<StandardMaterial3D> leaf_mat;
	Ref<StandardMaterial3D> pile_mat;
	AudioStreamPlayer3D *rustle = nullptr;
	Ref<AudioStreamWAV> snd_rustle;

	Vector3 pile_pos[PILE_COUNT];
	float kick_cool[PILE_COUNT] = {};
	int kick_count = 0;
	float t = 0.0f;
	float wet = 0.0f;
	float last_wet_draw = -1.0f;
	float snow_sm = 0.0f;
	bool piles_hidden = false;

	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;

	void build();
	void build_leaf_mesh();
	Ref<AudioStreamWAV> make_crunch();
	void kick_pile(int p_index);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_leaf_amount() const;
	int get_pile_count() const { return PILE_COUNT; }
	bool get_emitting() const;
	bool get_piles_hidden() const { return piles_hidden; }
	float get_wet_level() const { return wet; }
	float get_snow_cover() const { return snow_sm; }
	int get_kick_count() const { return kick_count; }
};

} // namespace godot
