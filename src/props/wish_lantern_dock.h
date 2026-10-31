#pragma once

// WishLanternDock —— 码头尽头的一小摞河灯。
//
// 靠近按 E 放一盏：纸灯落到水面，点起暖光，慢慢向湖心漂走；
// 夜里尤其好看。每次放完要等几秒才能再放，最多同时漂四盏。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

#include "../interact/interactable.h"

namespace godot {

class MeshInstance3D;
class OmniLight3D;
class SkyCycle;
class StandardMaterial3D;
class TerrainBuilder;

class WishLanternDock : public Interactable {
	GDCLASS(WishLanternDock, Interactable)

private:
	struct Released {
		Node3D *root = nullptr;
		MeshInstance3D *paper = nullptr;
		OmniLight3D *light = nullptr;
		Ref<StandardMaterial3D> mat;
		Vector3 dir;
		float life = 0.0f;
		float phase = 0.0f;
	};

	TerrainBuilder *terrain = nullptr;
	SkyCycle *sky = nullptr;
	float water_level = -0.55f;
	float t = 0.0f;
	float cooldown = 0.0f;
	std::vector<Released> released;

	void build();
	void release();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_active_count() const;
	float get_light_energy() const;
	Vector3 get_released_pos(int p_index) const;
};

} // namespace godot
