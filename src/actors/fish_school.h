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
	struct Fish {
		Node3D *root = nullptr;
		Vector2 center = Vector2(-6.0f, 26.0f);
		Node3D *tail = nullptr;
		float angle = 0.0f;
		float radius = 6.0f;
		float speed = 0.08f;
		float depth = 1.6f; // 水面以下的米数
		float bob_w = 1.2f;
		float bob_phase = 0.0f;
		float yaw_offset = 0.0f;
		float urge = 0.0f; // 被玩家靠近时的惊逃强度
	};

	int fish_count = 16;
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;
	float anim_t = 0.0f;

	std::vector<Fish> fish;
	Node3D *player = nullptr;

	void build();
	void spawn_fish(int p_index, unsigned p_seed);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
