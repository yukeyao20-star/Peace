#pragma once

// WindCharms —— 木屋屋顶的风向标与门廊下的风铃。
//
// 风向标缓慢地跟着模拟风向转；门廊风铃随风轻摆，风大时偶尔叮咚几声。
// 雨雪会加大风力，让风铃更活跃。全部挂在 Homestead 局部坐标下。

#include <godot_cpp/classes/node3d.hpp>

#include <random>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class Node3D;
class SkyCycle;

class WindCharms : public Node3D {
	GDCLASS(WindCharms, Node3D)

private:
	SkyCycle *sky = nullptr;
	Node3D *vane = nullptr;
	Node3D *chime = nullptr;
	Node3D *tube[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_chime;
	float t = 0.0f;
	float wind_dir = 0.3f;
	float wind = 0.3f;
	float chime_t = 2.0f;
	std::mt19937 rng{20261231u};

	void build();
	Ref<AudioStreamWAV> make_chime();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	float get_wind() const { return wind; }
	float get_vane_rotation() const;
	int get_tube_count() const;
	bool get_chime_ready() const { return snd_chime.is_valid(); }
	bool get_chime_playing() const;
};

} // namespace godot
