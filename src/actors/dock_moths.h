#pragma once

// DockMoths —— 夜里绕着码头灯飞的蛾子。
//
// 6 只小飞蛾围着码头灯打转：各自不同半径/角速度、上下轻飘，
// 翅膀高频扑动。只在夜间且无雨雪时出现；玩家 45m 外整组隐藏。
// 灯的位置从世界里的 Dock/DockLantern 读取，找不到时退回固定坐标。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

class DockMoths : public Node3D {
	GDCLASS(DockMoths, Node3D)

private:
	struct Moth {
		Node3D *root = nullptr;
		Node3D *wing_l = nullptr;
		Node3D *wing_r = nullptr;
		float angle = 0.0f;
		float radius = 0.4f;
		float speed = 1.0f;
		float bob_w = 1.6f;
		float bob_phase = 0.0f;
		float wing_phase = 0.0f;
	};

	int count = 6;
	Vector3 lamp_pos = Vector3(-6.68f, 1.46f, -6.95f);
	float anim_t = 0.0f;
	std::vector<Moth> moths;
	Node *sky_cycle = nullptr;
	Node3D *player = nullptr;

	void build();
	void spawn_moth(int p_index, unsigned p_seed);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
