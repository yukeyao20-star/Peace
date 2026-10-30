#pragma once

// GameDirector —— 每日待办清单与结算 HUD（CanvasLayer，layer 10，压在时钟上面）。
//
// v0.2 目标链：把"采 6 颗浆果"单目标升级为逐日推进的待办清单——
//   * 第 1 天：采浆果 ×6（新手引导，行为与旧版一致）；
//   * 第 N≥2 天：从 采浆果/钓鱼/足球进球/打井水/开木箱 五类中确定性选
//     3 项（以天数为种子，同一天读档重进清单一致）；
//   * 当日全部完成 → 中央结算 + 提示"回床上睡觉迎接新的一天"；
//   * SkyCycle day_count 跨天（自然午夜或床上睡觉）→ 自动发布新清单。
//
// 事件来源（全部懒连接、逐帧重试，READY 顺序无关，照 berry_picked 套路）：
//   * BerryBush    组 "berry_bushes"        信号 berry_picked
//   * FishingSystem find_child 按名         信号 fish_caught
//   * SoccerBall   find_child 按名          信号 goal_scored
//   * Well / Chest 组 "interactables" 里 cast 信号 water_drawn / chest_opened
//
// UI 与引导（全部纯代码，无场景资源）：
//   * 左上角待办清单（多行，完成的行前缀 ✓）
//   * 底部操作提示     "WASD 移动 · 空格 跳跃 · E 互动"——新档驻留到玩家
//     完成首次交互才淡出；老玩家（读档有进度/天数>1）常规 10 秒淡出
//   * 目标方向指示     "▼ 任务名" 世界空间浮动牌悬在清单第一个未完成项的
//     目标上方（浆果就近换丛、钓鱼指湖心、足球跟球走、贴近自动收牌）
//   * 中央大字         当日完成 / 新一天发布，数秒后淡出
//
// 存档：get_daily_serial() 输出 "day:type,progress,target;..."，
// apply_daily_serial() 恢复（同时静默"新的一天"横幅）；
// 旧档只有 director_count/done 时由 SaveSystem 拼出第 1 天浆果序列走同一路径。
//
// 找不到浆果丛/字体时各 UI 部分自动降级，不崩溃。

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <vector>

namespace godot {

class Font;
class Label;
class Label3D;
class Node3D;
class PanelContainer;

class GameDirector : public CanvasLayer {
	GDCLASS(GameDirector, CanvasLayer)

public:
	enum TaskType {
		TASK_BERRY = 0, // 采浆果（berry_picked，按颗计）
		TASK_FISH,      // 钓鱼（fish_caught）
		TASK_GOAL,      // 足球进球（goal_scored）
		TASK_WELL,      // 打井水（water_drawn）
		TASK_CHEST,     // 开木箱（chest_opened）
		TASK_HARVEST,   // 照看菜畦：收获一茬作物（crop_harvested，按茬计）
		TASK_FEED,      // 喂农场动物（animal_fed）
		TASK_KIND_COUNT
	};
	struct DailyTask {
		int type = TASK_BERRY;
		int target = 1;
		int progress = 0;
	};

private:
	int cur_day = 1;              // 当前清单对应的天数（跟随 SkyCycle day_count）
	std::vector<DailyTask> tasks; // 当日待办（第 1 天 1 项，第 2 天起 3 项）
	bool day_done = false;        // 当日清单是否全部完成
	bool suppress_announce = false; // 读档恢复当天时不弹"新的一天"横幅

	// 旧版单目标兼容字段（存档 director_count/done 的镜像，第 1 天即浆果任务）
	int target = 6;
	int count = 0;
	bool done = false;

	bool connected = false;        // 已连上全部浆果丛信号
	bool sources_connected = false; // 已连上钓鱼/足球/井/箱信号
	bool farm_spawned = false;     // 家园菜畦已生成
	String pending_farm_serial;    // 菜畦存档待回放（菜畦生成前一帧收到时先存着）
	mutable String last_farm_serial; // 最近一次成功聚合的菜畦序列快照（退出拆树期组扫描不可用时兜底）
	float hint_t = 0.0f;           // 提示存活计时（秒）
	bool hint_persistent = false;  // 操作提示驻留中（完成首次交互前不淡出）
	float done_t = 0.0f;           // 中央大字显示计时（秒，>0 表示正在显示）

	Label *goal_label = nullptr;   // 左上角待办清单
	Label *hint_label = nullptr;   // 底部操作提示
	PanelContainer *hint_panel = nullptr; // 提示玻璃底板（淡出作用于它）
	Label *center_label = nullptr; // 中央结算大字

	// —— 目标方向指示（世界空间浮动标记，挂场景根；ROADMAP #3①）——
	Label3D *goal_marker = nullptr;   // 标记牌（懒建，CJK 字体缺失时不立牌）
	NodePath marker_target_path;      // 当前目标节点绝对路径（空 = 湖心特判/未解析）
	NodePath player_path;             // Player 缓存路径（免逐帧 find_child 全树扫描）
	int marker_task_type = -1;        // 当前标记指向的任务类型（变更才改字/重解析）
	float marker_t = 0.0f;            // 浮动动画计时

	class AudioStreamPlayer *goal_player = nullptr; // 达成音效播放器
	Ref<class AudioStream> snd_goal;                // 达成音效流（goal_shimmer.mp3）
	class AudioStreamPlayer *pick_player = nullptr; // 进度反馈音效播放器
	Ref<class AudioStream> snd_pick;                // 进度反馈音效（organic_chime.mp3）

	void build_ui();
	void connect_bushes();   // 懒连接浆果丛（首帧再找，避开 READY 顺序问题）
	void connect_sources();  // 懒连接钓鱼/足球/井/箱（逐帧重试直到全部连上）
	void spawn_farm_plots(); // 园内生成 4 块菜畦（锚定园内第一丛浆果，确定性布局）
	void refresh_goal();
	Ref<Font> load_cjk_font() const;

	void ensure_goal_marker();                 // 懒建世界空间目标标记
	Node3D *resolve_marker_target(int p_type); // 按任务类型解析目标节点（钓鱼无节点走湖心特判）
	void update_goal_marker();                 // 逐帧：目标失效重解析 / 贴近收牌 / 浮动
	void note_first_interaction();             // 完成首次交互 → 结束操作提示驻留

	void roll_day_tasks(int p_day);       // 按天数种子确定性生成当日清单
	void check_day_rollover();            // 轮询 SkyCycle day_count，跨天换清单
	void mark_task_progress(int p_type);  // 某类待办 +1 并检查当日是否完成
	void announce_center(const String &p_text, float p_seconds);
	void sync_legacy_fields();            // 把清单状态镜像回 count/target/done
	void apply_farm_serial_to_plots(const String &p_serial); // 序列逐块下发到 farm_plots 组

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void on_berry_picked();
	void on_fish_caught();
	void on_goal_scored();
	void on_water_drawn();
	void on_chest_opened();
	void on_chest_looted();
	void on_plot_harvested();
	void on_animal_fed();

	int get_count() const { return count; }
	int get_target() const { return target; }
	bool is_done() const { return done; }
	int get_current_day() const { return cur_day; }

	// —— 每日清单存档序列（SaveSystem 读写）——
	String get_daily_serial() const;
	void apply_daily_serial(const String &p_serial);

	// —— 菜畦存档序列（SaveSystem 读写；"state,%,浇灌;..." 4 块，顺序=生成顺序）——
	String get_farm_serial() const;
	void apply_farm_serial(const String &p_serial);
};

} // namespace godot
