#pragma once

// FishDryingRack —— 湖边晒鱼架：两根交叉木架挑着麻绳，四条鱼干随风轻摆；
// 下雨会拉起油布，海鸥在架子上空绕圈。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class MeshInstance3D;
class Node3D;
class SkyCycle;
class FishDryingRack : public Interactable {
	GDCLASS(FishDryingRack, Interactable)
	static const int FISH_COUNT = 4;
	Node3D *fish[FISH_COUNT] = {};
	Node3D *gull = nullptr;
	MeshInstance3D *cover = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	int flips = 0;
	bool covered = false;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_fish_count() const { return FISH_COUNT; }
	int get_flips() const { return flips; }
	bool is_covered() const { return covered; }
	bool is_gull_visible() const;
};
} // namespace godot
