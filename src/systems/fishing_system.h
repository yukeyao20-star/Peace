#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <random>

namespace godot {

class CanvasLayer;
class ColorRect;
class Panel;
class Label;
class MeshInstance3D;
class Node3D;

} // namespace godot

class Player; // Player 在全局命名空间（player.h 未包 namespace）

namespace godot {

// FishingSystem —— 湖边钓鱼小游戏。
//
// 玩法循环：
//   1. 玩家走到湖岸（湖心距 13~22m）→ HUD 提示"按 F 抛竿"；
//   2. 按 F 抛竿：浮漂落在身前水面，水下出现鱼影绕浮漂游动（透过水面
//      折射自然可见）；2~6 秒后咬钩；
//   3. 咬钩进入控力小游戏：按住 空格/左键 张力上升，松开张力回落；
//      绿色目标区间随鱼的挣扎上下漂移——保持在区间内累计上鱼进度
//      （小鱼 3s / 大鱼 5s），出区间则累计逃跑计时（小 2.5s / 大 1.6s）；
//   4. 上鱼：鱼跃出水面飞向玩家，HUD 结算（鱼的大小 + 累计条数）；
//      逃跑：提示后回到岸边状态，1.5s 冷却后可再抛。
//   大鱼 30% 概率：区间更窄、漂移更快、耐力更久。
class FishingSystem : public Node {
	GDCLASS(FishingSystem, Node)

	// 湖参数（与 terrain_builder.h 保持一致）
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;

	Player *player = nullptr; // 懒查找
	std::mt19937 rng{20261001};

	// 状态机：0 岸边待机 / 1 已抛竿等咬钩 / 2 控力中 / 3 上鱼跃出 / 4 逃跑冷却
	int state = 0;
	float state_t = 0.0f;
	bool f_prev = false;
	bool near_shore = false;

public:
	int get_state() const { return state; }
	bool is_charging() const { return charging; }
	bool is_near_shore() const { return near_shore; }
	int get_caught_count() const { return caught_count; } // 存档/待办展示用

protected:

	// —— 蓄力抛竿 ——
	// 按住 F 蓄力（0→1 约 1.1s），松手按蓄力大小决定抛多远。
	// 之前是"按一下 F 固定抛 5.2m"——玩家侧身站着就把浮漂丢草地上了。
	bool charging = false;
	float charge = 0.0f;
	static constexpr float CHARGE_TIME = 1.1f; // 蓄满所需秒数
	Vector3 cast_origin;                       // 抛竿时玩家位置（走开就自动收竿）
	bool f_consumed = false;                   // 本次按压已被收竿消耗，松开前不许再蓄力
	float cast_cooldown = 0.0f;                // 收竿/结算后的短暂保护冷却（秒）

	// 地形查询（懒查找一次）：用来确认落点真的在水里
	class TerrainBuilder *terrain = nullptr;
	// 沿 dir 从 max_d 往回扫，找第一个水深够的落点；找不到返回 false
	bool find_water_spot(const Vector2 &p_from, const Vector2 &p_dir,
			float p_max_d, float p_min_d, Vector2 &r_out) const;

	// 鱼与浮漂
	Node3D *bobber = nullptr;
	Node3D *fish = nullptr;
	Vector3 bobber_pos;
	bool big_fish = false;
	float nibble_t = 0.0f;   // 距咬钩剩余秒数
	float swim_phase = 0.0f; // 游动相位

	// 鱼竿与鱼线（竿 = 细长锥形圆柱 + 握把 + 卷线器；线 = 每帧重建的面向相机薄带）
	Node3D *rod_pivot = nullptr;        // 竿的枢轴：位置=握竿手，-Z 轴 = 竿指向
	class MeshInstance3D *line = nullptr;
	float cast_swing = 0.0f;            // 抛竿挥动动画剩余时间
	void update_rod();                  // 每帧摆竿 + 拉线
	Vector3 rod_tip_world() const;

	// 控力小游戏数据
	float tension = 0.0f;      // 0..1 张力
	float zone_center = 0.5f;  // 目标区间中心（随鱼漂移）
	float zone_half = 0.16f;   // 区间半宽（大鱼更窄）
	float catch_prog = 0.0f;   // 已坚持秒数
	float catch_need = 3.0f;   // 需要坚持秒数
	float escape_t = 0.0f;     // 出区间累计
	float escape_limit = 2.5f;
	float swim_walk = 0.0f;    // 区间漂移的随机游走
	int caught_count = 0;

	// —— 鱼种系统 ——
	// 五种鱼：昼间青鳞鱼/红尾鲤，夜间月影鲈/夜鳗，全时段稀有金鳞鱼(6%)。
	// 咬钩时按昼夜+权重定 species；结算/图鉴/纪录全按种走。
	enum FishSpecies {
		FISH_QINGLIN = 0, // 青鳞鱼（昼，常见）
		FISH_HONGWEI,     // 红尾鲤（昼，中见）
		FISH_YUEYING,     // 月影鲈（夜，常见）
		FISH_YEMAN,       // 夜鳗（夜，中见）
		FISH_JINLIN,      // 金鳞鱼（稀有，昼夜都有）
		FISH_KIND_COUNT
	};
	static const char *species_name(int p_species);   // UTF-8 中文名
	static bool species_nocturnal(int p_species);     // 是否夜行种
	static Color species_tint(int p_species);         // 鱼影着色（水下的第一辨识度）

	class SkyCycle *sky = nullptr; // 懒查找（读昼夜定鱼种池）
	int pick_species(bool p_night); // 按昼夜权重定种（金鳞 6% 两池都有）

	// —— 鱼获图鉴（B 键呼出，user://fishing_records.cfg 持久化）——
	int rec_count[FISH_KIND_COUNT] = { 0 };  // 各鱼种累计
	float rec_best[FISH_KIND_COUNT] = { 0 }; // 各鱼种最长纪录（cm）
	float cur_size_cm = 0.0f;                // 本条鱼的体长
	int cur_species = FISH_QINGLIN;          // 本条鱼的鱼种
	bool b_prev = false;                     // B 键边沿检测
	bool book_open = false;
	CanvasLayer *book_layer = nullptr; // 图鉴层（懒建，玻璃面板）
	void build_book_ui();
	void refresh_book();
	void load_records();
	void save_records();

	// UI（CanvasLayer 代码搭建）
	CanvasLayer *layer = nullptr;
	Label *hint = nullptr;       // 底部提示（抛竿/咬钩/结果）
	Panel *bar_bg = nullptr; // 张力条底（玻璃托槽）
	ColorRect *bar_zone = nullptr; // 目标区间（绿）
	ColorRect *bar_cur = nullptr;  // 当前张力游标（黄）
	ColorRect *prog = nullptr;   // 上鱼进度细条
	Label *result = nullptr;     // 中央结算
	Panel *charge_bg = nullptr;   // 蓄力条底槽（玻璃托槽）
	ColorRect *charge_fill = nullptr; // 蓄力条填充（张力条上方一行）
	Label *book_label = nullptr;  // 图鉴正文（多行，refresh_book 刷新）

	// 音效
	class AudioStreamPlayer *audio_player = nullptr;
	Ref<class AudioStream> snd_cast;    // 抛竿入水 (zen_wood_drop.mp3)
	Ref<class AudioStream> snd_bite;    // 咬钩提示 (zen_select.mp3)
	Ref<class AudioStream> snd_caught;  // 上鱼欢庆 (jingle_zen_bonus.mp3)
	Ref<class AudioStream> snd_escaped; // 鱼跑了 (zen_collapse.mp3)

	void play_sound(const Ref<class AudioStream> &p_stream, float p_volume_db = -8.0f);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	FishingSystem() = default;
	~FishingSystem() override = default;

private:
	void build_ui();
	void build_tackle();     // 建浮漂+鱼影模型（初始隐藏）
	void start_cast(float p_charge);
	void start_hook();
	void finish_caught();
	void finish_escaped();
	void cancel_fishing();
	bool check_near_shore(const Vector3 &p_pos) const;
	void tick_idle(float p_delta, bool f_down, bool f_release);
	void tick_waiting(float p_delta, bool f_edge);
	void tick_hooking(float p_delta, bool hold);
	void tick_leaping(float p_delta);
	void set_result(const String &p_text, float p_seconds);
};

} // namespace godot
