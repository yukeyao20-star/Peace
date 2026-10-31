#pragma once

// DragonflySwarm —— 白天湖岸的蜻蜓群（氛围小生物）。
//
// 6 只低模蜻蜓在湖岸浅水/芦苇上方悬停、突进、急停：
//   * 每只在一个"家"附近随机取点，停顿 0.6~1.6s 后快速飞向新点；
//   * 身体 + 四片半透明薄翅，翅膀高频扑动；
//   * 只在白天且无雨雪时出现，玩家 70m 外整组隐藏。
// 完全程序化，无外部资源。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

class DragonflySwarm : public Node3D {
	GDCLASS(DragonflySwarm, Node3D)

private:
	struct Dragonfly {
		Node3D *root = nullptr;
		Node3D *wing_l = nullptr;
		Node3D *wing_r = nullptr;
		Vector3 home;
		Vector3 pos;
		Vector3 target;
		float retarget_t = 0.0f;
		float speed = 2.8f;
		float wing_phase = 0.0f;
	};

	int count = 6;
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;
	float anim_t = 0.0f;
	std::vector<Dragonfly> bugs;
	Node *sky_cycle = nullptr;
	Node3D *player = nullptr;

	void build();
	void spawn_dragonfly(int p_index, unsigned p_seed);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
