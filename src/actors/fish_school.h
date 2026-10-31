#pragma once

// FishSchool —— 湖里的小鱼群（水下氛围生物）。
//
// 湖心周围 16 条低模小鱼（桥下水湾 + 西北主湖两群），各自不同半径/深度/角速度绕圈游动：
//   * 身体胶囊 + 尾鳍圆锥，尾鳍左右摆动；朝向沿切线；
//   * 只在玩家靠近湖边（<60m）时显示，远处整组隐藏省 draw call；
//   * 完全程序化，无外部资源。
// 水面海拔 -0.55 与 terrain_builder.h water_level 联动。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

class FishSchool : public Node3D {
	GDCLASS(FishSchool, Node3D)

private:
	// boids 鱼：位置/速度在 XZ 平面，深度单独处理。
	// 规则权重参考 sergioabreu-g/boids 的 Flock/Boid 结构，
	// 同种聚拢与异种避让参考 liambeckingham/godot-boids 的鱼群版本。
	struct Fish {
		Node3D *root = nullptr;
		Node3D *tail = nullptr;
		Vector2 center = Vector2(-6.0f, 26.0f);
		Vector2 pos;
		Vector2 vel;
		float depth = 1.6f; // 水面以下的米数
		float bob_w = 1.2f;
		float bob_phase = 0.0f;
		float yaw_offset = 0.0f;
		int species = 0;
	};

	int fish_count = 16;
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;
	float anim_t = 0.0f;

	std::vector<Fish> fish;
	Node3D *player = nullptr;
	class SkyCycle *sky = nullptr;
	float ice_mix = 0.0f; // 湖面封冻程度（0..1）：冰下鱼群上浮、游得更慢
	class TerrainBuilder *terrain = nullptr;
	std::vector<std::vector<int>> neighbors;
	int neighbor_checks = 0;

	void build();
	void spawn_fish(int p_index, unsigned p_seed);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	Vector2 get_fish_position(int p_index) const;
	int get_neighbor_checks() const { return neighbor_checks; }
	int get_school_size() const { return (int)fish.size(); }
};

} // namespace godot
