#pragma once

// StonePath —— 木屋门口通往码头/长椅的碎石小径。
//
// 一条微微弯曲的踏脚石路，沿途三盏木杆小灯（入夜亮起），入口一块木牌。
// 雨天地面被打湿：石头颜色变深、反光增强（读 SkyCycle 的雨量）。

#include <godot_cpp/classes/node3d.hpp>

#include <random>
#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class Font;
class Label3D;
class MeshInstance3D;
class OmniLight3D;
class ShaderMaterial;
class SkyCycle;
class StandardMaterial3D;
class TerrainBuilder;

class StonePath : public Node3D {
	GDCLASS(StonePath, Node3D)

private:
	TerrainBuilder *terrain = nullptr;
	SkyCycle *sky = nullptr;
	struct Puddle {
		MeshInstance3D *mi = nullptr;
		Ref<StandardMaterial3D> mat;
		Vector3 center;
		float radius = 0.5f;
	};
	struct Ripple {
		MeshInstance3D *mi = nullptr;
		Ref<StandardMaterial3D> mat;
		float t = 0.0f;
		float dur = 1.0f;
		bool active = false;
	};
	struct Moth {
		Node3D *root = nullptr;
		MeshInstance3D *wing_l = nullptr;
		MeshInstance3D *wing_r = nullptr;
		Vector3 center;
		float phase = 0.0f;
		float radius = 0.4f;
		float speed = 1.0f;
		float lift = 0.0f;
	};

	Ref<ShaderMaterial> stone_mat;
	Ref<StandardMaterial3D> lamp_glass[3];
	OmniLight3D *lamp_light[3] = {nullptr, nullptr, nullptr};
	Node3D *lamp_pivot[3] = {nullptr, nullptr, nullptr};
	MeshInstance3D *light_pool[3] = {nullptr, nullptr, nullptr};
	Ref<StandardMaterial3D> light_pool_mat[3];
	std::vector<Puddle> puddles;
	std::vector<Ripple> ripples;
	std::vector<Moth> moths;
	AudioStreamPlayer3D *splash = nullptr;
	Ref<AudioStreamWAV> snd_splash;
	float ripple_t = 0.8f;
	float step_t = 0.0f;
	std::mt19937 rng{20260911u};
	float wet = 0.0f;
	float t = 0.0f;
	int stone_count = 0;

	void build_path();
	void build_lamps();
	void build_gate();
	void build_puddles();
	void build_moths();
	void build_light_pools();
	void spawn_ripple(const Vector3 &p_pos);
	void make_splash();
	Ref<AudioStreamWAV> make_splash_wav();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_stone_count() const { return stone_count; }
	int get_lamp_count() const { return 3; }
	float get_lamp_energy(int p_index) const;
	int get_puddle_count() const { return (int)puddles.size(); }
	int get_moth_count() const { return (int)moths.size(); }
	int get_moth_visible_count() const;
	bool get_splash_ready() const { return snd_splash.is_valid(); }
	float get_puddle_alpha(int p_index) const;
	float get_pool_alpha(int p_index) const;
};

} // namespace godot
