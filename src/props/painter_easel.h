#pragma once

// PainterEasel —— 湖东草坡上的画架：三脚木架撑着一幅未干的湖景小画，
// 脚边是颜料盒和斜靠的画笔；按 E 看看画。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class Node3D;
class MeshInstance3D;
class SkyCycle;
class PainterEasel : public Interactable {
	GDCLASS(PainterEasel, Interactable)
	Node3D *canvas = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	int looks = 0;
	void build();
	void update_look();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_looks() const { return looks; }
	bool is_snowy() const;
};
} // namespace godot
