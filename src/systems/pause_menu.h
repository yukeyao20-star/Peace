#pragma once

// PauseMenu —— 暂停菜单（CanvasLayer，layer 100，压在地图/结算 HUD 之上）。
//
// Esc 呼出/关闭：暂停期间 get_tree().paused=true，Player/SkyCycle/HUD 等默认
// Pausable 节点全部停摆（HUD 冻结），本节点 PROCESS_MODE_ALWAYS 自行保活。
// 场景树里必须排在 Player 之前（本帧先暂停，Player 同帧跳过，不会看到 Esc 边沿）。
//
// 三态机：idle → open（菜单显示，世界已暂停）→ resuming（菜单已隐藏但仍暂停，
// 等 Esc 松开才真正解除——防恢复当帧立即再吃到同一次按键）→ idle。
//
// Esc 全局单一归属本菜单：呼出 / 菜单内再按返回游戏 / 大地图打开时只收起地图
// （player 不再有自己的 Esc 逻辑，杜绝"关地图 + 开暂停"同帧双响应）。
//
// 功能：继续 / 音量滑条（Master 总线，线性 0..1）+ 持久化 user://settings.cfg /
//       画质高/低（低档 = 关 SSR/SSAO/Glow 三特效 + 渲染缩放 0.7，docs/PERF.md 接口）/
//       全屏切换（最大化 ↔ 全屏即时生效）/ 保存并退出（SaveSystem::save_now 兜底 quit）。
//
// 设计要点：
//   * 轮询 Esc（照 player.cpp 的 is_key_pressed 边沿套路）而非 _unhandled_input，
//     规避 GDVIRTUAL 虚函数覆盖的不确定性（MEMORY 坑 3）；
//   * 暂停前记录鼠标模式，恢复时还原（游玩 CAPTURED / 大地图 HIDDEN 都不破坏）；
//   * 中文一律 String::utf8 + NotoSansSC（MEMORY 坑 13/14），纯代码建 UI 无场景资源。

#include <godot_cpp/classes/canvas_layer.hpp>

class Player; // 全局命名空间（src/actors/player.h），PauseMenu 判定"Esc 只关地图"用

namespace godot {

class AudioStream;
class AudioStreamPlayer;
class Button;
class Control;
class Font;
class HSlider;
class Label;
class SaveSystem;
class WorldEnvironment;

class PauseMenu : public CanvasLayer {
	GDCLASS(PauseMenu, CanvasLayer)

private:
	bool ui_built = false;
	bool open = false;      // 菜单当前是否呼出（此时世界已暂停）
	bool resuming = false;  // 已请求恢复，等 Esc 松开再真正解除暂停
	bool esc_prev = false;  // Esc 上一帧状态（边沿检测）
	int restore_mouse_mode = 0; // 暂停前的 Input::MouseMode（枚举跨头文件，存 int）
	float volume_linear = 0.8f; // Master 总线音量（线性 0..1）
	bool quality_low = false;   // 低画质档：关 SSR/SSAO/Glow + scaling_3d_scale 0.7（PERF.md 低配兜底）
	bool dirty_settings = false; // 设置已改未落盘（关菜单/退出/EXIT_TREE 时写）

	Control *root = nullptr;            // 全屏遮罩 + 面板容器（呼出时才可见）
	Button *btn_resume = nullptr;
	Button *btn_quality = nullptr;
	Button *btn_fullscreen = nullptr;
	Button *btn_quit = nullptr;
	HSlider *volume_slider = nullptr;
	Label *volume_value = nullptr;
	AudioStreamPlayer *click_player = nullptr; // 按钮点击音（ui_click.mp3）
	AudioStreamPlayer *back_player = nullptr;  // 呼出/返回音（ui_back.mp3）
	Ref<AudioStream> snd_click;
	Ref<AudioStream> snd_back;

	void build_ui();
	Ref<Font> load_cjk_font() const;
	void load_settings();
	void save_settings();
	void apply_volume();
	void apply_quality();
	void refresh_fullscreen_text();
	void refresh_quality_text();
	void refresh_volume_label();
	void request_resume(); // open → resuming（真正解除在 finish_resume）
	void finish_resume();  // resuming → idle：解除暂停 + 还原鼠标
	SaveSystem *find_save() const;
	Player *find_player() const; // 场景树兄弟节点，判地图开合状态
	WorldEnvironment *find_world_env() const; // 场景树兄弟节点，画质开关落点

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void toggle();
	bool get_is_open() const { return open; }

	void on_resume_pressed();
	void on_quality_pressed();
	void on_fullscreen_pressed();
	void on_quit_pressed();
	void on_volume_changed(float p_value);
};

} // namespace godot
