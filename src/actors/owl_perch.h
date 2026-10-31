#pragma once

// OwlPerch —— 木屋外的两座栖木与夜里蹲在上面的猫头鹰。
//
// 白天看不见；入夜后出现在栖木顶端，头会跟着走近的玩家转，偶尔眨眼睛，
// 每隔一阵发出两声低低的“咕—咕”。程序化低模与合成音，无外部资源。

#include <godot_cpp/classes/node3d.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class SkyCycle;
class TerrainBuilder;

class OwlPerch : public Node3D {
	GDCLASS(OwlPerch, Node3D)

private:
	struct Owl {
		Node3D *root = nullptr;
		Node3D *head = nullptr;
		MeshInstance3D *eye_l = nullptr;
		MeshInstance3D *eye_r = nullptr;
		Vector3 home;
		float blink_t = 3.0f;
		float blink_phase = -1.0f;
		float hoot_t = 6.0f;
		float yaw = 0.0f;
		float phase = 0.0f;
	};

	std::vector<Owl> owls;
	TerrainBuilder *terrain = nullptr;
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_hoot;
	float t = 0.0f;
	std::mt19937 rng{20261111u};

	void build();
	void build_owl(int p_index, const Vector3 &p_home, unsigned p_seed);
	Ref<AudioStreamWAV> make_hoot();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_owl_count() const { return (int)owls.size(); }
	int get_visible_count() const;
	bool get_hoot_ready() const { return snd_hoot.is_valid(); }
};

} // namespace godot
