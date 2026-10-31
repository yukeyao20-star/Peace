#pragma once

// SwallowNest —— 门廊屋檐下的燕巢。
//
// 泥巴糊成的小碗贴在梁下，三只小燕子探头张望；两只成燕白天绕着屋檐飞，
// 每隔一阵俯冲到巢边喂食，叽叽喳喳。雨天与夜里，巢边安静下来。
//
// 参考：
//   * CSDN《Godot4.3 小鸡 NPC》—— 计时器驱动的行走/空闲状态（用于成燕的
//     绕飞-俯冲-喂食节奏）；
//   * kyrick/godot-boids —— 环绕飞行的 steering 参数；
//   * 常见鸟巢素材的构成：泥碗 + 干草边 + 雏鸟张口。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class SkyCycle;

class SwallowNest : public Node3D {
	GDCLASS(SwallowNest, Node3D)

	static const int CHICK_COUNT = 3;
	static const int SWALLOW_COUNT = 2;

	Node3D *chicks[CHICK_COUNT] = {};
	Node3D *swallows[SWALLOW_COUNT] = {};
	MeshInstance3D *wings[SWALLOW_COUNT][2] = {};
	SkyCycle *sky = nullptr;
	AudioStreamPlayer3D *chirp_voice = nullptr;
	Ref<AudioStreamWAV> snd_chirp;

	float t = 0.0f;
	float visit_timer = 4.0f;
	float feeding_t = 0.0f;
	int feeding_index = -1;
	int visits = 0;
	int swallows_visible = 0;
	bool rain_hide = false;

	void build();
	void build_chirp();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_chick_count() const { return CHICK_COUNT; }
	int get_swallows_visible() const { return swallows_visible; }
	int get_visits() const { return visits; }
	bool get_rain_hide() const { return rain_hide; }
};

} // namespace godot
