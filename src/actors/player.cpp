#include "player.h"

#include <cmath>

// mingw / 严格 C++17 下 <cmath> 不保证提供 M_PI（POSIX 扩展），补一个兜底定义
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
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

using namespace godot;

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

	ClassDB::bind_method(D_METHOD("set_camera_height", "value"), &Player::set_camera_height);
	ClassDB::bind_method(D_METHOD("get_camera_height"), &Player::get_camera_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_height"), "set_camera_height", "get_camera_height");
}

void Player::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// 按名字找到场景里的子节点（Model 只负责外观旋转，Camera3D 是跟随相机）
			model = Object::cast_to<Node3D>(get_node_or_null("Model"));
			camera = Object::cast_to<Camera3D>(get_node_or_null("Camera3D"));
			set_physics_process(true); // 打开物理帧回调（移动逻辑）
			set_process(true);         // 打开渲染帧回调（相机平滑）

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

			// —— 给胶囊贴上"脸"：两只眼睛 + 一个小圆鼻，让模型"正面"可读
			// （此前纯胶囊无朝向特征，正反全靠脑补）。正面 = 模型本地 +Z ——
			if (model != nullptr) {
				auto face_mat = [](const Color &c) {
					Ref<StandardMaterial3D> m;
					m.instantiate();
					m->set_albedo(c);
					m->set_roughness(0.6f);
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
					s->set_radial_segments(10);
					s->set_rings(6);
					return s;
				};
				auto add_part = [&](Ref<Mesh> p_mesh, Ref<Material> p_mat, const Vector3 &p_pos) {
					MeshInstance3D *mi = memnew(MeshInstance3D);
					mi->set_mesh(p_mesh);
					mi->set_material_override(p_mat);
					mi->set_position(p_pos);
					model->add_child(mi);
				};
				add_part(ball(0.055f), white, Vector3(-0.12f, 1.45f, 0.34f));
				add_part(ball(0.055f), white, Vector3(0.12f, 1.45f, 0.34f));
				add_part(ball(0.026f), dark, Vector3(-0.12f, 1.45f, 0.385f));
				add_part(ball(0.026f), dark, Vector3(0.12f, 1.45f, 0.385f));
				add_part(ball(0.035f), nose_m, Vector3(0.0f, 1.33f, 0.37f));
			}

			// 脚步声：真实雪地踩雪声优先读取外部音频资源（用户下载的踩雪声录音），其余程序化合成
			steps_voice = memnew(AudioStreamPlayer);
			steps_voice->set_name("Steps");
			steps_voice->set_volume_db(-9.0f);
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

			snd_wet = make_puddle();

			// 优先加载选中的石拱桥石板踏步音效
			if (rl != nullptr && rl->exists("res://assets/audio/stone_step.wav")) {
				Ref<Resource> r = rl->load("res://assets/audio/stone_step.wav");
				if (r.is_valid()) {
					snd_stone = Object::cast_to<AudioStream>(r.ptr());
				}
			}
			if (!snd_stone.is_valid()) {
				snd_stone = make_stone();
			}

			snd_wood = make_wood();
			snd_rustle = make_rustle(false);
			snd_rustle_deep = make_rustle(true);
			rustle_voice = memnew(AudioStreamPlayer);
			rustle_voice->set_name("Rustle");
			rustle_voice->set_volume_db(-16.0f);
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

		case NOTIFICATION_PROCESS:
			_update_look(get_process_delta_time());
			_update_camera(get_process_delta_time());
			break;

		default:
			break;
	}
}

void Player::_physics_step() {
	Input *input = Input::get_singleton();

	// 读键盘（世界坐标：W 朝 -Z，S 朝 +Z，A 朝 -X，D 朝 +X）
	const double ix = (input->is_key_pressed(KEY_D) ? 1.0 : 0.0) - (input->is_key_pressed(KEY_A) ? 1.0 : 0.0);
	const double iz = (input->is_key_pressed(KEY_S) ? 1.0 : 0.0) - (input->is_key_pressed(KEY_W) ? 1.0 : 0.0);

	Vector3 dir(ix, 0.0, iz);
	if (dir.length_squared() > 0.0001) {
		// WASD 按相机偏航角旋转：W 永远朝画面深处
		dir = Basis(Vector3(0.0, 1.0, 0.0), (float)cam_yaw).xform(dir).normalized();
	}

	const double delta = get_physics_process_delta_time();
	Vector3 velocity = get_velocity();

	// 灌木丛减速判定：穿过灌木丛时移速降至约 52%，带有明显的阻滞手感
	Vector3 gp = get_global_position();
	if (terrain_cache == nullptr) {
		Node *parent = get_parent();
		if (parent != nullptr) {
			terrain_cache = Object::cast_to<TerrainBuilder>(parent->get_node_or_null(NodePath("TerrainBuilder")));
		}
	}
	bool in_bush = false;
	if (terrain_cache != nullptr) {
		in_bush = terrain_cache->is_in_bush(gp.x, gp.z, gp.y);
	}
	double current_speed = in_bush ? (move_speed * 0.52) : move_speed;

	// 水平：直接设定速度（引擎负责与地形碰撞、滑动）
	velocity.x = dir.x * current_speed;
	velocity.z = dir.z * current_speed;

	// 垂直：重力 + 落地归零
	if (!is_on_floor()) {
		velocity.y -= gravity * delta;
	} else if (velocity.y < 0.0) {
		velocity.y = 0.0;
	}

	// 跳跃
	if (is_on_floor() && input->is_key_pressed(KEY_SPACE)) {
		velocity.y = jump_velocity;
	}

	set_velocity(velocity);
	move_and_slide();

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
	// 推门/互动时在朝向上叠加一个前倾再回弹的"推"的动作
	if (model != nullptr) {
		if (dir.length_squared() > 0.0001) {
			facing = dir; // 记住最后移动方向，站着不动时保持朝向
		}
		float yaw = Math::atan2(facing.x, facing.z); // 模型正面为 +Z，对齐移动方向（原式差 180° 倒着走）

		double lean = 0.0;
		if (lean_time > 0.0) {
			lean_time -= delta;
			double phase = 1.0 - Math::clamp(lean_time / 0.5, 0.0, 1.0); // 0..1
			lean = std::sin(phase * M_PI) * 0.5;                         // 前倾 0.5rad 再回正
		}

		Basis b = Basis(Vector3(0.0, 1.0, 0.0), yaw) * Basis(Vector3(1.0, 0.0, 0.0), lean);
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

// 互动：找最近的可交互物 → 更新提示 → E 键边沿触发 interact() + 推门动作
void Player::_update_interaction() {
	Interactable *best = nullptr;
	double best_d = 3.2 * 3.2; // 互动半径 3.2m

	Array nodes = get_tree()->get_nodes_in_group("interactables");
	for (int i = 0; i < nodes.size(); i++) {
		Node *n = Object::cast_to<Node>(nodes[i]);
		Interactable *it = Object::cast_to<Interactable>(n);
		if (it == nullptr) {
			continue;
		}
		double d = get_global_position().distance_squared_to(it->get_interact_pos());
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
	Input *input = Input::get_singleton();
	bool e_down = input->is_key_pressed(KEY_E);
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

// 鼠标视角：点击画面捕获鼠标，ESC 释放；捕获期间鼠标速度驱动偏航/俯仰，
// 滚轮缩放距离。轮询路线（Input 单例）而非 _unhandled_input——与工程
// "_notification 优先于 GDVIRTUAL override"的既有约定一致。
void Player::_update_look(double delta) {
	Input *input = Input::get_singleton();

	bool esc = input->is_key_pressed(KEY_ESCAPE);
	if (esc && !esc_prev && mouse_captured) {
		mouse_captured = false;
		input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
		play_ui(1);
	}
	esc_prev = esc;

	bool lmb = input->is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
	if (lmb && !lmb_prev && !mouse_captured) {
		mouse_captured = true;
		input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
		play_ui(2);
	}
	lmb_prev = lmb;

	if (!mouse_captured) {
		return;
	}

	Vector2 vel = input->get_last_mouse_velocity(); // 像素/秒（捕获模式的累计相对速度）
	cam_yaw -= (double)vel.x * delta * 0.0032;
	cam_pitch = Math::clamp(cam_pitch + (double)vel.y * delta * 0.0028, -0.5, 1.25);

	if (input->is_mouse_button_pressed(MOUSE_BUTTON_WHEEL_UP)) {
		cam_dist = Math::max(2.5, cam_dist - 0.6);
	}
	if (input->is_mouse_button_pressed(MOUSE_BUTTON_WHEEL_DOWN)) {
		cam_dist = Math::min(9.0, cam_dist + 0.6);
	}
}

// 第三人称越肩相机：以"胸口支点"为球心，偏航/俯仰决定球面机位；
// 支点→机位有遮挡时沿视线回拉（防穿墙，回拉直达不平滑，避免闪墙）。
void Player::_update_camera(double delta) {
	if (camera == nullptr) {
		return;
	}

	const Vector3 head = get_global_position() + Vector3(0.0, camera_height, 0.0);
	Vector3 off = Basis(Vector3(0.0, 1.0, 0.0), (float)cam_yaw).xform(
			Vector3(0.0, std::sin(cam_pitch), std::cos(cam_pitch)) * cam_dist);
	Vector3 want = head + off;

	bool pulled = false;
	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	if (space != nullptr) {
		Ref<PhysicsRayQueryParameters3D> ray = PhysicsRayQueryParameters3D::create(head, want);
		Array ex;
		ex.push_back(get_rid());
		ray->set_exclude(ex);
		Dictionary hit = space->intersect_ray(ray);
		if (!hit.is_empty()) {
			Vector3 hp = hit["position"];
			want = head + (hp - head) * 0.86;
			pulled = true;
		}
	}

	if (pulled) {
		camera->set_global_position(want);
	} else {
		double t = delta * camera_smoothing;
		if (t > 1.0) {
			t = 1.0;
		}
		camera->set_global_position(camera->get_global_position().lerp(want, t));
	}
	camera->look_at(head + Vector3(0.0, 0.25, 0.0), Vector3(0.0, 1.0, 0.0));
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
		step_acc = 0.0;
		land_timer = 0.40; // 连跳与落地缓冲：接地点草叶保持倒伏并回弹 0.4 秒
		last_land_pos = get_global_position();
		play_step(_step_kind(), 0.86f); // 着地声偏沉，音色跟随当前地面

		// 落地双脚并排留下较深印痕
		if (FootprintManager::get_singleton() != nullptr) {
			Vector3 pos = get_global_position();
			float yaw = Math::atan2(facing.x, facing.z);
			Vector3 right_vec(-facing.z, 0.0f, facing.x);
			if (right_vec.length_squared() > 0.001f) {
				right_vec = right_vec.normalized();
			}
			FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_HUMAN_LEFT, pos - right_vec * 0.14f, yaw, 1.08f);
			FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_HUMAN_RIGHT, pos + right_vec * 0.14f, yaw, 1.08f);
		}
		return;
	}

	if (!on_floor) {
		return;
	}

	const Vector3 v = get_velocity();
	const double speed = Vector2(v.x, v.z).length();
	if (speed < 0.6) {
		return; // 站着不动不响
	}

	step_acc += speed * delta;
	if (step_acc >= 2.1) {
		step_acc = 0.0;
		std::uniform_real_distribution<float> pitch_dist(0.92f, 1.08f);
		play_step(_step_kind(), pitch_dist(rng));

		// 跑步/行走留痕：左右脚交替印痕
		if (FootprintManager::get_singleton() != nullptr) {
			Vector3 pos = get_global_position();
			float yaw = Math::atan2(facing.x, facing.z);
			Vector3 right_vec(-facing.z, 0.0f, facing.x);
			if (right_vec.length_squared() > 0.001f) {
				right_vec = right_vec.normalized();
			}
			float lat_off = foot_side ? 0.13f : -0.13f;
			Vector3 foot_pos = pos + right_vec * lat_off;
			FootprintManager::FootprintType ft = foot_side ? FootprintManager::TYPE_HUMAN_RIGHT : FootprintManager::TYPE_HUMAN_LEFT;
			FootprintManager::get_singleton()->spawn_footprint(ft, foot_pos, yaw, 1.0f);
			foot_side = !foot_side;
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
	// 1. 木屋室内地板：宅基地 (12,8) 半径 7m 内且脚底高于宅基平台（0.35+地板抬升）
	if (Vector2(gp.x - 12.0f, gp.z - 8.0f).length() < 7.0f && gp.y > 0.52f) {
		return 4;
	}
	// 2. 石拱桥面（硬质拱石，石板脆响）
	if (Math::abs(gp.z - 26.0f) < 2.3f && gp.x > -21.5f && gp.x < 9.5f) {
		return 3;
	}

	// 3. 天气覆盖（室外路面受雨雪覆盖）
	if (get_parent() != nullptr) {
		SkyCycle *sc = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
		if (sc != nullptr) {
			if (sc->get_snow_amount() > 0.35f) {
				return 1; // 真实雪地咯吱咯吱
			}
			if (sc->get_rain_amount() > 0.30f) {
				return 2; // 雨天轻微踩水声
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
		vol = -11.0f; // 雨天轻微踩水声（清亮水花，轻巧溅起）
	} else if (kind == 4) {
		snd = snd_wood;
		vol = -9.0f;  // 木地板咚咚
	} else if (kind == 3) {
		snd = snd_stone;
		vol = -10.5f; // 石板嗒嗒
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
