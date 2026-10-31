#pragma once

// GardenGnome —— 草地上的小地精。
//
// 红尖帽、白胡子、旧靴子，手里提着一盏小灯；天黑后灯会亮。按 E 跟他打个招呼。
// 参考此前调研的花园小雕像比例。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"

namespace godot {
class MeshInstance3D;
class SkyCycle;
class GardenGnome : public Interactable {
	GDCLASS(GardenGnome, Interactable)
	MeshInstance3D *lamp = nullptr;
	SkyCycle *sky = nullptr;
	int greets = 0;
	bool lamp_on = false;
	float t = 0.0f;
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
	int get_greets() const { return greets; }
	bool get_lamp_on() const { return lamp_on; }
};
} // namespace godot
