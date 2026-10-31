#pragma once

// HeronPond —— 浅水里的苍鹭（可惊飞的氛围水鸟）。
//
// 两只苍鹭站在湖岸浅水（水深 0.15~0.6m）的芦苇边：
//   * 平时单腿站着、偶尔低头啄水；
//   * 玩家进入 6m 内会展开翅膀起飞，沿弧线飞远后隐藏；
//   * 20~30 秒后回到原处。
// 全部程序化，无外部资源。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class TerrainBuilder;

class HeronPond : public Node3D {
	GDCLASS(HeronPond, Node3D)

private:
	struct Heron {
		Node3D *root = nullptr;
		Node3D *wing_l = nullptr;
		Node3D *wing_r = nullptr;
		Node3D *neck = nullptr;
		Vector3 home;
		bool hidden = false;
		bool flying = false;
		float fly_t = 0.0f;
		float fly_dur = 1.25f;
		Vector3 fly_from;
		Vector3 fly_to;
		float respawn_t = 0.0f;
		float phase = 0.0f;
		float peck_t = 0.0f;
	};

	std::vector<Heron> herons;
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;
	float anim_t = 0.0f;
	TerrainBuilder *terrain = nullptr;
	Node3D *player = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_call;

	void build();
	void spawn_heron(int p_index, const Vector3 &p_home, unsigned p_seed);
	void take_off(Heron &p_heron);
	Ref<AudioStreamWAV> make_call();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
