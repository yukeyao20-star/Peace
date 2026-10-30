#include "update_manager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void UpdateManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_check"), &UpdateManager::start_check);
	ClassDB::bind_method(D_METHOD("start_download"), &UpdateManager::start_download);
	ClassDB::bind_method(D_METHOD("apply_patch_and_restart"), &UpdateManager::apply_patch_and_restart);
	ClassDB::bind_method(D_METHOD("dismiss_dialog"), &UpdateManager::dismiss_dialog);
	ClassDB::bind_method(D_METHOD("open_website"), &UpdateManager::open_website);

	ClassDB::bind_method(D_METHOD("_on_check_completed", "result", "response_code", "headers", "body"), &UpdateManager::_on_check_completed);
	ClassDB::bind_method(D_METHOD("_on_download_completed", "result", "response_code", "headers", "body"), &UpdateManager::_on_download_completed);
}

UpdateManager::UpdateManager() {
	set_layer(15); // 置于顶层画布，高于常规 HUD 和普通遮罩
}

void UpdateManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			load_local_version_info();
			build_ui();

			// 检查并自动预先装载本地现存的补丁包
			check_and_load_existing_patch();

			// 异步延迟启动静默检查，不阻塞首帧渲染
			check_http = memnew(HTTPRequest);
			check_http->set_name("CheckHTTP");
			check_http->set_timeout(4.0); // 4秒超时，离线或断网平滑降级
			check_http->connect("request_completed", Callable(this, "_on_check_completed"));
			add_child(check_http);

			download_http = memnew(HTTPRequest);
			download_http->set_name("DownloadHTTP");
			download_http->set_use_threads(true); // 启用多线程分块下载，界面完全不卡顿
			download_http->connect("request_completed", Callable(this, "_on_download_completed"));
			add_child(download_http);

			set_process(true);

			// 0.8秒后启动检测，避免玩家进场景第一帧有任何突兀卡顿
			Callable(this, "start_check").call_deferred();
			break;
		}
		case NOTIFICATION_PROCESS: {
			if (state == STATE_DOWNLOADING) {
				update_progress_display();
			}
			if (toast_panel && toast_panel->is_visible()) {
				float delta = (float)get_process_delta_time();
				toast_timer -= delta;
				if (toast_timer <= 0.0f) {
					toast_panel->hide();
				}
			}
			break;
		}
		default:
			break;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 本地补丁与版本信息持久化
// ─────────────────────────────────────────────────────────────────────────────
void UpdateManager::load_local_version_info() {
	String path = "user://installed_version.json";
	if (FileAccess::file_exists(path)) {
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
		if (f.is_valid()) {
			String content = f->get_as_text();
			Variant v = JSON::parse_string(content);
			if (v.get_type() == Variant::DICTIONARY) {
				Dictionary d = v;
				if (d.has("version")) current_version = d["version"];
				if (d.has("version_code")) current_version_code = (int)d["version_code"];
			}
			f->close();
		}
	}
	// 本体版本是地板：记录的是"已装补丁版本"，本体升级后必须抬到本体版本，
	// 否则旧补丁记录会把 v0.2.0 本体认成 v0.1.2，远程比对与补丁加载全错。
	current_version = GAME_VERSION;
	current_version_code = Math::max(current_version_code, GAME_VERSION_CODE);
}

void UpdateManager::save_local_version_info() {
	String path = "user://installed_version.json";
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_valid()) {
		Dictionary d;
		d["version"] = remote_version;
		d["version_code"] = remote_version_code;
		d["updated_at"] = Time::get_singleton()->get_datetime_string_from_system(false, true);
		f->store_string(JSON::stringify(d, "\t"));
		f->close();
	}
}

bool UpdateManager::check_and_load_existing_patch() {
	String patch_path = "user://patch.pck";
	if (FileAccess::file_exists(patch_path)) {
		// 【版本门控】补丁版本低于游戏本体 → 绝不加载。旧补丁会覆盖新版资源
		// （v0.1.2 的 water.gdshader 盖掉 v0.2.0 → 小溪被旧剔除逻辑吃成干河床）。
		// 版本取自 installed_version.json（与补丁包同步写入）；读不到时保守放弃加载。
		bool stale = true;
		String ver_path = "user://installed_version.json";
		if (FileAccess::file_exists(ver_path)) {
			Ref<FileAccess> vf = FileAccess::open(ver_path, FileAccess::READ);
			if (vf.is_valid()) {
				Variant v = JSON::parse_string(vf->get_as_text());
				vf->close();
				if (v.get_type() == Variant::DICTIONARY) {
					Dictionary d = v;
					int patch_code = (int)d.get("version_code", 0);
					stale = (patch_code < GAME_VERSION_CODE);
				}
			}
		}
		if (stale) {
			UtilityFunctions::print(String::utf8("[UpdateManager] 检测到低于本体版本的陈旧补丁包，已跳过加载并隔离: ")
					+ patch_path);
			// 隔离而非删除（改名 .stale，可回溯）；后续新补丁下载会整体替换 patch.pck
			Ref<DirAccess> da = DirAccess::open("user://");
			if (da.is_valid() && da->file_exists("patch.pck")) {
				static int stale_seq = 0;
				String dst = String("patch.pck.stale") + String::num_int64(++stale_seq);
				if (da->rename("patch.pck", dst) != Error::OK) {
					// 改名失败也无妨：下次启动版本门控仍会拒载
				}
			}
			return false;
		}
		bool ok = ProjectSettings::get_singleton()->load_resource_pack(patch_path, true);
		if (ok) {
			UtilityFunctions::print(String::utf8("[UpdateManager] 成功加载本地热更新资源包: ") + patch_path);
			return true;
		}
	}
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// 网络检测与下载流
// ─────────────────────────────────────────────────────────────────────────────
void UpdateManager::start_check() {
	// 【工业规范环境门控】本地开发调试 / 编辑器运行环境下，禁止启动时强制拦截更新
	if (OS::get_singleton()->is_debug_build() || Engine::get_singleton()->is_editor_hint()) {
		UtilityFunctions::print(String::utf8("[UpdateManager] 本地开发/调试环境运行中，跳过启动自动下载更新。"));
		return;
	}

	if (!check_http) return;
	state = STATE_CHECKING;
	// 请求 VPS 官网热更新清单（附带时间戳防 CDN / 浏览器中间代理缓存）
	String url = "https://yukeyao.sbs/update/version.json?t=" + String::num_int64((int64_t)Time::get_singleton()->get_unix_time_from_system());
	PackedStringArray headers;
	headers.push_back("User-Agent: PeaceGameClient/1.0");
	check_http->request(url, headers);
}

void UpdateManager::_on_check_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_response_code != 200) {
		// 网络不通或未开服，静默退场，离线正常玩游戏
		state = STATE_IDLE;
		return;
	}

	String json_str = p_body.get_string_from_utf8();
	Variant v = JSON::parse_string(json_str);
	if (v.get_type() != Variant::DICTIONARY) {
		state = STATE_IDLE;
		return;
	}

	Dictionary d = v;
	remote_version = d.get("version", current_version);
	remote_version_code = d.get("version_code", current_version_code);
	update_title = d.get("title", String::utf8("全新版本更新"));
	if (d.has("patch_url")) {
		patch_download_url = d["patch_url"];
	} else if (d.has("download_url")) {
		patch_download_url = d["download_url"];
	} else {
		patch_download_url = "";
	}
	if (d.has("patch_size")) {
		patch_size_str = d["patch_size"];
	} else if (d.has("size")) {
		int64_t bytes = (int64_t)d["size"];
		patch_size_str = String::num((double)bytes / (1024.0 * 1024.0), 1) + " MB";
	}
	full_download_url = d.get("full_download_url", "https://yukeyao.sbs/#download");

	changelog_items.clear();
	if (d.has("changelog") && d["changelog"].get_type() == Variant::ARRAY) {
		Array arr = d["changelog"];
		for (int i = 0; i < arr.size(); ++i) {
			changelog_items.push_back(arr[i]);
		}
	}

	if (remote_version_code > current_version_code) {
		state = STATE_UPDATE_AVAILABLE;
		show_update_dialog();
	} else {
		state = STATE_UP_TO_DATE;
		show_toast(String::utf8("🌿 已是最新版本 (v") + current_version + String::utf8(")"), 2.2f);
	}
}

void UpdateManager::start_download() {
	if (patch_download_url.is_empty()) {
		open_website();
		return;
	}

	state = STATE_DOWNLOADING;
	btn_update->set_disabled(true);
	btn_skip->set_disabled(true);
	btn_update->set_text(String::utf8("正在下载补丁..."));
	progress_container->show();

	// 下载到临时文件
	String temp_file = OS::get_singleton()->get_user_data_dir() + "/patch_temp.pck";
	download_http->set_download_file(temp_file);
	download_poll_timer = 0.0f;
	last_downloaded_bytes = 0;

	PackedStringArray headers;
	headers.push_back("User-Agent: PeaceGameClient/1.0");
	download_http->request(patch_download_url, headers);
}

void UpdateManager::update_progress_display() {
	if (!download_http) return;
	int downloaded = download_http->get_downloaded_bytes();
	int total = download_http->get_body_size();

	float delta = (float)get_process_delta_time();
	download_poll_timer += delta;
	if (download_poll_timer >= 0.5f) {
		float speed_bytes = (float)(downloaded - last_downloaded_bytes) / download_poll_timer;
		current_speed_kb = speed_bytes / 1024.0f;
		last_downloaded_bytes = downloaded;
		download_poll_timer = 0.0f;
	}

	float pct = 0.0f;
	if (total > 0) {
		pct = (float)downloaded / (float)total * 100.0f;
		progress_bar->set_value(pct);

		float dl_mb = (float)downloaded / (1024.0f * 1024.0f);
		float tot_mb = (float)total / (1024.0f * 1024.0f);

		String stat = String::num(dl_mb, 1) + " MB / " + String::num(tot_mb, 1) + " MB";
		if (current_speed_kb > 1024.0f) {
			stat += String::utf8("  ·  ") + String::num(current_speed_kb / 1024.0f, 1) + " MB/s";
		} else {
			stat += String::utf8("  ·  ") + String::num((int)current_speed_kb) + " KB/s";
		}
		progress_stats_label->set_text(stat);
	} else {
		float dl_mb = (float)downloaded / (1024.0f * 1024.0f);
		progress_stats_label->set_text(String::utf8("已下载 ") + String::num(dl_mb, 1) + " MB");
	}
}

void UpdateManager::_on_download_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_response_code != 200) {
		state = STATE_FAILED;
		btn_update->set_disabled(false);
		btn_skip->set_disabled(false);
		btn_update->set_text(String::utf8("重试下载"));
		show_toast(String::utf8("❌ 下载中断，请检查网络或前往官网"), 4.0f);
		return;
	}

	String user_dir = OS::get_singleton()->get_user_data_dir();
	String temp_path = user_dir + "/patch_temp.pck";
	String final_path = user_dir + "/patch.pck";

	Ref<DirAccess> da = DirAccess::open(user_dir);
	if (da.is_valid()) {
		if (da->file_exists("patch.pck")) {
			da->remove("patch.pck");
		}
		da->rename("patch_temp.pck", "patch.pck");
	}

	save_local_version_info();
	current_version = remote_version;
	current_version_code = remote_version_code;

	// 立即热重载资源包！
	bool loaded = ProjectSettings::get_singleton()->load_resource_pack("user://patch.pck", true);

	state = STATE_READY_TO_APPLY;
	progress_bar->set_value(100.0);
	progress_stats_label->set_text(String::utf8("🌿 补丁已就绪！全新内容已无缝注入生效"));
	btn_update->set_disabled(false);
	btn_update->set_text(String::utf8("进入游戏"));

	// 重新绑定为完成关闭并进入游戏
	if (btn_update->is_connected("pressed", Callable(this, "start_download"))) {
		btn_update->disconnect("pressed", Callable(this, "start_download"));
	}
	if (!btn_update->is_connected("pressed", Callable(this, "apply_patch_and_restart"))) {
		btn_update->connect("pressed", Callable(this, "apply_patch_and_restart"));
	}
	btn_skip->hide();
}

void UpdateManager::apply_patch_and_restart() {
	if (state == STATE_READY_TO_APPLY) {
		dismiss_dialog();
		show_toast(String::utf8("🌸 欢迎体验全新版本！"), 3.0f);
	}
}

void UpdateManager::dismiss_dialog() {
	if (root_container) {
		root_container->hide();
	}
}

void UpdateManager::open_website() {
	OS::get_singleton()->shell_open(full_download_url.is_empty() ? "https://yukeyao.sbs/" : full_download_url);
}

// ─────────────────────────────────────────────────────────────────────────────
// 《Peace》专属治愈系自然手账风格 UI 搭建 (暖白羊皮纸、胡桃木边框、湖水碧蓝进度条)
// ─────────────────────────────────────────────────────────────────────────────
void UpdateManager::build_ui() {
	Ref<Font> font = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");

	root_container = memnew(Control);
	root_container->set_name("UpdateDialogRoot");
	root_container->set_anchors_preset(Control::PRESET_FULL_RECT);
	add_child(root_container);

	// 1. 全屏沉浸式暗场遮罩 (温暖柔和的暗色微光遮罩)
	backdrop = memnew(ColorRect);
	backdrop->set_anchors_preset(Control::PRESET_FULL_RECT);
	backdrop->set_color(Color(0.08f, 0.09f, 0.08f, 0.65f));
	root_container->add_child(backdrop);

	// 2. 主卡片容器 (580px 宽，优雅手账卡片)
	card_panel = memnew(PanelContainer);
	card_panel->set_name("CardPanel");
	card_panel->set_anchors_preset(Control::PRESET_CENTER);
	card_panel->set_offset(SIDE_LEFT, -290.0f);
	card_panel->set_offset(SIDE_RIGHT, 290.0f);
	card_panel->set_offset(SIDE_TOP, -225.0f);
	card_panel->set_offset(SIDE_BOTTOM, 225.0f);
	root_container->add_child(card_panel);

	// 暖白素雅手账质感底板：象牙羊皮纸白、胡桃木色温润双描边、柔和自然阴影
	Ref<StyleBoxFlat> card_style;
	card_style.instantiate();
	card_style->set_bg_color(Color(0.96f, 0.94f, 0.90f, 0.98f)); // 暖白羊皮纸
	card_style->set_border_color(Color(0.72f, 0.64f, 0.54f, 0.85f)); // 浅胡桃木质边框
	card_style->set_border_width_all(2);
	card_style->set_corner_radius_all(16);
	card_style->set_shadow_color(Color(0.12f, 0.10f, 0.08f, 0.28f));
	card_style->set_shadow_size(20);
	card_style->set_shadow_offset(Vector2(0, 8));
	card_style->set_content_margin(SIDE_LEFT, 28.0f);
	card_style->set_content_margin(SIDE_RIGHT, 28.0f);
	card_style->set_content_margin(SIDE_TOP, 24.0f);
	card_style->set_content_margin(SIDE_BOTTOM, 22.0f);
	card_panel->add_theme_stylebox_override("panel", card_style);

	main_vbox = memnew(VBoxContainer);
	main_vbox->add_theme_constant_override("separation", 12);
	card_panel->add_child(main_vbox);

	// 顶部头部排版
	HBoxContainer *header_box = memnew(HBoxContainer);
	header_box->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
	main_vbox->add_child(header_box);

	// 田园草木绿胶囊徽标
	badge_label = memnew(Label);
	badge_label->set_text(String::utf8(" 🌿 新版本速递 "));
	badge_label->add_theme_font_size_override("font_size", 12);
	badge_label->add_theme_color_override("font_color", Color(0.20f, 0.44f, 0.28f)); // 深森林青绿
	Ref<StyleBoxFlat> badge_style;
	badge_style.instantiate();
	badge_style->set_bg_color(Color(0.85f, 0.91f, 0.83f, 0.95f)); // 浅木草芽色
	badge_style->set_border_color(Color(0.68f, 0.78f, 0.65f, 0.90f));
	badge_style->set_border_width_all(1);
	badge_style->set_corner_radius_all(999);
	badge_style->set_content_margin(SIDE_LEFT, 10.0f);
	badge_style->set_content_margin(SIDE_RIGHT, 10.0f);
	badge_style->set_content_margin(SIDE_TOP, 3.0f);
	badge_style->set_content_margin(SIDE_BOTTOM, 3.0f);
	badge_label->add_theme_stylebox_override("theme_override_styles/normal", badge_style);
	if (font.is_valid()) badge_label->add_theme_font_override("font", font);
	header_box->add_child(badge_label);

	// 标题：典雅沉稳深原木色
	title_label = memnew(Label);
	title_label->set_text(String::utf8("湖畔家园版本更新"));
	title_label->add_theme_font_size_override("font_size", 21);
	title_label->add_theme_color_override("font_color", Color(0.24f, 0.20f, 0.16f)); // 浓郁深木色
	if (font.is_valid()) title_label->add_theme_font_override("font", font);
	main_vbox->add_child(title_label);

	// 版本副标题：温和柔灰棕色
	version_sub_label = memnew(Label);
	version_sub_label->set_text(String::utf8("版本 v0.1.2  ·  无缝热加载无需重装"));
	version_sub_label->add_theme_font_size_override("font_size", 13);
	version_sub_label->add_theme_color_override("font_color", Color(0.55f, 0.49f, 0.42f));
	if (font.is_valid()) version_sub_label->add_theme_font_override("font", font);
	main_vbox->add_child(version_sub_label);

	// 纤细素雅木刻分隔线
	ColorRect *divider = memnew(ColorRect);
	divider->set_custom_minimum_size(Vector2(0, 1));
	divider->set_color(Color(0.82f, 0.76f, 0.68f, 0.75f));
	main_vbox->add_child(divider);

	// 更新日志区域（温润的米色内凹浅底槽）
	PanelContainer *log_panel = memnew(PanelContainer);
	Ref<StyleBoxFlat> log_style;
	log_style.instantiate();
	log_style->set_bg_color(Color(0.91f, 0.88f, 0.83f, 0.85f)); // 浅素砂纸底色
	log_style->set_border_color(Color(0.80f, 0.75f, 0.68f, 0.60f));
	log_style->set_border_width_all(1);
	log_style->set_corner_radius_all(8);
	log_style->set_content_margin(SIDE_LEFT, 14.0f);
	log_style->set_content_margin(SIDE_RIGHT, 14.0f);
	log_style->set_content_margin(SIDE_TOP, 12.0f);
	log_style->set_content_margin(SIDE_BOTTOM, 12.0f);
	log_panel->add_theme_stylebox_override("panel", log_style);
	main_vbox->add_child(log_panel);

	changelog_box = memnew(VBoxContainer);
	changelog_box->add_theme_constant_override("separation", 6);
	log_panel->add_child(changelog_box);

	// 进度条区域（初始隐藏）
	progress_container = memnew(VBoxContainer);
	progress_container->add_theme_constant_override("separation", 6);
	main_vbox->add_child(progress_container);

	progress_bar = memnew(ProgressBar);
	progress_bar->set_custom_minimum_size(Vector2(0, 8));
	progress_bar->set_show_percentage(false);
	Ref<StyleBoxFlat> pbg_style;
	pbg_style.instantiate();
	pbg_style->set_bg_color(Color(0.85f, 0.82f, 0.76f, 0.9f));
	pbg_style->set_corner_radius_all(4);
	Ref<StyleBoxFlat> pfg_style;
	pfg_style.instantiate();
	pfg_style->set_bg_color(Color(0.28f, 0.62f, 0.54f)); // 碧蓝清澈湖水绿
	pfg_style->set_corner_radius_all(4);
	progress_bar->add_theme_stylebox_override("background", pbg_style);
	progress_bar->add_theme_stylebox_override("fill", pfg_style);
	progress_container->add_child(progress_bar);

	progress_stats_label = memnew(Label);
	progress_stats_label->set_text(String::utf8("准备下载..."));
	progress_stats_label->add_theme_font_size_override("font_size", 12);
	progress_stats_label->add_theme_color_override("font_color", Color(0.48f, 0.42f, 0.36f));
	if (font.is_valid()) progress_stats_label->add_theme_font_override("font", font);
	progress_container->add_child(progress_stats_label);
	progress_container->hide();

	// 底部操作按钮栏
	btn_box = memnew(HBoxContainer);
	btn_box->set_alignment(BoxContainer::ALIGNMENT_END);
	btn_box->add_theme_constant_override("separation", 10);
	main_vbox->add_child(btn_box);

	// 前往官网按钮 (次级操作)
	btn_website = memnew(Button);
	btn_website->set_text(String::utf8("前往官网"));
	btn_website->set_flat(true);
	btn_website->add_theme_color_override("font_color", Color(0.50f, 0.44f, 0.38f));
	btn_website->add_theme_color_override("font_hover_color", Color(0.25f, 0.20f, 0.16f));
	btn_website->add_theme_font_size_override("font_size", 13);
	if (font.is_valid()) btn_website->add_theme_font_override("font", font);
	btn_website->connect("pressed", Callable(this, "open_website"));
	btn_box->add_child(btn_website);

	// 稍后再说按钮 (次级操作)
	btn_skip = memnew(Button);
	btn_skip->set_text(String::utf8("暂不更新"));
	btn_skip->set_custom_minimum_size(Vector2(88, 36));
	Ref<StyleBoxFlat> skip_btn_style;
	skip_btn_style.instantiate();
	skip_btn_style->set_bg_color(Color(0.92f, 0.89f, 0.84f, 0.95f));
	skip_btn_style->set_border_color(Color(0.78f, 0.72f, 0.64f, 0.80f));
	skip_btn_style->set_border_width_all(1);
	skip_btn_style->set_corner_radius_all(8);
	btn_skip->add_theme_stylebox_override("normal", skip_btn_style);
	btn_skip->add_theme_font_size_override("font_size", 13);
	btn_skip->add_theme_color_override("font_color", Color(0.42f, 0.36f, 0.30f));
	if (font.is_valid()) btn_skip->add_theme_font_override("font", font);
	btn_skip->connect("pressed", Callable(this, "dismiss_dialog"));
	btn_box->add_child(btn_skip);

	// 立即更新主按钮 (温润森林绿主色)
	btn_update = memnew(Button);
	btn_update->set_text(String::utf8("一键热更新"));
	btn_update->set_custom_minimum_size(Vector2(120, 36));
	Ref<StyleBoxFlat> update_btn_style;
	update_btn_style.instantiate();
	update_btn_style->set_bg_color(Color(0.28f, 0.56f, 0.38f, 0.98f)); // 优雅森林草木绿
	update_btn_style->set_border_color(Color(0.22f, 0.46f, 0.30f, 0.90f));
	update_btn_style->set_border_width_all(1);
	update_btn_style->set_corner_radius_all(8);
	update_btn_style->set_shadow_color(Color(0.20f, 0.40f, 0.28f, 0.20f));
	update_btn_style->set_shadow_size(6);
	btn_update->add_theme_stylebox_override("normal", update_btn_style);
	btn_update->add_theme_font_size_override("font_size", 13);
	btn_update->add_theme_color_override("font_color", Color(0.98f, 0.98f, 0.96f));
	if (font.is_valid()) btn_update->add_theme_font_override("font", font);
	btn_update->connect("pressed", Callable(this, "start_download"));
	btn_box->add_child(btn_update);

	// 3. 右上角轻量级提示胶囊 (Toast)
	toast_panel = memnew(PanelContainer);
	toast_panel->set_anchors_preset(Control::PRESET_TOP_RIGHT);
	toast_panel->set_offset(SIDE_LEFT, -240.0f);
	toast_panel->set_offset(SIDE_RIGHT, -20.0f);
	toast_panel->set_offset(SIDE_TOP, 16.0f);
	toast_panel->set_offset(SIDE_BOTTOM, 50.0f);
	Ref<StyleBoxFlat> toast_style;
	toast_style.instantiate();
	toast_style->set_bg_color(Color(0.96f, 0.94f, 0.88f, 0.95f));
	toast_style->set_border_color(Color(0.72f, 0.64f, 0.54f, 0.80f));
	toast_style->set_border_width_all(1);
	toast_style->set_corner_radius_all(16);
	toast_style->set_content_margin(SIDE_LEFT, 14.0f);
	toast_style->set_content_margin(SIDE_RIGHT, 14.0f);
	toast_panel->add_theme_stylebox_override("panel", toast_style);
	add_child(toast_panel);

	toast_label = memnew(Label);
	toast_label->add_theme_font_size_override("font_size", 12);
	toast_label->add_theme_color_override("font_color", Color(0.30f, 0.26f, 0.20f));
	if (font.is_valid()) toast_label->add_theme_font_override("font", font);
	toast_panel->add_child(toast_label);
	toast_panel->hide();

	// 初始弹窗隐藏
	root_container->hide();
}

void UpdateManager::show_update_dialog() {
	if (!root_container) return;

	Ref<Font> font = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");

	title_label->set_text(update_title);
	version_sub_label->set_text(String::utf8("版本 v") + remote_version +
			(patch_size_str.is_empty() ? "" : (String::utf8("  ·  补丁体积：") + patch_size_str)) +
			String::utf8("  ·  无缝热加载无需重装"));

	// 刷新更新日志条目
	for (int i = changelog_box->get_child_count() - 1; i >= 0; --i) {
		Node *c = changelog_box->get_child(i);
		c->queue_free();
	}

	for (int i = 0; i < changelog_items.size(); ++i) {
		HBoxContainer *item_box = memnew(HBoxContainer);
		item_box->add_theme_constant_override("separation", 8);

		Label *dot = memnew(Label);
		dot->set_text(String::utf8("•"));
		dot->add_theme_color_override("font_color", Color(0.32f, 0.58f, 0.42f)); // 自然草木绿
		dot->add_theme_font_size_override("font_size", 14);
		if (font.is_valid()) dot->add_theme_font_override("font", font);

		Label *text = memnew(Label);
		text->set_text(changelog_items[i]);
		text->add_theme_color_override("font_color", Color(0.32f, 0.28f, 0.22f)); // 温暖深灰棕
		text->add_theme_font_size_override("font_size", 13);
		if (font.is_valid()) text->add_theme_font_override("font", font);

		item_box->add_child(dot);
		item_box->add_child(text);
		changelog_box->add_child(item_box);
	}

	btn_update->set_text(String::utf8("一键热更新"));
	btn_update->set_disabled(false);
	btn_skip->show();
	btn_skip->set_disabled(false);
	progress_container->hide();

	if (btn_update->is_connected("pressed", Callable(this, "apply_patch_and_restart"))) {
		btn_update->disconnect("pressed", Callable(this, "apply_patch_and_restart"));
	}
	if (!btn_update->is_connected("pressed", Callable(this, "start_download"))) {
		btn_update->connect("pressed", Callable(this, "start_download"));
	}

	root_container->show();
}

void UpdateManager::show_toast(const String &p_text, float p_duration) {
	if (!toast_panel || !toast_label) return;
	toast_label->set_text(p_text);
	toast_timer = p_duration;
	toast_panel->show();
}

} // namespace godot
