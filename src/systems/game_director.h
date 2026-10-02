#pragma once

// GameDirector —— 玩法目标与结算 HUD（CanvasLayer，layer 10，压在时钟上面）。
//
// 目标：采摘浆果。BerryBush 每采一颗发出 "berry_picked" 信号，
// 本类懒连接 "berry_bushes" 组里所有浆果丛（READY 顺序无关），
// 计满 BERRY_TARGET 颗即达成，屏幕中央弹出结算。
//
// UI 三件套（全部纯代码，无场景资源）：
//   * 左上角目标进度   "目标 · 采浆果 3/10"
//   * 底部操作提示     "WASD 移动 · 空格 跳跃 · E 互动"，10 秒淡出
//   * 中央结算大字     达成时显示，8 秒后淡出（进度常驻）
//
// 找不到浆果丛/字体时各 UI 部分自动降级，不崩溃。

#include <godot_cpp/classes/canvas_layer.hpp>

namespace godot {

class Font;
class Label;

class GameDirector : public CanvasLayer {
	GDCLASS(GameDirector, CanvasLayer)

private:
	int target = 6;         // 目标采摘数（园里 3 丛 × 45s 重长，约 2 分钟节奏）
	int count = 0;          // 已采数
	bool done = false;      // 是否已达成
	bool connected = false; // 是否已连上浆果丛信号
	float hint_t = 0.0f;    // 提示存活计时（秒）
	float done_t = 0.0f;    // 结算显示计时（秒，>0 表示正在显示）

	Label *goal_label = nullptr;   // 左上角进度
	Label *hint_label = nullptr;   // 底部操作提示
	Label *center_label = nullptr; // 中央结算

	class AudioStreamPlayer *goal_player = nullptr; // 达成音效播放器
	Ref<class AudioStream> snd_goal;                // 达成音效流（goal_shimmer.mp3）
	class AudioStreamPlayer *pick_player = nullptr; // 采摘反馈音效播放器
	Ref<class AudioStream> snd_pick;                // 采摘反馈音效（organic_chime.mp3）

	void build_ui();
	void connect_bushes(); // 懒连接浆果丛（首帧再找，避开 READY 顺序问题）
	void refresh_goal();
	Ref<Font> load_cjk_font() const;

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void on_berry_picked();

	int get_count() const { return count; }
	int get_target() const { return target; }
	bool is_done() const { return done; }

	// —— 存档恢复用（SaveSystem 读档后写回）——
	void set_count(int p) {
		count = p;
		refresh_goal(); // 进度文案立即跟上
	}
	void set_done(bool p) { done = p; }
};

} // namespace godot
