#pragma once

// Telescope —— 山坡草地上的小望远镜。
//
// 三脚木架 + 黄铜镜筒；天黑了按 E 凑近看星星，会说出今晚看到的那片天区。
// 白天按 E 只会提醒你等天黑。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class CPUParticles3D;
class SkyCycle;
class Telescope : public Interactable {
	GDCLASS(Telescope, Interactable)
	CPUParticles3D *sparkles = nullptr;
	SkyCycle *sky = nullptr;
	int peeks = 0;
	bool night = false;
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
	int get_peeks() const { return peeks; }
	bool get_night() const { return night; }
};
} // namespace godot
