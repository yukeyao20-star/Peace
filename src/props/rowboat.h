#pragma once

// Rowboat —— 湖南岸的小木船与码头。
//
// 码头从岸边伸进湖里（带真实碰撞，可走上去），船系在码头侧面：
//   * 靠近按 E 上船，再按 E 下船（下船落在船侧水里，自动进入游泳）；
//   * 船上 W/S 前后划，A/D 转向；船头始终贴在水面，随波轻晃；
//   * 只在足够深的水里移动，靠岸会自动停住；
//   * 划动时船尾拖出细碎水花。
// 水面海拔 -0.55 与 terrain_builder.h water_level 联动。

#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

class Player; // 全局命名空间（player.h 的定义不在 godot 内）

namespace godot {

class AudioStreamPlayer3D;
class CPUParticles3D;
class OmniLight3D;
class TerrainBuilder;

class Rowboat : public Interactable {
	GDCLASS(Rowboat, Interactable)

private:
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;
	Vector3 home = Vector3(-4.3f, -0.55f, -7.6f);

	Node3D *hull = nullptr;
	Node3D *oar_l = nullptr;
	Node3D *oar_r = nullptr;
	CPUParticles3D *wake = nullptr;
	AudioStreamPlayer3D *oar_voice = nullptr;
	OmniLight3D *bow_lamp = nullptr;
	OmniLight3D *dock_lamp = nullptr;
	Node3D *bow_pivot = nullptr;       // 船头灯挂点（随船轻摆）
	CPUParticles3D *oar_drip = nullptr; // 桨叶带起的水珠
	AudioStreamPlayer3D *lap_voice = nullptr; // 船边水声
	Ref<AudioStreamWAV> snd_lap;
	float oar_phase = 0.0f;
	CPUParticles3D *dock_splash = nullptr;
	CPUParticles3D *boat_splash = nullptr;
	AudioStreamPlayer3D *dock_patter = nullptr;
	AudioStreamPlayer3D *boat_patter = nullptr;
	Ref<AudioStreamWAV> snd_patter;
	Node *sky = nullptr;
	bool rain_fx_built = false;
	void build_rain_fx(Node *p_dock);
	Ref<AudioStreamWAV> make_patter();
	Ref<AudioStreamWAV> snd_oar;
	float oar_t = 0.0f;
	int oar_side = 1;
	Ref<AudioStreamWAV> make_oar_splash();
	Player *rider = nullptr;
	bool occupied = false;
	float yaw = 0.0f;
	float speed = 0.0f;
	float bob_t = 0.0f;
	bool e_prev = false;
	TerrainBuilder *terrain = nullptr;

	void build_hull();
	void build_dock();
	void make_lap();
	Ref<AudioStreamWAV> synth_lap();
	void board(Player *p_rider);
	void exit_boat();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool is_occupied() const { return occupied; }
	float get_speed() const { return speed; }
	float get_row_phase() const { return oar_phase; }
};

} // namespace godot
