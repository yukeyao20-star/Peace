#pragma once

// HearthFire —— 木屋室内的石壁炉：常燃的火、暖光、柴火噼啪声与一只烧水的铜壶。
//
// 节点挂在 Homestead 下，局部坐标与 Cabin 的壁炉一致（火膛中心约 -3.45, 0.55, -0.6）。
// E 添柴：火势与暖光短暂冲高，铜壶蒸汽随火焰起伏。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class OmniLight3D;

class HearthFire : public Interactable {
	GDCLASS(HearthFire, Interactable)

private:
	OmniLight3D *light = nullptr;
	CPUParticles3D *flame = nullptr;
	CPUParticles3D *sparks = nullptr;
	CPUParticles3D *steam = nullptr;
	AudioStreamPlayer3D *crackle = nullptr;
	Ref<AudioStreamWAV> snd_crackle;
	float t = 0.0f;
	float boost = 0.0f; // 添柴后的火势余韵（秒）

	void build();
	Ref<AudioStreamWAV> make_crackle();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.6f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	float get_light_energy() const;
	float get_boost() const { return boost; }
	bool get_flame_emitting() const;
	bool get_steam_emitting() const;
};

} // namespace godot
