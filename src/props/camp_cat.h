#pragma once

// CampCat —— 火堆边打盹的橘猫。
//
// 平时蜷在火边轻轻呼吸、尾巴慢慢摆；玩家走近会抬头看你一眼，并发出
// 低低的呼噜声；下大雨时会挪到长椅底下躲雨，雨停再回火边。

#include <godot_cpp/classes/node3d.hpp>

#include <random>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class SkyCycle;
class TerrainBuilder;

class CampCat : public Interactable {
	GDCLASS(CampCat, Interactable)

private:
	TerrainBuilder *terrain = nullptr;
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	Node3D *body = nullptr;
	Node3D *head = nullptr;
	Node3D *tail = nullptr;
	MeshInstance3D *ear_l = nullptr;
	MeshInstance3D *ear_r = nullptr;
	AudioStreamPlayer3D *purr = nullptr;
	CPUParticles3D *hearts = nullptr;
	Ref<AudioStreamWAV> snd_purr;
	float pet_t = 0.0f;
	Vector3 home;
	Vector3 shelter;
	float t = 0.0f;
	float purr_fade = 0.0f;
	bool purring = false;
	std::mt19937 rng{20261212u};

	void build();
	void make_hearts();
	void make_purr();
	Ref<AudioStreamWAV> synth_purr();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool get_purr_ready() const { return snd_purr.is_valid(); }
	bool get_purr_playing() const;
	bool get_at_shelter() const;
	float get_pet_time() const { return pet_t; }
	bool get_hearts_emitting() const;

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
};

} // namespace godot
