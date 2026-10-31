#pragma once

// PorchLights —— 门廊檐下的一串小灯：白天是暗玻璃珠，入夜亮成暖黄，
// 风里轻轻晃，按 E 可以开关。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class MultiMesh;
class MultiMeshInstance3D;
class OmniLight3D;
class SkyCycle;
class StandardMaterial3D;
class PorchLights : public Interactable {
	GDCLASS(PorchLights, Interactable)
	static const int BULB_COUNT = 16;
	Ref<MultiMesh> bulbs;
	MultiMeshInstance3D *bulb_mmi = nullptr;
	OmniLight3D *lamps[2] = {};
	Ref<StandardMaterial3D> bulb_mat;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	bool enabled = true;
	bool lit = false;
	Vector3 bulb_pos[BULB_COUNT];
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	bool is_lit() const { return lit; }
	bool is_enabled() const { return enabled; }
	int get_bulb_count() const { return BULB_COUNT; }
};
} // namespace godot
