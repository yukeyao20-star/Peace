#pragma once

// IceHole —— 结冰湖面上一处未冻的冰洞：深色水面、碎冰圈、缓缓升起寒气，
// 洞口下有几尾鱼影来回游动，水面偶尔泛开一圈涟漪。
//
// 节点摆在南岸码头东侧的水面上（y ≈ 水面高度），雪量够大湖面封冻时显形。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <vector>

namespace godot {

class CPUParticles3D;
class MeshInstance3D;
class AudioStreamPlayer3D;
class AudioStreamWAV;
class SkyCycle;
class StandardMaterial3D;

class IceHole : public Node3D {
	GDCLASS(IceHole, Node3D)

private:
	SkyCycle *sky = nullptr;
	std::vector<Node3D *> fish;
	std::vector<float> fish_phase;
	std::vector<float> fish_radius;
	CPUParticles3D *mist = nullptr;
	MeshInstance3D *ripple = nullptr;
	Ref<StandardMaterial3D> ripple_mat;
	float t = 0.0f;
	float ice = 0.0f;
	// 冰鸣与裂纹
	AudioStreamPlayer3D *ice_voice = nullptr;
	Ref<AudioStreamWAV> snd_ice;
	MeshInstance3D *crack_a = nullptr;
	MeshInstance3D *crack_b = nullptr;
	Ref<StandardMaterial3D> crack_mat;
	float rumble_t = 14.0f;
	float crack_t = 0.0f;
	int rumble_count = 0;
	bool fishing = false;
	float fishing_mix = 0.0f;

	void build();
	Ref<AudioStreamWAV> make_ice_groan();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void set_fishing(bool p_on) { fishing = p_on; }
	bool get_fishing() const { return fishing; }
	bool get_ice_visible() const { return is_visible(); }
	bool get_mist_emitting() const;
	int get_fish_count() const { return (int)fish.size(); }
	float get_ripple_scale() const;
	int get_rumble_count() const { return rumble_count; }
	float get_crack_alpha() const;
	bool get_ice_sound_ready() const { return snd_ice.is_valid(); }
};

} // namespace godot
