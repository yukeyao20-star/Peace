#pragma once

// Hammock —— 两棵树桩之间的吊床。
//
// 靠近按 E 躺进去，网面会轻轻起伏；再按 E 下来。躺下时视角放低，
// 玩家模型放平，像真的在午后打个盹。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

class Player;

namespace godot {

class Node3D;
class TerrainBuilder;

class Hammock : public Interactable {
	GDCLASS(Hammock, Interactable)

private:
	Player *rider = nullptr;
	Node3D *fabric = nullptr;
	TerrainBuilder *terrain = nullptr;
	bool e_prev = false;
	float t = 0.0f;

	void build();
	void stand_up();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool is_ridden() const { return rider != nullptr; }
	Vector3 get_fabric_pos() const;
};

} // namespace godot
