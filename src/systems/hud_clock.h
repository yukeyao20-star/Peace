#pragma once

// HUDClock —— 屏幕顶部 HUD 时钟。
//
// 每帧从 SkyCycle 读 time_of_day（0..1），换算成游戏内时刻（00:00~23:59），
// 与 SkyCycle::get_phase_name()（"白天 · 晴" 这类时段+天气短语）一起显示
// 在画面顶部中央，行首附程序化绘制的月相图标（按 get_moon_phase() 实时
// 明暗建模），行中附当日月相名（八相）：
// 如 "[满月图标] 08:37 · 白天 · 晴 · 满月 · 第 1 天 · 20°C"。
//
// 纯显示组件：找不到 SkyCycle 时自动隐藏，不参与任何游戏逻辑。
// 场景里挂在 World 根下（CanvasLayer，layer 1），sky_cycle_path 指向 SkyCycle。

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace godot {

class HBoxContainer;
class Label;
class SkyCycle;
class TextureRect;

class HUDClock : public CanvasLayer {
	GDCLASS(HUDClock, CanvasLayer)

private:
	NodePath sky_cycle_path;
	SkyCycle *cycle = nullptr; // 懒查找（READY 顺序无关）
	HBoxContainer *box = nullptr;   // [月相图标][时钟文字] 一行水平居中
	Label *label = nullptr;
	TextureRect *moon_icon = nullptr;
	Ref<ImageTexture> moon_tex;
	int moon_key = -1; // 已生成贴图的月相量化 key（相变才重绘，不逐帧重画）

	// 文本刷新门控：显示值（时刻分钟/第几天/气温/月相名/时段天气短语）全部
	// 未变时跳过 set_text（文本不变时 Label 重建布局是纯浪费）。
	// phase_name 参与比较以保证天气切换当帧就能反映到文字。
	int last_min = -1;
	int last_day = -1;
	int last_temp = -32768;
	int last_midx = -1;
	String last_phase;

	void refresh(); // 依据 SkyCycle 当前状态刷新文字；找不到就整层隐藏
	void update_moon_icon(float phase); // 按 phase 0..1 重绘月相图标贴图

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void set_sky_cycle_path(const NodePath &p) { sky_cycle_path = p; }
	NodePath get_sky_cycle_path() const { return sky_cycle_path; }
};

} // namespace godot
