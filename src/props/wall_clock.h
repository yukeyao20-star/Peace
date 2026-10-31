#pragma once

// WallClock —— 木屋后墙上的木质挂钟。
//
// 奶油色表盘 + 十二个刻度 + 时针/分针，指针直接读 SkyCycle 的 time_of_day：
// 一天里时针转两圈、分针转二十四圈，跨午夜也连续不跳变。
// 下面挂一只轻轻摆动的钟摆，每过一刻钟发出一点木头轻响。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class SkyCycle;

class WallClock : public Node3D {
	GDCLASS(WallClock, Node3D)

private:
	SkyCycle *sky = nullptr;
	Node3D *hour_pivot = nullptr;
	Node3D *minute_pivot = nullptr;
	Node3D *pendulum = nullptr;
	AudioStreamPlayer3D *tick_voice = nullptr;
	Ref<AudioStreamWAV> snd_tick;
	float t = 0.0f;
	int last_quarter = -1;

	void build();
	Ref<AudioStreamWAV> make_tick();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	float get_hour_angle() const;
	float get_minute_angle() const;
	float get_pendulum_angle() const;
	bool get_tick_ready() const { return snd_tick.is_valid(); }
};

} // namespace godot
