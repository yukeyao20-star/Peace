#pragma once

// Sundial —— 花园里的石日晷：青铜晷针的影子随日头慢慢挪，按 E 能读出园子里的时辰。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class MeshInstance3D;
class Node3D;
class SkyCycle;
class Sundial : public Interactable {
	GDCLASS(Sundial, Interactable)
	Node3D *shadow = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	SkyCycle *sky = nullptr;
	float tod = 0.0f;
	int reads = 0;
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
	float get_hour() const { return tod * 24.0f; }
	bool is_daytime() const { return tod > 0.24f && tod < 0.76f; }
	int get_reads() const { return reads; }
};
} // namespace godot
