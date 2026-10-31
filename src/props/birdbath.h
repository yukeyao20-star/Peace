#pragma once

// Birdbath —— 花圃边的石鸟浴盆。
//
// 石座上一只浅水盆，水面会慢慢蒸发；按 E 可以重新添水。白天小鸟落到盆沿，
// 低头啜水、扑棱翅膀，水面泛起细圈；人一靠近就飞走。雪天水面结一层薄冰。
//
// 参考：
//   * godotshaders「Water Ripples」—— 以波源位置为中心的程序化环状涟漪；
//   * Godot compute texture demo —— 高度场涟漪思路（本作只用两圈简单环）；
//   * CSDN《Godot4.3 小鸡 NPC》—— 落地-惊飞的计时状态机。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class SkyCycle;

class Birdbath : public Interactable {
	GDCLASS(Birdbath, Interactable)

	static const int BIRD_COUNT = 2;

	Node3D *birds[BIRD_COUNT] = {};
	MeshInstance3D *wings[BIRD_COUNT][2] = {};
	MeshInstance3D *water = nullptr;
	MeshInstance3D *ice = nullptr;
	MeshInstance3D *ripples[2] = {};
	Ref<class StandardMaterial3D> ripple_mats[2];
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;
	AudioStreamPlayer3D *splash_voice = nullptr;
	Ref<AudioStreamWAV> snd_splash;

	float t = 0.0f;
	float water_level = 1.0f;
	float ripple_age[2] = { 1.0f, 1.0f };
	float bird_timer = 3.0f;
	int ripple_next = 0;
	int refills = 0;
	int birds_visible = 0;
	bool ice_on = false;
	bool bird_on_ground[BIRD_COUNT] = { true, true };

	void build();
	void build_splash();
	void spawn_ripple();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	float get_water_level() const { return water_level; }
	int get_birds_visible() const { return birds_visible; }
	int get_refills() const { return refills; }
	bool get_ice_on() const { return ice_on; }
};

} // namespace godot
