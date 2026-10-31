#include "update_manager.h"

#include "ui_style.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
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
			if (OS::get_singleton()->get_environment("ZCODE_SHOW_UPDATE") == "1") {
				update_title = String::utf8("Peace · 版本速递");
				remote_version = "0.2.1";
				patch_size_str = "86 MB";
				changelog_items.push_back(String::utf8("湖盆加深到 4.65m，湖里有了鱼群"));
				changelog_items.push_back(String::utf8("深水自动游泳：空格上浮 / Ctrl 下潜"));
				changelog_items.push_back(String::utf8("夜间月光会在水面拖出一条银色光路"));
				changelog_items.push_back(String::utf8("鸭群多了两只小鸭，青蛙会跳进水里"));
				show_update_dialog();
			}

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
	Ref<Font> chalk = uistyle::chalk_font();
	Ref<Font> heavy = uistyle::heavy_cjk(900, 0.28f);
	const Color ink = uistyle::ink_brown();

	root_container = memnew(Control);
	root_container->set_name("UpdateDialogRoot");
	root_container->set_anchors_preset(Control::PRESET_FULL_RECT);
	add_child(root_container);

	// —— 暗场：一层薄暗 + 四周晕影，而不是整块黑幕 ——
	backdrop = memnew(ColorRect);
	backdrop->set_anchors_preset(Control::PRESET_FULL_RECT);
	backdrop->set_color(Color(0.035f, 0.040f, 0.033f, 0.52f));
	root_container->add_child(backdrop);

	TextureRect *vignette = memnew(TextureRect);
	vignette->set_anchors_preset(Control::PRESET_FULL_RECT);
	vignette->set_texture(uistyle::vignette_texture());
	vignette->set_modulate(Color(1.0f, 1.0f, 1.0f, 0.95f));
	vignette->set_stretch_mode(TextureRect::STRETCH_SCALE);
	vignette->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root_container->add_child(vignette);

	// —— 木板：平铺木纹、方角，上面钉着一张纸 ——
	// 板子投影：让整块木板从暗场里浮起来
	PanelContainer *board_shadow = memnew(PanelContainer);
	board_shadow->set_anchors_preset(Control::PRESET_CENTER);
	board_shadow->set_offset(SIDE_LEFT, -276.0f);
	board_shadow->set_offset(SIDE_RIGHT, 276.0f);
	board_shadow->set_offset(SIDE_TOP, -212.0f);
	board_shadow->set_offset(SIDE_BOTTOM, 212.0f);
	Ref<StyleBoxFlat> shadow_style;
	shadow_style.instantiate();
	shadow_style->set_bg_color(Color(0, 0, 0, 0));
	shadow_style->set_corner_radius_all(6);
	shadow_style->set_shadow_color(Color(0.01f, 0.01f, 0.006f, 0.55f));
	shadow_style->set_shadow_size(34);
	shadow_style->set_shadow_offset(Vector2(0, 14));
	board_shadow->add_theme_stylebox_override("panel", shadow_style);
	board_shadow->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root_container->add_child(board_shadow);

	card_panel = memnew(PanelContainer);
	card_panel->set_name("CardPanel");
	card_panel->set_anchors_preset(Control::PRESET_CENTER);
	card_panel->set_offset(SIDE_LEFT, -270.0f);
	card_panel->set_offset(SIDE_RIGHT, 270.0f);
	card_panel->set_offset(SIDE_TOP, -206.0f);
	card_panel->set_offset(SIDE_BOTTOM, 206.0f);
	card_panel->add_theme_stylebox_override("panel", uistyle::make_wood_tiled(0.995f));
	root_container->add_child(card_panel);

	// 木框倒角线：左上受光、右下压暗
	PanelContainer *board_bevel = memnew(PanelContainer);
	board_bevel->set_anchors_preset(Control::PRESET_CENTER);
	board_bevel->set_offset(SIDE_LEFT, -263.0f);
	board_bevel->set_offset(SIDE_RIGHT, 263.0f);
	board_bevel->set_offset(SIDE_TOP, -199.0f);
	board_bevel->set_offset(SIDE_BOTTOM, 199.0f);
	Ref<StyleBoxFlat> bevel_style;
	bevel_style.instantiate();
	bevel_style->set_bg_color(Color(0, 0, 0, 0));
	bevel_style->set_corner_radius_all(5);
	bevel_style->set_border_width(SIDE_TOP, 2);
	bevel_style->set_border_width(SIDE_LEFT, 2);
	bevel_style->set_border_width(SIDE_RIGHT, 1);
	bevel_style->set_border_width(SIDE_BOTTOM, 1);
	bevel_style->set_border_color(Color(1.0f, 0.92f, 0.78f, 0.10f));
	board_bevel->add_theme_stylebox_override("panel", bevel_style);
	board_bevel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root_container->add_child(board_bevel);

	Control *card_layer = memnew(Control);
	card_layer->set_anchors_preset(Control::PRESET_FULL_RECT);
	card_layer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	card_panel->add_child(card_layer);

	// 纸的凹槽底：比纸略大一圈的暗色内陷，让纸像压在木板上
	PanelContainer *paper_pocket = memnew(PanelContainer);
	paper_pocket->set_position(Vector2(4.0f, 4.0f));
	paper_pocket->set_size(Vector2(480.0f, 360.0f));
	Ref<StyleBoxFlat> pocket_style;
	pocket_style.instantiate();
	pocket_style->set_bg_color(Color(0.10f, 0.07f, 0.04f, 0.30f));
	pocket_style->set_corner_radius_all(4);
	pocket_style->set_border_color(Color(0.03f, 0.03f, 0.02f, 0.45f));
	pocket_style->set_border_width_all(1);
	pocket_style->set_shadow_color(Color(0.02f, 0.015f, 0.01f, 0.45f));
	pocket_style->set_shadow_size(8);
	pocket_style->set_shadow_offset(Vector2(0, 3));
	paper_pocket->add_theme_stylebox_override("panel", pocket_style);
	paper_pocket->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	card_layer->add_child(paper_pocket);

	// 底层纸：稍微歪一点、偏右下，露出一个纸边
	PanelContainer *paper_back = memnew(PanelContainer);
	paper_back->set_position(Vector2(14.0f, 12.0f));
	paper_back->set_size(Vector2(472.0f, 356.0f));
	paper_back->add_theme_stylebox_override("panel", uistyle::make_parchment_textured(0.88f));
	paper_back->set_modulate(Color(0.86f, 0.82f, 0.74f, 0.95f));
	paper_back->set_rotation(Math::deg_to_rad(1.2f));
	paper_back->set_pivot_offset(Vector2(236.0f, 178.0f));
	card_layer->add_child(paper_back);

	// 叶枝：从纸角后面探出来
	TextureRect *sprig = memnew(TextureRect);
	sprig->set_texture(uistyle::leaf_icon_texture());
	sprig->set_size(Vector2(70.0f, 70.0f));
	sprig->set_position(Vector2(428.0f, 214.0f));
	sprig->set_pivot_offset(Vector2(35.0f, 35.0f));
	sprig->set_rotation(Math::deg_to_rad(28.0f));
	sprig->set_modulate(Color(1.0f, 1.0f, 1.0f, 0.95f));
	sprig->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	card_layer->add_child(sprig);

	// 主纸：细墨线边框 + 投影，纸面用羊皮纸纹理
	PanelContainer *paper_outer = memnew(PanelContainer);
	paper_outer->set_position(Vector2(8.0f, 8.0f));
	paper_outer->set_size(Vector2(472.0f, 356.0f));
	Ref<StyleBoxFlat> paper_edge;
	paper_edge.instantiate();
	paper_edge->set_bg_color(Color(0, 0, 0, 0));
	paper_edge->set_border_color(Color(0.34f, 0.26f, 0.18f, 0.55f));
	paper_edge->set_border_width_all(1);
	paper_edge->set_corner_radius_all(3);
	paper_edge->set_shadow_color(Color(0.03f, 0.02f, 0.01f, 0.38f));
	paper_edge->set_shadow_size(18);
	paper_edge->set_shadow_offset(Vector2(0, 7));
	paper_outer->add_theme_stylebox_override("panel", paper_edge);
	card_layer->add_child(paper_outer);

	PanelContainer *paper = memnew(PanelContainer);
	Ref<StyleBoxFlat> paper_sheet;
	paper_sheet.instantiate();
	paper_sheet->set_bg_color(uistyle::paper_cream());
	paper_sheet->set_corner_radius_all(2);
	paper_sheet->set_content_margin(SIDE_LEFT, 26.0f);
	paper_sheet->set_content_margin(SIDE_RIGHT, 26.0f);
	paper_sheet->set_content_margin(SIDE_TOP, 20.0f);
	paper_sheet->set_content_margin(SIDE_BOTTOM, 18.0f);
	paper->add_theme_stylebox_override("panel", paper_sheet);
	paper_outer->add_child(paper);

	main_vbox = memnew(VBoxContainer);
	main_vbox->add_theme_constant_override("separation", 9);
	paper->add_child(main_vbox);

	// 顶部一条深绿色窄带：像手账页边的书签条
	ColorRect *strip = memnew(ColorRect);
	strip->set_custom_minimum_size(Vector2(0, 3));
	strip->set_color(Color(uistyle::deep_green().r, uistyle::deep_green().g, uistyle::deep_green().b, 0.9f));
	main_vbox->add_child(strip);

	// 小字眉：PEACE · 更新手记
	Label *caps = memnew(Label);
	caps->set_text(String::utf8("P E A C E    ·    U P D A T E    N O T E"));
	caps->add_theme_font_size_override("font_size", 11);
	caps->add_theme_color_override("font_color", Color(ink.r, ink.g, ink.b, 0.55f));
	if (font.is_valid()) caps->add_theme_font_override("font", font);
	main_vbox->add_child(caps);

	// 大标题：可被远程标题覆盖
	title_label = memnew(Label);
	title_label->set_text(String::utf8("新版本来信"));
	title_label->add_theme_font_size_override("font_size", 31);
	title_label->add_theme_color_override("font_color", Color(ink.r * 0.82f, ink.g * 0.82f, ink.b * 0.82f));
	Ref<Font> title_heavy = uistyle::heavy_cjk(700, 0.12f);
	if (title_heavy.is_valid()) title_label->add_theme_font_override("font", title_heavy);
	main_vbox->add_child(title_label);

	TextureRect *underline = memnew(TextureRect);
	underline->set_texture(uistyle::ink_underline_texture());
	underline->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	underline->set_custom_minimum_size(Vector2(188, 8));
	underline->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	underline->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	main_vbox->add_child(underline);

	// 版本 / 体积 / 无需重装
	version_sub_label = memnew(Label);
	version_sub_label->set_text(String::utf8("v0.1.2  ·  无缝热加载无需重装"));
	version_sub_label->add_theme_font_size_override("font_size", 12);
	version_sub_label->add_theme_color_override("font_color", Color(ink.r, ink.g, ink.b, 0.68f));
	if (font.is_valid()) version_sub_label->add_theme_font_override("font", font);
	HBoxContainer *meta_row = memnew(HBoxContainer);
	meta_row->add_theme_constant_override("separation", 8);
	main_vbox->add_child(meta_row);
	version_sub_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	meta_row->add_child(version_sub_label);

	btn_website = memnew(Button);
	btn_website->set_text(String::utf8("官网 ↗"));
	btn_website->set_flat(true);
	btn_website->add_theme_font_size_override("font_size", 11);
	btn_website->add_theme_color_override("font_color", Color(ink.r, ink.g, ink.b, 0.50f));
	btn_website->add_theme_color_override("font_hover_color", uistyle::deep_green());
	if (font.is_valid()) btn_website->add_theme_font_override("font", font);
	btn_website->connect("pressed", Callable(this, "open_website"));
	meta_row->add_child(btn_website);

	// 细分隔线
	ColorRect *rule = memnew(ColorRect);
	rule->set_custom_minimum_size(Vector2(0, 1));
	rule->set_color(Color(ink.r, ink.g, ink.b, 0.18f));
	main_vbox->add_child(rule);

	// “本 次 更 新”：两侧引线的小节标题
	HBoxContainer *sec = memnew(HBoxContainer);
	sec->add_theme_constant_override("separation", 10);
	main_vbox->add_child(sec);
	ColorRect *sec_l = memnew(ColorRect);
	sec_l->set_custom_minimum_size(Vector2(0, 1));
	sec_l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sec_l->set_color(Color(ink.r, ink.g, ink.b, 0.16f));
	sec->add_child(sec_l);
	Label *sec_t = memnew(Label);
	sec_t->set_text(String::utf8("本 次 更 新"));
	sec_t->add_theme_font_size_override("font_size", 10);
	sec_t->add_theme_color_override("font_color", Color(ink.r, ink.g, ink.b, 0.52f));
	if (font.is_valid()) sec_t->add_theme_font_override("font", font);
	sec->add_child(sec_t);
	ColorRect *sec_r = memnew(ColorRect);
	sec_r->set_custom_minimum_size(Vector2(0, 1));
	sec_r->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sec_r->set_color(Color(ink.r, ink.g, ink.b, 0.16f));
	sec->add_child(sec_r);

	// 更新条目列表（撑满中段，避免底部留白）
	changelog_box = memnew(VBoxContainer);
	changelog_box->add_theme_constant_override("separation", 7);
	changelog_box->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(changelog_box);

	Label *note = memnew(Label);
	note->set_text(String::utf8("小更新，不影响你的存档与进度。"));
	note->add_theme_font_size_override("font_size", 11);
	note->add_theme_color_override("font_color", Color(ink.r, ink.g, ink.b, 0.45f));
	if (font.is_valid()) note->add_theme_font_override("font", font);
	main_vbox->add_child(note);

	// 进度（初始隐藏）
	progress_container = memnew(VBoxContainer);
	progress_container->add_theme_constant_override("separation", 5);
	main_vbox->add_child(progress_container);
	progress_bar = memnew(ProgressBar);
	progress_bar->set_custom_minimum_size(Vector2(0, 6));
	progress_bar->set_show_percentage(false);
	Ref<StyleBoxFlat> pbg;
	pbg.instantiate();
	pbg->set_bg_color(Color(ink.r, ink.g, ink.b, 0.12f));
	pbg->set_corner_radius_all(3);
	Ref<StyleBoxFlat> pfg;
	pfg.instantiate();
	pfg->set_bg_color(uistyle::deep_green());
	pfg->set_corner_radius_all(3);
	progress_bar->add_theme_stylebox_override("background", pbg);
	progress_bar->add_theme_stylebox_override("fill", pfg);
	progress_container->add_child(progress_bar);
	progress_stats_label = memnew(Label);
	progress_stats_label->set_text(String::utf8("准备下载..."));
	progress_stats_label->add_theme_font_size_override("font_size", 11);
	progress_stats_label->add_theme_color_override("font_color", Color(ink.r, ink.g, ink.b, 0.62f));
	if (font.is_valid()) progress_stats_label->add_theme_font_override("font", font);
	progress_container->add_child(progress_stats_label);
	progress_container->hide();

	// 页脚：署名 + 方形按钮（取消在左、确认在右且略“按下去”）
	HBoxContainer *footer = memnew(HBoxContainer);
	footer->add_theme_constant_override("separation", 8);
	main_vbox->add_child(footer);

	Control *footer_spacer = memnew(Control);
	footer_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	footer_spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	footer->add_child(footer_spacer);

	btn_skip = memnew(Button);
	btn_skip->set_text(String::utf8("暂不更新"));
	btn_skip->set_custom_minimum_size(Vector2(112, 46));
	btn_skip->add_theme_stylebox_override("normal", uistyle::make_button_style(false, 0));
	btn_skip->add_theme_stylebox_override("hover", uistyle::make_button_style(false, 1));
	btn_skip->add_theme_stylebox_override("pressed", uistyle::make_button_style(false, 2));
	btn_skip->add_theme_font_size_override("font_size", 16);
	btn_skip->add_theme_color_override("font_color", Color(0.97f, 0.93f, 0.84f));
	btn_skip->add_theme_color_override("font_hover_color", Color(1.0f, 0.97f, 0.89f));
	btn_skip->add_theme_color_override("font_pressed_color", Color(0.90f, 0.86f, 0.77f));
	btn_skip->add_theme_color_override("font_outline_color", Color(0.10f, 0.06f, 0.03f, 0.9f));
	btn_skip->add_theme_constant_override("outline_size", 2);
	if (heavy.is_valid()) btn_skip->add_theme_font_override("font", heavy);
	btn_skip->connect("pressed", Callable(this, "dismiss_dialog"));
	PanelContainer *skip_shadow = memnew(PanelContainer);
	Ref<StyleBoxFlat> skip_sh;
	skip_sh.instantiate();
	skip_sh->set_bg_color(Color(0, 0, 0, 0));
	skip_sh->set_corner_radius_all(8);
	skip_sh->set_shadow_color(Color(0.02f, 0.015f, 0.01f, 0.45f));
	skip_sh->set_shadow_size(6);
	skip_sh->set_shadow_offset(Vector2(0, 3));
	skip_shadow->add_theme_stylebox_override("panel", skip_sh);
	skip_shadow->add_child(btn_skip);
	footer->add_child(skip_shadow);

	btn_update = memnew(Button);
	btn_update->set_text(String::utf8("立即更新"));
	btn_update->set_custom_minimum_size(Vector2(146, 46));
	btn_update->add_theme_stylebox_override("normal", uistyle::make_button_style(true, 0));
	btn_update->add_theme_stylebox_override("hover", uistyle::make_button_style(true, 1));
	btn_update->add_theme_stylebox_override("pressed", uistyle::make_button_style(true, 2));
	btn_update->add_theme_font_size_override("font_size", 17);
	btn_update->add_theme_color_override("font_color", Color(0.98f, 0.96f, 0.89f));
	btn_update->add_theme_color_override("font_hover_color", Color(1.0f, 0.99f, 0.93f));
	btn_update->add_theme_color_override("font_pressed_color", Color(0.92f, 0.91f, 0.84f));
	btn_update->add_theme_color_override("font_disabled_color", Color(0.82f, 0.82f, 0.76f, 0.7f));
	btn_update->add_theme_color_override("font_outline_color", Color(0.04f, 0.10f, 0.06f, 0.9f));
	btn_update->add_theme_constant_override("outline_size", 3);
	if (heavy.is_valid()) btn_update->add_theme_font_override("font", heavy);
	btn_update->connect("pressed", Callable(this, "start_download"));
	PanelContainer *update_shadow = memnew(PanelContainer);
	Ref<StyleBoxFlat> up_sh;
	up_sh.instantiate();
	up_sh->set_bg_color(Color(0, 0, 0, 0));
	up_sh->set_corner_radius_all(8);
	up_sh->set_shadow_color(Color(0.01f, 0.03f, 0.015f, 0.55f));
	up_sh->set_shadow_size(10);
	up_sh->set_shadow_offset(Vector2(0, 4));
	update_shadow->add_theme_stylebox_override("panel", up_sh);
	update_shadow->add_child(btn_update);
	footer->add_child(update_shadow);


	// 纸角火漆印：代替传统署名
	TextureRect *seal = memnew(TextureRect);
	seal->set_texture(uistyle::seal_texture());
	seal->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	seal->set_size(Vector2(56.0f, 56.0f));
	seal->set_position(Vector2(-4.0f, 314.0f));
	seal->set_pivot_offset(Vector2(28.0f, 28.0f));
	seal->set_rotation(Math::deg_to_rad(-9.0f));
	seal->set_modulate(Color(1.0f, 1.0f, 1.0f, 0.92f));
	seal->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	card_layer->add_child(seal);
	Label *seal_cap = memnew(Label);
	seal_cap->set_text(String::utf8("Peace"));
	seal_cap->set_position(Vector2(58.0f, 336.0f));
	seal_cap->add_theme_font_size_override("font_size", 12);
	seal_cap->add_theme_color_override("font_color", Color(ink.r, ink.g, ink.b, 0.55f));
	if (font.is_valid()) seal_cap->add_theme_font_override("font", font);
	card_layer->add_child(seal_cap);

	// 顶部图钉（带一小片投影）
	TextureRect *pin_shadow = memnew(TextureRect);
	pin_shadow->set_texture(uistyle::pin_texture());
	pin_shadow->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	pin_shadow->set_size(Vector2(24.0f, 14.0f));
	pin_shadow->set_position(Vector2(232.0f, 12.0f));
	pin_shadow->set_modulate(Color(0.0f, 0.0f, 0.0f, 0.35f));
	pin_shadow->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	card_layer->add_child(pin_shadow);

	TextureRect *pin = memnew(TextureRect);
	pin->set_texture(uistyle::pin_texture());
	pin->set_size(Vector2(20.0f, 20.0f));
	pin->set_position(Vector2(234.0f, -2.0f));
	pin->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	card_layer->add_child(pin);

	// 四角木钉：把"钉在板子上"的质感补齐
	const float nail_xy[4][2] = {
		{ -11.0f, -11.0f }, { 484.0f, -11.0f }, { -11.0f, 376.0f }, { 484.0f, 376.0f }
	};
	for (int i = 0; i < 4; i++) {
		TextureRect *nail = memnew(TextureRect);
		nail->set_texture(uistyle::pin_texture());
		nail->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		nail->set_size(Vector2(13.0f, 13.0f));
		nail->set_position(Vector2(nail_xy[i][0], nail_xy[i][1]));
		nail->set_modulate(Color(0.95f, 0.88f, 0.76f, 0.85f));
		nail->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		card_layer->add_child(nail);
	}

	// 右上角轻提示：一小块黑板
	toast_panel = memnew(PanelContainer);
	toast_panel->set_anchors_preset(Control::PRESET_TOP_RIGHT);
	toast_panel->set_offset(SIDE_LEFT, -260.0f);
	toast_panel->set_offset(SIDE_RIGHT, -20.0f);
	toast_panel->set_offset(SIDE_TOP, 16.0f);
	toast_panel->set_offset(SIDE_BOTTOM, 52.0f);
	toast_panel->add_theme_stylebox_override("panel", uistyle::make_blackboard_textured(0.98f));
	add_child(toast_panel);
	toast_label = memnew(Label);
	toast_label->add_theme_font_size_override("font_size", 13);
	toast_label->add_theme_color_override("font_color", uistyle::chalk_white());
	if (chalk.is_valid()) toast_label->add_theme_font_override("font", chalk);
	toast_panel->add_child(toast_label);
	toast_panel->hide();

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

		TextureRect *dot = memnew(TextureRect);
		dot->set_texture(uistyle::leaf_icon_texture());
		dot->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		dot->set_custom_minimum_size(Vector2(13, 13));
		dot->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		dot->set_modulate(Color(0.85f, 0.9f, 0.8f, 0.95f));

		Label *text = memnew(Label);
		text->set_text(changelog_items[i]);
		text->add_theme_color_override("font_color", Color(0.26f, 0.20f, 0.14f)); // 墨水棕
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
