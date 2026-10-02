#pragma once

// Bird —— 绕圆轨道盘旋的飞鸟（扑翼动画）。
//
// 场景里把节点放在"轨道中心 + 巡航高度"处即可；READY 时以节点世界坐标
// 为圆心，用坐标哈希做种随机出轨道半径/角速度/扑翼频率（每只不同、可复现）。
// 机身朝 -Z 建模（Godot look_at 的前方惯例），飞行时 -Z 指向速度方向。
//
// 夜归：懒查找兄弟节点 SkyCycle 读取 time_of_day 换算太阳高度；
//   黄昏（太阳落下）沿轨道边放大边爬升、收扑翼滑翔离场（归巢过夜），
//   黎明对称回归恢复盘旋。找不到 SkyCycle 时按白天处理（行为不变）。
// 雨雪：wet 因子（rain+snow）压低巡航高度贴湖面低飞、小幅收翼；
//   鸣叫闭嘴（见下）。
//
// 鸣叫：READY 时程序化合成三种短哨 PCM（AudioStreamWAV 16-bit，无外部资源），
//   挂一个 AudioStreamPlayer3D 声源随鸟移动；白天每 4~11s 随机鸣叫一声，
//   音高微随机防机械感；夜里归巢不叫；雨雪天（SkyCycle rain/snow 因子）
//   闭嘴躲雨，湿度越大鸣叫间隔越长。

#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <random>

namespace godot {

class Node;
class AudioStreamPlayer3D;

class Bird : public Node3D {
	GDCLASS(Bird, Node3D)

private:
	Node3D *wing_l = nullptr;
	Node3D *wing_r = nullptr;
	Node *sky_cycle = nullptr; // 兄弟节点 SkyCycle（懒查找，昼夜来源）
	float night = 0.0f;        // 夜幕因子 0（白天）..1（已离场）
	Vector3 center;        // 轨道圆心
	Vector3 last_pos;      // 上一帧位置（求速度方向）
	float phase = 0.0f;    // 轨道相位
	float anim_t = 0.0f;   // 扑翼计时
	float radius = 10.0f;  // 轨道半径
	float speed = 0.35f;   // 角速度（rad/s，可负=反向）
	float flap = 8.0f;     // 扑翼频率（rad/s）
	bool first_frame = true;

	AudioStreamPlayer3D *voice = nullptr; // 3D 鸟鸣声源（挂在鸟身上跟随移动）
	Ref<AudioStreamWAV> chirps[3];        // 三种程序化合成的鸣叫声
	std::mt19937 rng{2026};               // 鸣叫节奏/选声/音高的随机源（READY 重播种）
	float chirp_t = 5.0f;                 // 距下次鸣叫的倒计时（秒）

	void build();
	Ref<AudioStreamWAV> make_chirp(int kind);

protected:
	static void _bind_methods() {}

public:
	void _notification(int p_what);
};

} // namespace godot
