#pragma once

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class UpdateManager : public CanvasLayer {
	GDCLASS(UpdateManager, CanvasLayer)

public:
	enum UpdateState {
		STATE_IDLE,
		STATE_CHECKING,
		STATE_UP_TO_DATE,
		STATE_UPDATE_AVAILABLE,
		STATE_DOWNLOADING,
		STATE_READY_TO_APPLY,
		STATE_FAILED
	};

private:
	UpdateState state = STATE_IDLE;
	String current_version = "0.1.0";
	int current_version_code = 100;

	// 游戏本体编译期版本（与 project.godot config/version 同步，升版一起改）。
	// 用户目录里的旧版 patch.pck（version_code < 本体）绝不能加载——补丁内的旧
	// 资源会覆盖新版同名资源（water.gdshader/world.tscn 等），造成"小溪消失"
	// 这类诡异回退（2026-10-24 实锤：v0.1.2 patch.pck 把 v0.2.0 的新水面着色器
	// 盖回旧版，is_stream 剔除逻辑把溪流吃成干河床）。
	static constexpr const char *GAME_VERSION = "0.2.0";
	static constexpr int GAME_VERSION_CODE = 200;

	// 远程元数据
	String remote_version;
	int remote_version_code = 0;
	String update_title;
	PackedStringArray changelog_items;
	String patch_download_url;
	String patch_size_str;
	String full_download_url;

	// 网络请求节点
	HTTPRequest *check_http = nullptr;
	HTTPRequest *download_http = nullptr;

	// UI 组件
	Control *root_container = nullptr;
	ColorRect *backdrop = nullptr;
	PanelContainer *card_panel = nullptr;
	VBoxContainer *main_vbox = nullptr;

	Label *badge_label = nullptr;
	Label *title_label = nullptr;
	Label *version_sub_label = nullptr;
	VBoxContainer *changelog_box = nullptr;

	// 进度相关
	Control *progress_container = nullptr;
	ProgressBar *progress_bar = nullptr;
	Label *progress_stats_label = nullptr;

	// 底部操作按钮栏
	HBoxContainer *btn_box = nullptr;
	Button *btn_update = nullptr;
	Button *btn_skip = nullptr;
	Button *btn_website = nullptr;

	// 轻提示（Toast）
	PanelContainer *toast_panel = nullptr;
	Label *toast_label = nullptr;
	float toast_timer = 0.0f;

	// 下载测速估算
	float download_poll_timer = 0.0f;
	int64_t last_downloaded_bytes = 0;
	float current_speed_kb = 0.0f;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	UpdateManager();
	~UpdateManager() override = default;

	void start_check();
	void start_download();
	void apply_patch_and_restart();
	void dismiss_dialog();
	void open_website();

	void _on_check_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_download_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

private:
	void build_ui();
	void setup_design_styles();
	void show_update_dialog();
	void show_toast(const String &p_text, float p_duration = 3.0f);
	void update_progress_display();
	void load_local_version_info();
	void save_local_version_info();
	bool check_and_load_existing_patch();
};

} // namespace godot
