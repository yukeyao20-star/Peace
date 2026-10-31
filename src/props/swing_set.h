#pragma once

// SwingSet —— 长椅南边草地上的木秋千。
//
// 靠近按 E 坐上，秋千会自己慢慢荡起来；再按 E 下来。
// 座位与绳索会随摆角真实移动，玩家跟着座位走。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

class Player;

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class SkyCycle;
class StandardMaterial3D;

class SwingSet : public Interactable {
	GDCLASS(SwingSet, Interactable)

private:
	Player *rider = nullptr;
	Node3D *pivot = nullptr;
	bool e_prev = false;
	float angle = 0.0f;
	float vel = 0.0f;
	float phase = 0.0f;
	float amp = 0.12f;
	float creak_t = 1.6f;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_creak;
	SkyCycle *sky = nullptr;
	MeshInstance3D *glow[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	Ref<StandardMaterial3D> glow_mat;
	float glow_t = 0.0f;

	void build();
	void stand_up();
	void make_creak();
	Ref<AudioStreamWAV> synth_creak();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	float get_angle() const { return angle; }
	bool is_ridden() const { return rider != nullptr; }
	bool is_creak_ready() const { return snd_creak.is_valid(); }
	int get_glow_visible_count() const;
};

} // namespace godot
