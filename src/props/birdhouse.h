#pragma once

// Birdhouse —— 草地上的小木鸟屋。
//
// 木柱顶着一间尖顶小屋，屋前有栖木；蓝鸲飞进飞出，停在栖木上啾鸣。
// 按 E 可以探头看看屋里的蛋。
// 参考此前调研的鸟食台/燕巢做法（栖木、进出屋、啾鸣）。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class SkyCycle;

class Birdhouse : public Interactable {
	GDCLASS(Birdhouse, Interactable)
	static const int BIRD_COUNT = 2;
	Node3D *birds[BIRD_COUNT] = {};
	MeshInstance3D *wings[BIRD_COUNT][2] = {};
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;
	AudioStreamPlayer3D *chirp = nullptr;
	Ref<AudioStreamWAV> snd_chirp;
	float t = 0.0f;
	float timers[BIRD_COUNT] = { 2.0f, 5.0f };
	bool inside[BIRD_COUNT] = { false, false };
	int birds_visible = 0;
	int peeks = 0;
	void build();
	void build_chirp();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
	int get_birds_visible() const { return birds_visible; }
	int get_peeks() const { return peeks; }
};

} // namespace godot
