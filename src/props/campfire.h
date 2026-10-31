#pragma once

// Campfire —— 码头旁的火堆（昼夜都燃着，夜里是暖光与噼啪声）。
//
// 石圈 + 交叉柴火 + 火焰/火星粒子 + 暖橙色 OmniLight（轻微闪烁）
// + 程序化柴火噼啪循环声。节点放在岸上，READY 时贴合地形高度。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class Node3D;
class OmniLight3D;
class SkyCycle;
class StandardMaterial3D;
class TerrainBuilder;

class Campfire : public Interactable {
	GDCLASS(Campfire, Interactable)

private:
	OmniLight3D *light = nullptr;
	CPUParticles3D *flame = nullptr;
	CPUParticles3D *embers = nullptr;
	AudioStreamPlayer3D *crackle = nullptr;
	AudioStreamPlayer3D *hiss = nullptr; // 雨点落进火堆的嗞声
	CPUParticles3D *steam = nullptr;     // 雨中被蒸起的白汽
	Ref<AudioStreamWAV> snd_crackle;
	Ref<AudioStreamWAV> snd_hiss;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	float boost = 0.0f; // 添柴后的火势余韵（秒）
	float wet = 0.0f;   // 雨强平滑值

	// —— 烤棉花糖 ——
	Node3D *roast_root = nullptr;
	MeshInstance3D *marshmallow = nullptr;
	MeshInstance3D *stick = nullptr;
	Ref<StandardMaterial3D> marshmallow_mat;
	CPUParticles3D *roast_smoke = nullptr;
	AudioStreamPlayer3D *eat_voice = nullptr;
	Ref<AudioStreamWAV> snd_crunch;
	Node3D *player_node = nullptr;
	int roast_state = 0; // 0 空闲 / 1 正在烤
	float roast_t = 0.0f;
	int golden_count = 0;
	int burnt_count = 0;

	void build_roast();
	void start_roast(const Vector3 &p_player_pos);
	void stop_roast();
	void update_roast(float p_delta);
	Ref<AudioStreamWAV> make_crunch();

	void build();
	Ref<AudioStreamWAV> make_crackle();
	Ref<AudioStreamWAV> make_hiss();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_roast_state() const { return roast_state; }
	float get_roast_time() const { return roast_t; }
	int get_roast_stage() const; // 0 生 / 1 金黄 / 2 焦
	int get_golden_count() const { return golden_count; }
	int get_burnt_count() const { return burnt_count; }
	bool get_roast_visible() const;
};

} // namespace godot
