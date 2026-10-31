#pragma once

// FloatingLanterns —— 湖面上慢慢漂的六盏纸灯笼。
//
// 白天是安静的纸灯，入夜后灯芯亮起暖光，随水波轻轻起伏、缓慢打转。
// 不参与交互，纯氛围；位置固定在浅水湾一带。

#include <godot_cpp/classes/node3d.hpp>

#include <random>
#include <vector>

namespace godot {

class MeshInstance3D;
class OmniLight3D;
class SkyCycle;
class StandardMaterial3D;
class TerrainBuilder;

class FloatingLanterns : public Node3D {
	GDCLASS(FloatingLanterns, Node3D)

private:
	struct Lantern {
		Node3D *root = nullptr;
		MeshInstance3D *paper = nullptr;
		OmniLight3D *light = nullptr;
		Ref<StandardMaterial3D> mat;
		Vector3 anchor;
		float phase = 0.0f;
		float radius = 0.4f;
		float speed = 0.12f;
		float bob_phase = 0.0f;
	};

	std::vector<Lantern> lanterns;
	TerrainBuilder *terrain = nullptr;
	SkyCycle *sky = nullptr;
	float water_level = -0.55f;
	float t = 0.0f;
	std::mt19937 rng{20270303u};

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_lantern_count() const { return (int)lanterns.size(); }
	int get_lit_count() const;
	float get_light_energy() const;
	Vector3 get_lantern_pos(int p_index) const;
};

} // namespace godot
