#pragma once

// IcicleField —— 木屋屋檐下的冰凌（雪季装饰）。
//
// 雪量上来后檐口慢慢长出长短不一的冰凌；雪开始化时滴水、偶尔"叮"一声，
// 雪停干净后冰凌缩没。挂在 Homestead 局部坐标下（与木屋一起转 45°）。

#include <godot_cpp/classes/node3d.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class ShaderMaterial;
class SkyCycle;
class TerrainBuilder;

class IcicleField : public Node3D {
	GDCLASS(IcicleField, Node3D)

private:
	struct Icicle {
		Node3D *root = nullptr;
		float max_len = 0.4f;
		float phase = 0.0f;
		float cur = 0.0f;
	};

	std::vector<Icicle> icicles;
	SkyCycle *sky = nullptr;
	Ref<ShaderMaterial> ice_mat;
	CPUParticles3D *drip = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_drop;
	float t = 0.0f;
	float drip_t = 3.0f;
	float grow = 0.0f;
	std::mt19937 rng{20261201u};

	void build();
	void make_drip();
	Ref<AudioStreamWAV> synth_drop();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_icicle_count() const { return (int)icicles.size(); }
	int get_visible_count() const;
	bool get_drip_emitting() const;
	bool get_drop_ready() const { return snd_drop.is_valid(); }
	float get_grow() const { return grow; }
};

} // namespace godot
