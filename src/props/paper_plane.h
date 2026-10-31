#pragma once

// PaperPlane —— 门廊边的一叠便签纸，按 E 折一架纸飞机扔出去。
//
// 纸飞机有轻微升力，会顺着风滑一段再落地；落地后安静地躺一会儿，
// 之后可以再折一架。
//
// 参考：
//   * NikShaw/PaperPlaneGlider —— 多个阻力点计算风力（本作只做简化的
//     滑翔速度模型）；
//   * addmix/godot_aerodynamic_physics 的 Glider example —— 重心/升力中心
//     的摆放思路；
//   * Cerulean player_glide.gd —— 用速度长度修正与转向力做滑翔。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

class Player;

namespace godot {

class MeshInstance3D;
class Node3D;
class SkyCycle;

class PaperPlane : public Interactable {
	GDCLASS(PaperPlane, Interactable)

	Node3D *plane = nullptr;
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;

	Vector3 vel;
	float rest_t = 0.0f;
	float flight_t = 0.0f;
	bool active = false;
	int flights = 0;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.0f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_flights() const { return flights; }
	bool get_active() const { return active; }
	Vector3 get_plane_pos() const;
};

} // namespace godot
