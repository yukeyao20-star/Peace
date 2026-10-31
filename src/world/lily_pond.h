#pragma once

// LilyPond —— 码头西侧水湾的睡莲与锦鲤。
//
// 程序化生成一片浮在水面的睡莲叶（含少量荷花）、水下游动的锦鲤、
// 鱼碰水面时扩散的圆环、以及随风慢慢漂的小落叶。全部低模 + 卡通材质。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class StandardMaterial3D;
class SkyCycle;
class TerrainBuilder;

class LilyPond : public Node3D {
	GDCLASS(LilyPond, Node3D)

private:
	struct Koi {
		Node3D *root = nullptr;
		Vector3 home;
		float radius = 1.0f;
		float phase = 0.0f;
		float speed = 0.4f;
		float bob_phase = 0.0f;
		bool jumping = false;
		float jump_t = 0.0f;
		float jump_dur = 0.75f;
		Vector3 jump_from;
		Vector3 jump_to;
	};

	struct Ring {
		MeshInstance3D *mi = nullptr;
		Ref<StandardMaterial3D> mat;
		Vector3 pos;
		float t = 0.0f;
		float dur = 1.2f;
		bool active = false;
	};

	struct Leaf {
		MeshInstance3D *mi = nullptr;
		Vector3 home;
		float phase = 0.0f;
		float drift = 0.0f;
	};

	std::vector<Vector3> pad_spots;
	std::vector<Koi> koi;
	std::vector<Ring> rings;
	std::vector<Leaf> leaves;

	TerrainBuilder *terrain = nullptr;
	SkyCycle *sky = nullptr;
	float water_level = -0.55f;
	float wet = 0.0f;
	float t = 0.0f;
	float ring_t = 1.5f;
	float plop_t = 5.0f;
	float jump_t = 10.0f;
	std::mt19937 rng{20261214u};

	AudioStreamPlayer3D *plop = nullptr;
	Ref<AudioStreamWAV> snd_plop;

	void build_pads();
	void build_flowers();
	void build_koi();
	void build_rings();
	void build_leaves();
	void spawn_ring(const Vector3 &p_pos);
	void make_plop();
	Ref<AudioStreamWAV> synth_plop();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_pad_count() const { return (int)pad_spots.size(); }
	int get_koi_count() const { return (int)koi.size(); }
	int get_active_jumps() const {
		int n = 0;
		for (const Koi &k : koi) {
			if (k.jumping) {
				n++;
			}
		}
		return n;
	}
};

} // namespace godot
