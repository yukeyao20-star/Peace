#pragma once

// Beehive —— 草甸边木桩上的蜂巢。
//
// 白天一群小蜜蜂绕着巢飞、翅膀快速振动，靠近能听见低低的嗡鸣；
// 入夜或下雨时它们回巢，蜂巢安静下来。

#include <godot_cpp/classes/node3d.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class SkyCycle;
class TerrainBuilder;

class Beehive : public Node3D {
	GDCLASS(Beehive, Node3D)

private:
	struct Bee {
		Node3D *root = nullptr;
		MeshInstance3D *wing_l = nullptr;
		MeshInstance3D *wing_r = nullptr;
		float phase = 0.0f;
		float radius = 0.7f;
		float speed = 2.0f;
		float lift = 0.0f;
	};

	std::vector<Bee> bees;
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	AudioStreamPlayer3D *buzz = nullptr;
	Ref<AudioStreamWAV> snd_buzz;
	Vector3 hive_center;
	float t = 0.0f;
	std::mt19937 rng{20270404u};

	void build();
	Ref<AudioStreamWAV> make_buzz();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_bee_count() const { return (int)bees.size(); }
	int get_visible_bee_count() const;
	bool get_buzz_ready() const { return snd_buzz.is_valid(); }
	bool get_buzz_playing() const;
};

} // namespace godot
