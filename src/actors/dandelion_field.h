#pragma once

// DandelionField —— 草甸上的蒲公英与飘散的白色种子。
//
// 白天晴好时，细小的种子随风慢慢飘；走近一丛蒲公英会"噗"地散开，
// 光秃的花茎过一阵再长回白色绒球。夜里/雨雪天种子停飞。

#include <godot_cpp/classes/node3d.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class SkyCycle;
class TerrainBuilder;

class DandelionField : public Node3D {
	GDCLASS(DandelionField, Node3D)

private:
	struct Plant {
		Node3D *root = nullptr;
		MeshInstance3D *puff = nullptr;
		Vector3 pos;
		bool bare = false;
		float regrow_t = 0.0f;
		float grow_t = 1.0f;
		float phase = 0.0f;
	};

	std::vector<Plant> plants;
	TerrainBuilder *terrain = nullptr;
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	CPUParticles3D *ambient = nullptr;
	CPUParticles3D *burst = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_puff;
	float t = 0.0f;
	float burst_cd = 0.0f;
	std::mt19937 rng{20261010u};

	void build();
	void build_ambient();
	void make_puff();
	Ref<AudioStreamWAV> synth_puff();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_plant_count() const { return (int)plants.size(); }
	int get_bare_count() const;
	bool get_ambient_enabled() const;
	bool get_puff_ready() const { return snd_puff.is_valid(); }
};

} // namespace godot
