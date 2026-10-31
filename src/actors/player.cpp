#include "player.h"

#include "../systems/ui_style.h"

#include <cmath>

// mingw / 严格 C++17 下 <cmath> 不保证提供 M_PI（POSIX 扩展），补一个兜底定义
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "footprint_manager.h"
#include "interactable.h"
#include "sky_cycle.h"
#include "terrain_builder.h"
#include "../interact/bed.h"

using namespace godot;

namespace {

// 键位收口（ROADMAP #5）：一律读 project.godot [input] 的动作；动作缺失时
// （极简测试场景没带 [input]）退回硬编码键，行为与收口前一致。
bool act_pressed(const char *p_action, Key p_fallback) {
	Input *in = Input::get_singleton();
	InputMap *im = InputMap::get_singleton();
	if (im != nullptr && im->has_action(StringName(p_action))) {
		return in->is_action_pressed(StringName(p_action));
	}
	return in->is_key_pressed(p_fallback);
}

} // namespace

void Player::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_move_speed", "value"), &Player::set_move_speed);
	ClassDB::bind_method(D_METHOD("get_move_speed"), &Player::get_move_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed"), "set_move_speed", "get_move_speed");

	ClassDB::bind_method(D_METHOD("set_jump_velocity", "value"), &Player::set_jump_velocity);
	ClassDB::bind_method(D_METHOD("get_jump_velocity"), &Player::get_jump_velocity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_velocity"), "set_jump_velocity", "get_jump_velocity");

	ClassDB::bind_method(D_METHOD("set_camera_distance", "value"), &Player::set_camera_distance);
	ClassDB::bind_method(D_METHOD("get_camera_distance"), &Player::get_camera_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_distance"), "set_camera_distance", "get_camera_distance");

	// 鼠标灵敏度（弧度/像素）。做成属性是为了不重编译就能调手感。
	ClassDB::bind_method(D_METHOD("set_mouse_sensitivity", "value"), &Player::set_mouse_sensitivity);
	ClassDB::bind_method(D_METHOD("get_mouse_sensitivity"), &Player::get_mouse_sensitivity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mouse_sensitivity"), "set_mouse_sensitivity", "get_mouse_sensitivity");

	ClassDB::bind_method(D_METHOD("set_camera_height", "value"), &Player::set_camera_height);
	ClassDB::bind_method(D_METHOD("get_camera_height"), &Player::get_camera_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_height"), "set_camera_height", "get_camera_height");

	ClassDB::bind_method(D_METHOD("set_costume", "costume"), &Player::set_costume);
	ClassDB::bind_method(D_METHOD("get_costume"), &Player::get_costume);
	ClassDB::bind_method(D_METHOD("start_sleep", "bed_pos", "yaw"), &Player::start_sleep);
	ClassDB::bind_method(D_METHOD("stop_sleep"), &Player::stop_sleep);
	ClassDB::bind_method(D_METHOD("get_is_sleeping"), &Player::get_is_sleeping);
	ClassDB::bind_method(D_METHOD("teleport_to", "position"), &Player::teleport_to);
	ClassDB::bind_method(D_METHOD("toggle_map"), &Player::toggle_map);
	ClassDB::bind_method(D_METHOD("get_is_map_open"), &Player::get_is_map_open);
	ClassDB::bind_method(D_METHOD("launch_firework"), &Player::launch_firework);
	ClassDB::bind_method(D_METHOD("_on_map_gui_input", "event"), &Player::_on_map_gui_input);
	ClassDB::bind_method(D_METHOD("_on_map_bg_gui_input", "event"), &Player::_on_map_bg_gui_input);
	ClassDB::bind_method(D_METHOD("_on_map_mouse_exited"), &Player::_on_map_mouse_exited);
	ClassDB::bind_method(D_METHOD("_on_firework_burst", "burst", "sparks", "willow", "bang_voice", "flash_light", "max_energy"), &Player::_on_firework_burst);
	ClassDB::bind_method(D_METHOD("_on_firework_cleanup", "burst_root", "rocket", "launch_voice"), &Player::_on_firework_cleanup);
}

void Player::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// 按名字找到场景里的子节点（Model 只负责外观旋转，Camera3D 是跟随相机）
			model = Object::cast_to<Node3D>(get_node_or_null("Model"));
			camera = Object::cast_to<Camera3D>(get_node_or_null("Camera3D"));
			// 深度精度：Godot 默认 near 0.05 / far 4000，对 448m 的地图过于浪费——
			// 深度缓冲的有效精度大致按 near 反比、按 far 正比，远处两个面（桥面与
			// 引道地形、桥面与贴地装饰薄片）只差几厘米时就会互相覆盖着闪。
			// 收窄到 0.1 / 800（第三人称相机最近 2.5m，800m 覆盖全图对角）后精度约提升一个量级。
			if (camera != nullptr) {
				camera->set_as_top_level(true); // 顶层模式：彻底断开与物理胶囊体逐帧跳跃位移的直接耦合，消除移动视角抖动
				camera->set_near(0.1);
				camera->set_far(800.0);
			}
			set_physics_process(true); // 打开物理帧回调（移动逻辑）
			set_process(true);         // 打开渲染帧回调（相机平滑）
			// 优化斜坡与桥梁行走平滑度：增大最大爬坡角度与吸附距离，彻底杜绝走小桥卡壳、必须跳跃的现象
			set_floor_max_angle(0.95f); // ~54.4度
			set_floor_snap_length(0.35f); // 35cm 吸附高度
			set_floor_stop_on_slope_enabled(true);
			// 打开输入回调：_input 只做"把鼠标原始位移累加起来"这一件事。
			// 视角计算仍在 NOTIFICATION_PROCESS 里（不破坏工程既有的主循环约定）。
			set_process_input(true);

			// 互动提示标签（跟随目标物体，平时隐藏）
			prompt = memnew(Label3D);
			prompt->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
			prompt->set_pixel_size(0.005);
			prompt->set_font_size(52);
			prompt->set_outline_size(12);
			prompt->set_modulate(Color(1.0, 0.95, 0.7));
			prompt->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true); // 被墙挡住也能看见
			// 默认字体无 CJK 字形（中文 prompt 显示方框），挂随工程打包的 Noto Sans SC
			// （OFL 开源许可；与 hud_clock.cpp 同一资源，ResourceLoader 自带缓存）
			Ref<Font> cjk = ResourceLoader::get_singleton()->load(
					"res://assets/fonts/NotoSansSC-Regular.ttf");
			if (cjk.is_valid()) {
				prompt->set_font(cjk);
			}
			prompt->hide();
			add_child(prompt);

			// —— 交互提示的屏幕空间版（HUD）：3D 悬浮牌在视角边缘/贴脸时会被
			// 裁剪，底部居中的 HUD 文字永不被裁。与 3D 牌同步显隐 ——
			hud_layer = memnew(CanvasLayer);
			hud_layer->set_layer(6);
			add_child(hud_layer);
			hud_prompt = memnew(Label);
			hud_prompt->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
			hud_prompt->set_offset(SIDE_LEFT, -200.0f);
			hud_prompt->set_offset(SIDE_RIGHT, 200.0f);
			hud_prompt->set_offset(SIDE_TOP, -108.0f);
			hud_prompt->set_offset(SIDE_BOTTOM, -84.0f);
			hud_prompt->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			hud_prompt->add_theme_font_size_override("font_size", 26);
			hud_prompt->add_theme_color_override("font_color", Color(1.0f, 0.95f, 0.7f));
			hud_prompt->add_theme_color_override("font_outline_color", Color(0.1f, 0.08f, 0.05f));
			hud_prompt->add_theme_constant_override("outline_size", 6);
			if (cjk.is_valid()) {
				hud_prompt->add_theme_font_override("font", cjk);
			}
			hud_prompt->hide();
			hud_layer->add_child(hud_prompt);

			// 右上角全交互按键操作指南卡片 (G / E / F / R / M / 空格)
			PanelContainer *guide_panel = memnew(PanelContainer);
			guide_panel->set_name("HotkeyGuidePanel");
			guide_panel->set_anchors_preset(Control::PRESET_TOP_RIGHT);
			guide_panel->set_offset(SIDE_LEFT, -214.0f);
			guide_panel->set_offset(SIDE_RIGHT, -16.0f);
			guide_panel->set_offset(SIDE_TOP, 16.0f);
			guide_panel->set_offset(SIDE_BOTTOM, 190.0f);

			// 小黑板：墨绿板面 + 原木边框，粉笔字（与木牌/木屋同一材质语言）
			guide_panel->add_theme_stylebox_override("panel", uistyle::make_blackboard_textured(0.98f));
			hud_layer->add_child(guide_panel);

			VBoxContainer *guide_vbox = memnew(VBoxContainer);
			guide_vbox->add_theme_constant_override("separation", 3);
			guide_panel->add_child(guide_vbox);

			const char *hints[] = {
				"[E] 交互 · 门窗 / 床 / 箱",
				"[F] 钓鱼 · 湖畔甩竿提竿",
				"[G] 烟花 · 升空绚烂燃放",
				"[R] 翻滚 · 趣味滚地冲刺",
				"[M] 舆图 · 苍湖全景漫游",
				"[空格] 跳跃 · 支持凌空二段跳",
				"[空格/Ctrl] 游泳 · 上浮 / 下潜"
			};

			for (int hi = 0; hi < 7; ++hi) {
				Label *hl = memnew(Label);
				hl->set_text(String::utf8(hints[hi]));
				uistyle::dress_chalk_label(hl, 13, uistyle::chalk_font());
				guide_vbox->add_child(hl);
			}
			firework_hint = nullptr;

			build_map_ui();
			build_sleep_ui();
			rebuild_costume();
			build_skate_shoes();
			build_propeller();

			// 脚步声：真实雪地踩雪声优先读取外部音频资源（用户下载的踩雪声录音），其余程序化合成
			steps_voice = memnew(AudioStreamPlayer);
			steps_voice->set_name("Steps");
			steps_voice->set_volume_db(-9.0f);
			// 一次性触发音效走 SFX 总线；风/雨/虫鸣等循环环境层留 Master（见 default_bus_layout.tres）
			steps_voice->set_bus(StringName("SFX"));
			add_child(steps_voice);
			ResourceLoader *rl = ResourceLoader::get_singleton();

			// 优先加载选中的草地与泥土踏步外部音效，若无则使用程序化合成保底
			if (rl != nullptr && rl->exists("res://assets/audio/grass_step.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/grass_step.wav");
				if (r.is_valid()) {
					snd_grass = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_grass.is_valid()) {
				snd_grass = make_grass();
			}

			if (rl != nullptr && rl->exists("res://assets/audio/dirt_step.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/dirt_step.wav");
				if (r.is_valid()) {
					snd_dirt = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_dirt.is_valid()) {
				snd_dirt = make_dirt();
			}

			snd_pitch = make_pitch();

			// 优先加载 4 种切片踩雪声变体（提取自用户提供的踩雪声录音，位于 res://assets/audio/）
			for (int i = 0; i < 4; ++i) {
				String p = vformat("res://assets/audio/snow_step_%d.wav", i + 1);
				if (rl != nullptr && rl->exists(p)) {
					Ref<Resource> r = rl->load(p);
					if (r.is_valid()) {
						snd_snow_variants[i] = Object::cast_to<AudioStream>(r.ptr());
					}
				}
			}
			// 也尝试加载完整的 MP3（snow_step.mp3 或 踩雪声.mp3）备用
			if (rl != nullptr && rl->exists("res://assets/audio/snow_step.mp3")) {
				Ref<Resource> r = rl->load("res://assets/audio/snow_step.mp3");
				if (r.is_valid()) {
					snd_snow = Object::cast_to<AudioStream>(r.ptr());
				}
			} else if (rl != nullptr && rl->exists("res://assets/audio/踩雪声.mp3")) {
				Ref<Resource> r = rl->load("res://assets/audio/踩雪声.mp3");
				if (r.is_valid()) {
					snd_snow = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			// 若没有任何外部音频资源成功载入，使用程序化合成保底
			if (!snd_snow.is_valid() && !snd_snow_variants[0].is_valid()) {
				snd_snow = make_snow();
			}

			// 雨天真实踩水声（用户提供，puddle_step.wav）
			if (rl != nullptr && rl->exists("res://assets/audio/puddle_step.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/puddle_step.wav");
				if (r.is_valid()) {
					snd_wet = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_wet.is_valid()) {
				snd_wet = make_puddle();
			}

			// 优先加载用户提供的最新清脆石板踏步音效与变体（stone_step_crisp_1..4）
			for (int i = 0; i < 4; ++i) {
				String path = vformat("res://assets/audio/stone_step_crisp_%d.wav", i + 1);
				if (rl != nullptr && rl->exists(path)) {
					Ref<Resource> r = rl->load(path);
					if (r.is_valid()) {
						snd_stone_variants[i] = Object::cast_to<AudioStream>(r.ptr());
					}
				}
				if (!snd_stone_variants[i].is_valid()) {
					String old_path = vformat("res://assets/audio/stone_step_%d.wav", i + 1);
					if (rl != nullptr && rl->exists(old_path)) {
						Ref<Resource> r = rl->load(old_path);
						if (r.is_valid()) {
							snd_stone_variants[i] = Object::cast_to<AudioStream>(r.ptr());
						}
					}
				}
			}
			if (rl != nullptr && rl->exists("res://assets/audio/stone_step_crisp_1.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/stone_step_crisp_1.wav");
				if (r.is_valid()) {
					snd_stone = Object::cast_to<AudioStream>(r.ptr());
				}
			} else if (rl != nullptr && rl->exists("res://assets/audio/stone_step.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/stone_step.wav");
				if (r.is_valid()) {
					snd_stone = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_stone.is_valid() && !snd_stone_variants[0].is_valid()) {
				snd_stone = make_stone();
			}

			// 优先加载用户提供的真实木板踏步音效与变体
			for (int i = 0; i < 4; ++i) {
				String path = vformat("res://assets/audio/wood_step_%d.wav", i + 1);
				if (rl != nullptr && rl->exists(path)) {
					Ref<Resource> r = rl->load(path);
					if (r.is_valid()) {
						snd_wood_variants[i] = Object::cast_to<AudioStream>(r.ptr());
					}
				}
			}
			if (rl != nullptr && rl->exists("res://assets/audio/wood_step.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/wood_step.wav");
				if (r.is_valid()) {
					snd_wood = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_wood.is_valid() && !snd_wood_variants[0].is_valid()) {
				snd_wood = make_wood();
			}

			// 跳跃落地声音：优先加载用户提供的真实起跳落地踩地声（land_thud.wav）
			if (rl != nullptr && rl->exists("res://assets/audio/land_thud.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/land_thud.wav");
				if (r.is_valid()) {
					snd_land_grass = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_land_grass.is_valid()) {
				snd_land_grass = make_land_grass();
			}
			if (rl != nullptr && rl->exists("res://assets/audio/land_wet.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/land_wet.wav");
				if (r.is_valid()) {
					snd_land_wet = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_land_wet.is_valid()) {
				snd_land_wet = make_land_wet();
			}
			if (rl != nullptr && rl->exists("res://assets/audio/land_snow.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/land_snow.wav");
				if (r.is_valid()) {
					snd_land_snow = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_land_snow.is_valid()) {
				snd_land_snow = snd_snow_variants[1].is_valid() ? snd_snow_variants[1] : snd_snow;
			}
			snd_land_pitch = make_land_pitch();
			snd_double_jump = make_double_jump_snd();
			snd_rustle = make_rustle(false);
			snd_rustle_deep = make_rustle(true);
			rustle_voice = memnew(AudioStreamPlayer);
			rustle_voice->set_name("Rustle");
			rustle_voice->set_volume_db(-16.0f);
			rustle_voice->set_bus(StringName("SFX"));
			add_child(rustle_voice);

			// 界面与交互音效（优先采用选中的禅意音效 zen_press / zen_collapse）
			if (rl != nullptr && rl->exists("res://assets/audio/zen_press.mp3")) {
				Ref<Resource> r = rl->load("res://assets/audio/zen_press.mp3");
				if (r.is_valid()) {
					snd_ui_click = Object::cast_to<AudioStream>(r.ptr());
				}
			} else if (rl != nullptr && rl->exists("res://assets/audio/ui_click.mp3")) {
				Ref<Resource> r = rl->load("res://assets/audio/ui_click.mp3");
				if (r.is_valid()) {
					snd_ui_click = Object::cast_to<AudioStream>(r.ptr());
				}
			}

			if (rl != nullptr && rl->exists("res://assets/audio/zen_collapse.mp3")) {
				Ref<Resource> r = rl->load("res://assets/audio/zen_collapse.mp3");
				if (r.is_valid()) {
					snd_ui_back = Object::cast_to<AudioStream>(r.ptr());
				}
			} else if (rl != nullptr && rl->exists("res://assets/audio/ui_back.mp3")) {
				Ref<Resource> r = rl->load("res://assets/audio/ui_back.mp3");
				if (r.is_valid()) {
					snd_ui_back = Object::cast_to<AudioStream>(r.ptr());
				}
			}

			if (rl != nullptr && rl->exists("res://assets/audio/zen_expand.mp3")) {
				Ref<Resource> r = rl->load("res://assets/audio/zen_expand.mp3");
				if (r.is_valid()) {
					snd_ui_expand = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			ui_voice = memnew(AudioStreamPlayer);
			ui_voice->set_name("UIVoice");
			ui_voice->set_volume_db(-10.0f);
			ui_voice->set_bus(StringName("SFX"));
			add_child(ui_voice);

			const Vector3 gp = get_global_position();
			uint32_t seed = (uint32_t)std::llround(gp.x * 73856093.0)
					^ ((uint32_t)std::llround(gp.z * 19349663.0) << 13);
			rng.seed(seed);

			// 物理碰撞层：第 1 层地形建筑 + 第 2 层动物实体，严防穿模
			set_collision_mask_value(1, true);
			set_collision_mask_value(2, true);

			// 第三人称鼠标视角：开局即捕获鼠标（编辑器里绝不捕获，防抢焦点）
			cam_dist = camera_distance;
			if (!Engine::get_singleton()->is_editor_hint()) {
				mouse_captured = true;
				Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
			}

			// 确保全局脚印与痕迹管理器就绪
			if (FootprintManager::get_singleton() == nullptr && get_parent() != nullptr) {
				Node *existing = get_parent()->get_node_or_null(NodePath("FootprintManager"));
				if (existing == nullptr) {
					FootprintManager *fm = memnew(FootprintManager);
					fm->set_name("FootprintManager");
					get_parent()->add_child(fm);
				}
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (steps_voice != nullptr) {
				steps_voice->stop();
				steps_voice->set_stream(Ref<AudioStream>());
			}
			if (rustle_voice != nullptr) {
				rustle_voice->stop();
				rustle_voice->set_stream(Ref<AudioStream>());
			}
			if (ui_voice != nullptr) {
				ui_voice->stop();
				ui_voice->set_stream(Ref<AudioStream>());
			}
			snd_grass = Ref<AudioStream>();
			snd_dirt = Ref<AudioStream>();
			snd_pitch = Ref<AudioStreamWAV>();
			snd_double_jump = Ref<AudioStreamWAV>();
			snd_snow = Ref<AudioStream>();
			for (int i = 0; i < 4; ++i) {
				snd_snow_variants[i] = Ref<AudioStream>();
			}
			snd_stone = Ref<AudioStream>();
			snd_ui_click = Ref<AudioStream>();
			snd_ui_back = Ref<AudioStream>();
			snd_ui_expand = Ref<AudioStream>();
		} break;

		case NOTIFICATION_PHYSICS_PROCESS:
			_physics_step();
			break;

		case NOTIFICATION_PROCESS: {
			float p_delta = get_process_delta_time();
			if (!is_sleeping && !is_map_open) {
				_update_look(p_delta); // 睡觉或展开大地图时鼠标不转动视角
			}
			_update_camera(p_delta);

			// 动态烟花强光高能衰减驱动：真实照亮夜空、大地与建筑
			for (auto it = active_firework_flashes.begin(); it != active_firework_flashes.end();) {
				if (it->light == nullptr || !it->light->is_inside_tree()) {
					it = active_firework_flashes.erase(it);
					continue;
				}
				it->lifetime += p_delta;
				float t = it->lifetime;
				if (t < 0.12f) {
					// 初始 120ms 耀眼爆闪
					float ratio = t / 0.12f;
					it->light->set_param(OmniLight3D::PARAM_ENERGY, it->max_energy * ratio);
				} else if (t < 1.6f) {
					// 后续 1.5 秒二次缓释渐暗
					float ratio = 1.0f - (t - 0.12f) / (1.6f - 0.12f);
					float curved = ratio * ratio;
					it->light->set_param(OmniLight3D::PARAM_ENERGY, it->max_energy * curved);
				} else {
					it->light->set_param(OmniLight3D::PARAM_ENERGY, 0.0f);
					it->light->queue_free();
					it = active_firework_flashes.erase(it);
					continue;
				}
				++it;
			}

			// 夜晚踩过发光蘑菇：带走荧光微粒飘动
			update_shroom_glow_trail(p_delta);

			// 瞬态屏幕提示倒计时自动隐去（3 秒消失）
			if (hud_prompt_timer > 0.0f) {
				hud_prompt_timer -= p_delta;
				if (hud_prompt_timer <= 0.0f) {
					hud_prompt_timer = 0.0f;
					if (hud_prompt != nullptr && near_target == nullptr) {
						hud_prompt->hide();
					}
				}
			}

			// 夜晚提示：天黑时在右上角显示按 G 放烟花提示，白天自动隐藏
			if (firework_hint != nullptr && get_parent() != nullptr) {
				SkyCycle *sc = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
				if (sc != nullptr) {
					bool is_night = sc->get_night_factor() > 0.35f;
					if (is_night != firework_hint->is_visible()) {
						firework_hint->set_visible(is_night);
					}
				}
			}
		} break;

		default:
			break;
	}
}

void Player::_physics_step() {
	Input *input = Input::get_singleton();
	const double delta = get_physics_process_delta_time();

	// 1. 全局大地图状态阻断（M 在 _input 事件驱动，Esc 关地图归 PauseMenu 统一处理）
	if (is_map_open) {
		set_velocity(Vector3(0, 0, 0));
		move_and_slide();
		return;
	}

	// 2. 上床睡觉状态与黑屏过场
	if (is_sleeping) {
		set_velocity(Vector3(0, 0, 0));
		move_and_slide();
		if (model != nullptr) {
			// 平躺：姿态与位置都在 start_sleep 里算好（sleep_lie_basis / sleep_lie_pos），
			// 这里只叠一层很轻的呼吸起伏（把整个身体在世界上抬降 1.8cm）。
			float breathe = std::sin(sleep_fade_t * 1.55f) * 0.018f;
			model->set_basis(sleep_lie_basis);
			model->set_position(sleep_lie_pos + Vector3(0.0f, breathe, 0.0f));
		}
		update_sleep_cutscene(delta);

		// 按键防抖处理：必须等待玩家先松开之前按下的 E/空格键，且过场已展示 0.6s 之后，
		// 再次主动按下按键才允许提前中断过场！严防上床交互当帧残留按下立刻跳出！
		bool any_cancel_down = act_pressed("interact", KEY_E) || act_pressed("jump", KEY_SPACE);
		if (!any_cancel_down) {
			sleep_key_released = true;
		}
		if (sleep_key_released && sleep_fade_t > 0.6f && any_cancel_down) {
			stop_sleep();
		}
		return;
	}

	// 3. 躺下滚着移动模式（R 键切换）
	bool r_down = act_pressed("roll", KEY_R);
	if (r_down && !r_prev) {
		is_rolling = !is_rolling;
		roll_spin = 0.0;
		play_ui(is_rolling ? 0 : 1);
	}
	r_prev = r_down;

	// 4. 夜晚放烟花功能（G 键触发，与 F 键钓鱼彻底分开，绝不冲突）
	bool g_down = act_pressed("firework", KEY_G);
	if (g_down && !g_prev) {
		launch_firework();
	}
	g_prev = g_down;

	// 读移动输入（世界坐标：W 朝 -Z，S 朝 +Z，A 朝 -X，D 朝 +X；动作表在 project.godot [input]）
	const double ix = (act_pressed("move_right", KEY_D) ? 1.0 : 0.0) - (act_pressed("move_left", KEY_A) ? 1.0 : 0.0);
	const double iz = (act_pressed("move_back", KEY_S) ? 1.0 : 0.0) - (act_pressed("move_forward", KEY_W) ? 1.0 : 0.0);

	Vector3 dir(ix, 0.0, iz);
	if (dir.length_squared() > 0.0001) {
		// WASD 按相机偏航角旋转：W 永远朝画面深处
		dir = Basis(Vector3(0.0, 1.0, 0.0), (float)cam_yaw).xform(dir).normalized();
	}

	Vector3 velocity = get_velocity();

	// 灌木丛减速判定：穿过灌木丛时移速降至约 52%，带有明显的阻滞手感
	Vector3 gp = get_global_position();
	if (terrain_cache == nullptr) {
		Node *parent = get_parent();
		if (parent != nullptr) {
			terrain_cache = Object::cast_to<TerrainBuilder>(parent->get_node_or_null(NodePath("Terrain")));
			if (terrain_cache == nullptr) {
				terrain_cache = Object::cast_to<TerrainBuilder>(parent->get_node_or_null(NodePath("TerrainBuilder")));
			}
		}
	}
	bool in_bush = false;
	is_ice_skating = false;
	float water_level_here = -0.55f;
	float water_depth_here = 0.0f;
	if (terrain_cache != nullptr) {
		in_bush = terrain_cache->is_in_bush(gp.x, gp.z, gp.y);
		is_ice_skating = terrain_cache->is_on_frozen_lake(gp.x, gp.z, gp.y);
		water_level_here = terrain_cache->get_water_level();
		water_depth_here = terrain_cache->water_depth_at(gp.x, gp.z);
	}
	// 深水自动进入游泳：水深 >1.25m 且身体已沉到水面下 0.35m（岸边蹚水仍走正常走路）
	bool swimming = !is_ice_skating && water_depth_here > 1.25f && gp.y < water_level_here - 0.35f;
	if (swimming) {
		is_ice_skating = false;
	}
	is_swimming = swimming;
	if (propeller != nullptr) {
		propeller->set_visible(swimming);
	}
	if (is_swimming && !was_swimming) {
		play_land(2); // 入水水花声
	}
	was_swimming = is_swimming;
	// 踏上冰面时自动穿上滑冰鞋，离开冰面时自动脱下
	set_skate_shoes_visible(is_ice_skating && !is_rolling);

	double current_speed = in_bush ? (move_speed * 0.52) : (is_rolling ? (move_speed * 1.25) : (is_ice_skating ? (move_speed * 1.65) : move_speed));
	if (is_swimming) {
		current_speed = 4.6; // 水中推进：比走路略慢，但六向自由
	}

	// 水平：游泳（水的阻力，加速平滑）/ 冰面滑冰（超低阻尼）/ 普通地面直接设定
	if (is_swimming) {
		Vector3 target_vel = dir * (float)current_speed;
		float k = Math::min(1.0f, (float)delta * 3.4f);
		velocity.x = Math::lerp((float)velocity.x, target_vel.x, k);
		velocity.z = Math::lerp((float)velocity.z, target_vel.z, k);
	} else if (is_ice_skating) {
		Vector3 target_vel = dir * (float)current_speed;
		if (dir.length_squared() > 0.001) {
			// 在冰面上推进：敏捷推背加速滑行
			velocity.x = Math::lerp((float)velocity.x, (float)target_vel.x, (float)(delta * 4.8));
			velocity.z = Math::lerp((float)velocity.z, (float)target_vel.z, (float)(delta * 4.8));
		} else {
			// 松开按键：超低阻尼、极远距离丝滑惯性滑行（畅快滑行飘移减速）
			velocity.x = Math::lerp((float)velocity.x, 0.0f, (float)(delta * 0.38));
			velocity.z = Math::lerp((float)velocity.z, 0.0f, (float)(delta * 0.38));
		}
	} else {
		velocity.x = dir.x * current_speed;
		velocity.z = dir.z * current_speed;
	}

	// 垂直：游泳六向（空格上浮 / Ctrl·C 下潜）vs 走路重力 + 跳跃
	bool space_down = act_pressed("jump", KEY_SPACE);
	if (is_swimming) {
		double up = space_down ? 1.0 : 0.0;
		double down = (input->is_key_pressed(KEY_CTRL) || input->is_key_pressed(KEY_C)) ? 1.0 : 0.0;
		double vy_target = (up - down) * 2.6; // 无输入时中性浮力，悬停
		velocity.y = (double)Math::lerp((float)velocity.y, (float)vy_target,
				(float)Math::min(1.0, delta * 4.5));
		jump_count = 0;
		is_double_jumping = false;
		double_jump_spin = 0.0;
	} else {
	// 垂直：重力 + 落地归零与跳跃计数重置
	if (is_on_floor()) {
		if (velocity.y < 0.0) {
			velocity.y = 0.0;
		}
		jump_count = 0;
		is_double_jumping = false;
		double_jump_spin = 0.0;
	} else {
		velocity.y -= gravity * delta;
		// 如果从边缘走落或掉下悬崖未主动起跳，默认已消耗一段跳，允许进行空中二段跳
		if (jump_count == 0) {
			jump_count = 1;
		}
	}

	// 跳跃与二段跳系统（空格键边沿触发）
	if (space_down && !space_prev) {
		if (is_on_floor()) {
			// 一段跳：地面起跳
			velocity.y = jump_velocity;
			jump_count = 1;
			// 冰面起跳空中旋转一圈 (360度优雅自转跳跃)
			if (is_ice_skating) {
				is_ice_jumping = true;
				ice_jump_spin = 0.0;
			}
			// 下雨天起跳时播放专属踩水飞溅起跳声
			int cur_kind = _step_kind();
			if (cur_kind == 2) {
				play_step(2, 1.15f);
			}
		} else if (jump_count == 1) {
			// 二段跳：空中凌空借力二次起跳！
			velocity.y = jump_velocity * 0.95; // 二段跳垂直初速饱满充足
			jump_count = 2;
			is_double_jumping = true;
			double_jump_spin = 0.0;

			// 空中若输入了方向，二段跳立即顺滑修正并赋予空中机动冲力
			if (dir.length_squared() > 0.001) {
				velocity.x = dir.x * current_speed * 1.15;
				velocity.z = dir.z * current_speed * 1.15;
			}

			// 播放二段跳专属清脆空气动力爆鸣声
			if (steps_voice != nullptr && snd_double_jump.is_valid()) {
				steps_voice->set_stream(snd_double_jump);
				steps_voice->set_pitch_scale(1.0f + ((float)(rand() % 10) - 5.0f) * 0.015f);
				steps_voice->set_volume_db(-6.0f);
				steps_voice->play();
			}
		}
	}
	}
	space_prev = space_down;

	set_velocity(velocity);
	move_and_slide();

	// 游泳高度夹取：上不冲出水面（允许头顶微露呼吸），下不钻进湖底
	if (is_swimming && terrain_cache != nullptr) {
		Vector3 p = get_global_position();
		float wl = terrain_cache->get_water_level();
		float wd = terrain_cache->water_depth_at(p.x, p.z);
		float bed = wl - wd;
		Vector3 v = get_velocity();
		bool clamped = false;
		if (p.y > wl - 0.75f) {
			p.y = wl - 0.75f;
			if (v.y > 0.0) {
				v.y = 0.0;
			}
			clamped = true;
		}
		if (p.y < bed + 0.75f) {
			p.y = bed + 0.75f;
			if (v.y < 0.0) {
				v.y = 0.0;
			}
			clamped = true;
		}
		if (clamped) {
			set_global_position(p);
			set_velocity(v);
		}
	}

	// 防坠兜底：任何情况下掉出世界（y<-60）直接传送回出生点
	// （历史事故：扩展失效期间地形碰撞消失，主角坠穿场景被一起存盘）
	if (get_global_position().y < -60.0) {
		set_global_position(Vector3(0.0, 0.5, -28.0));
		Vector3 v0 = get_velocity();
		v0.y = 0.0;
		set_velocity(v0);
	}

	// 落地保持计时器递减
	if (land_timer > 0.0) {
		land_timer = Math::max(0.0, land_timer - delta);
	}

	// 脚步声：走路按距离触发，落地瞬间补一声音量更沉的着地声
	_update_footsteps(delta, is_on_floor());
	was_on_floor = is_on_floor();

	// 穿草/灌木声：在深草区或穿过灌木丛移动时触发，伴随沙沙声
	if (rustle_voice != nullptr && is_on_floor()) {
		const Vector3 v2 = get_velocity();
		double spd = Vector2(v2.x, v2.z).length();
		const Vector3 gp2 = get_global_position();
		bool tall = (Vector2(gp2.x + 20.0f, gp2.z + 6.0f).length() < 5.5f)
				|| (Vector2(gp2.x - 36.0f, gp2.z - 16.0f).length() < 5.5f);
		bool in_bush_moving = in_bush && spd > 0.35;
		if ((spd > 0.8 && tall && _step_kind() == 0) || in_bush_moving) {
			rustle_acc += spd * delta;
			if (rustle_acc >= (in_bush ? 0.70 : 0.95)) {
				rustle_acc = 0.0;
				std::uniform_real_distribution<float> pd(0.92f, 1.08f);
				rustle_voice->set_stream(snd_rustle_deep);
				rustle_voice->set_volume_db(in_bush ? -12.0f : -14.0f);
				rustle_voice->set_pitch_scale(pd(rng));
				rustle_voice->play();
			}
		} else {
			rustle_acc = 0.0;
		}
	}

	// 让"外观模型"转向移动方向（Player 本体不转，保证跟随相机稳定），
	// 躺平滚地模式下沿着自身纵轴飞速自转滚动！
	if (model != nullptr) {
		if (dir.length_squared() > 0.0001) {
			facing = dir; // 记住最后移动方向，站着不动时保持朝向
		}
		float yaw = Math::atan2(facing.x, facing.z);

		if (is_swimming) {
			// 横躺俯泳：身体沿前进方向水平，抬头/低头跟随上浮下潜
			float yaw = Math::atan2(facing.x, facing.z);
			const Vector3 hv = get_velocity();
			float pitch = Math::clamp(-(float)hv.y * 0.14f, -0.55f, 0.55f);
			swim_t += delta;
			swim_roll = std::sin((float)swim_t * 2.1f) * 0.06f;
			Basis b = Basis(Vector3(0.0, 1.0, 0.0), yaw)
					* Basis(Vector3(1.0, 0.0, 0.0), 1.570796f + pitch)
					* Basis(Vector3(0.0, 0.0, 1.0), swim_roll);
			model->set_basis(b);
			// 胶囊中心（局部 0,0.9,0）抬到 Player 原点上方 0.62m，水平漂浮
			Vector3 center_offset = b.xform(Vector3(0.0f, 0.9f, 0.0f));
			model->set_position(Vector3(0.0f, 0.62f, 0.0f) - center_offset);
			// 螺旋桨转速跟速度走，悬停时怠速
			if (propeller_blades != nullptr) {
				float spd = Vector2(hv.x, hv.z).length();
				propeller_blades->rotate_object_local(Vector3(0.0, 1.0, 0.0),
						(float)delta * (5.0f + spd * 6.0f));
			}
		} else if (is_rolling) {
			const Vector3 hv = get_velocity();
			double hspd = Vector2(hv.x, hv.z).length();
			if (hspd > 0.15) {
				// 顺应前进方向向前自转翻滚（取负角匹配右手系 X 轴正向旋转法则，确保顶端向前翻转、着地处抓地）
				roll_spin -= delta * hspd * 6.5;
			}
			// 横向平躺翻滚：躯干横置（垂直于前进方向左右横放，像热狗一样向前翻滚）
			// 局部 Y 轴横转 90 度贴合 X 轴，绕横向轴做顺滑向前自转滚动
			Basis b = Basis(Vector3(0.0, 1.0, 0.0), yaw) * Basis(Vector3(1.0, 0.0, 0.0), (float)roll_spin) * Basis(Vector3(0.0, 0.0, 1.0), 1.570796f);
			model->set_basis(b);
			// 胶囊体几何中心在 Model 内部位于 (0, 0.9, 0)。
			// 在平躺旋转后，必须补偿旋转偏移，确保胶囊体几何中心水平坐标精确处于 (0, 0)
			// 与 Player 坐标完全重合，消除半个身位的旋转半径偏移，同时高度贴地平置 (y = 0.38m)。
			Vector3 center_offset = b.xform(Vector3(0.0f, 0.9f, 0.0f));
			model->set_position(Vector3(0.0f, 0.38f, 0.0f) - center_offset);
		} else {
			double lean = 0.0;
			if (lean_time > 0.0) {
				lean_time -= delta;
				double phase = 1.0 - Math::clamp(lean_time / 0.5, 0.0, 1.0); // 0..1
				lean = std::sin(phase * M_PI) * 0.5;                         // 前倾 0.5rad 再回正
			}

			// 滑冰起跳空中 360 度旋转：在滞空跳跃期间绕自身垂直 Y 轴平滑自转 360 度 (2*pi)
			if (is_ice_jumping) {
				if (!is_on_floor()) {
					if (ice_jump_spin < Math::TAU) {
						// 滞空空中转速：~0.55s 完成 360 度优雅阿克塞尔/贝尔曼转体
						ice_jump_spin = Math::min(ice_jump_spin + delta * 11.4, (double)Math::TAU);
					}
				} else {
					is_ice_jumping = false;
					ice_jump_spin = 0.0;
				}
			}

			// 二段跳空中水平灵动旋转 360 度（绕自身垂直 Y 轴水平陀螺自转一圈，不再竖着翻滚）
			if (is_double_jumping) {
				if (!is_on_floor()) {
					if (double_jump_spin < Math::TAU) {
						double_jump_spin = Math::min(double_jump_spin + delta * 13.5, (double)Math::TAU);
					}
				} else {
					is_double_jumping = false;
					double_jump_spin = 0.0;
				}
			}

			Basis b = Basis(Vector3(0.0, 1.0, 0.0), yaw + (float)ice_jump_spin + (float)double_jump_spin) *
					Basis(Vector3(1.0, 0.0, 0.0), lean);
			model->set_basis(b);

			// 走路颠簸感：步频同步的小幅起伏（每 1.05m 半周期，与 2.1m 步距/脚步声合拍）
			const Vector3 hv = get_velocity();
			double hspd = Vector2(hv.x, hv.z).length();
			float bob_y = 0.0f;
			if (hspd > 0.6 && is_on_floor()) {
				bob_phase += delta * hspd * 2.99;
				bob_y = std::abs(std::sin(bob_phase)) * 0.05f;
			}
			model->set_position(Vector3(0.0, bob_y, 0.0));
		}
	}

	// 天气测试快捷键 T / F8：快速切换天气（晴天 -> 雨天 -> 雪天），便于随时试听雨天水花与真实踩雪音效
	bool t_down = input->is_key_pressed(KEY_T) || input->is_key_pressed(KEY_F8);
	if (t_down && !weather_toggle_prev && get_parent() != nullptr) {
		SkyCycle *sc = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
		if (sc != nullptr) {
			int cur_w = sc->get_weather();
			if (cur_w == 4) {
				// 当前是雪天 -> 转为晴天
				sc->set_weather(0);
				sc->set_cloud(0.0f);
				sc->set_cloud_target(0.0f);
				sc->set_rain(0.0f);
				sc->set_rain_target(0.0f);
				sc->set_snow(0.0f);
				sc->set_snow_target(0.0f);
			} else if (cur_w == 2 || cur_w == 3) {
				// 当前是雨天 -> 转为大雪天
				sc->set_weather(4);
				sc->set_cloud(0.95f);
				sc->set_cloud_target(0.95f);
				sc->set_rain(0.0f);
				sc->set_rain_target(0.0f);
				sc->set_snow(1.0f);
				sc->set_snow_target(1.0f);
			} else {
				// 当前是晴天/多云 -> 转为雨天
				sc->set_weather(2);
				sc->set_cloud(0.92f);
				sc->set_cloud_target(0.92f);
				sc->set_rain(0.6f);
				sc->set_rain_target(0.6f);
				sc->set_snow(0.0f);
				sc->set_snow_target(0.0f);
			}
		}
	}
	weather_toggle_prev = t_down;

	_update_interaction();
}

// 互动：找最近的可交互物 → 视线遮挡校验（防隔墙开箱/开窗） → 更新提示 → E 键边沿触发
void Player::_update_interaction() {
	Interactable *best = nullptr;
	double best_d = 1e9;

	PhysicsDirectSpaceState3D *space = get_world_3d() != nullptr ? get_world_3d()->get_direct_space_state() : nullptr;
	const Vector3 eye_pos = get_global_position() + Vector3(0.0, 1.25, 0.0);

	Array nodes = get_tree()->get_nodes_in_group("interactables");
	for (int i = 0; i < nodes.size(); i++) {
		Node *n = Object::cast_to<Node>(nodes[i]);
		Interactable *it = Object::cast_to<Interactable>(n);
		if (it == nullptr) {
			continue;
		}
		Vector3 target_pos = it->get_interact_pos();
		double d = get_global_position().distance_squared_to(target_pos);
		float max_d = it->get_interact_distance();
		if (d > (double)(max_d * max_d)) {
			continue;
		}

		// 视线遮挡射线检测：杜绝在房屋外面隔墙开箱子、隔墙操作室内家具
		// 窗户本身嵌入在墙体/窗洞内壁碰撞体中，直接使用 requires_line_of_sight() 免除自相交阻断
		if (it->requires_line_of_sight() && space != nullptr) {
			Ref<PhysicsRayQueryParameters3D> ray = PhysicsRayQueryParameters3D::create(eye_pos, target_pos);
			ray->set_collision_mask(1); // 第 1 层：建筑与地形
			Array ex;
			ex.push_back(get_rid());
			ray->set_exclude(ex);
			Dictionary hit = space->intersect_ray(ray);
			if (!hit.is_empty()) {
				Vector3 hit_pos = hit["position"];
				float hit_dist = eye_pos.distance_to(hit_pos);
				float total_dist = eye_pos.distance_to(target_pos);
				if (hit_dist < total_dist - 0.28f) {
					continue; // 视线被墙体或障碍物阻挡，不可隔墙交互！
				}
			}
		}

		if (d < best_d) {
			best_d = d;
			best = it;
		}
	}

	// 切换目标时刷新提示文案与位置
	if (best != near_target && prompt != nullptr) {
		near_target = best;
		if (best != nullptr) {
			prompt->set_text(best->get_prompt());
			prompt->set_global_position(best->get_interact_pos() + Vector3(0.0, 0.85, 0.0));
			prompt->show();
		} else {
			prompt->hide();
		}
		if (hud_prompt != nullptr) { // HUD 同步显隐（永不裁剪的那份）
			if (best != nullptr) {
				hud_prompt->set_text(best->get_prompt());
				hud_prompt->show();
			} else {
				hud_prompt->hide();
			}
		}
	}
	// 已选中的目标开着/关着时提示文案可能变化（开门后变"关门"）
	if (near_target != nullptr && prompt != nullptr && prompt->is_visible()) {
		prompt->set_text(near_target->get_prompt());
		prompt->set_global_position(near_target->get_interact_pos() + Vector3(0.0, 0.85, 0.0));
		if (hud_prompt != nullptr) {
			hud_prompt->set_text(near_target->get_prompt());
		}
	}

	// E 键按下瞬间触发（按住不放只触发一次）
	bool e_down = act_pressed("interact", KEY_E);
	if (e_down && !e_prev && near_target != nullptr) {
		near_target->interact(get_global_position());
		lean_time = 0.5; // 播放推门/推窗的前倾动作
		play_ui(0);
	}
	e_prev = e_down;
}

void Player::play_ui(int type) {
	if (ui_voice == nullptr) {
		return;
	}
	Ref<AudioStream> snd = snd_ui_click;
	float vol = -10.0f;
	if (type == 1) {
		snd = snd_ui_back;
		vol = -11.0f;
	} else if (type == 2) {
		snd = snd_ui_expand.is_valid() ? snd_ui_expand : snd_ui_click;
		vol = -10.0f;
	}
	if (snd.is_valid()) {
		ui_voice->set_stream(snd);
		ui_voice->set_volume_db(vol);
		ui_voice->play();
	}
}

// 鼠标原始位移：在这里累加，_update_look 每帧消费一次。
//
// 为什么不用 Input::get_last_mouse_velocity()：
//   它返回的是"像素/秒"的瞬时速度估计，拿它再乘帧时间积分回去，
//   等效于用上一段事件的平均速度推算这一帧该转多少 —— 手感上就是
//   慢速微调转不到位、快速甩动又过冲，而且帧率一变灵敏度就跟着变。
//   视角要的是"这一帧累计移动了多少像素"，直接取位移才对。
//
// 注意这里是"轮询 + _input 只做累加"的分工：真正的视角计算仍在
// NOTIFICATION_PROCESS 里（与工程既有的 _notification 主循环约定一致）。
void Player::_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo()) {
		// M 开关大地图（动作优先、键码兜底）。Esc 关地图已移交 PauseMenu 统一处理：
		// Esc 全局只归它管，避免"关地图 + 开暂停菜单"同帧双响应。
		InputMap *im = InputMap::get_singleton();
		bool m_hit = (im != nullptr && im->has_action(StringName("map_toggle")))
			? k->is_action_pressed(StringName("map_toggle"))
			: (k->get_keycode() == KEY_M || k->get_physical_keycode() == KEY_M);
		if (m_hit) {
			toggle_map();
			return;
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && mouse_captured) {
		mouse_rel += mm->get_relative();
		input_seen = true;
	}
}
// 鼠标视角：点击画面捕获鼠标，滚轮缩放距离；Esc 的释放与还原由 PauseMenu
// 统一接管（呼出时存模式并显示光标，恢复时还原）。左右方向键恒速环绕兜底。
// 轮询路线（Input 单例）而非 _unhandled_input——与工程
// "_notification 优先于 GDVIRTUAL override"的既有约定一致。
void Player::_update_look(double delta) {
	if (is_map_open) {
		return; // 大地图开启期间绝对禁止捕获鼠标与转动视角！
	}

	Input *input = Input::get_singleton();

	// 键盘转向：与鼠标同向约定——向右转 = cam_yaw 减小。不要求鼠标已捕获，
	// 两者可叠加；调用点已挡掉睡觉与大地图状态。
	float kb_turn = 0.0f;
	if (act_pressed("camera_left", KEY_LEFT)) {
		kb_turn += 1.0f;
	}
	if (act_pressed("camera_right", KEY_RIGHT)) {
		kb_turn -= 1.0f;
	}
	if (kb_turn != 0.0f) {
		cam_yaw += (double)kb_turn * 2.4 * delta; // ≈137°/秒，与鼠标微调手感不冲突
	}

	bool lmb = input->is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
	if (lmb && !lmb_prev && !mouse_captured) {
		mouse_captured = true;
		input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
		play_ui(2);
	}
	lmb_prev = lmb;

	if (!mouse_captured) {
		mouse_rel = Vector2(); // 没捕获时不要把位移攒着，否则下次一进来会猛地跳一下
		return;
	}

	// 本帧累计的像素位移（消费后清零）
	Vector2 rel = mouse_rel;
	mouse_rel = Vector2();
	if (!input_seen) {
		// 兜底：万一 _input 一次都没被派发（本工程历史上担心过这件事），
		// 退回老的"速度 × 帧时间"，至少还能转。
		rel = input->get_last_mouse_velocity() * (float)delta;
	}
	cam_yaw -= (double)rel.x * mouse_sens;
	cam_pitch = Math::clamp(cam_pitch + (double)rel.y * mouse_sens_y, -0.5, 1.25);

	if (input->is_mouse_button_pressed(MOUSE_BUTTON_WHEEL_UP)) {
		cam_dist = Math::max(2.5, cam_dist - 0.6);
	}
	if (input->is_mouse_button_pressed(MOUSE_BUTTON_WHEEL_DOWN)) {
		cam_dist = Math::min(9.0, cam_dist + 0.6);
	}
}

// 第三人称越肩相机：以"胸口支点"为球心，偏航/俯仰决定球面机位；
// 支点→机位有遮挡时沿视线回拉（防穿墙，回拉直达不平滑，避免闪墙）。
//
// **平滑只作用在跟随点上，不作用在环绕上**：
//   原来是对最终机位做 lerp（camera_smoothing=12，时间常数约 83ms）——
//   鼠标一转，目标机位是绕球面瞬移的，但相机位置在慢慢追，
//   结果就是"视角永远慢半拍"。用户报的"有延迟/有积分"主要就是这一条。
//   现在改成：跟随时（角色走动）平滑，环绕（鼠标转动）直接给到位。
void Player::_update_camera(double delta) {
	if (camera == nullptr) {
		return;
	}

	// —— 睡觉机位：木屋内固定机位，床尾斜上方看向床头 ——
	// 跟随相机在床上的支点会被抬到床面上方 1.55m，加上机位距离后基本飘到屋顶外面，
	// 所以睡觉时必须换成手工机位，否则玩家根本看不到自己"平躺在床上"。
	// 前 0.35s 从当前机位平滑推进过去，避免生硬跳切。
	if (is_sleeping) {
		Vector3 bx = Vector3(std::cos(bed_yaw), 0.0f, -std::sin(bed_yaw)); // 床局部 +X（指向床尾）
		Vector3 bz = Vector3(std::sin(bed_yaw), 0.0f, std::cos(bed_yaw));  // 床局部 +Z（床的侧向）
		Vector3 want = bed_pos + bx * 2.0f + bz * 0.55f + Vector3(0.0f, 1.85f, 0.0f);
		Vector3 look = bed_pos + Vector3(0.0f, 0.50f, 0.0f);
		float t = Math::clamp(sleep_fade_t / 0.35f, 0.0f, 1.0f);
		camera->set_global_position(camera->get_global_position().lerp(want, t));
		camera->look_at(look, Vector3(0.0, 1.0, 0.0));
		return; // 不走跟随 + 遮挡回拉（机位是手工摆在屋里的）
	}

	const Vector3 head = get_global_position() + Vector3(0.0, is_swimming ? 0.55 : camera_height, 0.0);

	if (!cam_follow_init) {
		cam_follow = head;
		cam_follow_init = true;
	}
	double factor = 1.0 - std::exp(-camera_smoothing * delta);
	if (factor > 1.0) {
		factor = 1.0;
	}
	cam_follow = cam_follow.lerp(head, factor);

	double cam_dist_eff = is_swimming ? Math::min(cam_dist, 4.0) : cam_dist;
	Vector3 off = Basis(Vector3(0.0, 1.0, 0.0), (float)cam_yaw).xform(
			Vector3(0.0, std::sin(cam_pitch), std::cos(cam_pitch)) * cam_dist_eff);
	Vector3 want = cam_follow + off;

	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	if (space != nullptr) {
		Ref<PhysicsRayQueryParameters3D> ray = PhysicsRayQueryParameters3D::create(cam_follow, want);
		ray->set_collision_mask(1); // 仅阻挡第 1 层建筑与地形实体，严防被动物或草叶错误触发猛烈回拉
		Array ex;
		ex.push_back(get_rid());
		ray->set_exclude(ex);
		Dictionary hit = space->intersect_ray(ray);
		if (!hit.is_empty()) {
			// 支点→机位之间有遮挡：沿视线回拉
			Vector3 hp = hit["position"];
			Vector3 dir_from_follow = (hp - cam_follow);
			float d = dir_from_follow.length();
			if (d > 0.45f) {
				// 留足 20cm 距墙缓冲，防止贴墙穿模或过度贴近玩家后脑勺
				want = cam_follow + dir_from_follow.normalized() * (d - 0.20f);
			} else {
				// 空间极端狭小时，将相机微抬并保持在头顶斜后方，避免完全挤死
				want = cam_follow + Vector3(0.0f, 0.40f, 0.0f) + dir_from_follow.normalized() * 0.35f;
			}
		}
	}

	// 水下机位约束：人在水里时相机尽量也留在水下，否则会隔着水面看人；
	// 同时不能扎进湖底。
	if (is_swimming && terrain_cache != nullptr) {
		float wl = terrain_cache->get_water_level();
		if (get_global_position().y < wl - 1.0f && want.y > wl - 0.22f) {
			want.y = wl - 0.22f;
		}
		float wd = terrain_cache->water_depth_at(want.x, want.z);
		float bed = wl - wd;
		if (want.y < bed + 0.35f) {
			want.y = bed + 0.35f;
		}
	}

	// 最终机位与注视目标：
	// 重点：look_at 对准与相机在同一平滑坐标系中的跟随焦点 cam_follow，
	// 彻底消除以往因 head 逐物理帧离散跳跃导致的上下颠簸与剧烈视角抖动！
	camera->set_global_position(want);
	camera->look_at(cam_follow + Vector3(0.0, 0.25, 0.0), Vector3(0.0, 1.0, 0.0));
}

// 脚步声触发：在地面且水平速度足够时按累计距离触发（约 2.1m 一步），
// 从空中落地的瞬间立即补一声着地音（音高低一点，更沉）。
// 雪面判定：SkyCycle 的雪量因子 > 0.45 时换雪地嘎吱音色。
void Player::_update_footsteps(double delta, bool on_floor) {
	if (steps_voice == nullptr) {
		return;
	}

	// 落地边沿（起跳后重新触地）
	if (on_floor && !was_on_floor) {
		is_ice_jumping = false;
		ice_jump_spin = 0.0;
		step_acc = 0.0;
		land_timer = 0.40; // 连跳与落地缓冲：接地点草叶保持倒伏并回弹 0.4 秒
		last_land_pos = get_global_position();
		const int kind = _step_kind();
		play_land(kind); // 专属跳跃落地音效（草地/雨天踩水/足球场/雪地真实双脚着地冲击）

		// 落地留下浅浅的自然压痕（冰面落地留下左右双脚清晰单刀刹停割痕）
		if (FootprintManager::get_singleton() != nullptr) {
			Vector3 pos = get_global_position();
			float yaw = Math::atan2(facing.x, facing.z);
			if (kind == 7 || is_ice_skating) {
				Vector3 fwd = facing.normalized();
				if (fwd.length_squared() < 0.001f) {
					fwd = Vector3(0.0f, 0.0f, -1.0f);
				}
				Vector3 right = fwd.cross(Vector3(0.0f, 1.0f, 0.0f)).normalized();
				// 落地时双脚各切一道刹停痕
				FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_SKATE_LEFT, pos - right * 0.18f, yaw - 0.15f, 1.15f, kind);
				FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_SKATE_RIGHT, pos + right * 0.18f, yaw + 0.15f, 1.15f, kind);
			} else {
				FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_HUMAN_TRACK, pos, yaw, 1.15f, kind);
			}
		}
		return;
	}

	if (!on_floor) {
		return;
	}

	const Vector3 v = get_velocity();
	const double speed = Vector2(v.x, v.z).length();
	if (speed < 0.3) {
		return; // 站着/躺着不动不响
	}

	const int kind = _step_kind();

	// 躺下翻滚移动的特殊处理：横向宽体辗压痕 + 身上粘雪/泥巴
	if (is_rolling) {
		roll_track_acc += speed * delta;
		if (roll_track_acc >= 0.75) { // 每滚动 0.75m 铺一道横向圆柱辗印，并播放一次辗压地面音（雪地深压咯吱咯吱）
			roll_track_acc = 0.0;
			if (FootprintManager::get_singleton() != nullptr) {
				// 翻滚痕迹与本体精确对齐：以胶囊体与地面的实际接触中心为准，消除前后偏差
				Vector3 pos = get_global_position();
				float yaw = Math::atan2(facing.x, facing.z);
				FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_ROLL_TRACK, pos, yaw, 1.0f, kind);
			}
			// 躺地翻滚辗压声：若在雪地则连续发出深沉咯吱咯吱积雪挤压脆响，草地/泥地发出松软翻滚踏地声
			std::uniform_real_distribution<float> pitch_dist(0.90f, 1.05f);
			play_step(kind, pitch_dist(rng));
		}
		// 累积身上雪和泥巴
		float snow = 0.0f;
		float rain = 0.0f;
		if (get_parent() != nullptr) {
			SkyCycle *sc = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
			if (sc != nullptr) {
				snow = sc->get_snow_amount();
				rain = sc->get_rain_amount();
			}
		}
		if (snow > 0.15f) {
			snow_stain = Math::min(1.0f, snow_stain + (float)delta * 0.45f);
		}
		if (rain > 0.20f || kind == 2 || kind == 6) {
			mud_stain = Math::min(1.0f, mud_stain + (float)delta * 0.45f);
		}
		update_body_stains(delta);
		return;
	}

	// 站立行走模式：泥和雪随行走振动非常缓慢地脱落
	if (snow_stain > 0.0f || mud_stain > 0.0f) {
		snow_stain = Math::max(0.0f, snow_stain - (float)delta * 0.02f);
		mud_stain = Math::max(0.0f, mud_stain - (float)delta * 0.015f);
		update_body_stains(delta);
	}

	// 冰面滑冰模式：左一下、右一下交替向外侧蹬冰切削，单脚独立长弧线划痕（绝不双脚并列并排）
	if (is_ice_skating) {
		skate_track_acc += speed * delta;
		if (skate_track_acc >= 0.95) { // 滑行推进间距拉长至 0.95m，与更长更飘逸的冰痕完美连贯
			skate_track_acc = 0.0;
			skate_leg_left = !skate_leg_left; // 左右严格交替
			if (FootprintManager::get_singleton() != nullptr) {
				Vector3 fwd = facing.normalized();
				if (fwd.length_squared() < 0.001f) {
					fwd = Vector3(0.0f, 0.0f, -1.0f);
				}
				Vector3 right = fwd.cross(Vector3(0.0f, 1.0f, 0.0f)).normalized();
				float side_dist = skate_leg_left ? -0.22f : 0.22f; // 左右脚蹬冰开步间距约 44cm
				Vector3 blade_pos = get_global_position() + right * side_dist;

				// 蹬冰时刀刃向滑行方向外侧斜蹬约 14°（±0.24 rad），刻出流畅大弧线
				float yaw = Math::atan2(facing.x, facing.z) + (skate_leg_left ? -0.24f : 0.24f);
				FootprintManager::FootprintType skate_type = skate_leg_left ? FootprintManager::TYPE_SKATE_LEFT : FootprintManager::TYPE_SKATE_RIGHT;
				FootprintManager::get_singleton()->spawn_footprint(skate_type, blade_pos, yaw, 1.0f, kind);
			}
		}
	} else {
		skate_track_acc = 0.0;
	}

	step_acc += speed * delta;
	float step_interval = is_ice_skating ? 1.35f : 1.70f; // 滑冰步频更灵敏清脆
	if (step_acc >= step_interval) {
		step_acc = 0.0;
		std::uniform_real_distribution<float> pitch_dist(0.92f, 1.08f);
		play_step(kind, pitch_dist(rng));

		// 行走留痕：普通路面留下单一微痕，冰面滑行则由专属冰刀划痕负责
		if (!is_ice_skating && FootprintManager::get_singleton() != nullptr) {
			Vector3 pos = get_global_position();
			float yaw = Math::atan2(facing.x, facing.z);
			FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_HUMAN_TRACK, pos, yaw, 0.95f, kind);
		}
	}
}

// 当前地面音色判定（每步实时读天气与所处地形）：
// 木屋地板（宅基地圈内且脚面高于地基）→ 4；
// 石桥面（硬质地面，优先于天气——雪积在拱石上踩的仍是石板声）→ 3；
// 雪量 >0.45 → 1 雪地嘎吱；雨量 >0.5 → 2 湿泥吧唧；否则 0 草地/泥土。
// 找不到 SkyCycle 视为干燥草地。
// 桥面区域取自 terrain_builder 的 on_bridge_deck 排除矩形（x -21.6..9.6, |z-26|<2.6），
// 略收窄到桥宽以内避免岸边误判；拱桥上玩家不可能站在桥洞下，无需按高度过滤。
int Player::_step_kind() const {
	const Vector3 gp = get_global_position();
	// 1. 冰面滑冰（冬天下雪结冰，踏在湖面/溪流冰层上）：返回 7（清脆冰刃刮擦滑行踏冰声）
	if (is_ice_skating) {
		return 7;
	}

	// 2. 木屋室内地板：宅基地 (12,8) 半径 7m 内且脚底高于宅基平台（0.35+地板抬升）
	if (Vector2(gp.x - 12.0f, gp.z - 8.0f).length() < 7.0f && gp.y > 0.52f) {
		return 4;
	}
	// 3. 石拱桥面（硬质拱石，石板脆响）
	if (Math::abs(gp.z - 26.0f) < 2.3f && gp.x > -21.5f && gp.x < 9.5f) {
		return 3;
	}
	// 4. 湖上小木桥（原木桥面，踏实木板声）：桥跨于湖湾与入湖小河上 (32, 62)，范围 4.0m 且脚底在桥面高度 (y > -0.2f)
	if (std::hypot(gp.x - 32.0f, gp.z - 62.0f) < 4.0f && gp.y > -0.2f) {
		return 4;
	}

	// 3. 天气覆盖（室外路面受雨雪覆盖）：
	// 无论处于自然草地、泥土小径还是足球场，雨天踩水或雪天积雪均覆盖为相应天气材质声音
	if (get_parent() != nullptr) {
		SkyCycle *sc = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
		if (sc != nullptr) {
			int w_state = sc->get_weather();
			float snow_val = sc->get_snow_amount();
			float rain_val = sc->get_rain_amount();
			if (w_state == 4 || snow_val > 0.15f) {
				return 1; // 真实雪地咯吱咯吱与落雪深陷
			}
			if (w_state == 2 || w_state == 3 || rain_val > 0.15f) {
				return 2; // 雨天踩水溅射与水花落地
			}
		}
	}

	// 4. 地貌/地表分类（干燥或晴天）
	if (terrain_cache == nullptr && get_parent() != nullptr) {
		terrain_cache = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("Terrain")));
		if (terrain_cache == nullptr) {
			terrain_cache = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("TerrainBuilder")));
		}
	}
	if (terrain_cache != nullptr) {
		if (terrain_cache->in_pitch(gp.x, gp.z)) {
			return 5; // 足球场专用草坪（软弹细密，无杂草沙沙）
		}
		if (terrain_cache->dist_to_path(gp.x, gp.z) < 1.45f) {
			return 6; // 泥土小径（坚实紧凑，微砂石感，无草丛沙沙）
		}
	}

	return 0; // 自然荒草地
}

void Player::play_step(int kind, float pitch) {
	if (steps_voice == nullptr) {
		return;
	}
	Ref<AudioStream> snd;
	float vol;
	if (kind == 1) {
		// 雪地踩雪声：采用用户指定的真实踩雪录音（snow_step）
		// 落地声（pitch < 0.90）固定选取第 2 个变体（能量最充实，落雪深压质感最强）
		if (pitch < 0.90f && snd_snow_variants[1].is_valid()) {
			snd = snd_snow_variants[1];
			vol = -10.5f; // 落地沉重深陷音量
		} else {
			// 行走踏步：在可用的切片变体中轮换，生动自然，绝不重复机械
			int count = 0;
			for (int i = 0; i < 4; ++i) {
				if (snd_snow_variants[i].is_valid()) {
					count++;
				}
			}
			if (count > 0) {
				snd = snd_snow_variants[snow_variant_idx % count];
				snow_variant_idx = (snow_variant_idx + 1) % count;
			} else {
				snd = snd_snow;
			}
			vol = -12.5f; // 行走踩雪声微调降柔（由 -8.5dB 调整至 -12.5dB），更符合环境背景
		}
	} else if (kind == 2) {
		snd = snd_wet;
		vol = -11.0f; // 雨天真实踩水声（清亮水花，轻巧溅起）
	} else if (kind == 4) {
		// 踩木板声：在真实木板切片变体中轮换，生动丰富
		int count = 0;
		for (int i = 0; i < 4; ++i) {
			if (snd_wood_variants[i].is_valid()) count++;
		}
		if (count > 0) {
			snd = snd_wood_variants[wood_variant_idx % count];
			wood_variant_idx = (wood_variant_idx + 1) % count;
		} else {
			snd = snd_wood;
		}
		vol = -10.0f;  // 木地板踩踏声
	} else if (kind == 3) {
		// 踩石板声：在真实石板切片变体中轮换
		int count = 0;
		for (int i = 0; i < 4; ++i) {
			if (snd_stone_variants[i].is_valid()) count++;
		}
		if (count > 0) {
			snd = snd_stone_variants[stone_variant_idx % count];
			stone_variant_idx = (stone_variant_idx + 1) % count;
		} else {
			snd = snd_stone;
		}
	} else if (kind == 7) {
		// 冰面滑冰：冰刀划破冰层的清脆金属擦冰高频脆声
		snd = snd_stone_variants[3].is_valid() ? snd_stone_variants[3] : snd_stone;
		vol = -11.5f;
		pitch *= 1.35f; // 升音高，模拟冰刀破冰的高频刮擦与踏冰感
	} else if (kind == 5) {
		snd = snd_pitch;
		vol = -12.0f; // 足球场细密草坪（短促软弹吸音）
	} else if (kind == 6) {
		snd = snd_dirt;
		vol = -10.0f; // 泥土小径（干爽坚实，带细微泥砂质感）
	} else {
		snd = snd_grass;
		vol = -11.0f; // 荒野草甸基线（自然草土踏步）
	}
	if (snd.is_valid()) {
		steps_voice->set_stream(snd);
		steps_voice->set_pitch_scale(pitch);
		steps_voice->set_volume_db(vol);
		steps_voice->play();
	}
}

// 专属跳跃着地音效（双脚缓冲、体重动量吸收与地面特性）：
void Player::play_land(int kind) {
	if (steps_voice == nullptr) {
		return;
	}
	Ref<AudioStream> snd;
	float vol = -8.0f;
	float pitch = 0.96f + ((float)(rand() % 10) - 5.0f) * 0.01f;

	if (kind == 1) { // 真实雪地落地：深陷雪堆的双段松软压缩沉音（双脚陷雪缓冲）
		if (snd_land_snow.is_valid()) {
			snd = snd_land_snow;
		} else if (snd_snow_variants[1].is_valid()) {
			snd = snd_snow_variants[1];
		} else {
			snd = snd_snow;
		}
		vol = -8.0f;
	} else if (kind == 7) { // 冰面落地：坚实冰壳与冰刀硬着陆轻脆回响
		snd = snd_stone_variants[3].is_valid() ? snd_stone_variants[3] : snd_stone;
		vol = -8.0f;
		pitch = 1.25f;
	} else if (kind == 2) { // 雨天踩水落地：饱满水花与泥水双脚冲击沉闷音（告别单薄小水点）
		snd = snd_land_wet;
		vol = -7.5f;
	} else if (kind == 5) { // 足球场草坪落地：高弹性减震密集草皮缓冲着地音（深厚软弹，动量吸收）
		snd = snd_land_pitch;
		vol = -8.5f;
	} else if (kind == 4) { // 木地板落地：沉厚木板空腔回响
		snd = snd_wood;
		vol = -7.5f;
		pitch = 0.84f;
	} else if (kind == 3) { // 石桥石板落地：坚实双脚石面顿挫音
		snd = snd_stone.is_valid() ? snd_stone : Ref<AudioStream>(snd_dirt);
		vol = -8.0f;
		pitch = 0.86f;
	} else if (kind == 6) { // 泥土小径落地：厚实沙土冲量
		snd = snd_dirt.is_valid() ? snd_dirt : Ref<AudioStream>(snd_land_grass);
		vol = -8.0f;
		pitch = 0.88f;
	} else { // 0 普通荒草：草皮缓冲与泥土下压厚实音
		snd = snd_land_grass;
		vol = -8.0f;
	}

	if (snd.is_valid()) {
		steps_voice->set_stream(snd);
		steps_voice->set_pitch_scale(pitch);
		steps_voice->set_volume_db(vol);
		steps_voice->play();
	}
}

// 荒野普通草地跳跃着地音（草皮厚垫缓冲 + 泥土动量吸收，低沉厚实）：
Ref<AudioStreamWAV> Player::make_land_grass() {
	const int sr = 22050;
	const float dur = 0.12f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);
		lp += 0.15f * (raw - lp); // 柔和低通草皮摩擦

		// 纯净自然的双脚踏地低沉缓冲音（65Hz 单一厚实温润阻尼冲击）
		float thud = 0.38f * std::sin(6.2831853f * 65.0f * t) * std::exp(-t * 36.0f);
		float env = Math::clamp(t * 500.0f, 0.0f, 1.0f) * std::exp(-t * 32.0f);
		float s = (lp * 1.0f + thud) * env * 0.42f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 二段跳专属空气动力清脆爆鸣声（轻盈向上的滑音冲气感，带 280Hz -> 680Hz 上扬与清脆气浪微爆）：
Ref<AudioStreamWAV> Player::make_double_jump_snd() {
	const int sr = 22050;
	const float dur = 0.16f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	float phase = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);
		lp += 0.22f * (raw - lp);

		// 上升音调：280Hz -> 720Hz 充满动能与灵动的凌空借力感
		float freq = 280.0f + 440.0f * (t / dur);
		phase += 6.2831853f * freq / (float)sr;
		float chirp = 0.42f * std::sin(phase);

		// 气浪扩散带通噪声：轻盈气爆
		float whoosh = (raw - lp) * 0.35f;

		// 快速起音 (8ms) + 柔和指数衰减
		float attack = Math::clamp(t / 0.008f, 0.0f, 1.0f);
		float decay = std::exp(-t * 22.0f);
		float env = attack * decay;

		float s = (chirp * 0.75f + whoosh * 0.45f) * env * 0.55f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 雨天泥水泥浆跳跃着地音（双脚体重动量冲击 + 饱满溅开水花与泥水混合声）：
Ref<AudioStreamWAV> Player::make_land_wet() {
	const int sr = 22050;
	const float dur = 0.22f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp1 = 0.0f, lp2 = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);

		lp1 += 0.25f * (raw - lp1);
		lp2 += 0.08f * (raw - lp2);
		float splash = lp1 - lp2; // 400~1200Hz 饱满水花翻涌与喷溅

		// 沉重脚底入水泥潭冲击波
		float f_curr = 115.0f - 60.0f * Math::clamp(t / 0.07f, 0.0f, 1.0f);
		float plop = 0.52f * std::sin(6.2831853f * f_curr * t) * std::exp(-t * 22.0f);

		// 次级水花二次飞溅（28ms 后脚掌压平）
		float t2 = t - 0.028f;
		float plop2 = (t2 > 0.0f) ? (0.34f * std::sin(6.2831853f * 85.0f * t2) * std::exp(-t2 * 26.0f)) : 0.0f;

		float splash_env = Math::clamp(t * 500.0f, 0.0f, 1.0f) * std::exp(-t * 16.0f);
		float s = (plop + plop2 + splash * splash_env * 1.8f) * 0.55f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 足球场草坪跳跃着地音（专业密植草皮：高弹性吸收动量，深厚软弹气囊感）：
Ref<AudioStreamWAV> Player::make_land_pitch() {
	const int sr = 22050;
	const float dur = 0.16f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);
		lp += 0.09f * (raw - lp); // 极佳的低通吸音质感，草坪海绵垫感

		// 双峰软弹回弹低频波（110Hz -> 50Hz 阻尼吸收）
		float f_turf = 110.0f - 55.0f * Math::clamp(t / 0.06f, 0.0f, 1.0f);
		float turf_thump = 0.54f * std::sin(6.2831853f * f_turf * t) * std::exp(-t * 28.0f);

		// 草坪纤维柔和被压缩回弹
		float fiber_env = Math::clamp(t * 600.0f, 0.0f, 1.0f) * std::exp(-t * 30.0f);
		float s = (lp * 1.3f + turf_thump) * fiber_env * 0.50f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 荒野普通草地脚步（自然草土踏步：松软泥土着地 + 细微草丛缓冲，柔和厚实）：
Ref<AudioStreamWAV> Player::make_grass() {
	const int sr = 22050;
	const float dur = 0.11f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);
		lp += 0.15f * (raw - lp); // 柔和低通滤波草皮摩擦
		float env = Math::clamp(t * 400.0f, 0.0f, 1.0f) * std::exp(-t * 36.0f);
		float thump = 0.35f * std::sin(6.2831853f * 85.0f * t) * std::exp(-t * 32.0f);
		float s = (lp * 1.6f + thump) * env * 0.45f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 泥土小径脚步：压实泥土与细小碎石砂砾，坚实短促，带细腻砂石质感（无草沙沙声）
Ref<AudioStreamWAV> Player::make_dirt() {
	const int sr = 22050;
	const float dur = 0.09f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);
		lp += 0.28f * (raw - lp);
		float grit = (raw - lp) * 0.30f * std::exp(-t * 60.0f);
		float thump = 0.42f * std::sin(6.2831853f * 120.0f * t) * std::exp(-t * 45.0f);
		float env = Math::clamp(t * 800.0f, 0.0f, 1.0f) * std::exp(-t * 40.0f);
		float s = (lp * 1.3f + thump + grit) * env * 0.48f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 足球场草坪脚步：平整修剪的密植草皮，深厚吸音，软弹柔和
Ref<AudioStreamWAV> Player::make_pitch() {
	const int sr = 22050;
	const float dur = 0.10f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);
		lp += 0.10f * (raw - lp); // 极低截断滤波，突出深层草皮吸音垫感
		float turf_thud = 0.40f * std::sin(6.2831853f * 75.0f * t) * std::exp(-t * 36.0f);
		float env = Math::clamp(t * 500.0f, 0.0f, 1.0f) * std::exp(-t * 32.0f);
		float s = (lp * 1.6f + turf_thud) * env * 0.40f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 真实雪地脚步：双段压雪碎裂"咯吱咯吱"，松软缓冲起音（绝无枪声刺耳爆音）
Ref<AudioStreamWAV> Player::make_snow() {
	const int sr = 22050;
	const float dur = 0.22f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp1 = 0.0f, lp2 = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);

		// 带通平滑提取温润冰晶挤压频段（700~1900Hz）
		lp1 += 0.22f * (raw - lp1);
		lp2 += 0.06f * (raw - lp2);
		float crystal = lp1 - lp2;

		// 咯吱咯吱：双峰压雪包络（第 1 峰脚跟着地破雪壳，第 2 峰脚掌辗转压实）
		float p1 = std::exp(-std::pow((t - 0.040f) / 0.026f, 2.0f));
		float p2 = std::exp(-std::pow((t - 0.120f) / 0.038f, 2.0f)) * 0.80f;
		float crunch_env = (p1 + p2);

		// 柔和 22ms 缓冲起音，彻底杜绝任何瞬态脉冲（防开枪感）
		float attack = Math::clamp(t / 0.022f, 0.0f, 1.0f);
		// 厚重雪堆压缩低频（90Hz 温润低吟）
		float low_body = 0.32f * std::sin(6.2831853f * 92.0f * t) * std::exp(-t * 24.0f);

		float s = (crystal * 1.25f + low_body) * crunch_env * attack * 0.42f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 雨天轻微踩水声：鞋底踩入浅水坑的清亮"啪嗒"与水花微溅散开（轻盈润泽）
Ref<AudioStreamWAV> Player::make_puddle() {
	const int sr = 22050;
	const float dur = 0.14f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);

		lp += 0.30f * (raw - lp);
		float splash_noise = raw - lp; // 提取水花飞溅微粒

		// 水面挤开流体音：滑频 260Hz -> 120Hz 柔和下坠
		float f_curr = 260.0f - 140.0f * Math::clamp(t / 0.06f, 0.0f, 1.0f);
		float plop = 0.30f * std::sin(6.2831853f * f_curr * t) * std::exp(-t * 50.0f);

		// 水花飞溅散开包络
		float splash_env = Math::clamp(t * 700.0f, 0.0f, 1.0f) * std::exp(-t * 32.0f);
		float droplets = splash_noise * splash_env * 0.9f;

		float s = (plop + droplets) * 0.40f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 石桥面石板脚步：硬质地面 = 短促的"嗒"——亮高通噪声（起音 0.5ms，比其他
// 音色快一个量级）+ 微弱 480Hz 石面残响 ring（快衰减），0.09s。
Ref<AudioStreamWAV> Player::make_stone() {
	const int sr = 22050;
	const float dur = 0.09f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);

		lp += 0.42f * (raw - lp);          // 宽低通后取残差 → 亮高通，石板脆响
		float hp = raw - lp;
		float env = Math::clamp(t * 2000.0f, 0.0f, 1.0f) * std::exp(-t * 55.0f);
		float ring = 0.16f * std::sin(6.2831853f * 480.0f * t) * std::exp(-t * 40.0f);
		float s = (hp * 1.5f + ring) * env * 0.5f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF); // 小端 16-bit
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 木屋地板脚步：中低频"咚"——170Hz 主体快衰减 + 320Hz 板面共鸣 + 少量
// 低通噪声做"踏"的质感，0.11s（木地板中空，比石板闷、比草地实）。
Ref<AudioStreamWAV> Player::make_wood() {
	const int sr = 22050;
	const float dur = 0.11f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);

		lp += 0.22f * (raw - lp);
		float env = Math::clamp(t * 600.0f, 0.0f, 1.0f) * std::exp(-t * 30.0f);
		float thump = 0.55f * std::sin(6.2831853f * 170.0f * t) * std::exp(-t * 38.0f);
		float board = 0.18f * std::sin(6.2831853f * 320.0f * t) * std::exp(-t * 55.0f);
		float s = (lp * 1.2f + thump + board) * env * 0.55f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// 穿草声（程序化合成，22050Hz 16-bit 单声道）：
// 带通噪声的"沙——"：起-落包络模拟植被被拨开再合拢，0.24s；
// deep=true 为高草丛版：低通更窄（更闷更密）、包络更长、带两次拨动。
Ref<AudioStreamWAV> Player::make_rustle(bool deep) {
	const int sr = 22050;
	const float dur = deep ? 0.30f : 0.24f;
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float raw = noise_dist(rng);

		lp += (deep ? 0.18f : 0.32f) * (raw - lp);
		float tn = Math::clamp(t / dur, 0.0f, 1.0f);
		float env = std::sin(3.1415926f * tn);            // 起-落包络（拨开→合拢）
		if (deep) {
			env *= 1.0f + 0.4f * std::sin(6.2831853f * 3.0f * tn); // 高草：多次拨动
		}
		float s = lp * 2.4f * env * 0.5f;

		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

// ─────────────────────────────────────────────────────────────────────────────
// 外衣与换装系统：支持默认便服、双汇王中王火腿肠、甜玉米香肠、劲脆黑椒牛肉肠
// ─────────────────────────────────────────────────────────────────────────────
void Player::set_costume(int p_c) {
	costume = p_c % 4;
	rebuild_costume();
	if (hud_prompt != nullptr) {
		const char *names[] = { "经典探险便服", "双汇王中王火腿肠", "双汇甜玉米火腿肠", "劲脆黑椒牛肉肠" };
		hud_prompt->set_text(String::utf8("✨ 已换上：") + String::utf8(names[costume]));
		hud_prompt->show();
	}
}

void Player::rebuild_costume() {
	if (model == nullptr) {
		return;
	}
	if (costume_root != nullptr) {
		costume_root->queue_free();
		costume_root = nullptr;
	}

	costume_root = memnew(Node3D);
	costume_root->set_name("CostumeRoot");
	model->add_child(costume_root);

	auto face_mat = [](const Color &c, float rough = 0.5f, float metal = 0.0f) {
		Ref<StandardMaterial3D> m;
		m.instantiate();
		m->set_albedo(c);
		m->set_roughness(rough);
		m->set_metallic(metal);
		return m;
	};

	Ref<StandardMaterial3D> white = face_mat(Color(0.96f, 0.96f, 0.94f));
	Ref<StandardMaterial3D> dark = face_mat(Color(0.12f, 0.10f, 0.10f));
	Ref<StandardMaterial3D> nose_m = face_mat(Color(0.85f, 0.30f, 0.22f));

	auto ball = [](float r) {
		Ref<SphereMesh> s;
		s.instantiate();
		s->set_radius(r);
		s->set_height(r * 2.0f);
		s->set_radial_segments(12);
		s->set_rings(8);
		return s;
	};

	auto add_part = [&](Ref<Mesh> p_mesh, Ref<Material> p_mat, const Vector3 &p_pos, const Vector3 &p_rot = Vector3()) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(p_mesh);
		mi->set_material_override(p_mat);
		mi->set_position(p_pos);
		if (p_rot.length_squared() > 0.0001f) {
			mi->set_rotation(p_rot);
		}
		costume_root->add_child(mi);
		return mi;
	};

	if (costume == 0) {
		// 0: 原版便服（眼睛 + 鼻子）
		add_part(ball(0.055f), white, Vector3(-0.12f, 1.45f, 0.34f));
		add_part(ball(0.055f), white, Vector3(0.12f, 1.45f, 0.34f));
		add_part(ball(0.026f), dark, Vector3(-0.12f, 1.45f, 0.385f));
		add_part(ball(0.026f), dark, Vector3(0.12f, 1.45f, 0.385f));
		add_part(ball(0.035f), nose_m, Vector3(0.0f, 1.33f, 0.37f));
	} else if (costume == 1) {
		// 1: 双汇王中王火腿肠外衣！
		const Color sh_red(0.86f, 0.14f, 0.16f);
		const Color gold_ring(0.92f, 0.82f, 0.36f);
		const Color brand_blue(0.12f, 0.32f, 0.72f);
		const Color brand_white(0.98f, 0.98f, 0.98f);

		// 红色火腿肠全包裹外皮
		Ref<CapsuleMesh> sausage_casing;
		sausage_casing.instantiate();
		sausage_casing->set_radius(0.38f);
		sausage_casing->set_height(1.82f);
		sausage_casing->set_radial_segments(24);
		sausage_casing->set_rings(12);
		add_part(sausage_casing, face_mat(sh_red, 0.28f, 0.05f), Vector3(0.0f, 0.91f, 0.0f));

		// 顶部扎口金属环与扭结收口
		Ref<TorusMesh> clip_ring;
		clip_ring.instantiate();
		clip_ring->set_inner_radius(0.08f);
		clip_ring->set_outer_radius(0.16f);
		clip_ring->set_rings(12);
		clip_ring->set_ring_segments(12);
		add_part(clip_ring, face_mat(gold_ring, 0.25f, 0.85f), Vector3(0.0f, 1.83f, 0.0f), Vector3(1.570796f, 0.0f, 0.0f));
		add_part(ball(0.06f), face_mat(sh_red, 0.4f), Vector3(0.0f, 1.88f, 0.0f));

		// 底部扎口金属环与扭结收口
		add_part(clip_ring, face_mat(gold_ring, 0.25f, 0.85f), Vector3(0.0f, 0.02f, 0.0f), Vector3(1.570796f, 0.0f, 0.0f));
		add_part(ball(0.06f), face_mat(sh_red, 0.4f), Vector3(0.0f, -0.03f, 0.0f));

		// 胸口双汇王中王腰封（白色椭圆底标 + 蓝色主标 + 金星）
		Ref<BoxMesh> badge_base;
		badge_base.instantiate();
		badge_base->set_size(Vector3(0.44f, 0.38f, 0.06f));
		add_part(badge_base, face_mat(brand_white, 0.3f), Vector3(0.0f, 1.05f, 0.365f));

		Ref<BoxMesh> badge_blue;
		badge_blue.instantiate();
		badge_blue->set_size(Vector3(0.38f, 0.26f, 0.06f));
		add_part(badge_blue, face_mat(brand_blue, 0.35f), Vector3(0.0f, 1.05f, 0.375f));

		Ref<BoxMesh> star;
		star.instantiate();
		star->set_size(Vector3(0.12f, 0.12f, 0.06f));
		add_part(star, face_mat(gold_ring, 0.2f, 0.5f), Vector3(0.0f, 1.05f, 0.385f), Vector3(0.0f, 0.0f, 0.785f));

		// 火腿肠呆萌眼睛与小嘴巴
		add_part(ball(0.055f), white, Vector3(-0.13f, 1.48f, 0.37f));
		add_part(ball(0.055f), white, Vector3(0.13f, 1.48f, 0.37f));
		add_part(ball(0.026f), dark, Vector3(-0.13f, 1.48f, 0.415f));
		add_part(ball(0.026f), dark, Vector3(0.13f, 1.48f, 0.415f));
		add_part(ball(0.035f), nose_m, Vector3(0.0f, 1.37f, 0.39f));
	} else if (costume == 2) {
		// 2: 双汇甜玉米香肠外衣！
		const Color corn_yellow(0.96f, 0.82f, 0.22f);
		const Color green_clip(0.32f, 0.72f, 0.28f);
		const Color kernel_gold(0.99f, 0.92f, 0.45f);

		Ref<CapsuleMesh> sausage_casing;
		sausage_casing.instantiate();
		sausage_casing->set_radius(0.38f);
		sausage_casing->set_height(1.82f);
		sausage_casing->set_radial_segments(24);
		sausage_casing->set_rings(12);
		add_part(sausage_casing, face_mat(corn_yellow, 0.28f, 0.05f), Vector3(0.0f, 0.91f, 0.0f));

		Ref<TorusMesh> clip_ring;
		clip_ring.instantiate();
		clip_ring->set_inner_radius(0.08f);
		clip_ring->set_outer_radius(0.16f);
		add_part(clip_ring, face_mat(green_clip, 0.3f, 0.4f), Vector3(0.0f, 1.83f, 0.0f), Vector3(1.570796f, 0.0f, 0.0f));
		add_part(ball(0.06f), face_mat(corn_yellow, 0.4f), Vector3(0.0f, 1.88f, 0.0f));
		add_part(clip_ring, face_mat(green_clip, 0.3f, 0.4f), Vector3(0.0f, 0.02f, 0.0f), Vector3(1.570796f, 0.0f, 0.0f));
		add_part(ball(0.06f), face_mat(corn_yellow, 0.4f), Vector3(0.0f, -0.03f, 0.0f));

		const Vector3 kernels[] = {
			Vector3(0.0f, 1.15f, 0.375f),
			Vector3(-0.18f, 0.92f, 0.34f),
			Vector3(0.16f, 0.85f, 0.35f),
			Vector3(-0.12f, 0.65f, 0.36f),
			Vector3(0.14f, 0.52f, 0.35f),
			Vector3(-0.35f, 0.85f, 0.05f),
			Vector3(0.35f, 0.95f, 0.05f)
		};
		for (const Vector3 &kp : kernels) {
			add_part(ball(0.045f), face_mat(kernel_gold, 0.25f), kp);
		}

		add_part(ball(0.055f), white, Vector3(-0.13f, 1.48f, 0.37f));
		add_part(ball(0.055f), white, Vector3(0.13f, 1.48f, 0.37f));
		add_part(ball(0.026f), dark, Vector3(-0.13f, 1.48f, 0.415f));
		add_part(ball(0.026f), dark, Vector3(0.13f, 1.48f, 0.415f));
		add_part(ball(0.035f), nose_m, Vector3(0.0f, 1.37f, 0.39f));
	} else if (costume == 3) {
		// 3: 劲脆黑椒牛肉肠外衣！
		const Color beef_maroon(0.52f, 0.18f, 0.14f);
		const Color silver_clip(0.85f, 0.86f, 0.88f);
		const Color pepper_dot(0.12f, 0.10f, 0.10f);

		Ref<CapsuleMesh> sausage_casing;
		sausage_casing.instantiate();
		sausage_casing->set_radius(0.38f);
		sausage_casing->set_height(1.82f);
		sausage_casing->set_radial_segments(24);
		sausage_casing->set_rings(12);
		add_part(sausage_casing, face_mat(beef_maroon, 0.22f, 0.05f), Vector3(0.0f, 0.91f, 0.0f));

		Ref<TorusMesh> clip_ring;
		clip_ring.instantiate();
		clip_ring->set_inner_radius(0.08f);
		clip_ring->set_outer_radius(0.16f);
		add_part(clip_ring, face_mat(silver_clip, 0.2f, 0.9f), Vector3(0.0f, 1.83f, 0.0f), Vector3(1.570796f, 0.0f, 0.0f));
		add_part(ball(0.06f), face_mat(beef_maroon, 0.4f), Vector3(0.0f, 1.88f, 0.0f));
		add_part(clip_ring, face_mat(silver_clip, 0.2f, 0.9f), Vector3(0.0f, 0.02f, 0.0f), Vector3(1.570796f, 0.0f, 0.0f));
		add_part(ball(0.06f), face_mat(beef_maroon, 0.4f), Vector3(0.0f, -0.03f, 0.0f));

		for (int pi = 0; pi < 10; pi++) {
			float y = 0.5f + (float)pi * 0.11f;
			float a = (float)pi * 1.618f;
			Vector3 pp(std::cos(a) * 0.37f, y, std::sin(a) * 0.37f);
			add_part(ball(0.025f), face_mat(pepper_dot, 0.6f), pp);
		}

		add_part(ball(0.055f), white, Vector3(-0.13f, 1.48f, 0.37f));
		add_part(ball(0.055f), white, Vector3(0.13f, 1.48f, 0.37f));
		add_part(ball(0.026f), dark, Vector3(-0.13f, 1.48f, 0.415f));
		add_part(ball(0.026f), dark, Vector3(0.13f, 1.48f, 0.415f));
		add_part(ball(0.035f), nose_m, Vector3(0.0f, 1.37f, 0.39f));
	}

	// 初始化身上的泥/雪附着节点容器
	if (stains_root != nullptr) {
		stains_root->queue_free();
		stains_root = nullptr;
	}
	stains_root = memnew(Node3D);
	stains_root->set_name("BodyStains");
	model->add_child(stains_root);

	// 预先创建泥点斑块与雪花冰斑块（薄片紧贴皮肤，泥巴为自然土灰色，带透明度渐变）
	Ref<StandardMaterial3D> snow_mat;
	snow_mat.instantiate();
	snow_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	snow_mat->set_albedo(Color(0.96f, 0.98f, 1.0f, 0.95f));
	snow_mat->set_roughness(0.35f);
	snow_dynamic_mat = snow_mat;

	Ref<StandardMaterial3D> mud_mat;
	mud_mat.instantiate();
	mud_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	// 需求明确：泥巴不能是很多突出的肉球，必须是贴在身上的薄片，土灰色 (Color(0.44, 0.38, 0.30))
	mud_mat->set_albedo(Color(0.44f, 0.38f, 0.30f, 0.92f));
	mud_mat->set_roughness(0.88f);
	mud_dynamic_mat = mud_mat;

	// 身体胶囊体各处表面附着点（紧贴胶囊体皮肤表面外 0.01m，绝不突兀悬浮）
	const Vector3 stain_offsets[] = {
		Vector3(0.20f, 0.45f, 0.23f),
		Vector3(-0.21f, 0.75f, 0.22f),
		Vector3(0.16f, 1.15f, 0.24f),
		Vector3(-0.14f, 1.35f, 0.23f),
		Vector3(0.0f, 0.85f, -0.28f),
		Vector3(-0.22f, 0.55f, -0.21f),
		Vector3(0.24f, 0.95f, -0.19f),
		Vector3(0.28f, 0.65f, 0.06f),
		Vector3(-0.29f, 1.05f, 0.02f),
		Vector3(0.0f, 1.58f, 0.12f),
		Vector3(0.05f, 0.28f, 0.20f),
		Vector3(-0.06f, 0.32f, -0.22f)
	};

	for (int i = 0; i < 12; i++) {
		// 雪斑（微扁小白薄片）
		MeshInstance3D *sn = memnew(MeshInstance3D);
		sn->set_name(vformat("Snow_%d", i));
		Ref<SphereMesh> sm;
		sm.instantiate();
		sm->set_radius(0.045f + ((float)(i % 4)) * 0.012f);
		sm->set_height(0.012f); // 薄扁片，贴身
		sn->set_mesh(sm);
		sn->set_material_override(snow_mat);
		sn->set_position(stain_offsets[i]);
		sn->set_visible(false);
		stains_root->add_child(sn);

		// 泥巴斑（贴身微薄片，土灰色薄泥渍）
		MeshInstance3D *md = memnew(MeshInstance3D);
		md->set_name(vformat("Mud_%d", i));
		Ref<SphereMesh> mm;
		mm.instantiate();
		mm->set_radius(0.052f + ((float)((i + 2) % 4)) * 0.012f);
		mm->set_height(0.014f); // 微薄扁平泥渍贴身
		md->set_mesh(mm);
		md->set_material_override(mud_mat);
		md->set_position(stain_offsets[i] + Vector3(0.01f, -0.01f, 0.01f));
		md->set_visible(false);
		stains_root->add_child(md);
	}
}

void Player::update_body_stains(double delta) {
	if (stains_root == nullptr) {
		return;
	}
	int snow_count = (int)(snow_stain * 12.0f);
	int mud_count = (int)(mud_stain * 12.0f);

	// 渐变脱落：随着 mud_stain / snow_stain 降低，透明度与缩放平滑衰减渐变淡出
	if (mud_dynamic_mat.is_valid()) {
		float alpha_k = Math::clamp(mud_stain * 1.25f, 0.0f, 0.92f);
		mud_dynamic_mat->set_albedo(Color(0.44f, 0.38f, 0.30f, alpha_k));
	}
	if (snow_dynamic_mat.is_valid()) {
		float alpha_k = Math::clamp(snow_stain * 1.25f, 0.0f, 0.95f);
		snow_dynamic_mat->set_albedo(Color(0.96f, 0.98f, 1.0f, alpha_k));
	}

	for (int i = 0; i < 12; i++) {
		MeshInstance3D *sn = Object::cast_to<MeshInstance3D>(stains_root->find_child(vformat("Snow_%d", i), false, false));
		if (sn != nullptr) {
			if (i < snow_count) {
				sn->set_visible(true);
				float sc = Math::clamp((snow_stain - (float)i / 12.0f) * 12.0f, 0.15f, 1.05f);
				sn->set_scale(Vector3(sc, sc, sc));
			} else {
				sn->set_visible(false);
			}
		}

		MeshInstance3D *md = Object::cast_to<MeshInstance3D>(stains_root->find_child(vformat("Mud_%d", i), false, false));
		if (md != nullptr) {
			if (i < mud_count) {
				md->set_visible(true);
				float sc = Math::clamp((mud_stain - (float)i / 12.0f) * 12.0f, 0.15f, 1.08f);
				md->set_scale(Vector3(sc, sc, sc));
			} else {
				md->set_visible(false);
			}
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 滑冰鞋系统：踏上冰面时自动穿上专业滑冰鞋（银亮合金冰刀+运动鞋靴）
// ─────────────────────────────────────────────────────────────────────────────
void Player::build_skate_shoes() {
	if (model == nullptr) {
		return;
	}
	if (skate_shoes_root != nullptr) {
		skate_shoes_root->queue_free();
		skate_shoes_root = nullptr;
	}

	skate_shoes_root = memnew(Node3D);
	skate_shoes_root->set_name("SkateShoes");
	model->add_child(skate_shoes_root);

	// 材质：银亮反光合金刀刃、深色鞋帮、金色鞋带/固定扣
	auto make_mat = [](const Color &c, float rough, float metal) {
		Ref<StandardMaterial3D> m;
		m.instantiate();
		m->set_albedo(c);
		m->set_roughness(rough);
		m->set_metallic(metal);
		return m;
	};

	Ref<StandardMaterial3D> boot_mat = make_mat(Color(0.14f, 0.15f, 0.18f), 0.40f, 0.05f); // 酷黑专业冰鞋靴体
	Ref<StandardMaterial3D> sole_mat = make_mat(Color(0.24f, 0.25f, 0.28f), 0.30f, 0.10f); // 坚固鞋底托架
	Ref<StandardMaterial3D> blade_mat = make_mat(Color(0.92f, 0.94f, 0.98f), 0.12f, 0.92f); // 银光锃亮合金冰刀
	Ref<StandardMaterial3D> gold_mat = make_mat(Color(0.95f, 0.78f, 0.25f), 0.25f, 0.85f); // 亮金鞋扣与刀托支柱

	auto add_mesh = [&](Ref<Mesh> mesh, Ref<Material> mat, const Vector3 &pos, const Vector3 &rot = Vector3()) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(mesh);
		mi->set_material_override(mat);
		mi->set_position(pos);
		if (rot.length_squared() > 0.0001f) {
			mi->set_rotation(rot);
		}
		skate_shoes_root->add_child(mi);
		return mi;
	};

	// 左右双脚对称构造（左脚 x = -0.16m，右脚 x = +0.16m，脚底贴地高度 y ≈ 0.03m）
	const float feet_x[2] = { -0.16f, 0.16f };
	for (int f = 0; f < 2; f++) {
		float fx = feet_x[f];

		// 1. 鞋帮与鞋靴本体（饱满圆润的运动冰鞋轮廓，略微向前延展）
		Ref<BoxMesh> boot_body;
		boot_body.instantiate();
		boot_body->set_size(Vector3(0.15f, 0.14f, 0.32f));
		add_mesh(boot_body, boot_mat, Vector3(fx, 0.08f, 0.02f));

		// 前脚尖圆头（贴合脚掌前端）
		Ref<CylinderMesh> toe_cap;
		toe_cap.instantiate();
		toe_cap->set_top_radius(0.075f);
		toe_cap->set_bottom_radius(0.075f);
		toe_cap->set_height(0.13f);
		add_mesh(toe_cap, boot_mat, Vector3(fx, 0.075f, 0.16f));

		// 2. 鞋底大托架（深黑加强型硬质耐寒塑料中底）
		Ref<BoxMesh> sole;
		sole.instantiate();
		sole->set_size(Vector3(0.16f, 0.035f, 0.35f));
		add_mesh(sole, sole_mat, Vector3(fx, 0.032f, 0.02f));

		// 3. 冰刀刀托金色支柱（前后两根牢固的垂直合金刀架连接立柱）
		Ref<BoxMesh> post_front;
		post_front.instantiate();
		post_front->set_size(Vector3(0.025f, 0.045f, 0.045f));
		add_mesh(post_front, gold_mat, Vector3(fx, 0.008f, 0.12f));

		Ref<BoxMesh> post_rear;
		post_rear.instantiate();
		post_rear->set_size(Vector3(0.025f, 0.045f, 0.045f));
		add_mesh(post_rear, gold_mat, Vector3(fx, 0.008f, -0.10f));

		// 4. 银亮合金冰刀刀身（长 0.44m，超薄 0.012m，前后微微上翘起弧）
		Ref<BoxMesh> blade;
		blade.instantiate();
		blade->set_size(Vector3(0.012f, 0.035f, 0.42f));
		add_mesh(blade, blade_mat, Vector3(fx, -0.016f, 0.02f));

		// 冰刀前端上翘圆弧齿峰（专业花样/冰球刀尖微翘）
		Ref<CylinderMesh> blade_tip;
		blade_tip.instantiate();
		blade_tip->set_top_radius(0.025f);
		blade_tip->set_bottom_radius(0.025f);
		blade_tip->set_height(0.012f);
		add_mesh(blade_tip, blade_mat, Vector3(fx, -0.005f, 0.22f), Vector3(0.0f, 0.0f, 1.570796f));

		// 冰刀后端上翘小圆尾
		Ref<CylinderMesh> blade_tail;
		blade_tail.instantiate();
		blade_tail->set_top_radius(0.022f);
		blade_tail->set_bottom_radius(0.022f);
		blade_tail->set_height(0.012f);
		add_mesh(blade_tail, blade_mat, Vector3(fx, -0.006f, -0.185f), Vector3(0.0f, 0.0f, 1.570796f));
	}

	skate_shoes_root->set_visible(false); // 默认未滑冰时隐藏
}

void Player::set_skate_shoes_visible(bool p_vis) {
	if (skate_shoes_root != nullptr && skate_shoes_root->is_visible() != p_vis) {
		skate_shoes_root->set_visible(p_vis);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// 经过荧光蘑菇：身边出现3-5个与蘑菇孢子同样大小的柔和荧光微粒，在身体周围轻盈漂浮
// ─────────────────────────────────────────────────────────────────────────────
void Player::build_shroom_glow_trail() {
	if (shroom_glow_trail != nullptr) {
		return;
	}
	shroom_glow_trail = memnew(CPUParticles3D);
	shroom_glow_trail->set_name("ShroomGlowTrail");
	shroom_glow_trail->set_use_local_coordinates(false);

	Ref<SphereMesh> mote;
	mote.instantiate();
	// 与场景发光蘑菇孢子光尘完全相同大小 (半径 0.012m，直径 0.024m)
	mote->set_radius(0.012f);
	mote->set_height(0.024f);
	mote->set_radial_segments(6);
	mote->set_rings(3);

	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_albedo(Color(0.52f, 0.95f, 0.65f, 1.0f)); // 与荧光蘑菇同源的清透仙灵莹绿光
	mote->set_material(m);

	Ref<Gradient> ramp;
	ramp.instantiate();
	ramp->add_point(0.0f, Color(0.52f, 0.95f, 0.65f, 0.0f));
	ramp->add_point(0.20f, Color(0.52f, 0.95f, 0.65f, 0.95f));
	ramp->add_point(0.70f, Color(0.52f, 0.95f, 0.65f, 0.85f));
	ramp->add_point(1.0f, Color(0.52f, 0.95f, 0.65f, 0.0f));

	// 身边出现 3~5 个荧光（设置固定 4 个，轻度随机）
	shroom_glow_trail->set_amount(4);
	shroom_glow_trail->set_lifetime(3.8f);
	shroom_glow_trail->set_lifetime_randomness(0.25f);
	shroom_glow_trail->set_randomness_ratio(0.25f);
	shroom_glow_trail->set_mesh(mote);
	shroom_glow_trail->set_color_ramp(ramp);
	shroom_glow_trail->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	shroom_glow_trail->set_spread(180.0f);
	shroom_glow_trail->set_gravity(Vector3(0.0f, 0.018f, 0.0f)); // 近乎失重般在身边浮游
	shroom_glow_trail->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.04f);
	shroom_glow_trail->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	// 尺寸严格保持一致（同样大小，杜绝大小突变）
	shroom_glow_trail->set_param_min(CPUParticles3D::PARAM_SCALE, 0.95f);
	shroom_glow_trail->set_param_max(CPUParticles3D::PARAM_SCALE, 1.05f);
	shroom_glow_trail->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	shroom_glow_trail->set_emission_sphere_radius(0.65f); // 笼罩躯干周围
	shroom_glow_trail->set_position(Vector3(0.0f, 0.95f, 0.0f)); // 发射中心位于胸腹高度
	shroom_glow_trail->set_emitting(false);
	add_child(shroom_glow_trail);
}

void Player::update_shroom_glow_trail(double delta) {
	if (terrain_cache == nullptr && get_parent() != nullptr) {
		terrain_cache = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("Terrain")));
		if (terrain_cache == nullptr) {
			terrain_cache = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("TerrainBuilder")));
		}
	}
	Vector3 gp = get_global_position();
	// 经过发光蘑菇周围 1.6m：持续带走身边 3~5 个荧光
	if (terrain_cache != nullptr && terrain_cache->is_near_glowing_mushroom(gp, 1.6f)) {
		shroom_glow_timer = 5.0f;
	}

	if (shroom_glow_timer > 0.0f) {
		shroom_glow_timer -= (float)delta;
		if (shroom_glow_trail == nullptr) {
			build_shroom_glow_trail();
		}
		if (shroom_glow_trail != nullptr && !shroom_glow_trail->is_emitting()) {
			shroom_glow_trail->set_amount(4);
			shroom_glow_trail->set_emitting(true);
		}
	} else if (shroom_glow_trail != nullptr && shroom_glow_trail->is_emitting()) {
		shroom_glow_trail->set_emitting(false);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 上床睡觉系统与过场动画：平躺床上、黑屏渐变过场、天色瞬移/快进至清晨
// ─────────────────────────────────────────────────────────────────────────────
void Player::build_sleep_ui() {
	if (sleep_layer != nullptr) {
		return;
	}
	sleep_layer = memnew(CanvasLayer);
	sleep_layer->set_layer(12); // 高于普通 HUD 与地图
	add_child(sleep_layer);

	sleep_overlay = memnew(ColorRect);
	sleep_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
	sleep_overlay->set_color(Color(0.0f, 0.0f, 0.0f, 0.0f));
	sleep_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	sleep_layer->add_child(sleep_overlay);

	Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");

	sleep_title = memnew(Label);
	sleep_title->set_anchors_preset(Control::PRESET_CENTER);
	sleep_title->set_offset(SIDE_LEFT, -300.0f);
	sleep_title->set_offset(SIDE_RIGHT, 300.0f);
	sleep_title->set_offset(SIDE_TOP, -40.0f);
	sleep_title->set_offset(SIDE_BOTTOM, 20.0f);
	sleep_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	sleep_title->add_theme_font_size_override("font_size", 38);
	sleep_title->add_theme_color_override("font_color", Color(1.0f, 0.94f, 0.82f, 0.0f));
	if (cjk.is_valid()) sleep_title->add_theme_font_override("font", cjk);
	sleep_layer->add_child(sleep_title);

	sleep_subtitle = memnew(Label);
	sleep_subtitle->set_anchors_preset(Control::PRESET_CENTER);
	sleep_subtitle->set_offset(SIDE_LEFT, -300.0f);
	sleep_subtitle->set_offset(SIDE_RIGHT, 300.0f);
	sleep_subtitle->set_offset(SIDE_TOP, 30.0f);
	sleep_subtitle->set_offset(SIDE_BOTTOM, 70.0f);
	sleep_subtitle->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	sleep_subtitle->add_theme_font_size_override("font_size", 22);
	sleep_subtitle->add_theme_color_override("font_color", Color(0.85f, 0.80f, 0.70f, 0.0f));
	if (cjk.is_valid()) sleep_subtitle->add_theme_font_override("font", cjk);
	sleep_layer->add_child(sleep_subtitle);

	sleep_layer->hide();
}

void Player::start_sleep(const Vector3 &p_bed_pos, float p_yaw) {
	is_sleeping = true;
	bed_pos = p_bed_pos;
	bed_yaw = p_yaw;
	sleep_fade_t = 0.0f;
	sleep_advancing = false;
	sleep_key_released = false; // 重置按键松开防抖状态

	// 角色平躺在床上。床垫（BedBlanket）上表面在木屋局部 y=0.74，木屋原点比
	// 床节点高 0.35m → 床面在世界 y ≈ 1.09；角色胶囊半径 0.35，
	// 所以躺平时几何中心要落在床面上方 0.35m 处。
	set_global_position(p_bed_pos + Vector3(0.0f, 0.38f, 0.0f));
	set_velocity(Vector3(0.0f, 0.0f, 0.0f));

	// 躺姿：身体沿**床的长轴**（床局部 ±X 那条 2.2m 的边），头朝床头（局部 -X 侧）。
	// 旧版直接拿 bed_yaw 当模型 yaw，结果身体横铺在只有 1.3m 宽的床面上、
	// 头脚颠倒挂在床外 —— 这是"睡觉没弄好"的主因。
	// 床局部 +X 在世界里是 (cos yaw, 0, -sin yaw)；要让身体轴（模型局部 +Y）
	// 指向床头方向 (-cos yaw, 0, +sin yaw)，模型 yaw 得再 +90°。
	sleep_lie_basis = Basis(Vector3(0.0, 1.0, 0.0), p_yaw + 1.5707963f)
			* Basis(Vector3(1.0, 0.0, 0.0), -1.5707963f);
	// 胶囊几何中心在 Model 局部 (0, 0.9, 0)。减去它的旋转偏移后，几何中心
	// 正好落在 Player 原点上方 LIE_CENTER_Y：1.09(床面) + 0.35(胶囊半径) - 1.18(Player 原点)
	const float LIE_CENTER_Y = 0.26f;
	sleep_lie_pos = Vector3(0.0f, LIE_CENTER_Y, 0.0f)
			- sleep_lie_basis.xform(Vector3(0.0f, 0.9f, 0.0f));
	if (model != nullptr) {
		model->set_basis(sleep_lie_basis);
		model->set_position(sleep_lie_pos);
	}

	SkyCycle *sc = get_parent() != nullptr ? Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle"))) : nullptr;
	float current_tod = (sc != nullptr) ? sc->get_time_of_day() : 0.5f;

	// 一觉睡到清晨（用户要求：睡觉直接到白天，不再"白天睡到夜里"）。
	// 若当前已经过了清晨（tod > 0.28），这一觉要跨过午夜 → 天数 +1，
	// 否则月相与"第几天"会停住不动。
	sleep_target_tod = 0.28f;
	sleep_day_advance = (current_tod > sleep_target_tod) ? 1 : 0;

	if (sleep_layer != nullptr) {
		sleep_layer->show();
		if (sleep_title != nullptr) {
			sleep_title->set_text(String::utf8("—— 一夜好眠 ——"));
		}
		if (sleep_subtitle != nullptr) {
			sleep_subtitle->set_text(String::utf8("星河长明，清晨破晓而来..."));
		}
	}
	play_ui(0);
}

void Player::update_sleep_cutscene(double delta) {
	if (!is_sleeping) {
		return;
	}
	sleep_fade_t += (float)delta;

	// 过场时间轴：
	//   0.0 ~ 0.9s : 平躺展示 —— 不黑屏，镜头切到床边机位看着自己躺下（带呼吸起伏）
	//   0.9 ~ 1.8s : 黑屏渐入，字幕浮现
	//   1.8 ~ 2.8s : 纯黑，天色推到清晨（跨午夜则天数 +1）
	//   2.8 ~ 3.8s : 黑屏淡出，清晨天光
	//   > 3.8s     : 起床
	float alpha = 0.0f;
	float text_alpha = 0.0f;

	if (sleep_fade_t < 0.9f) {
		alpha = 0.0f; // 先让玩家看清自己躺在床上
		text_alpha = 0.0f;
	} else if (sleep_fade_t < 1.8f) {
		float in_t = (sleep_fade_t - 0.9f) / 0.9f;
		alpha = Math::clamp(in_t, 0.0f, 1.0f);
		text_alpha = alpha;
	} else if (sleep_fade_t < 2.8f) {
		alpha = 1.0f;
		text_alpha = 1.0f;
		if (!sleep_advancing) {
			sleep_advancing = true;
			SkyCycle *sc = get_parent() != nullptr ? Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle"))) : nullptr;
			if (sc != nullptr) {
				sc->set_time_of_day(sleep_target_tod);
				if (sleep_day_advance > 0) {
					sc->set_day_count(sc->get_day_count() + sleep_day_advance);
				}
			}
		}
	} else if (sleep_fade_t < 3.8f) {
		float out_t = (sleep_fade_t - 2.8f) / 1.0f;
		alpha = Math::clamp(1.0f - out_t, 0.0f, 1.0f);
		text_alpha = Math::clamp(1.0f - out_t * 1.5f, 0.0f, 1.0f);
	} else {
		stop_sleep();
		return;
	}

	if (sleep_overlay != nullptr) {
		sleep_overlay->set_color(Color(0.04f, 0.04f, 0.06f, alpha));
	}
	if (sleep_title != nullptr) {
		sleep_title->add_theme_color_override("font_color", Color(1.0f, 0.94f, 0.82f, text_alpha));
	}
	if (sleep_subtitle != nullptr) {
		sleep_subtitle->add_theme_color_override("font_color", Color(0.85f, 0.80f, 0.70f, text_alpha));
	}
}

void Player::stop_sleep() {
	if (!is_sleeping) {
		return;
	}
	is_sleeping = false;
	sleep_fade_t = 0.0f;
	sleep_advancing = false;

	if (sleep_layer != nullptr) {
		sleep_layer->hide();
		if (sleep_overlay != nullptr) {
			sleep_overlay->set_color(Color(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	// 同步唤醒 Bed 物体状态（保证 Bed::is_sleeping 状态一致，提示文案恢复）
	if (get_tree() != nullptr) {
		Array nodes = get_tree()->get_nodes_in_group("interactables");
		for (int i = 0; i < nodes.size(); i++) {
			Bed *b = Object::cast_to<Bed>(nodes[i]);
			if (b != nullptr && b->get_is_sleeping()) {
				b->wake_up();
			}
		}
	}

	// 醒来站到床边：沿床的侧向（局部 +Z，朝屋里那一侧）让开 1.25m。
	// 旧版写死 world +X 方向 1.2m，完全没考虑床转了 45°，会把人塞进床架或墙里。
	Vector3 bz = Vector3(std::sin(bed_yaw), 0.0f, std::cos(bed_yaw));
	set_global_position(bed_pos + bz * 1.25f + Vector3(0.0f, -0.30f, 0.0f));
	if (model != nullptr) {
		model->set_rotation(Vector3(0.0f, 0.0f, 0.0f));
		model->set_position(Vector3(0.0f, 0.0f, 0.0f));
	}
	if (hud_prompt != nullptr) {
		hud_prompt->set_text(String::utf8("☀️ 清晨来临，精神焕发！"));
		hud_prompt->show();
	}
	play_ui(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// 全局大地图与传送系统：M 键开启，全景大地图直选6大传送点
// ─────────────────────────────────────────────────────────────────────────────
// 全局大地图与传送系统：M 键开启，全景大地图直选8大传送点
// ─────────────────────────────────────────────────────────────────────────────
// 全局大地图与传送系统：M 键开启，可爱圆润全景地图直接点击瞬息漫游
// ─────────────────────────────────────────────────────────────────────────────
void Player::toggle_map() {
	if (map_layer == nullptr) {
		return;
	}
	is_map_open = !is_map_open;
	if (is_map_open) {
		map_hover_x = -9999.0f;
		map_hover_z = -9999.0f;
		Vector3 p = get_global_position();
		float u = Math::clamp(p.x / 220.0f + 0.5f, 0.02f, 0.98f);
		float v = Math::clamp(p.z / 220.0f + 0.5f, 0.02f, 0.98f);
		if (map_player_marker != nullptr && map_texture_rect != nullptr) {
			Vector2 sz = map_texture_rect->get_size();
			float mw = sz.x > 0.0f ? sz.x : 440.0f;
			float mh = sz.y > 0.0f ? sz.y : 440.0f;
			map_player_marker->set_position(Vector2(u * mw, v * mh));
			map_player_marker->set_visible(true);
		}
		if (map_coords_label != nullptr) {
			map_coords_label->set_text(String::utf8("🧭 当前位置: X=") + String::num(p.x, 1) + ", Z=" + String::num(p.z, 1) + String::utf8("  ·  移动瞄准星轻触瞬息漫游"));
		}
		if (map_crosshair != nullptr) {
			map_crosshair->set_visible(false);
		}
		map_layer->show();
		// 打开地图不要把系统鼠标显示出来，使用游戏专属瞄准 UI
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_HIDDEN);
		mouse_captured = false;
		play_ui(2);
	} else {
		map_layer->hide();
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
		mouse_captured = true;
		mouse_rel = Vector2();
		lmb_prev = true;
		play_ui(1);
	}
}

void Player::teleport_to(const Vector3 &p_pos) {
	set_global_position(p_pos + Vector3(0.0f, 0.35f, 0.0f));
	set_velocity(Vector3(0.0f, 0.0f, 0.0f));
	if (is_map_open) {
		toggle_map();
	}
	if (hud_prompt != nullptr) {
		hud_prompt->set_text(String::utf8("✨ 已传送至目标位置"));
		hud_prompt->show();
		hud_prompt_timer = 3.0f; // 3 秒后自动消失
	}
	play_ui(2);
}

void Player::build_map_ui() {
	Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");

	map_layer = memnew(CanvasLayer);
	map_layer->set_layer(10);
	add_child(map_layer);

	// 1. 全屏柔和暗场环境遮罩 (自然林间深影，轻触暗场亦可卷起舆图)
	ColorRect *bg = memnew(ColorRect);
	bg->set_color(Color(0.06f, 0.07f, 0.06f, 0.70f));
	bg->set_anchors_preset(Control::PRESET_FULL_RECT);
	bg->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	bg->set_default_cursor_shape(Control::CURSOR_ARROW);
	bg->connect("gui_input", Callable(this, "_on_map_bg_gui_input"));
	map_layer->add_child(bg);

	// 2. 主卡片面板 (540px 宽, 620px 高, 居中呈现可爱圆润的手绘地图手账)
	map_panel = memnew(Control);
	map_panel->set_anchors_preset(Control::PRESET_CENTER);
	map_panel->set_offset(SIDE_LEFT, -270.0f);
	map_panel->set_offset(SIDE_RIGHT, 270.0f);
	map_panel->set_offset(SIDE_TOP, -310.0f);
	map_panel->set_offset(SIDE_BOTTOM, 310.0f);
	map_panel->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	map_layer->add_child(map_panel);

	// 可爱圆润羊皮纸卡片底板：温暖象牙白底色、胡桃木精细边框、26px 大圆角与柔和阴影
	Ref<StyleBoxFlat> panel_style;
	panel_style.instantiate();
	panel_style->set_bg_color(Color(0.955f, 0.925f, 0.86f, 0.99f)); // 陈年地图纸
	panel_style->set_border_color(Color(uistyle::wood_mid().r, uistyle::wood_mid().g, uistyle::wood_mid().b, 0.99f));
	panel_style->set_border_width_all(7); // 厚木画框
	panel_style->set_corner_radius_all(20);
	panel_style->set_shadow_color(Color(0.10f, 0.08f, 0.06f, 0.45f));
	panel_style->set_shadow_size(30);
	panel_style->set_shadow_offset(Vector2(0, 12));
	// 内衬细线(双线装订):expand 5px 画一圈 1px 淡棕细线,与外框成手账双线
	Ref<StyleBoxFlat> panel_inner;
	panel_inner.instantiate();
	panel_inner->set_bg_color(Color(0, 0, 0, 0));
	panel_inner->set_border_color(Color(uistyle::wood_dark().r, uistyle::wood_dark().g, uistyle::wood_dark().b, 0.55f));
	panel_inner->set_border_width_all(2);
	panel_inner->set_corner_radius_all(12);

	PanelContainer *panel_bg = memnew(PanelContainer);
	panel_bg->set_anchors_preset(Control::PRESET_FULL_RECT);
	panel_bg->add_theme_stylebox_override("panel", panel_style);
	map_panel->add_child(panel_bg);

	// 手账双线内衬（纯装饰,不吃输入）
	Panel *inner_line = memnew(Panel);
	inner_line->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	inner_line->set_anchors_preset(Control::PRESET_FULL_RECT);
	inner_line->set_offset(SIDE_LEFT, 11.0f);
	inner_line->set_offset(SIDE_RIGHT, -11.0f);
	inner_line->set_offset(SIDE_TOP, 11.0f);
	inner_line->set_offset(SIDE_BOTTOM, -11.0f);
	inner_line->add_theme_stylebox_override("panel", panel_inner);
	map_panel->add_child(inner_line);

	// 顶部右侧可爱圆润关闭按钮 (✕)
	Button *top_close_btn = memnew(Button);
	top_close_btn->set_position(Vector2(494.0f, 14.0f));
	top_close_btn->set_size(Vector2(32.0f, 32.0f));
	top_close_btn->set_text("✕");
	top_close_btn->add_theme_font_size_override("font_size", 16);
	if (cjk.is_valid()) top_close_btn->add_theme_font_override("font", cjk);
	top_close_btn->set_focus_mode(Control::FOCUS_NONE);
	top_close_btn->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
	top_close_btn->set_mouse_filter(Control::MOUSE_FILTER_STOP);

	Ref<StyleBoxFlat> top_close_sb;
	top_close_sb.instantiate();
	top_close_sb->set_bg_color(Color(0.85f, 0.40f, 0.30f, 0.15f));
	top_close_sb->set_corner_radius_all(16);
	top_close_btn->add_theme_stylebox_override("normal", top_close_sb);

	Ref<StyleBoxFlat> top_close_hover;
	top_close_hover.instantiate();
	top_close_hover->set_bg_color(Color(0.85f, 0.40f, 0.30f, 0.90f));
	top_close_hover->set_corner_radius_all(16);
	top_close_btn->add_theme_stylebox_override("hover", top_close_hover);
	top_close_btn->add_theme_stylebox_override("pressed", top_close_hover);
	top_close_btn->add_theme_color_override("font_color", Color(0.55f, 0.38f, 0.28f));
	top_close_btn->add_theme_color_override("font_hover_color", Color(1.0f, 1.0f, 1.0f));
	top_close_btn->connect("pressed", Callable(this, "toggle_map"));
	map_panel->add_child(top_close_btn);

	// 顶部自然徽标胶囊 (Pill Badge)
	Label *badge = memnew(Label);
	badge->set_text(String::utf8(" 🧭 探险手记 · 苍湖舆图 "));
	badge->set_position(Vector2(195.0f, 14.0f));
	badge->set_size(Vector2(150.0f, 22.0f));
	badge->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	badge->add_theme_font_size_override("font_size", 12);
	badge->add_theme_color_override("font_color", Color(0.24f, 0.17f, 0.10f));
	Ref<StyleBoxFlat> badge_sb;
	badge_sb.instantiate();
	badge_sb->set_bg_color(Color(uistyle::wood_light().r, uistyle::wood_light().g, uistyle::wood_light().b, 0.96f));
	badge_sb->set_border_color(Color(uistyle::wood_dark().r, uistyle::wood_dark().g, uistyle::wood_dark().b, 0.9f));
	badge_sb->set_border_width_all(2);
	badge_sb->set_corner_radius_all(999);
	badge->add_theme_stylebox_override("theme_override_styles/normal", badge_sb);
	if (cjk.is_valid()) badge->add_theme_font_override("font", cjk);
	map_panel->add_child(badge);

	// 标题（深木暖色）
	Label *title = memnew(Label);
	title->set_text(String::utf8("✦ 苍湖旷野全景舆图 ✦"));
	title->set_position(Vector2(0.0f, 38.0f));
	title->set_size(Vector2(540.0f, 30.0f));
	title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	title->add_theme_font_size_override("font_size", 20);
	title->add_theme_color_override("font_color", Color(0.24f, 0.18f, 0.12f));
	if (cjk.is_valid()) title->add_theme_font_override("font", cjk);
	map_panel->add_child(title);

	// 副标题（温和柔褐灰色）
	Label *sub = memnew(Label);
	sub->set_text(String::utf8("移动瞄准星，轻触舆图任意位置即可瞬息漫游传送"));
	sub->set_position(Vector2(0.0f, 68.0f));
	sub->set_size(Vector2(540.0f, 20.0f));
	sub->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	sub->add_theme_font_size_override("font_size", 12);
	sub->add_theme_color_override("font_color", Color(0.55f, 0.48f, 0.40f));
	if (cjk.is_valid()) sub->add_theme_font_override("font", cjk);
	map_panel->add_child(sub);

	// ─────────────────────────────────────────────────────────────────────────
	// 绘制 256x256 自然手绘水彩风格全景地形缩略图 (水墨碧蓝湖水、柔和草甸、高山飞瀑、田园木屋)
	// ─────────────────────────────────────────────────────────────────────────
	const int MW = 256;
	const int MH = 256;
	Ref<Image> map_img = Image::create(MW, MH, false, Image::FORMAT_RGBA8);

	for (int my = 0; my < MH; my++) {
		float wz = ((float)my / (float)(MH - 1) - 0.5f) * 220.0f; // 核心范围 [-110, +110]
		for (int mx = 0; mx < MW; mx++) {
			float wx = ((float)mx / (float)(MW - 1) - 0.5f) * 220.0f;

			// 柔和水彩草甸绿底色，带有微弱纸纹起伏感
			float grain = std::sin(wx * 0.35f) * std::cos(wz * 0.35f) * 0.015f;
			Color col(0.52f + grain, 0.68f + grain, 0.46f + grain);

			// 1. 高山飞瀑与峡谷清流 (优先绘制在底层，大湖在上方自然覆盖交汇处，小溪在湖水下方！)
			if (std::hypot(wx - 122.0f, wz - 122.0f) < 32.0f) {
				col = Color(0.92f, 0.94f, 0.98f); // 东北大雪山巍峨峰顶积雪
			}
			const Vector2 stream_pts[6] = {
				Vector2(104.0f, 104.0f),
				Vector2(94.0f, 94.0f),
				Vector2(84.0f, 84.0f),
				Vector2(68.0f, 76.0f),
				Vector2(50.0f, 70.0f),
				Vector2(44.0f, 66.5f) // 平滑流入湖岸线下方
			};
			for (int si = 0; si < 5; ++si) {
				Vector2 sa = stream_pts[si], sb = stream_pts[si+1];
				Vector2 sab = sb - sa;
				float st_len2 = sab.length_squared();
				float st_t = Math::clamp((Vector2(wx, wz) - sa).dot(sab) / st_len2, 0.0f, 1.0f);
				if ((Vector2(wx, wz) - (sa + sab * st_t)).length() < 2.4f) {
					col = Color(0.24f, 0.65f, 0.96f); // 纯正明丽天蓝
				}
			}
			if (std::hypot(wx - 84.0f, wz - 84.0f) < 3.2f) {
				col = Color(0.96f, 0.98f, 1.0f); // 飞瀑跌水白雾浪花
			}
			if (std::hypot(wx - 50.0f, wz - 70.0f) < 3.2f) {
				col = Color(0.82f, 0.56f, 0.28f); // 木拱桥暖松木标识
			}

			// 2. 湖泊水域判定 (绘制在小溪上方！大湖水面自然盖住入湖河口，让小溪在湖水下面)
			float edge_nw = 44.0f - std::hypot(wx - (-14.0f), wz - 66.0f);
			float edge_ne = 32.0f - std::hypot(wx - 16.0f, wz - 56.0f);
			float edge_峡 = (1.05f - std::hypot((wx - (-6.0f)) / 13.0f, (wz - 26.0f) / 18.0f)) * 15.0f;
			float edge_s = 22.0f - std::hypot(wx - (-15.0f), wz - 8.0f);
			float dist_to_shore = Math::max(Math::max(edge_nw, edge_ne), Math::max(edge_峡, edge_s));

			// 检查是否位于高山小溪入湖口附近 (x∈[38, 48], z∈[63, 72])，入湖口沙滩完全敞开让溪水汇入湖中
			bool is_river_mouth = (wx >= 38.0f && wx <= 48.0f && wz >= 63.0f && wz <= 72.0f);

			if (dist_to_shore > 0.0f) {
				// 纯正明艳、清澈透亮的蔚蓝湖水 (浅滩明蓝到深水宝石蓝)
				col = Color(0.24f, 0.65f, 0.96f).lerp(Color(0.10f, 0.40f, 0.84f), Math::clamp(dist_to_shore / 22.0f, 0.0f, 0.85f));
				// 仅在最靠近陆地的 2.2 米极窄浅滩边缘勾勒暖金细沙圈 (入湖口不画沙子)
				if (!is_river_mouth && dist_to_shore < 2.2f) {
					col = Color(0.88f, 0.82f, 0.66f);
				}
			}

			// 3. 足球场绿茵草坪与白线 (中心 60,-40, 半宽 24, 半深 15)
			float px = std::abs(wx - 60.0f);
			float pz = std::abs(wz - (-40.0f));
			if (px <= 24.5f && pz <= 15.5f) {
				float stripe = std::fmod(std::floor((wx - 36.0f) / 3.0f), 2.0f);
				col = stripe < 1.0f ? Color(0.28f, 0.54f, 0.26f) : Color(0.34f, 0.58f, 0.30f);
				if (std::abs(px - 24.0f) < 0.6f || std::abs(pz - 15.0f) < 0.6f || px < 0.6f) {
					col = Color(0.95f, 0.97f, 0.97f);
				}
				if (std::abs(std::hypot(wx - 60.0f, wz - (-40.0f)) - 4.5f) < 0.6f) {
					col = Color(0.95f, 0.97f, 0.97f);
				}
			}

			// 4. 宅基地与木屋 (中心 12,8)
			float hx = std::abs(wx - 12.0f);
			float hz = std::abs(wz - 8.0f);
			if (hx < 12.0f && hz < 12.0f) {
				col = Color(0.72f, 0.62f, 0.48f); // 院落平地暖陶土
				if (hx < 4.5f && hz < 4.0f) {
					col = Color(0.78f, 0.38f, 0.28f); // 红瓦木屋房顶
				}
			}

			// 5. 古朴石拱桥 (x ∈ [-21, 9], z 接近 26)
			if (wx >= -21.0f && wx <= 9.0f && std::abs(wz - 26.0f) < 3.2f) {
				col = Color(0.62f, 0.60f, 0.56f); // 典雅花岗岩石拱桥
			}

			// 6. 湖心梅花桩 (-3.5, 15)
			if (std::hypot(wx - (-3.5f), wz - 15.0f) < 3.0f) {
				col = Color(0.92f, 0.46f, 0.64f); // 柔美梅花粉红标识
			}

			// 6b. 家园菜园 (4 块菜畦, 宅东侧 x∈[12.8,15.2] z∈[9.4,11.4]) —— 嫩绿格纹田垄
			if (wx >= 12.8f && wx <= 15.2f && wz >= 9.4f && wz <= 11.4f) {
				int furrow = ((int)std::floor(wx * 2.0f) + (int)std::floor(wz * 2.0f)) & 1;
				col = furrow ? Color(0.42f, 0.72f, 0.32f) : Color(0.60f, 0.80f, 0.42f);
			}

			// 6c. 东侧牧场 (动物家点群 x∈[7,31] z∈[-12,13]) —— 柔和草黄牧区
			if (std::hypot((wx - 22.0f) / 14.0f, (wz - 1.0f) / 14.0f) < 1.0f) {
				col = col.lerp(Color(0.78f, 0.74f, 0.42f), 0.45f);
			}

			// 7. 边缘环山水墨晕染
			float edge_dist = std::hypot(wx, wz);
			if (edge_dist > 95.0f) {
				col = col.lerp(Color(0.68f, 0.64f, 0.58f), Math::clamp((edge_dist - 95.0f) / 15.0f, 0.0f, 0.75f));
			}

			map_img->set_pixel(mx, my, col);
		}
	}

	Ref<ImageTexture> map_tex = ImageTexture::create_from_image(map_img);

	// 缩略图容器 (居中 440x440 可爱圆润卡片)
	Control *map_container = memnew(Control);
	map_container->set_position(Vector2(50.0f, 92.0f));
	map_container->set_size(Vector2(440.0f, 440.0f));
	map_panel->add_child(map_container);

	// 缩略图外层可爱圆润装裱木框
	PanelContainer *map_frame = memnew(PanelContainer);
	map_frame->set_position(Vector2(-4.0f, -4.0f));
	map_frame->set_size(Vector2(448.0f, 448.0f));
	Ref<StyleBoxFlat> frame_sb;
	frame_sb.instantiate();
	frame_sb->set_bg_color(Color(0.92f, 0.89f, 0.82f, 1.0f));
	frame_sb->set_border_color(Color(0.55f, 0.42f, 0.28f, 1.0f));
	frame_sb->set_border_width_all(3);
	frame_sb->set_corner_radius_all(22);
	map_frame->add_theme_stylebox_override("panel", frame_sb);
	map_container->add_child(map_frame);

	map_texture_rect = memnew(TextureRect);
	map_texture_rect->set_texture(map_tex);
	map_texture_rect->set_position(Vector2(0.0f, 0.0f));
	map_texture_rect->set_size(Vector2(440.0f, 440.0f));
	map_texture_rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
	map_texture_rect->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	map_texture_rect->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
	map_texture_rect->connect("gui_input", Callable(this, "_on_map_gui_input"));
	map_texture_rect->connect("mouse_exited", Callable(this, "_on_map_mouse_exited"));
	map_container->add_child(map_texture_rect);

	// —— 地点文字标注（世界坐标 → 440px 贴图；wx/220+0.5 再乘 440）——
	{
		Ref<Font> poi_font = ResourceLoader::get_singleton()->load(
				"res://assets/fonts/NotoSansSC-Regular.ttf");
		auto add_poi = [&](float p_wx, float p_wz, const char *p_text, const Color &p_col) {
			Label *poi = memnew(Label);
			poi->set_text(String::utf8(p_text));
			poi->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			float px = (p_wx / 220.0f + 0.5f) * 440.0f;
			float py = (p_wz / 220.0f + 0.5f) * 440.0f;
			poi->set_position(Vector2(px - 48.0f, py - 9.0f));
			poi->set_size(Vector2(96.0f, 18.0f));
			poi->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			poi->add_theme_font_size_override("font_size", 13);
			poi->add_theme_color_override("font_color", p_col);
			poi->add_theme_color_override("font_outline_color", Color(0.12f, 0.10f, 0.08f, 0.92f));
			poi->add_theme_constant_override("outline_size", 5);
			if (poi_font.is_valid()) {
				poi->add_theme_font_override("font", poi_font);
			}
			map_container->add_child(poi);
		};
		add_poi(12.0f, 20.5f, "🏡 家园", Color(1.0f, 0.95f, 0.86f));
		add_poi(14.0f, 12.2f, "🌱 菜园", Color(0.78f, 0.95f, 0.66f));
		add_poi(22.0f, -14.5f, "🐄 牧场", Color(1.0f, 0.92f, 0.70f));
		add_poi(60.0f, -49.5f, "⚽ 球场", Color(1.0f, 1.0f, 1.0f));
		add_poi(-6.0f, 32.5f, "🌉 石桥", Color(1.0f, 1.0f, 1.0f));
		add_poi(97.0f, 87.0f, "💦 瀑布", Color(0.80f, 0.93f, 1.0f));
		add_poi(-2.0f, 44.5f, "🎣 湖畔", Color(0.80f, 0.93f, 1.0f));
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 旅人实时位置徽标 (Player Position Dynamic Marker)
	// ─────────────────────────────────────────────────────────────────────────
	map_player_marker = memnew(Control);
	map_player_marker->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_player_marker->set_visible(false);
	map_container->add_child(map_player_marker);

	ColorRect *p_outer = memnew(ColorRect);
	p_outer->set_color(Color(0.92f, 0.28f, 0.20f));
	p_outer->set_position(Vector2(-6.0f, -6.0f));
	p_outer->set_size(Vector2(12.0f, 12.0f));
	p_outer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_player_marker->add_child(p_outer);

	ColorRect *p_inner = memnew(ColorRect);
	p_inner->set_color(Color(1.0f, 0.95f, 0.60f));
	p_inner->set_position(Vector2(-3.0f, -3.0f));
	p_inner->set_size(Vector2(6.0f, 6.0f));
	p_inner->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_player_marker->add_child(p_inner);

	Label *p_tag = memnew(Label);
	p_tag->set_text(String::utf8("📍 旅人"));
	p_tag->set_position(Vector2(-18.0f, -24.0f));
	p_tag->add_theme_font_size_override("font_size", 11);
	p_tag->add_theme_color_override("font_color", Color(0.85f, 0.20f, 0.15f));
	if (cjk.is_valid()) p_tag->add_theme_font_override("font", cjk);
	p_tag->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_player_marker->add_child(p_tag);

	// ─────────────────────────────────────────────────────────────────────────
	// 灵动游戏风格瞄准星 UI (Game-Style Dynamic Reticle Cursor)
	// 包含四向灵动弧形边框、中心准核、发光光环与坐标跟随
	// ─────────────────────────────────────────────────────────────────────────
	map_crosshair = memnew(Control);
	map_crosshair->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_crosshair->set_visible(false);
	map_container->add_child(map_crosshair);

	// 1. 发光外环 (圆润环形准星)
	PanelContainer *aim_ring = memnew(PanelContainer);
	aim_ring->set_position(Vector2(-16.0f, -16.0f));
	aim_ring->set_size(Vector2(32.0f, 32.0f));
	aim_ring->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	Ref<StyleBoxFlat> ring_sb;
	ring_sb.instantiate();
	ring_sb->set_bg_color(Color(0.98f, 0.85f, 0.35f, 0.16f)); // 灵动琥珀柔光
	ring_sb->set_border_color(Color(1.0f, 0.88f, 0.38f, 0.95f)); // 亮金准星光环
	ring_sb->set_border_width_all(2);
	ring_sb->set_corner_radius_all(16);
	ring_sb->set_shadow_color(Color(0.95f, 0.65f, 0.18f, 0.55f));
	ring_sb->set_shadow_size(8);
	aim_ring->add_theme_stylebox_override("panel", ring_sb);
	map_crosshair->add_child(aim_ring);

	// 2. 中心灵动光点 (Center Aim Dot)
	ColorRect *aim_dot = memnew(ColorRect);
	aim_dot->set_position(Vector2(-2.5f, -2.5f));
	aim_dot->set_size(Vector2(5.0f, 5.0f));
	aim_dot->set_color(Color(1.0f, 0.32f, 0.22f, 1.0f)); // 绯红聚焦点
	aim_dot->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_crosshair->add_child(aim_dot);

	// 3. 四向刻度准线 (Crosshair Ticks)
	const float tick_len = 5.0f;
	const float tick_thick = 2.0f;
	const float tick_dist = 18.0f;
	// 上
	ColorRect *t_top = memnew(ColorRect);
	t_top->set_position(Vector2(-tick_thick * 0.5f, -tick_dist - tick_len));
	t_top->set_size(Vector2(tick_thick, tick_len));
	t_top->set_color(Color(1.0f, 0.88f, 0.38f, 0.90f));
	t_top->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_crosshair->add_child(t_top);
	// 下
	ColorRect *t_bot = memnew(ColorRect);
	t_bot->set_position(Vector2(-tick_thick * 0.5f, tick_dist));
	t_bot->set_size(Vector2(tick_thick, tick_len));
	t_bot->set_color(Color(1.0f, 0.88f, 0.38f, 0.90f));
	t_bot->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_crosshair->add_child(t_bot);
	// 左
	ColorRect *t_left = memnew(ColorRect);
	t_left->set_position(Vector2(-tick_dist - tick_len, -tick_thick * 0.5f));
	t_left->set_size(Vector2(tick_len, tick_thick));
	t_left->set_color(Color(1.0f, 0.88f, 0.38f, 0.90f));
	t_left->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_crosshair->add_child(t_left);
	// 右
	ColorRect *t_right = memnew(ColorRect);
	t_right->set_position(Vector2(tick_dist, -tick_thick * 0.5f));
	t_right->set_size(Vector2(tick_len, tick_thick));
	t_right->set_color(Color(1.0f, 0.88f, 0.38f, 0.90f));
	t_right->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	map_crosshair->add_child(t_right);

	// 地图下方居中坐标状态条
	map_coords_label = memnew(Label);
	map_coords_label->set_position(Vector2(0.0f, 538.0f));
	map_coords_label->set_size(Vector2(540.0f, 22.0f));
	map_coords_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	map_coords_label->set_text(String::utf8("🧭 当前位置: X=0.0, Z=0.0  ·  移动瞄准星轻触瞬息漫游"));
	map_coords_label->add_theme_font_size_override("font_size", 12);
	map_coords_label->add_theme_color_override("font_color", Color(0.48f, 0.38f, 0.28f));
	if (cjk.is_valid()) map_coords_label->add_theme_font_override("font", cjk);
	map_panel->add_child(map_coords_label);

	// 底部可爱圆润胶囊关闭按钮
	Button *close_btn = memnew(Button);
	close_btn->set_position(Vector2(160.0f, 568.0f));
	close_btn->set_size(Vector2(220.0f, 36.0f));
	close_btn->set_text(String::utf8("✦ 卷起舆图 (M / ESC) ✦"));
	close_btn->add_theme_font_size_override("font_size", 13);
	if (cjk.is_valid()) close_btn->add_theme_font_override("font", cjk);
	close_btn->set_focus_mode(Control::FOCUS_NONE);
	close_btn->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
	close_btn->set_mouse_filter(Control::MOUSE_FILTER_STOP);

	Ref<StyleBoxFlat> close_normal;
	close_normal.instantiate();
	close_normal->set_bg_color(Color(uistyle::wood_light().r, uistyle::wood_light().g, uistyle::wood_light().b, 0.99f));
	close_normal->set_border_color(Color(uistyle::wood_dark().r, uistyle::wood_dark().g, uistyle::wood_dark().b, 1.0f));
	close_normal->set_border_width_all(2);
	close_normal->set_corner_radius_all(18);

	Ref<StyleBoxFlat> close_hover;
	close_hover.instantiate();
	close_hover->set_bg_color(Color(0.72f, 0.58f, 0.38f, 1.0f));
	close_hover->set_border_color(Color(1.0f, 0.92f, 0.62f, 1.0f)); // 悬停暖纸金边
	close_hover->set_border_width_all(1);
	close_hover->set_corner_radius_all(18);

	close_btn->add_theme_stylebox_override("normal", close_normal);
	close_btn->add_theme_stylebox_override("hover", close_hover);
	close_btn->add_theme_stylebox_override("pressed", close_normal);
	close_btn->add_theme_stylebox_override("focus", close_hover);
	close_btn->add_theme_color_override("font_color", Color(1.0f, 0.97f, 0.92f));
	close_btn->add_theme_color_override("font_hover_color", Color(1.0f, 1.0f, 1.0f));
	close_btn->connect("pressed", Callable(this, "toggle_map"));
	map_panel->add_child(close_btn);

	map_layer->hide();
}

void Player::_on_map_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Vector2 mpos = mm->get_position();
		if (map_crosshair != nullptr) {
			map_crosshair->set_position(mpos);
			if (!map_crosshair->is_visible()) {
				map_crosshair->set_visible(true);
			}
		}
		if (map_texture_rect != nullptr && map_coords_label != nullptr) {
			Vector2 sz = map_texture_rect->get_size();
			if (sz.x > 0.0f && sz.y > 0.0f) {
				float u = mpos.x / sz.x;
				float v = mpos.y / sz.y;
				float tx = (u - 0.5f) * 220.0f;
				float tz = (v - 0.5f) * 220.0f;
				// 节流坐标刷新：位移超 0.5 米才更新文字
				if (Math::abs(tx - map_hover_x) >= 0.5f || Math::abs(tz - map_hover_z) >= 0.5f) {
					map_hover_x = tx;
					map_hover_z = tz;
					map_coords_label->set_text(String::utf8("🎯 准星指向: X=") + String::num(tx, 1) + ", Z=" + String::num(tz, 1) + String::utf8("  ·  轻触瞬息漫游"));
				}
			}
		}
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		Vector2 mpos = mb->get_position();
		if (map_texture_rect != nullptr) {
			Vector2 sz = map_texture_rect->get_size();
			if (sz.x > 0.0f && sz.y > 0.0f) {
				// 将地图 0..sz 坐标映射至世界核心坐标 [-110, +110]
				float u = mpos.x / sz.x;
				float v = mpos.y / sz.y;
				float target_x = (u - 0.5f) * 220.0f;
				float target_z = (v - 0.5f) * 220.0f;

				// 采样地形高度以防卡地底或悬空
				float target_y = 0.5f;
				if (terrain_cache != nullptr) {
					target_y = terrain_cache->get_terrain_height(target_x, target_z);
				}

				teleport_to(Vector3(target_x, target_y + 0.35f, target_z));
			}
		}
	}
}

void Player::_on_map_bg_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		toggle_map();
	}
}

void Player::_on_map_mouse_exited() {
	if (map_crosshair != nullptr) {
		map_crosshair->set_visible(false);
	}
	map_hover_x = -9999.0f;
	map_hover_z = -9999.0f;
	Vector3 p = get_global_position();
	if (map_coords_label != nullptr) {
		map_coords_label->set_text(String::utf8("🧭 当前位置: X=") + String::num(p.x, 1) + ", Z=" + String::num(p.z, 1) + String::utf8("  ·  轻触舆图任意处瞬息漫游"));
	}
}

void Player::launch_firework() {
	Node *parent = get_parent();
	if (parent == nullptr) {
		return;
	}

	const Vector3 player_pos = get_global_position();
	// 烟花升空目标点：向斜前方远处 35m、高 24m 抛物线升空，远景高空绽放尽收眼底
	Vector3 fwd = facing.normalized();
	if (fwd.length_squared() < 0.001f) {
		fwd = Vector3(0.0f, 0.0f, -1.0f);
	}
	Vector3 shoot_dir = fwd * 35.0f + Vector3(0.0f, 24.0f, 0.0f);
	Vector3 burst_pos = player_pos + shoot_dir;

	// 1. 升空哨音 / 尾焰声
	ResourceLoader *rl = ResourceLoader::get_singleton();
	Ref<AudioStream> snd_launch;
	Ref<AudioStream> snd_bang;
	Ref<Texture2D> spark_tex;
	if (rl != nullptr) {
		if (rl->exists("res://assets/audio/firework_launch.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/firework_launch.wav");
			if (r.is_valid()) snd_launch = Object::cast_to<AudioStream>(r.ptr());
		}
		if (rl->exists("res://assets/audio/firework_bang.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/firework_bang.wav");
			if (r.is_valid()) snd_bang = Object::cast_to<AudioStream>(r.ptr());
		}
		if (rl->exists("res://assets/firework_spark.png")) {
			Ref<Resource> r = rl->load("res://assets/firework_spark.png");
			if (r.is_valid()) spark_tex = Object::cast_to<Texture2D>(r.ptr());
		}
	}

	AudioStreamPlayer3D *launch_voice = memnew(AudioStreamPlayer3D);
	launch_voice->set_position(player_pos);
	launch_voice->set_unit_size(16.0f);
	launch_voice->set_max_distance(180.0f);
	if (snd_launch.is_valid()) {
		launch_voice->set_stream(snd_launch);
	}
	parent->add_child(launch_voice);
	launch_voice->play();

	// 2. 升空火箭拖尾烟花粒子（高发光火光飞箭）
	Ref<StandardMaterial3D> trail_mat;
	trail_mat.instantiate();
	trail_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	trail_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	trail_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	trail_mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	trail_mat->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	trail_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	trail_mat->set_emission(Color(1.0f, 0.90f, 0.45f));
	trail_mat->set_emission_energy_multiplier(8.5f);
	if (spark_tex.is_valid()) {
		trail_mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, spark_tex);
	}

	Ref<QuadMesh> trail_mesh;
	trail_mesh.instantiate();
	trail_mesh->set_size(Vector2(0.38f, 0.38f));
	trail_mesh->set_material(trail_mat);

	CPUParticles3D *rocket = memnew(CPUParticles3D);
	rocket->set_position(player_pos + Vector3(0.0f, 0.6f, 0.0f));
	rocket->set_amount(48);
	rocket->set_lifetime(0.75f);
	rocket->set_mesh(trail_mesh);
	rocket->set_direction(shoot_dir.normalized());
	rocket->set_spread(6.0f);
	rocket->set_gravity(Vector3(0.0f, -4.0f, 0.0f));
	rocket->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 48.0f);
	rocket->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 54.0f);
	rocket->set_color(Color(1.0f, 0.92f, 0.55f, 1.0f));
	rocket->set_one_shot(true);
	rocket->set_explosiveness_ratio(0.88f);
	parent->add_child(rocket);
	rocket->set_emitting(true);

	// 3. 在夜空炸开震撼绚丽烟花（0.72 秒升至顶峰爆炸）
	Node3D *burst_root = memnew(Node3D);
	burst_root->set_name("FireworkBurst");
	burst_root->set_position(burst_pos);
	parent->add_child(burst_root);

	// 绚丽纯正高饱和度烟花色彩库
	static const Color FW_COLORS[6] = {
		Color(1.0f, 0.85f, 0.15f),  // 盛世鎏金 (Golden Crown)
		Color(1.0f, 0.18f, 0.42f),  // 炽焰绯红 (Crimson Flame)
		Color(0.20f, 0.85f, 1.00f),  // 苍穹冰蓝 (Cyan Sky)
		Color(0.25f, 1.00f, 0.45f),  // 翡翠青芒 (Emerald Jade)
		Color(0.95f, 0.35f, 1.00f),  // 魅影幽紫 (Amethyst Violet)
		Color(1.0f, 0.52f, 0.10f),  // 熔岩暖橙 (Molten Orange)
	};
	int c_idx = rng() % 6;
	Color burst_col = FW_COLORS[c_idx];
	Color spark_col = FW_COLORS[(c_idx + 1 + (rng() % 5)) % 6]; // 互补第二色

	// 爆炸立体音效
	AudioStreamPlayer3D *bang_voice = memnew(AudioStreamPlayer3D);
	bang_voice->set_position(Vector3(0.0f, 0.0f, 0.0f));
	bang_voice->set_unit_size(36.0f);
	bang_voice->set_max_distance(400.0f);
	bang_voice->set_volume_db(2.0f); // 增强爆破震撼力
	if (snd_bang.is_valid()) {
		bang_voice->set_stream(snd_bang);
	}
	burst_root->add_child(bang_voice);

	// 粒子层 1：爆炸主花冠（球面全向高速炸裂，强发光核心）
	Ref<StandardMaterial3D> pmat;
	pmat.instantiate();
	pmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	pmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	pmat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	pmat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	pmat->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	pmat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	pmat->set_emission(burst_col);
	pmat->set_emission_energy_multiplier(9.0f); // 极其璀璨明亮的辉光
	if (spark_tex.is_valid()) {
		pmat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, spark_tex);
	}

	Ref<QuadMesh> pmesh;
	pmesh.instantiate();
	pmesh->set_size(Vector2(0.65f, 0.65f));
	pmesh->set_material(pmat);

	CPUParticles3D *burst = memnew(CPUParticles3D);
	burst->set_name("BurstParticles");
	burst->set_amount(260); // 增加粒子密度至 260
	burst->set_lifetime(2.2f);
	burst->set_one_shot(true);
	burst->set_explosiveness_ratio(0.98f);
	burst->set_mesh(pmesh);
	burst->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	burst->set_emission_sphere_radius(0.6f);
	burst->set_spread(180.0f);
	burst->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	burst->set_gravity(Vector3(0.0f, -4.8f, 0.0f));
	burst->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 22.0f);
	burst->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 38.0f);
	burst->set_param_min(CPUParticles3D::PARAM_DAMPING, 4.0f);
	burst->set_param_max(CPUParticles3D::PARAM_DAMPING, 6.2f);
	burst->set_color(burst_col);
	burst->set_emitting(false);
	burst_root->add_child(burst);

	// 粒子层 2：金碎花次级微粒（漫天飞舞闪烁）
	Ref<StandardMaterial3D> smat;
	smat.instantiate();
	smat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	smat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	smat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	smat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	smat->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	smat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	smat->set_emission(spark_col);
	smat->set_emission_energy_multiplier(7.5f);
	if (spark_tex.is_valid()) {
		smat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, spark_tex);
	}

	Ref<QuadMesh> spark_mesh;
	spark_mesh.instantiate();
	spark_mesh->set_size(Vector2(0.42f, 0.42f));
	spark_mesh->set_material(smat);

	CPUParticles3D *sparks = memnew(CPUParticles3D);
	sparks->set_name("SparkParticles");
	sparks->set_amount(160);
	sparks->set_lifetime(2.5f);
	sparks->set_one_shot(true);
	sparks->set_explosiveness_ratio(0.95f);
	sparks->set_mesh(spark_mesh);
	sparks->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	sparks->set_emission_sphere_radius(0.8f);
	sparks->set_spread(180.0f);
	sparks->set_gravity(Vector3(0.0f, -3.8f, 0.0f));
	sparks->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 14.0f);
	sparks->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 28.0f);
	sparks->set_param_min(CPUParticles3D::PARAM_DAMPING, 3.2f);
	sparks->set_param_max(CPUParticles3D::PARAM_DAMPING, 5.2f);
	sparks->set_color(spark_col);
	sparks->set_emitting(false);
	burst_root->add_child(sparks);

	// 粒子层 3：垂丝金柳 (Golden Willow Tails) 缓落长尾流星
	Ref<StandardMaterial3D> wmat;
	wmat.instantiate();
	wmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	wmat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	wmat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	wmat->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	wmat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	wmat->set_emission(Color(1.0f, 0.95f, 0.72f));
	wmat->set_emission_energy_multiplier(8.0f);
	if (spark_tex.is_valid()) {
		wmat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, spark_tex);
	}

	Ref<QuadMesh> willow_mesh;
	willow_mesh.instantiate();
	willow_mesh->set_size(Vector2(0.28f, 0.28f));
	willow_mesh->set_material(wmat);

	CPUParticles3D *willow = memnew(CPUParticles3D);
	willow->set_name("WillowParticles");
	willow->set_amount(110);
	willow->set_lifetime(3.0f); // 3 秒持久垂坠流光
	willow->set_one_shot(true);
	willow->set_explosiveness_ratio(0.85f);
	willow->set_mesh(willow_mesh);
	willow->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	willow->set_emission_sphere_radius(0.4f);
	willow->set_spread(180.0f);
	willow->set_gravity(Vector3(0.0f, -6.8f, 0.0f)); // 像金色柳树垂落
	willow->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 10.0f);
	willow->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 20.0f);
	willow->set_param_min(CPUParticles3D::PARAM_DAMPING, 2.5f);
	willow->set_param_max(CPUParticles3D::PARAM_DAMPING, 4.0f);
	willow->set_color(Color(1.0f, 0.96f, 0.80f, 1.0f));
	willow->set_emitting(false);
	burst_root->add_child(willow);

	// 照亮夜空的超高能动态点光源（范围 140 米，峰值能量 95.0，大幅照亮山谷、湖泊、林木和房屋）
	OmniLight3D *flash_light = memnew(OmniLight3D);
	flash_light->set_name("FlashLight");
	flash_light->set_color(burst_col);
	flash_light->set_param(OmniLight3D::PARAM_ENERGY, 0.0f);
	flash_light->set_param(OmniLight3D::PARAM_RANGE, 140.0f);
	flash_light->set_param(OmniLight3D::PARAM_ATTENUATION, 1.25f);
	burst_root->add_child(flash_light);

	// 0.75 秒后在夜空绽放
	Ref<SceneTreeTimer> burst_timer = get_tree()->create_timer(0.75);
	burst_timer->connect("timeout", Callable(this, "_on_firework_burst").bind(burst, sparks, willow, bang_voice, flash_light, 95.0f));

	// 4.0 秒后安全释放全部烟花资源
	Ref<SceneTreeTimer> cleanup_timer = get_tree()->create_timer(4.0);
	cleanup_timer->connect("timeout", Callable(this, "_on_firework_cleanup").bind(burst_root, rocket, launch_voice));
}

void Player::_on_firework_burst(Node *burst, Node *sparks, Node *willow, Node *bang_voice, Node *flash_light, float max_energy) {
	CPUParticles3D *b = Object::cast_to<CPUParticles3D>(burst);
	if (b != nullptr && b->is_inside_tree()) {
		b->set_emitting(true);
	}
	CPUParticles3D *s = Object::cast_to<CPUParticles3D>(sparks);
	if (s != nullptr && s->is_inside_tree()) {
		s->set_emitting(true);
	}
	CPUParticles3D *w = Object::cast_to<CPUParticles3D>(willow);
	if (w != nullptr && w->is_inside_tree()) {
		w->set_emitting(true);
	}
	AudioStreamPlayer3D *bv = Object::cast_to<AudioStreamPlayer3D>(bang_voice);
	if (bv != nullptr && bv->is_inside_tree()) {
		bv->play();
	}
	OmniLight3D *fl = Object::cast_to<OmniLight3D>(flash_light);
	if (fl != nullptr && fl->is_inside_tree()) {
		fl->set_param(OmniLight3D::PARAM_ENERGY, max_energy);
		// 登记到动态衰减容器中进行逐物理帧平滑渐暗，产生强烈的爆闪冲击与自然渐隐
		active_firework_flashes.push_back({ fl, 0.0f, max_energy, fl->get_color() });
	}
}

void Player::_on_firework_cleanup(Node *burst_root, Node *rocket, Node *launch_voice) {
	if (burst_root != nullptr && burst_root->is_inside_tree()) {
		burst_root->queue_free();
	}
	if (rocket != nullptr && rocket->is_inside_tree()) {
		rocket->queue_free();
	}
	if (launch_voice != nullptr && launch_voice->is_inside_tree()) {
		launch_voice->queue_free();
	}
}

// —— 游泳螺旋桨：背后三叶推进器（银白桨叶 + 青色轮毂，入水自动出现） ——
void Player::build_propeller() {
	if (model == nullptr) {
		return;
	}
	propeller = memnew(Node3D);
	propeller->set_name("SwimPropeller");
	propeller->set_position(Vector3(0.0f, -0.12f, 0.0f)); // 脚后（局部 +Y 经横置后指向身体前方，-Y 即脚后）
	model->add_child(propeller);

	Ref<StandardMaterial3D> metal;
	metal.instantiate();
	metal->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	metal->set_albedo(Color(0.78f, 0.86f, 0.92f));
	metal->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	metal->set_emission(Color(0.30f, 0.62f, 0.72f));
	metal->set_emission_energy_multiplier(0.55f);

	// 轮毂
	Ref<SphereMesh> hub;
	hub.instantiate();
	hub->set_radius(0.055f);
	hub->set_height(0.11f);
	hub->set_radial_segments(10);
	hub->set_rings(5);
	hub->set_material(metal);
	MeshInstance3D *hub_mi = memnew(MeshInstance3D);
	hub_mi->set_mesh(hub);
	propeller->add_child(hub_mi);

	// 三片桨叶（绕 Z 轴旋转的盘面）
	propeller_blades = memnew(Node3D);
	propeller_blades->set_name("Blades");
	propeller->add_child(propeller_blades);
	for (int i = 0; i < 3; i++) {
		Node3D *arm = memnew(Node3D);
		arm->set_rotation(Vector3(0.0f, (float)i * 2.0943951f, 0.0f)); // 盘面绕身体前进轴
		propeller_blades->add_child(arm);
		Ref<BoxMesh> blade;
		blade.instantiate();
		blade->set_size(Vector3(0.055f, 0.016f, 0.30f));
		blade->set_material(metal);
		MeshInstance3D *bmi = memnew(MeshInstance3D);
		bmi->set_mesh(blade);
		bmi->set_position(Vector3(0.0f, 0.0f, 0.17f));
		arm->add_child(bmi);
	}

	propeller->set_visible(false);
}
