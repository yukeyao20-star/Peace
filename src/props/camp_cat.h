#pragma once

// CampCat —— 火堆边打盹的橘猫。
//
// 平时蜷在火边轻轻呼吸、尾巴慢慢摆；玩家走近会抬头看你一眼，并发出
// 低低的呼噜声；下大雨时会挪到长椅底下躲雨，雨停再回火边。

#include <godot_cpp/classes/node3d.hpp>

#include <random>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class SkyCycle;
class TerrainBuilder;

class CampCat : public Node3D {
	GDCLASS(CampCat, Node3D)

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
	Ref<AudioStreamWAV> snd_purr;
	Vector3 home;
	Vector3 shelter;
	float t = 0.0f;
	float purr_fade = 0.0f;
	bool purring = false;
	std::mt19937 rng{20261212u};

	void build();
	void make_purr();
	Ref<AudioStreamWAV> synth_purr();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool get_purr_ready() const { return snd_purr.is_valid(); }
	bool get_purr_playing() const;
	bool get_at_shelter() const;
};

} // namespace godot
