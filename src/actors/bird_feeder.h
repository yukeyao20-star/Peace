#pragma once

// BirdFeeder —— 木屋前院的一根松果鸟食台。
//
// 木柱 + 小托盘 + 垂下来的松果串；两只小山雀会飞来落在托盘边，
// 低头啄食、偶尔啾两声。玩家走近 4m 内它们会扑棱飞走，过一阵再回来。
// 雪天托盘上会积一层薄雪。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <godot_cpp/classes/ref.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class SkyCycle;

class BirdFeeder : public Node3D {
	GDCLASS(BirdFeeder, Node3D)

private:
	struct Bird {
		Node3D *root = nullptr;
		Node3D *head = nullptr;
		Node3D *wing_l = nullptr;
		Node3D *wing_r = nullptr;
		Vector3 perch;
		float phase = 0.0f;
		float peck_t = 0.0f;
		float wait_t = 0.0f;
		int state = 0; // 0 停在托盘 / 1 飞走 / 2 飞回
		float fly_t = 0.0f;
		Vector3 fly_from;
		Vector3 fly_to;
	};
	std::vector<Bird> birds;
	Node3D *snow_cap = nullptr;
	Node3D *drifts = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_chirp;
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	float t = 0.0f;
	float chirp_t = 4.0f;
	std::mt19937 rng{20261201u};

	void build();
	void build_bird(int p_index, const Vector3 &p_perch, unsigned p_seed, bool p_left);
	void send_away(Bird &p_b, const Vector3 &p_player_pos);
	Ref<AudioStreamWAV> make_chirp();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_bird_count() const { return (int)birds.size(); }
	int get_perched_count() const;
	bool get_chirp_ready() const { return snd_chirp.is_valid(); }
	bool get_snow_cap_visible() const;
};

} // namespace godot
