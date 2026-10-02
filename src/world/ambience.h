#pragma once

// Ambience —— 全场环境氛围声（风声 + 雨雪声 + 夜间虫鸣），与 SkyCycle 昼夜/天气联动。
//
// 场景集成（编排者用）：节点类型名 "Ambience"，挂在 World 根下即可；
//   ambience_path 无需场景配置；sky_cycle_path（默认 "../SkyCycle"）指向兄弟
//   SkyCycle。找不到 SkyCycle 时按白天/无雨处理（只出基线风声，不出虫鸣雨声）。
//
// 音层（全部 READY 时程序化合成，AudioStreamWAV 16-bit 22050Hz 单声道，无外部资源）：
//   风声（常驻）：5s 无缝循环。白噪声过一阶低通，截止系数随"阵风因子"缓慢
//     随机游走（向中值回拉防漂死），产生阵风起伏；循环点用 0.5s 等功率
//     首尾交叉淡化（延续滤波器状态再生成 0.5s"尾巴"，与开头淡化拼接），
//     无爆音。音量基线 -18dB，雨雪天（wet→1）升到 -12dB（垫底音量，
//     不得压过鸟鸣等 3D 声源与脚步声）。
//   雪风（常驻）：同法合成的更闷变体（低通系数小一个量级、阵风更平缓），
//     与明亮风声按 snow>rain 的主导度交叉淡化音量。
//   雨声：2.8s 无缝循环。滑动平均柔化的白噪声底 × 密集短噪声脉冲包络
//     （110 个随机短脉冲，环形回卷叠加 → 包络整周期连续，循环无缝），
//     wet>0.15 起淡入，音量随 wet。
//   虫鸣：0.3s 一次性短颤音（4500Hz 正弦 × 30Hz 幅度调制 × 10ms 起收门限），
//     夜间（night>0.3，即 SkyCycle::get_night_factor() 过入夜带）且 wet<0.3 时
//     每 3~8s 随机播一声（音高微随机
//     → 载波约 4140~4860Hz），-22dB 很轻，白天不播。
//
// 每帧 NOTIFICATION_PROCESS 平滑 lerp 天气因子与各层增益（防跳变），
// 增益低于 -55dB 暂停混音省资源、高于 -50dB 恢复（带迟滞防抖动）。
// 退出场景（NOTIFICATION_EXIT_TREE）时 stop 全部播放器并清掉成员流引用；
// stop 后需让音频线程跑完一次 mix 扫除 playback，否则退出即泄漏
// （AudioServer::finish 不清 playback_list，详见 ambience.cpp 注释）。
// master 属性（0..1，默认 1.0）线性乘在所有层上。

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <random>

namespace godot {

class AudioStreamPlayer;
class Node;
class Node3D;
class SkyCycle;
class TerrainBuilder;

class Ambience : public Node {
	GDCLASS(Ambience, Node)

private:
	NodePath sky_cycle_path = NodePath("../SkyCycle"); // 指向兄弟 SkyCycle
	float master = 1.0f; // 总音量 0..1（线性，乘在所有音层上）

	Node *sky_cycle = nullptr; // SkyCycle 节点（懒查找，昼夜/天气来源）
	TerrainBuilder *terrain_cache = nullptr; // 地形缓存（懒查找，判定水域）
	Node3D *player_cache = nullptr;          // 玩家位置缓存（懒查找，计算壁炉/湖边距离）

	AudioStreamPlayer *wind = nullptr;      // 常驻风声循环（湖畔清风 wind_breeze.mp3）
	AudioStreamPlayer *wind_snow = nullptr; // 雪天风声循环（更闷音色）
	AudioStreamPlayer *rain = nullptr;      // 雨声循环（淅沥小雨 rain_gentle.mp3）
	AudioStreamPlayer *cricket = nullptr;   // 夜间虫鸣（一次性短颤音）
	AudioStreamPlayer *birds = nullptr;     // 晨曦鸟鸣循环（birds_morning.mp3）
	AudioStreamPlayer *water = nullptr;     // 湖水拍岸循环（water_lap.mp3）
	AudioStreamPlayer *hearth = nullptr;    // 壁炉柴火循环（cabin_fireplace.mp3）

	Ref<AudioStream> snd_wind;         // 明亮风声循环（优先加载 wind_breeze.mp3）
	Ref<AudioStreamWAV> snd_wind_snow; // 闷风声循环
	Ref<AudioStream> snd_rain;         // 雨声循环（优先加载 rain_gentle.mp3）
	Ref<AudioStreamWAV> snd_chirp;     // 蟋蟀短颤音
	Ref<AudioStream> snd_birds;        // 晨曦鸟鸣（birds_morning.mp3）
	Ref<AudioStream> snd_water;        // 湖水拍岸（water_lap.mp3）
	Ref<AudioStream> snd_hearth;       // 壁炉柴火（cabin_fireplace.mp3）

	float wind_gain = 0.0f;   // 各层线性增益（每帧向目标平滑趋近）
	float snow_gain = 0.0f;
	float rain_gain = 0.0f;
	float birds_gain = 0.0f;
	float water_gain = 0.0f;
	float hearth_gain = 0.0f;

	float wet_s = 0.0f;       // 天气因子平滑值（rain+snow 的 clamp）
	float snow_s = 0.0f;      // 雪强平滑值
	float rain_s = 0.0f;      // 雨强平滑值

	bool wind_paused = true;  // 低音量暂停状态（防每帧重复 set）
	bool snow_paused = true;
	bool rain_paused = true;
	bool birds_paused = true;
	bool water_paused = true;
	bool hearth_paused = true;

	float cricket_t = 3.0f;   // 距下次虫鸣的倒计时（秒）

	std::mt19937 rng{2026u}; // 虫鸣间隔/音高随机源（Math 无 randf，走 std）

	void build();
	void shutdown_audio(); // 退出收口：stop 播放器 + 清流引用，防音频实例泄漏
	Ref<AudioStreamWAV> make_wind(bool muffled);
	Ref<AudioStreamWAV> make_rain();
	Ref<AudioStreamWAV> make_chirp();
	Ref<AudioStreamWAV> make_water_lap();
	void apply_gain(AudioStreamPlayer *p, float gain, bool &paused);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void set_sky_cycle_path(const NodePath &p) {
		sky_cycle_path = p;
		sky_cycle = nullptr; // 路径变了，缓存失效（PROCESS 会重新懒查找）
	}
	NodePath get_sky_cycle_path() const { return sky_cycle_path; }
	void set_master(float p) { master = p; }
	float get_master() const { return master; }
};

} // namespace godot
