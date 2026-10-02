#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <random>

#include "interactable.h"

namespace godot {
class AudioStream;
class AudioStreamPlayer;
class AudioStreamWAV;
class CanvasLayer;
class Label;
class TerrainBuilder;
} // namespace godot

using namespace godot;

// Player —— 键盘控制的主角（第三人称）。
//
// 继承 CharacterBody3D：碰撞与滑动求解交给引擎（move_and_slide），
// 我们只负责"这一帧要往哪走"。
//
// 操作：W/A/S/D 移动，空格 跳跃，E 与面前的门/窗互动。
//
// 互动逻辑：每帧在 "interactables" 组里找最近的可交互物（≤3.2m），
// 显示悬浮提示；按 E 触发 interact()，同时主角做一个"推"的前倾动作
// （模型绕 X 轴前倾再回弹，0.5 秒）。
class Player : public CharacterBody3D {
	GDCLASS(Player, CharacterBody3D)

	// —— 可在编辑器 Inspector 里调整的参数 ——
	double move_speed = 6.0;       // 水平移动速度（米/秒）
	double jump_velocity = 8.2;    // 起跳初速度（配 30 重力：跳得干脆不飘）
	double gravity = 30.0;         // 重力加速度（原 22 太飘）
	double camera_distance = 5.5;  // 第三人称相机初始距离（滚轮 2.5~9m 缩放）
	double camera_height = 1.55;   // 视线支点高度（脚底到胸口上方）
	double camera_smoothing = 12.0; // 相机跟随平滑（原 8 太肉，走位有拖拽感）

	Node3D *model = nullptr;
	Camera3D *camera = nullptr;

protected:
	static void _bind_methods();
	void _notification(int p_what);

	void _physics_step();
	void _update_look(double delta);
	void _update_camera(double delta);
	void _update_interaction();
	void _update_footsteps(double delta, bool on_floor);

	// —— 脚步声（程序化合成，无外部资源） ——
	godot::Ref<godot::AudioStreamWAV> make_grass();
	godot::Ref<godot::AudioStreamWAV> make_dirt();
	godot::Ref<godot::AudioStreamWAV> make_pitch();
	godot::Ref<godot::AudioStreamWAV> make_snow();
	godot::Ref<godot::AudioStreamWAV> make_puddle();
	godot::Ref<godot::AudioStreamWAV> make_stone();
	godot::Ref<godot::AudioStreamWAV> make_wood();
	godot::Ref<godot::AudioStreamWAV> make_rustle(bool deep);
	void play_step(int kind, float pitch); // kind: 0荒草 1雪地咯吱 2雨天踩水 3石板 4木板 5球场草坪 6泥土小径
	int _step_kind() const;                // 按天气/所处地形判定当前地面音色

public:
	Player() = default;
	~Player() override = default;

	void set_move_speed(double p_v) { move_speed = p_v; }
	double get_move_speed() const { return move_speed; }

	void set_jump_velocity(double p_v) { jump_velocity = p_v; }
	double get_jump_velocity() const { return jump_velocity; }

	void set_camera_distance(double p_v) { camera_distance = p_v; }
	double get_camera_distance() const { return camera_distance; }

	void set_camera_height(double p_v) { camera_height = p_v; }
	double get_camera_height() const { return camera_height; }

	double get_land_timer() const { return land_timer; }
	Vector3 get_last_land_pos() const { return last_land_pos; }
	Vector3 get_facing() const { return facing; }

private:
	// —— 互动状态 ——
	class Interactable *near_target = nullptr; // 当前最近的可交互物
	class Label3D *prompt = nullptr;           // 悬浮提示（"按 E 开门"等）
	godot::CanvasLayer *hud_layer = nullptr;   // 交互提示的屏幕空间层（永不裁剪）
	godot::Label *hud_prompt = nullptr;        // 屏幕下方居中的交互提示
	bool e_prev = false;                       // 上一物理帧 E 键状态（边沿触发）
	double lean_time = 0.0;                    // 推门动画剩余时间（>0 表示播放中）
	Vector3 facing = Vector3(0.0, 0.0, -1.0);  // 模型朝向（用于加前倾角度）

	// —— 第三人称鼠标视角状态 ——
	double cam_yaw = 0.0;    // 环绕角色的偏航角（0 = 相机在 +Z 后方，看 -Z）
	double cam_pitch = 0.47; // 俯仰角（>0 相机抬高向下看；鼠标上抬=看向地平线）
	double cam_dist = 5.5;   // 当前相机距离（滚轮缩放）
	bool mouse_captured = false; // 鼠标是否已捕获（点击捕获 / ESC 释放）
	bool esc_prev = false;
	bool lmb_prev = false;

	// —— 脚步声状态 ——
	godot::Ref<godot::AudioStream> snd_grass;      // 普通草甸脚步（优先加载 grass_step.wav）
	godot::Ref<godot::AudioStream> snd_dirt;       // 泥土小径坚实脚步（优先加载 dirt_step.wav）
	godot::Ref<godot::AudioStreamWAV> snd_pitch;      // 足球场平整草坪脚步
	godot::Ref<godot::AudioStream> snd_snow;          // 真实雪地踩雪声（用户提供）
	godot::Ref<godot::AudioStream> snd_snow_variants[4]; // 4 种真实踏雪切片变体（源自用户音频）
	int snow_variant_idx = 0;                         // 踏雪变体轮换索引
	godot::Ref<godot::AudioStreamWAV> snd_wet;        // 雨天水花轻溅踩水声
	godot::Ref<godot::AudioStream> snd_stone;         // 石桥面石板嗒声（优先加载 stone_step.wav）
	godot::Ref<godot::AudioStreamWAV> snd_wood;       // 木屋地板咚咚声
	godot::Ref<godot::AudioStreamWAV> snd_rustle;      // 过草沙沙声（普通草地，轻柔）
	godot::Ref<godot::AudioStreamWAV> snd_rustle_deep; // 高草丛拨草声（深沉茂密）
	mutable godot::TerrainBuilder *terrain_cache = nullptr; // 懒查找地形与地貌判定
	godot::AudioStreamPlayer *steps_voice = nullptr; // 脚步声源
	godot::AudioStreamPlayer *rustle_voice = nullptr; // 穿草声源（与脚步分声道）
	godot::AudioStreamPlayer *ui_voice = nullptr;     // 界面/交互声源
	godot::Ref<godot::AudioStream> snd_ui_click;     // 界面点击声
	godot::Ref<godot::AudioStream> snd_ui_back;      // 界面返回声
	godot::Ref<godot::AudioStream> snd_ui_expand;    // 界面滑出/聚焦声
	void play_ui(int type = 0);                       // 播放 UI 音效：0=点击/互动, 1=返回/收起, 2=滑出/聚焦
	bool weather_toggle_prev = false;                 // T / F8 键边缘检测
	double rustle_acc = 0.0;                          // 穿草触发距离累计
	std::mt19937 rng{1337};                           // 音高微随机（世界坐标哈希播种）
	double step_acc = 0.0;                            // 已走距离累计（够 2.1m 一步）
	bool foot_side = false;                           // 当前步足印左右交替（false=左脚, true=右脚）
	double bob_phase = 0.0;                           // 走路颠簸相位（与步距同步）
	bool was_on_floor = true;                         // 上一物理帧是否落地（落地音边沿）
	double land_timer = 0.0;                          // 落地倒伏缓冲时间（秒，用于连跳时接地点草叶回弹）
	Vector3 last_land_pos;                            // 上次接地点坐标（世界空间）
};
