#pragma once

// PaperBoat —— 湖边折一只小纸船放进水里，它会顺着水波慢慢漂远。
// 按 E 折一只新的；旧的那只漂一会儿就沉了。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class MeshInstance3D;
class Node3D;
class TerrainBuilder;
class PaperBoat : public Interactable {
	GDCLASS(PaperBoat, Interactable)
	Node3D *boat = nullptr;
	MeshInstance3D *ripple = nullptr;
	TerrainBuilder *terrain = nullptr;
	Vector3 origin;
	Vector3 dir;
	float drift = 0.0f;
	float life = 0.0f;
	bool active = false;
	int launches = 0;
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
	bool get_active() const { return active; }
	int get_launches() const { return launches; }
};
} // namespace godot
