#include "fishing_system.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include "player.h"

using namespace godot;

void FishingSystem::_bind_methods() {
}

void FishingSystem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return;
			}
			build_ui();
			build_tackle();
			set_process(true);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return;
			}
			if (audio_player != nullptr) {
				audio_player->stop();
				audio_player->set_stream(Ref<AudioStream>());
			}
			snd_cast = Ref<AudioStream>();
			snd_bite = Ref<AudioStream>();
			snd_caught = Ref<AudioStream>();
			snd_escaped = Ref<AudioStream>();
		} break;

		case NOTIFICATION_PROCESS: {
			float delta = (float)get_process_delta_time();
			Input *input = Input::get_singleton();

			// 懒查找玩家
			if (player == nullptr) {
				Node *p = get_parent();
				if (p != nullptr) {
					player = Object::cast_to<Player>(p->get_node_or_null(NodePath("Player")));
				}
				if (player == nullptr) {
					return;
				}
			}

			bool f_down = input->is_key_pressed(KEY_F);
			bool f_edge = f_down && !f_prev;
			f_prev = f_down;
			bool hold = input->is_key_pressed(KEY_SPACE)
					|| input->is_mouse_button_pressed(MOUSE_BUTTON_LEFT);

			switch (state) {
				case 0: tick_idle(delta, f_edge); break;
				case 1: tick_waiting(delta, f_edge); break;
				case 2: tick_hooking(delta, hold); break;
				case 3: tick_leaping(delta); break;
				case 4: { // 逃跑冷却
					state_t -= delta;
					if (state_t <= 0.0f) {
						state = 0;
					}
				} break;
			}
		} break;

		default:
			break;
	}
}

// —— UI 搭建（全部代码创建，CJK 字体与 hud_clock/player 同一资源） ——
void FishingSystem::build_ui() {
	Ref<Font> cjk = ResourceLoader::get_singleton()->load(
			"res://assets/fonts/NotoSansSC-Regular.ttf");

	layer = memnew(CanvasLayer);
	layer->set_layer(7);
	add_child(layer);

	auto mk_label = [&](int p_size, const Color &p_color) -> Label * {
		Label *l = memnew(Label);
		l->add_theme_font_size_override("font_size", p_size);
		l->add_theme_color_override("font_color", p_color);
		l->add_theme_color_override("font_outline_color", Color(0.08f, 0.06f, 0.04f));
		l->add_theme_constant_override("outline_size", 7);
		l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		if (cjk.is_valid()) {
			l->add_theme_font_override("font", cjk);
		}
		layer->add_child(l);
		l->hide();
		return l;
	};

	// 底部操作提示（与玩家 HUD 提示错开一行）
	hint = mk_label(26, Color(1.0f, 0.95f, 0.7f));
	hint->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	hint->set_offset(SIDE_LEFT, -320.0f);
	hint->set_offset(SIDE_RIGHT, 320.0f);
	hint->set_offset(SIDE_TOP, -148.0f);
	hint->set_offset(SIDE_BOTTOM, -120.0f);

	// 中央结算
	result = mk_label(40, Color(1.0f, 0.88f, 0.35f));
	result->set_anchors_preset(Control::PRESET_CENTER);
	result->set_offset(SIDE_LEFT, -400.0f);
	result->set_offset(SIDE_RIGHT, 400.0f);
	result->set_offset(SIDE_TOP, -80.0f);
	result->set_offset(SIDE_BOTTOM, -30.0f);

	// 张力条（屏幕中下）：底槽 + 绿色目标区间 + 黄色张力游标 + 顶部进度细条
	bar_bg = memnew(ColorRect);
	bar_bg->set_color(Color(0.05f, 0.05f, 0.08f, 0.75f));
	bar_bg->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	bar_bg->set_offset(SIDE_LEFT, -180.0f);
	bar_bg->set_offset(SIDE_RIGHT, 180.0f);
	bar_bg->set_offset(SIDE_TOP, -66.0f);
	bar_bg->set_offset(SIDE_BOTTOM, -46.0f);
	layer->add_child(bar_bg);
	bar_bg->hide();

	bar_zone = memnew(ColorRect);
	bar_zone->set_color(Color(0.25f, 0.80f, 0.30f, 0.92f));
	bar_zone->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	bar_bg->get_parent()->add_child(bar_zone);
	bar_zone->hide();

	bar_cur = memnew(ColorRect);
	bar_cur->set_color(Color(1.0f, 0.85f, 0.20f, 0.98f));
	bar_cur->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	layer->add_child(bar_cur);
	bar_cur->hide();

	prog = memnew(ColorRect);
	prog->set_color(Color(0.45f, 0.85f, 1.0f, 0.95f));
	prog->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	layer->add_child(prog);
	prog->hide();

	// 音效播放器与音频资源载入
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr) {
		if (rl->exists("res://assets/audio/zen_wood_drop.mp3")) {
			Ref<Resource> r = rl->load("res://assets/audio/zen_wood_drop.mp3");
			if (r.is_valid()) {
				snd_cast = Object::cast_to<AudioStream>(r.ptr());
			}
		}
		if (rl->exists("res://assets/audio/zen_select.mp3")) {
			Ref<Resource> r = rl->load("res://assets/audio/zen_select.mp3");
			if (r.is_valid()) {
				snd_bite = Object::cast_to<AudioStream>(r.ptr());
			}
		}
		if (rl->exists("res://assets/audio/jingle_zen_bonus.mp3")) {
			Ref<Resource> r = rl->load("res://assets/audio/jingle_zen_bonus.mp3");
			if (r.is_valid()) {
				snd_caught = Object::cast_to<AudioStream>(r.ptr());
			}
		}
		if (rl->exists("res://assets/audio/zen_collapse.mp3")) {
			Ref<Resource> r = rl->load("res://assets/audio/zen_collapse.mp3");
			if (r.is_valid()) {
				snd_escaped = Object::cast_to<AudioStream>(r.ptr());
			}
		}
	}
	audio_player = memnew(AudioStreamPlayer);
	audio_player->set_name("FishingVoice");
	add_child(audio_player);
}

void FishingSystem::play_sound(const Ref<AudioStream> &p_stream, float p_volume_db) {
	if (audio_player != nullptr && p_stream.is_valid()) {
		audio_player->set_stream(p_stream);
		audio_player->set_volume_db(p_volume_db);
		audio_player->play();
	}
}

// —— 浮漂 + 鱼影模型（初始隐藏，抛竿时摆位显示） ——
void FishingSystem::build_tackle() {
	auto mat_of = [](const Color &c, float p_alpha = 1.0f) {
		Ref<StandardMaterial3D> m;
		m.instantiate();
		m->set_albedo(Color(c.r, c.g, c.b, p_alpha));
		m->set_roughness(0.7f);
		if (p_alpha < 1.0f) {
			m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		}
		return m;
	};
	auto ball = [](float r, float h) {
		Ref<SphereMesh> s;
		s.instantiate();
		s->set_radius(r);
		s->set_height(h);
		s->set_radial_segments(10);
		s->set_rings(6);
		return s;
	};

	// 浮漂：红白两截小球
	bobber = memnew(Node3D);
	add_child(bobber);
	{
		MeshInstance3D *top = memnew(MeshInstance3D);
		top->set_mesh(ball(0.09f, 0.16f));
		top->set_material_override(mat_of(Color(0.90f, 0.15f, 0.12f)));
		top->set_position(Vector3(0.0f, 0.10f, 0.0f));
		bobber->add_child(top);
		MeshInstance3D *bottom = memnew(MeshInstance3D);
		bottom->set_mesh(ball(0.07f, 0.12f));
		bottom->set_material_override(mat_of(Color(0.95f, 0.95f, 0.92f)));
		bobber->add_child(bottom);
	}
	bobber->hide();

	// 鱼影：扁梭形身体 + 三角尾（深色剪影，透过水面折射看）
	fish = memnew(Node3D);
	add_child(fish);
	{
		Ref<StandardMaterial3D> dark = mat_of(Color(0.10f, 0.16f, 0.28f, 0.85f), 0.85f);
		MeshInstance3D *body = memnew(MeshInstance3D);
		body->set_mesh(ball(0.30f, 0.55f));
		body->set_material_override(dark);
		body->set_scale(Vector3(0.55f, 0.45f, 1.0f)); // 侧扁的鱼身
		fish->add_child(body);
		MeshInstance3D *tail = memnew(MeshInstance3D);
		Ref<BoxMesh> tm;
		tm.instantiate();
		tm->set_size(Vector3(0.05f, 0.22f, 0.26f));
		tail->set_mesh(tm);
		tail->set_material_override(dark);
		tail->set_position(Vector3(0.0f, 0.0f, -0.34f));
		tail->set_rotation(Vector3(0.0f, 0.7853f, 0.0f));
		fish->add_child(tail);
	}
	fish->hide();
}

bool FishingSystem::check_near_shore(const Vector3 &p_pos) const {
	// 1. 石桥上垂钓：站在跨湖石拱桥上（沿 X 轴跨湖，桥面高出水面）
	bool on_bridge = (Math::abs(p_pos.z - 26.0f) < 2.5f && p_pos.x > -20.0f && p_pos.x < 8.0f && p_pos.y > 0.0f);
	if (on_bridge) {
		return true;
	}

	// 2. 站在水上荷叶上：脚底标高贴近水面（荷叶平台）
	Vector2 p(p_pos.x, p_pos.z);
	float d_nw = (p - Vector2(-14.0f, 66.0f)).length();
	float d_ne = (p - Vector2(16.0f, 56.0f)).length();
	float d_south = (p - Vector2(-15.0f, 8.0f)).length();
	float b_dx = (p_pos.x - (-6.0f)) / 13.0f;
	float b_dz = (p_pos.z - 26.0f) / 18.0f;
	float d_bridge = std::sqrt(b_dx * b_dx + b_dz * b_dz);

	if (p_pos.y > water_level - 0.15f && p_pos.y < water_level + 0.35f) {
		if (d_nw < 44.0f || d_ne < 32.0f || d_south < 22.0f || d_bridge < 0.95f) {
			return true;
		}
	}

	// 3. 湖岸边陆地：在水面以上的各段岸边带
	if (p_pos.y <= water_level) {
		return false;
	}
	// 西北大湖岸边 (44m，岸边触发带 42~52m)
	if (d_nw >= 42.0f && d_nw <= 52.0f) {
		return true;
	}
	// 东北阳光水域岸边 (32m，岸边触发带 30~40m)
	if (d_ne >= 30.0f && d_ne <= 40.0f) {
		return true;
	}
	// 南湖湾岸边 (22m，岸边触发带 20~29m)
	if (d_south >= 20.0f && d_south <= 29.0f) {
		return true;
	}
	// 石桥水峡岸边 (椭圆归一化距离 0.85~1.40)
	if (d_bridge >= 0.85f && d_bridge <= 1.40f) {
		return true;
	}

	return false;
}

void FishingSystem::set_result(const String &p_text, float p_seconds) {
	if (result != nullptr) {
		result->set_text(p_text);
		result->show();
	}
	state_t = p_seconds; // 复用 state_t 做结算展示计时
}

void FishingSystem::start_cast() {
	state = 1;
	play_sound(snd_cast, -8.0f);
	Vector3 pp = player->get_global_position();
	bool on_bridge = (Math::abs(pp.z - 26.0f) < 2.5f && pp.x > -20.0f && pp.x < 8.0f && pp.y > 0.0f);

	Vector3 facing = player->get_facing();
	Vector2 fdir(facing.x, facing.z);
	if (fdir.length_squared() > 0.001f) {
		fdir.normalize();
	} else {
		fdir = Vector2(0.0f, 1.0f);
	}

	if (on_bridge) {
		// 桥上垂钓：顺着玩家朝向往栏杆外甩出 4.8m，浮漂落在桥下水面
		bobber_pos = Vector3(pp.x + fdir.x * 4.8f, water_level + 0.02f, pp.z + fdir.y * 4.8f);
	} else {
		// 岸边或荷叶上垂钓：若玩家朝向大致朝向水面，则顺着玩家朝向抛竿，否则朝最近湖心抛
		Vector2 p(pp.x, pp.z);
		Vector2 c_nw(-14.0f, 66.0f);
		Vector2 c_ne(16.0f, 56.0f);
		Vector2 c_south(-15.0f, 8.0f);
		Vector2 c_mid(-6.0f, 26.0f);
		Vector2 to_c = c_nw - p;
		if ((c_ne - p).length_squared() < to_c.length_squared()) {
			to_c = c_ne - p;
		}
		if ((c_south - p).length_squared() < to_c.length_squared()) {
			to_c = c_south - p;
		}
		if ((c_mid - p).length_squared() < to_c.length_squared()) {
			to_c = c_mid - p;
		}
		Vector2 c_dir = to_c.normalized();
		Vector2 cast_dir = (fdir.dot(c_dir) > -0.2f) ? fdir : c_dir;
		bobber_pos = Vector3(pp.x + cast_dir.x * 5.2f, water_level + 0.02f, pp.z + cast_dir.y * 5.2f);
	}

	// 定鱼：30% 大鱼
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	big_fish = d01(rng) < 0.30f;
	zone_half = big_fish ? 0.10f : 0.16f;
	catch_need = big_fish ? 5.0f : 3.0f;
	escape_limit = big_fish ? 1.6f : 2.5f;
	// 鱼影大小按档位
	float fs = big_fish ? 1.5f : 0.8f;
	fish->set_scale(Vector3(fs, fs, fs));

	nibble_t = 2.0f + d01(rng) * 4.0f;
	swim_phase = d01(rng) * 6.28f;
	tension = 0.0f;
	catch_prog = 0.0f;
	escape_t = 0.0f;
	swim_walk = 0.0f;

	bobber->set_global_position(bobber_pos);
	bobber->show();
	fish->show();
	if (hint != nullptr) {
		hint->set_text(String::utf8("鱼影靠近浮漂中……（再按 F 收竿）"));
		hint->show();
	}
}

void FishingSystem::start_hook() {
	state = 2;
	play_sound(snd_bite, -7.0f);
	if (hint != nullptr) {
		hint->set_text(String::utf8("咬钩了！按住 空格/左键 控力，保持在绿区！"));
		hint->show();
	}
	bar_bg->show();
	bar_zone->show();
	bar_cur->show();
	prog->show();
}

void FishingSystem::finish_caught() {
	state = 3;
	state_t = 0.9f; // 跃出动画时长
	caught_count++;
	play_sound(snd_caught, -6.0f);
	bar_bg->hide();
	bar_zone->hide();
	bar_cur->hide();
	prog->hide();
	if (hint != nullptr) {
		hint->hide();
	}
	String name = big_fish ? String::utf8("大鱼") : String::utf8("小鱼");
	set_result(String::utf8("钓到一条") + name + String::utf8("！（累计 ") + String::num_int64(caught_count) + String::utf8(" 条）"), 0.9f);
}

void FishingSystem::finish_escaped() {
	cancel_fishing();
	state = 4;
	state_t = 1.5f; // 冷却
	play_sound(snd_escaped, -8.0f);
	set_result(String::utf8("鱼跑了……"), 1.2f);
}

void FishingSystem::cancel_fishing() {
	state = 0;
	if (bobber != nullptr) {
		bobber->hide();
	}
	if (fish != nullptr) {
		fish->hide();
	}
	if (hint != nullptr) {
		hint->hide();
	}
	if (bar_bg != nullptr) {
		bar_bg->hide();
		bar_zone->hide();
		bar_cur->hide();
		prog->hide();
	}
}

void FishingSystem::tick_idle(float p_delta, bool f_edge) {
	// 结算展示计时（如有）
	if (result != nullptr && result->is_visible()) {
		state_t -= p_delta;
		if (state_t <= 0.0f) {
			result->hide();
		}
	}

	Vector3 pp = player->get_global_position();
	near_shore = check_near_shore(pp);
	if (near_shore) {
		bool on_bridge = (Math::abs(pp.z - 26.0f) < 2.5f && pp.x > -20.0f && pp.x < 8.0f && pp.y > 0.0f);
		if (hint != nullptr) {
			if (on_bridge) {
				hint->set_text(String::utf8("按 F 桥上抛竿钓鱼"));
			} else {
				hint->set_text(String::utf8("按 F 抛竿钓鱼"));
			}
			if (!hint->is_visible()) {
				hint->show();
			}
		}
		if (f_edge) {
			start_cast();
		}
	} else if (hint != nullptr && hint->is_visible()) {
		hint->hide();
	}
}

void FishingSystem::tick_waiting(float p_delta, bool f_edge) {
	if (f_edge) { // 收竿
		cancel_fishing();
		return;
	}
	// 走开 3m 自动收竿
	Vector3 pp = player->get_global_position();
	if (pp.distance_to(bobber_pos) > 12.0f) {
		cancel_fishing();
		return;
	}

	// 鱼绕浮漂游：半径在 1.5~4.5m 之间"一会靠近一会远离"
	swim_phase += p_delta * 1.1f;
	float rad = 3.0f + std::sin(swim_phase * 0.7f) * 1.5f;
	float ang = swim_phase;
	Vector3 fp = bobber_pos + Vector3(std::cos(ang) * rad, -0.85f, std::sin(ang) * rad);
	fish->set_global_position(fp);
	fish->set_rotation(Vector3(0.0f, -ang, 0.0f)); // 头朝切线

	nibble_t -= p_delta;
	if (nibble_t <= 0.0f) {
		start_hook();
	}
}

void FishingSystem::tick_hooking(float p_delta, bool hold) {
	// 张力：按住升、松开落
	float rise = big_fish ? 1.05f : 0.85f;
	float fall = 0.70f;
	tension += (hold ? rise : -fall) * p_delta;
	tension = Math::clamp(tension, 0.0f, 1.0f);

	// 目标区间随鱼的挣扎漂移（随机游走 + 回中；大鱼更快更贼）
	float swim_speed = big_fish ? 2.6f : 1.4f;
	swim_walk += (((float)rng() / (float)rng.max()) - 0.5f) * swim_speed * p_delta * 2.2f;
	swim_walk *= 1.0f - 0.9f * p_delta; // 阻尼回中
	zone_center = Math::clamp(0.5f + swim_walk, zone_half + 0.02f, 1.0f - zone_half - 0.02f);

	// 判定
	bool in_zone = tension >= zone_center - zone_half && tension <= zone_center + zone_half;
	if (in_zone) {
		catch_prog += p_delta;
		escape_t = Math::max(escape_t - p_delta * 0.5f, 0.0f); // 区间内缓慢消减逃跑值
	} else {
		escape_t += p_delta;
	}

	// 鱼在控力中更暴躁：快速远近拉扯
	swim_phase += p_delta * (big_fish ? 2.4f : 1.7f);
	float rad = 2.6f + std::sin(swim_phase * 1.3f) * 1.8f;
	float ang = swim_phase * 0.9f;
	fish->set_global_position(bobber_pos + Vector3(std::cos(ang) * rad, -0.85f, std::sin(ang) * rad));
	fish->set_rotation(Vector3(0.0f, -ang, 0.0f));
	// 浮漂被拖得点头
	bobber->set_global_position(bobber_pos + Vector3(0.0f, std::sin(swim_phase * 3.0f) * 0.05f - 0.06f, 0.0f));

	// UI 刷新（区间条 + 游标 + 进度）
	float bar_l = -180.0f, bar_w = 360.0f;
	bar_zone->set_offset(SIDE_LEFT, bar_l + (zone_center - zone_half) * bar_w);
	bar_zone->set_offset(SIDE_RIGHT, bar_l + (zone_center + zone_half) * bar_w);
	bar_zone->set_offset(SIDE_TOP, -66.0f);
	bar_zone->set_offset(SIDE_BOTTOM, -46.0f);
	bar_cur->set_offset(SIDE_LEFT, bar_l + tension * bar_w - 4.0f);
	bar_cur->set_offset(SIDE_RIGHT, bar_l + tension * bar_w + 4.0f);
	bar_cur->set_offset(SIDE_TOP, -70.0f);
	bar_cur->set_offset(SIDE_BOTTOM, -42.0f);
	prog->set_offset(SIDE_LEFT, bar_l);
	prog->set_offset(SIDE_RIGHT, bar_l + (catch_prog / catch_need) * bar_w);
	prog->set_offset(SIDE_TOP, -78.0f);
	prog->set_offset(SIDE_BOTTOM, -72.0f);

	if (catch_prog >= catch_need) {
		finish_caught();
	} else if (escape_t >= escape_limit) {
		finish_escaped();
	}
}

void FishingSystem::tick_leaping(float p_delta) {
	// 鱼跃出水面划弧飞向玩家（0.9s），到点消失回待机
	state_t -= p_delta;
	float t = 1.0f - Math::clamp(state_t / 0.9f, 0.0f, 1.0f); // 0..1
	Vector3 pp = player->get_global_position() + Vector3(0.0f, 1.0f, 0.0f);
	Vector3 from = bobber_pos;
	Vector3 pos = from.lerp(pp, t);
	pos.y += std::sin(t * 3.14159f) * 2.2f; // 弧线
	fish->set_global_position(pos);
	fish->set_rotation(Vector3(t * 9.4f, 0.0f, 0.0f)); // 翻滚
	if (state_t <= 0.0f) {
		cancel_fishing();
		// 结算文字再挂 1.6s
		if (result != nullptr && result->is_visible()) {
			state_t = 1.6f;
		}
	}
}
