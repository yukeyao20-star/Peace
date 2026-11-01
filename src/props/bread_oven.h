#pragma once

// BreadOven —— 石砌面包炉：圆顶炉膛、矮烟囱和一把木铲；按 E 生火烤面包，
// 炉口先亮起橘光，几分钟后炉门里飘出麦香。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"
namespace godot {
class CPUParticles3D;
class MeshInstance3D;
class Node3D;
class OmniLight3D;
class SkyCycle;
class BreadOven : public Interactable {
	GDCLASS(BreadOven, Interactable)
	Node3D *bread = nullptr;
	MeshInstance3D *snow_cap = nullptr;
	OmniLight3D *fire = nullptr;
	CPUParticles3D *smoke = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	float bake_t = 0.0f;
	bool baking = false;
	int bakes = 0;
	void build();
	void build_smoke();
	void finish_bake();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.3f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	bool is_baking() const { return baking; }
	int get_bakes() const { return bakes; }
	bool has_bread() const { return bread != nullptr && bread->is_visible(); }
};
} // namespace godot
