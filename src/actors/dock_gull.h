#pragma once

// DockGull —— 白天停在码头木牌顶上的海鸥。
//
// 晴朗白天蹲在牌子上偶尔叫两声；玩家靠近 5m 内会振翅飞走，过一阵
// （玩家走远后）再飞回来。夜里在别处休息，不出现。

#include <godot_cpp/classes/node3d.hpp>

#include <random>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class SkyCycle;

class DockGull : public Node3D {
	GDCLASS(DockGull, Node3D)

private:
	Node3D *root = nullptr;
	Node3D *wing_l = nullptr;
	Node3D *wing_r = nullptr;
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_call;
	Vector3 perch;
	float t = 0.0f;
	float call_t = 4.0f;
	float fly_t = 0.0f;
	float fly_dur = 2.0f;
	float respawn_t = 0.0f;
	Vector3 fly_from;
	Vector3 fly_to;
	int state = 0; // 0 停栖 / 1 飞走 / 2 消失等待
	std::mt19937 rng{20261220u};

	void build();
	void take_off(const Vector3 &p_player_pos);
	Ref<AudioStreamWAV> make_call();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool is_perched() const { return state == 0; }
	bool is_flying() const { return state == 1; }
	bool is_call_ready() const { return snd_call.is_valid(); }
};

} // namespace godot
