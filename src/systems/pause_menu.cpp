#include "pause_menu.h"

#include "../actors/player.h"
#include "save_system.h"
#include "ui_style.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

using namespace godot;

namespace {

constexpr float DEFAULT_VOLUME = 0.8f;
const char *SETTINGS_PATH = "user://settings.cfg"; // 与 savegame.cfg 互不干扰

} // namespace

void PauseMenu::_bind_methods() {
	ClassDB::bind_method(D_METHOD("toggle"), &PauseMenu::toggle);
	ClassDB::bind_method(D_METHOD("get_is_open"), &PauseMenu::get_is_open);
	ClassDB::bind_method(D_METHOD("on_resume_pressed"), &PauseMenu::on_resume_pressed);
	ClassDB::bind_method(D_METHOD("on_quality_pressed"), &PauseMenu::on_quality_pressed);
	ClassDB::bind_method(D_METHOD("on_fullscreen_pressed"), &PauseMenu::on_fullscreen_pressed);
	ClassDB::bind_method(D_METHOD("on_quit_pressed"), &PauseMenu::on_quit_pressed);
	ClassDB::bind_method(D_METHOD("on_volume_changed", "p_value"), &PauseMenu::on_volume_changed);
}

void PauseMenu::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不建 UI、不轮询
			}
			set_process_mode(Node::PROCESS_MODE_ALWAYS); // 暂停期间本菜单保活
			set_layer(100);
			load_settings();
			apply_volume(); // 存档里的音量开机即生效
			apply_quality(); // 存档里的画质档开机即生效
			build_ui();
			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			// 残留暂停状态会带崩后续场景，退出前兜底解除
			if (get_tree() != nullptr && (open || resuming)) {
				get_tree()->set_pause(false);
			}
			if (dirty_settings) {
				save_settings();
			}
			if (click_player != nullptr) {
				click_player->stop();
				click_player->set_stream(Ref<AudioStream>());
			}
			if (back_player != nullptr) {
				back_player->stop();
				back_player->set_stream(Ref<AudioStream>());
			}
			snd_click = Ref<AudioStream>();
			snd_back = Ref<AudioStream>();
		} break;
		case NOTIFICATION_PROCESS: {
			Input *input = Input::get_singleton();
			if (input == nullptr) {
				break;
			}
			// Esc 全局单一归属本菜单：呼出 / 菜单内返回游戏 / 地图打开时只收起地图。
			// 三态门在 resuming：恢复必须等 Esc 松开，松开当帧才 finish_resume。
			bool esc = input->is_key_pressed(KEY_ESCAPE);
			if (esc && !esc_prev && !resuming) {
				if (open) {
					request_resume(); // Esc 返回游戏（与面板 tip 一致）
				} else {
					Player *pl = find_player();
					if (pl != nullptr && pl->get_is_map_open()) {
						pl->toggle_map(); // 大地图打开时 Esc 只收起地图，不暂停世界
					} else {
						toggle(); // Esc 呼出
					}
				}
			}
			esc_prev = esc;
			// 恢复要等 Esc 松开：解除暂停后 Player 恢复轮询时不会吃到幽灵边沿
			if (resuming && !esc) {
				finish_resume();
			}
		} break;
		default:
			break;
	}
}

Ref<Font> PauseMenu::load_cjk_font() const {
	return uistyle::load_cjk();
}

namespace {

// 「湖畔玻璃」按钮三档：主按钮（金底深字）/ 次按钮（玻璃底）/ 退出（玻璃底+暖红悬停）。
// 默认 Godot 灰按钮是"廉价感"的根源——全部换主题样式盒（normal/hover/pressed 三态）。
enum BtnKind { BTN_PRIMARY, BTN_GLASS, BTN_QUIT };

void apply_btn_face(const Ref<StyleBoxFlat> &p_sb, float p_alpha) {
	p_sb->set_bg_color(Color(1.0f, 0.97f, 0.88f, p_alpha));
	p_sb->set_corner_radius_all(12);
	p_sb->set_border_width_all(1);
	p_sb->set_border_color(Color(1.0f, 0.97f, 0.88f, 0.14f));
	p_sb->set_content_margin_all(10.0f);
	p_sb->set_content_margin(Side::SIDE_LEFT, 16.0f);
	p_sb->set_content_margin(Side::SIDE_RIGHT, 16.0f);
}

void style_button(Button *p_btn, BtnKind p_kind) {
	Ref<StyleBoxFlat> n, h, p;
	n.instantiate();
	h.instantiate();
	p.instantiate();
	if (p_kind == BTN_PRIMARY) {
		// 主行动：浅松木板 + 深胡桃刻字，hover 提亮、pressed 压暗（像按下一块真木板）
		for (auto *sb : { n.ptr(), h.ptr(), p.ptr() }) {
			sb->set_corner_radius_all(10);
			sb->set_border_width_all(2);
			sb->set_border_color(Color(uistyle::wood_dark().r, uistyle::wood_dark().g, uistyle::wood_dark().b, 0.9f));
			sb->set_content_margin_all(10.0f);
			sb->set_content_margin(Side::SIDE_LEFT, 16.0f);
			sb->set_content_margin(Side::SIDE_RIGHT, 16.0f);
		}
		n->set_bg_color(Color(0.72f, 0.58f, 0.38f, 1.0f));
		h->set_bg_color(Color(0.80f, 0.66f, 0.44f, 1.0f));
		p->set_bg_color(Color(0.62f, 0.49f, 0.31f, 1.0f));
		p_btn->add_theme_color_override("font_color", Color(0.16f, 0.11f, 0.05f));
		p_btn->add_theme_color_override("font_hover_color", Color(0.13f, 0.09f, 0.04f));
		p_btn->add_theme_color_override("font_pressed_color", Color(0.13f, 0.09f, 0.04f));
		p_btn->add_theme_color_override("font_focus_color", Color(0.16f, 0.11f, 0.05f));
	} else if (p_kind == BTN_QUIT) {
		for (auto *sb : { n.ptr(), p.ptr() }) {
			sb->set_bg_color(Color(uistyle::wood_dark().r, uistyle::wood_dark().g, uistyle::wood_dark().b, 0.55f));
			sb->set_corner_radius_all(10);
			sb->set_border_width_all(1);
			sb->set_border_color(Color(1.0f, 0.95f, 0.85f, 0.10f));
			sb->set_content_margin_all(10.0f);
			sb->set_content_margin(Side::SIDE_LEFT, 16.0f);
			sb->set_content_margin(Side::SIDE_RIGHT, 16.0f);
		}
		// 悬停整块转暖红，暗示 irreversible（底/边/角全部逐属性设置,StyleBoxFlat 禁拷贝赋值）
		h->set_bg_color(Color(0.62f, 0.26f, 0.18f, 0.72f));
		h->set_border_color(Color(0.95f, 0.55f, 0.40f, 0.55f));
		h->set_border_width_all(1);
		h->set_corner_radius_all(10);
		h->set_content_margin_all(10.0f);
		h->set_content_margin(Side::SIDE_LEFT, 16.0f);
		h->set_content_margin(Side::SIDE_RIGHT, 16.0f);
		p_btn->add_theme_color_override("font_color", Color(0.93f, 0.88f, 0.78f, 0.92f));
		p_btn->add_theme_color_override("font_hover_color", Color(1.0f, 0.86f, 0.80f));
		p_btn->add_theme_color_override("font_pressed_color", Color(1.0f, 0.86f, 0.80f));
		p_btn->add_theme_color_override("font_focus_color", uistyle::wood_text());
	} else {
		// 次按钮：嵌进木牌的凹槽——深胡桃底、奶油刻字
		for (auto *sb : { n.ptr(), h.ptr(), p.ptr() }) {
			sb->set_bg_color(Color(uistyle::wood_dark().r, uistyle::wood_dark().g, uistyle::wood_dark().b, 0.55f));
			sb->set_corner_radius_all(10);
			sb->set_border_width_all(1);
			sb->set_border_color(Color(1.0f, 0.95f, 0.85f, 0.10f));
			sb->set_content_margin_all(10.0f);
			sb->set_content_margin(Side::SIDE_LEFT, 16.0f);
			sb->set_content_margin(Side::SIDE_RIGHT, 16.0f);
		}
		h->set_bg_color(Color(0.36f, 0.26f, 0.16f, 0.75f));
		h->set_border_color(Color(1.0f, 0.95f, 0.85f, 0.28f));
		p->set_bg_color(Color(0.22f, 0.15f, 0.09f, 0.9f));
		p_btn->add_theme_color_override("font_color", Color(0.93f, 0.88f, 0.78f, 0.92f));
		p_btn->add_theme_color_override("font_hover_color", uistyle::wood_text());
		p_btn->add_theme_color_override("font_pressed_color", uistyle::wood_text());
		p_btn->add_theme_color_override("font_focus_color", uistyle::wood_text());
	}
	p_btn->add_theme_stylebox_override("normal", n);
	p_btn->add_theme_stylebox_override("hover", h);
	p_btn->add_theme_stylebox_override("pressed", p);
	p_btn->add_theme_stylebox_override("focus", n); // 聚焦不画框，靠底色变化表达
}

} // namespace

void PauseMenu::build_ui() {
	if (ui_built) {
		return;
	}
	ui_built = true;

	Ref<Font> font = load_cjk_font();

	// —— 全屏遮罩根（呼出时才可见） ——
	root = memnew(Control);
	root->set_name("Root");
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->set_visible(false);
	add_child(root);

	ColorRect *dim = memnew(ColorRect);
	dim->set_name("Dim");
	dim->set_color(Color(0.0f, 0.0f, 0.0f, 0.45f));
	dim->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->add_child(dim);

	CenterContainer *center = memnew(CenterContainer);
	center->set_name("Center");
	center->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->add_child(center);

	// —— 面板（湖畔玻璃令牌 ui_style.h；内边距按菜单布局放大） ——
	PanelContainer *panel = memnew(PanelContainer);
	Ref<StyleBoxFlat> sb = uistyle::make_wood(16.0f); // 悬浮木牌：与木屋/篱笆同一材质语言
	sb->set_content_margin(SIDE_LEFT, 32.0f);
	sb->set_content_margin(SIDE_RIGHT, 32.0f);
	sb->set_content_margin(SIDE_TOP, 24.0f);
	sb->set_content_margin(SIDE_BOTTOM, 24.0f);
	panel->add_theme_stylebox_override("panel", sb);
	center->add_child(panel);

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_name("Menu");
	vb->add_theme_constant_override("separation", 14);
	panel->add_child(vb);

	// 标题
	Label *title = memnew(Label);
	title->set_text(String::utf8("·  湖  畔  小  憩  ·"));
	title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	uistyle::dress_label(title, 28, uistyle::text_cream(), font, 0); // 无描边：菜单标题不学野外 HUD
	vb->add_child(title);

	Label *subtitle = memnew(Label);
	subtitle->set_text(String::utf8("微风正好，歇一歇脚"));
	subtitle->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	uistyle::dress_label(subtitle, 14, Color(0.90f, 0.84f, 0.72f, 0.85f), font, 0);
	vb->add_child(subtitle);

	// 分隔细线（湖面波光意象：三段渐隐横线）
	{
		HBoxContainer *rule = memnew(HBoxContainer);
		rule->add_theme_constant_override("separation", 6);
		rule->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		vb->add_child(rule);
		const float segs[3] = { 60.0f, 110.0f, 60.0f };
		const float alphas[3] = { 0.06f, 0.22f, 0.06f };
		for (int i = 0; i < 3; i++) {
			ColorRect *seg = memnew(ColorRect);
			seg->set_color(Color(0.10f, 0.07f, 0.04f, alphas[i] + 0.12f)); // 木面刻痕线
			seg->set_custom_minimum_size(Vector2(segs[i], 2.0f));
			seg->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
			rule->add_child(seg);
		}
	}

	// 继续
	btn_resume = memnew(Button);
	btn_resume->set_name("ResumeBtn");
	btn_resume->set_text(String::utf8("继续游戏"));
	btn_resume->set_custom_minimum_size(Vector2(300.0f, 48.0f));
	if (font.is_valid()) {
		btn_resume->add_theme_font_override("font", font);
	}
	btn_resume->add_theme_font_size_override("font_size", 22);
	style_button(btn_resume, BTN_PRIMARY);
	btn_resume->connect("pressed", Callable(this, "on_resume_pressed"));
	vb->add_child(btn_resume);

	// 音量滑条行
	HBoxContainer *hb = memnew(HBoxContainer);
	hb->add_theme_constant_override("separation", 12);
	vb->add_child(hb);

	Label *vol_name = memnew(Label);
	vol_name->set_text(String::utf8("音量"));
	uistyle::dress_label(vol_name, 20, uistyle::text_soft(), font);
	vol_name->set_custom_minimum_size(Vector2(52.0f, 0.0f));
	hb->add_child(vol_name);

	volume_slider = memnew(HSlider);
	volume_slider->set_name("VolumeSlider");
	volume_slider->set_min(0.0f);
	volume_slider->set_max(1.0f);
	volume_slider->set_step(0.05f);
	volume_slider->set_value(volume_linear); // 先赋值再连接，避免开机置脏
	volume_slider->set_custom_minimum_size(Vector2(190.0f, 28.0f));
	volume_slider->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	{
		Ref<StyleBoxFlat> track, fill;
		track.instantiate();
		track->set_bg_color(Color(0.10f, 0.07f, 0.04f, 0.55f)); // 木面凹槽
		track->set_corner_radius_all(4);
		track->set_content_margin_all(6.0f); // 撑粗轨道
		fill.instantiate();
		fill->set_bg_color(Color(0.88f, 0.80f, 0.62f, 0.95f)); // 刻痕浅木金
		fill->set_corner_radius_all(4);
		fill->set_content_margin_all(6.0f);
		volume_slider->add_theme_stylebox_override("slider", track);
		volume_slider->add_theme_stylebox_override("grabber_area", fill);
	}
	hb->add_child(volume_slider);

	volume_value = memnew(Label);
	refresh_volume_label();
	uistyle::dress_label(volume_value, 18, uistyle::wood_text(), font, 0);
	volume_value->set_custom_minimum_size(Vector2(48.0f, 0.0f));
	volume_value->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	hb->add_child(volume_value);

	volume_slider->connect("value_changed", Callable(this, "on_volume_changed"));

	// 画质高/低（低档关 SSR/SSAO/Glow，运行时改 Environment 即时生效）
	btn_quality = memnew(Button);
	btn_quality->set_name("QualityBtn");
	refresh_quality_text();
	btn_quality->set_custom_minimum_size(Vector2(300.0f, 46.0f));
	if (font.is_valid()) {
		btn_quality->add_theme_font_override("font", font);
	}
	btn_quality->add_theme_font_size_override("font_size", 20);
	style_button(btn_quality, BTN_GLASS);
	btn_quality->connect("pressed", Callable(this, "on_quality_pressed"));
	vb->add_child(btn_quality);

	// 全屏切换
	btn_fullscreen = memnew(Button);
	btn_fullscreen->set_name("FullscreenBtn");
	refresh_fullscreen_text();
	btn_fullscreen->set_custom_minimum_size(Vector2(300.0f, 46.0f));
	if (font.is_valid()) {
		btn_fullscreen->add_theme_font_override("font", font);
	}
	btn_fullscreen->add_theme_font_size_override("font_size", 20);
	style_button(btn_fullscreen, BTN_GLASS);
	btn_fullscreen->connect("pressed", Callable(this, "on_fullscreen_pressed"));
	vb->add_child(btn_fullscreen);

	// 保存并退出
	btn_quit = memnew(Button);
	btn_quit->set_name("QuitBtn");
	btn_quit->set_text(String::utf8("保存并退出"));
	btn_quit->set_custom_minimum_size(Vector2(300.0f, 46.0f));
	if (font.is_valid()) {
		btn_quit->add_theme_font_override("font", font);
	}
	btn_quit->add_theme_font_size_override("font_size", 20);
	style_button(btn_quit, BTN_QUIT);
	btn_quit->connect("pressed", Callable(this, "on_quit_pressed"));
	vb->add_child(btn_quit);

	// 底部小字提示
	Label *tip = memnew(Label);
	tip->set_text(String::utf8("Esc · 继续垂钓与漫游"));
	tip->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	uistyle::dress_label(tip, 14, Color(0.88f, 0.81f, 0.68f, 0.72f), font, 0);
	vb->add_child(tip);

	// —— 菜单音效（挂在本节点下，暂停期间照常出声） ——
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr && rl->exists("res://assets/audio/ui_click.mp3")) {
		Ref<Resource> r = rl->load("res://assets/audio/ui_click.mp3");
		if (r.is_valid()) {
			snd_click = Object::cast_to<AudioStream>(r.ptr());
		}
	}
	click_player = memnew(AudioStreamPlayer);
	click_player->set_name("ClickSound");
	if (snd_click.is_valid()) {
		click_player->set_stream(snd_click);
	}
	click_player->set_volume_db(-6.0f);
	click_player->set_bus(StringName("SFX"));
	add_child(click_player);

	if (rl != nullptr && rl->exists("res://assets/audio/ui_back.mp3")) {
		Ref<Resource> r = rl->load("res://assets/audio/ui_back.mp3");
		if (r.is_valid()) {
			snd_back = Object::cast_to<AudioStream>(r.ptr());
		}
	}
	back_player = memnew(AudioStreamPlayer);
	back_player->set_name("BackSound");
	if (snd_back.is_valid()) {
		back_player->set_stream(snd_back);
	}
	back_player->set_volume_db(-6.0f);
	back_player->set_bus(StringName("SFX"));
	add_child(back_player);
}

void PauseMenu::load_settings() {
	Ref<ConfigFile> cfg;
	cfg.instantiate();
	if (cfg->load(SETTINGS_PATH) == OK) {
		float v = (float)cfg->get_value("audio", "master_volume", DEFAULT_VOLUME);
		volume_linear = Math::clamp(v, 0.0f, 1.0f);
		quality_low = (bool)cfg->get_value("video", "quality_low", false);
	}
}

void PauseMenu::save_settings() {
	Ref<ConfigFile> cfg;
	cfg.instantiate();
	cfg->set_value("audio", "master_volume", volume_linear);
	cfg->set_value("video", "quality_low", quality_low);
	cfg->save(SETTINGS_PATH); // user:// 不可写时静默失败，不打断游戏
	dirty_settings = false;
}

void PauseMenu::apply_volume() {
	AudioServer *as = AudioServer::get_singleton();
	if (as == nullptr || as->get_bus_count() < 1) {
		return;
	}
	float db = (volume_linear <= 0.001f) ? -80.0f : Math::linear_to_db(volume_linear);
	as->set_bus_volume_db(0, db); // Master 总线
}

void PauseMenu::apply_quality() {
	// 低配三特效（docs/PERF.md：SSR 是最大头，水面反射退化为着色器内置天空兜底）
	WorldEnvironment *we = find_world_env();
	if (we != nullptr) {
		Ref<Environment> env = we->get_environment();
		if (env.is_valid()) {
			env->set_ssr_enabled(!quality_low);
			env->set_ssao_enabled(!quality_low);
			env->set_glow_enabled(!quality_low);
		}
	}
	// 帧成本主杠杆（tools/perf_probe.gd 实测：几何瓶颈下 0.7 档再省 ~36%，比关特效更有效）
	Viewport *vp = get_viewport();
	if (vp != nullptr) {
		vp->set_scaling_3d_scale(quality_low ? 0.7f : 1.0f);
		// MSAA 4x→关：2026-10-30 性能专项实测再省 ~20%（spawn −10.6ms / 球场 −20.6ms /
		// 石桥 −13.5ms @3840×1944）。默认高档保持 project.godot 的 4x 不动。
		vp->set_msaa_3d(quality_low ? Viewport::MSAA_DISABLED : Viewport::MSAA_4X);
	}
}

void PauseMenu::refresh_fullscreen_text() {
	if (btn_fullscreen == nullptr) {
		return;
	}
	DisplayServer *ds = DisplayServer::get_singleton();
	bool fs = ds != nullptr && ds->window_get_mode() == DisplayServer::WINDOW_MODE_FULLSCREEN;
	btn_fullscreen->set_text(String::utf8(fs ? "全屏：开" : "全屏：关"));
}

void PauseMenu::refresh_quality_text() {
	if (btn_quality == nullptr) {
		return;
	}
	btn_quality->set_text(String::utf8(quality_low ? "画质：低（流畅）" : "画质：高"));
}

void PauseMenu::refresh_volume_label() {
	if (volume_value == nullptr) {
		return;
	}
	volume_value->set_text(String::num_int64((int64_t)Math::round(volume_linear * 100.0f)) + String("%"));
}

void PauseMenu::toggle() {
	if (open) {
		request_resume();
	} else if (!resuming) {
		// —— 呼出：暂停世界 + 显示菜单 ——
		open = true;
		if (get_tree() != nullptr) {
			get_tree()->set_pause(true);
		}
		Input *input = Input::get_singleton();
		if (input != nullptr) {
			restore_mouse_mode = (int)input->get_mouse_mode();
			input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
		}
		if (root != nullptr) {
			root->set_visible(true);
		}
		refresh_fullscreen_text();
		if (btn_resume != nullptr) {
			btn_resume->grab_focus(); // 方向键/回车可直接操作
		}
		if (back_player != nullptr && snd_back.is_valid()) {
			back_player->play();
		}
	}
}

void PauseMenu::request_resume() {
	if (!open) {
		return;
	}
	open = false;
	resuming = true;
	if (root != nullptr) {
		root->set_visible(false);
	}
	if (back_player != nullptr && snd_back.is_valid()) {
		back_player->play();
	}
	// 真正解除暂停放在 finish_resume：Esc 已松开（或按键恢复）后执行
}

void PauseMenu::finish_resume() {
	resuming = false;
	if (get_tree() != nullptr) {
		get_tree()->set_pause(false);
	}
	Input *input = Input::get_singleton();
	if (input != nullptr) {
		input->set_mouse_mode((Input::MouseMode)restore_mouse_mode);
	}
	if (dirty_settings) {
		save_settings();
	}
}

SaveSystem *PauseMenu::find_save() const {
	Node *p = get_parent();
	if (p == nullptr) {
		return nullptr;
	}
	return Object::cast_to<SaveSystem>(p->get_node_or_null(NodePath("SaveSystem")));
}

Player *PauseMenu::find_player() const {
	Node *p = get_parent();
	if (p == nullptr) {
		return nullptr;
	}
	return Object::cast_to<Player>(p->get_node_or_null(NodePath("Player")));
}

WorldEnvironment *PauseMenu::find_world_env() const {
	Node *p = get_parent();
	if (p == nullptr) {
		return nullptr;
	}
	return Object::cast_to<WorldEnvironment>(p->get_node_or_null(NodePath("WorldEnvironment")));
}

void PauseMenu::on_resume_pressed() {
	if (click_player != nullptr && snd_click.is_valid()) {
		click_player->play();
	}
	request_resume();
}

void PauseMenu::on_quality_pressed() {
	if (click_player != nullptr && snd_click.is_valid()) {
		click_player->play();
	}
	quality_low = !quality_low;
	apply_quality();
	refresh_quality_text();
	dirty_settings = true; // 关菜单/退出时才落盘
}

void PauseMenu::on_fullscreen_pressed() {
	if (click_player != nullptr && snd_click.is_valid()) {
		click_player->play();
	}
	DisplayServer *ds = DisplayServer::get_singleton();
	if (ds == nullptr) {
		return;
	}
	if (ds->window_get_mode() == DisplayServer::WINDOW_MODE_FULLSCREEN) {
		ds->window_set_mode(DisplayServer::WINDOW_MODE_MAXIMIZED); // 项目默认启动即最大化
	} else {
		ds->window_set_mode(DisplayServer::WINDOW_MODE_FULLSCREEN);
	}
	refresh_fullscreen_text();
}

void PauseMenu::on_quit_pressed() {
	SaveSystem *s = find_save();
	if (s != nullptr) {
		s->save_now(); // 退出前显式存档；节点缺失时其 EXIT_TREE 兜底
	}
	if (dirty_settings) {
		save_settings();
	}
	if (get_tree() != nullptr) {
		get_tree()->quit();
	}
}

void PauseMenu::on_volume_changed(float p_value) {
	volume_linear = Math::clamp(p_value, 0.0f, 1.0f);
	apply_volume();
	refresh_volume_label();
	dirty_settings = true; // 关菜单/退出时才落盘，避免拖动滑条狂写磁盘
}
