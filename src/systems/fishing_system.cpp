#include "fishing_system.h"

#include "ui_style.h"
#include "sky_cycle.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include "player.h"
#include "terrain_builder.h"

using namespace godot;

void FishingSystem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_state"), &FishingSystem::get_state);
	ClassDB::bind_method(D_METHOD("is_charging"), &FishingSystem::is_charging);
	ClassDB::bind_method(D_METHOD("is_near_shore"), &FishingSystem::is_near_shore);
	ClassDB::bind_method(D_METHOD("get_caught_count"), &FishingSystem::get_caught_count);

	ADD_SIGNAL(MethodInfo("fish_caught")); // 每钓上一条时发出（GameDirector 每日待办计数用）
}

void FishingSystem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return;
			}
			build_ui();
			build_tackle();
			load_records(); // 图鉴纪录（user://fishing_records.cfg）
			set_process(true);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return;
			}
			save_records(); // 纪录兜底存档（catch 时已随钓随存）
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

			// —— B 键鱼获图鉴（钓鱼中不响应，避免误触）——
			bool b_down = input != nullptr && input->is_key_pressed(KEY_B);
			if (b_down && !b_prev && state != 2) {
				book_open = !book_open;
				if (book_open && book_layer == nullptr) {
					build_book_ui();
				}
				if (book_layer != nullptr) {
					if (book_open) {
						refresh_book();
					}
					book_layer->set_visible(book_open);
				}
			}
			b_prev = b_down;

			// 懒查找玩家
			if (player == nullptr) {
				Node *p = get_parent();
				if (p != nullptr) {
					player = Object::cast_to<Player>(p->get_node_or_null(NodePath("Player")));
					terrain = Object::cast_to<TerrainBuilder>(p->get_node_or_null(NodePath("Terrain")));
					sky = Object::cast_to<SkyCycle>(p->get_node_or_null(NodePath("SkyCycle")));
				}
				if (player == nullptr) {
					return;
				}
			}

			bool f_down = input->is_key_pressed(KEY_F);
			bool f_edge = f_down && !f_prev;    // 按下（等待阶段用来收竿）
			bool f_release = !f_down && f_prev; // 松手 = 蓄力结束、抛出去
			f_prev = f_down;
			// 搏斗阶段：彻底去除空格键，仅保留鼠标左键控力
			bool hold = input->is_mouse_button_pressed(MOUSE_BUTTON_LEFT);

			if (!f_down) {
				f_consumed = false; // 物理松开 F 键后，才解除消耗限制
			}
			if (cast_cooldown > 0.0f) {
				cast_cooldown -= delta;
			}

			switch (state) {
				case 0: tick_idle(delta, f_down, f_release); break;
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

			// 鱼竿与鱼线每帧跟随玩家（摆在状态机之后：抛竿那一帧就能看到竿挥出去）
			update_rod();
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
		// 统一木质底板（与顶部木牌/木屋同一材质语言）
		l->add_theme_stylebox_override("normal", uistyle::make_wood_textured(
				Math::max(10.0f, p_size * 0.45f), 0.90f));
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

	// 张力条（屏幕中下）：玻璃托槽 + 绿色目标区间 + 黄色张力游标 + 顶部进度细条
	// 托槽用统一「湖畔玻璃」Panel（圆角+发丝边），绿区/游标仍是叠放的 ColorRect
	bar_bg = memnew(Panel);
	bar_bg->add_theme_stylebox_override("panel", uistyle::make_wood_textured(8.0f, 0.90f));
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

	// 蓄力条（张力条上面一行）：按住 F 时从 0 涨到满，松手抛出去。
	// 用暖黄→橙的填充色，和张力条（绿区/黄游标）区分开。托槽同款玻璃 Panel。
	charge_bg = memnew(Panel);
	charge_bg->add_theme_stylebox_override("panel", uistyle::make_wood_textured(8.0f, 0.90f));
	charge_bg->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	charge_bg->set_offset(SIDE_LEFT, -180.0f);
	charge_bg->set_offset(SIDE_RIGHT, 180.0f);
	charge_bg->set_offset(SIDE_TOP, -104.0f);
	charge_bg->set_offset(SIDE_BOTTOM, -92.0f);
	layer->add_child(charge_bg);
	charge_bg->hide();

	charge_fill = memnew(ColorRect);
	charge_fill->set_color(Color(1.0f, 0.72f, 0.22f, 0.95f));
	charge_fill->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	charge_fill->set_offset(SIDE_LEFT, -180.0f);
	charge_fill->set_offset(SIDE_RIGHT, -180.0f);
	charge_fill->set_offset(SIDE_TOP, -104.0f);
	charge_fill->set_offset(SIDE_BOTTOM, -92.0f);
	layer->add_child(charge_fill);
	charge_fill->hide();

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
	audio_player->set_bus(StringName("SFX"));
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

	// —— 鱼竿：细长锥形竿身 + 深色握把 + 金色卷线器 ——
	// 竿沿本地 -Z 摆放（这样 look_at 指向哪、竿就指哪）
	const float ROD_LEN = 2.45f;
	rod_pivot = memnew(Node3D);
	rod_pivot->set_name("FishingRod");
	add_child(rod_pivot);
	{
		Ref<CylinderMesh> rm;
		rm.instantiate();
		rm->set_top_radius(0.008f);
		rm->set_bottom_radius(0.026f);
		rm->set_height(ROD_LEN);
		rm->set_radial_segments(7);
		rm->set_rings(1);
		MeshInstance3D *rod = memnew(MeshInstance3D);
		rod->set_mesh(rm);
		rod->set_material_override(mat_of(Color(0.36f, 0.21f, 0.11f)));
		rod->set_rotation(Vector3(-Math::PI * 0.5f, 0.0f, 0.0f)); // 竿轴 Y → 本地 -Z
		rod->set_position(Vector3(0.0f, 0.0f, -ROD_LEN * 0.5f));  // 竿根落在枢轴上
		rod_pivot->add_child(rod);

		Ref<CylinderMesh> gm;
		gm.instantiate();
		gm->set_top_radius(0.030f);
		gm->set_bottom_radius(0.033f);
		gm->set_height(0.26f);
		gm->set_radial_segments(8);
		MeshInstance3D *grip = memnew(MeshInstance3D);
		grip->set_mesh(gm);
		grip->set_material_override(mat_of(Color(0.15f, 0.11f, 0.09f)));
		grip->set_rotation(Vector3(-Math::PI * 0.5f, 0.0f, 0.0f));
		grip->set_position(Vector3(0.0f, 0.0f, -0.13f));
		rod_pivot->add_child(grip);

		Ref<CylinderMesh> rlm;
		rlm.instantiate();
		rlm->set_top_radius(0.042f);
		rlm->set_bottom_radius(0.042f);
		rlm->set_height(0.05f);
		rlm->set_radial_segments(10);
		MeshInstance3D *reel = memnew(MeshInstance3D);
		reel->set_mesh(rlm);
		reel->set_material_override(mat_of(Color(0.78f, 0.74f, 0.32f)));
		reel->set_rotation(Vector3(Math::PI * 0.5f, 0.0f, 0.0f));
		reel->set_position(Vector3(0.0f, -0.055f, -0.30f));
		rod_pivot->add_child(reel);
	}
	rod_pivot->hide();

	// —— 鱼线：每帧重建的、始终面向相机的细薄带（Godot 的 LINE 只有 1px，太细看不见）——
	// 顶点是世界坐标（update_rod 里每帧重建），所以本节点必须挂在场景树里、
	// 且全局变换保持单位阵。**别忘了 add_child** —— 漏了它线就永远不渲染，
	// 而且 get_node_or_null("FishingLine") 也找不到（上一版就是漏了这一行）。
	line = memnew(MeshInstance3D);
	line->set_name("FishingLine");
	add_child(line);
	{
		Ref<ImmediateMesh> im;
		im.instantiate();
		line->set_mesh(im);
		Ref<StandardMaterial3D> lm;
		lm.instantiate();
		lm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED); // 鱼线是亮的细丝，不吃光照
		lm->set_albedo(Color(0.92f, 0.95f, 1.0f, 0.88f));
		lm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		// **必须关背面剔除**。这条带子的绕序是反的（侧向量取的是 seg×to_cam，
		// 出来的面法线朝相机，但 Godot 判正面用的是另一套绕序），默认 CULL_BACK
		// 会把整条线剔掉 —— 表现就是"节点在、AABB 也在、就是什么都看不到"。
		// 薄带子双面渲染没有额外代价，直接关掉最省事。
		lm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		line->set_material_override(lm);
	}
	line->hide();
}

// 竿梢的世界坐标（本地 -Z 方向 ROD_LEN 处）
Vector3 FishingSystem::rod_tip_world() const {
	if (rod_pivot == nullptr) {
		return Vector3();
	}
	return rod_pivot->get_global_transform().xform(Vector3(0.0f, 0.0f, -2.45f));
}

// 每帧摆竿 + 拉线：竿跟着玩家手走，朝向在"举着"和"指着浮漂"之间切换；
// 线从竿梢连到浮漂，中间按距离给一点下垂。
void FishingSystem::update_rod() {
	if (rod_pivot == nullptr || player == nullptr) {
		return;
	}
	// 举竿条件：正在钓鱼（state != 0）或者正在按 F 蓄力。
	// **不再"靠近水就举竿"** —— 用户明确要求：站在岸边只是提示，按 F 才举竿。
	// 松手抛竿那一帧 tick_idle 已经把 state 置成 1，所以不会有"竿闪一下又消失"。
	bool show = (state != 0) || charging;
	if (!show) {
		if (rod_pivot->is_visible()) {
			rod_pivot->hide();
			line->hide();
		}
		return;
	}
	if (!rod_pivot->is_visible()) {
		rod_pivot->show();
	}

	Vector3 fwd = player->get_facing();
	fwd.y = 0.0f;
	if (fwd.length_squared() < 0.0001f) {
		fwd = Vector3(0.0f, 0.0f, -1.0f);
	}
	fwd = fwd.normalized();
	Vector3 right = fwd.cross(Vector3(0.0f, 1.0f, 0.0f));
	if (right.length_squared() < 0.0001f) {
		right = Vector3(1.0f, 0.0f, 0.0f);
	}
	right = right.normalized();

	// 握竿手：身前右侧、胸口略高
	Vector3 hand = player->get_global_position() + Vector3(0.0f, 1.24f, 0.0f)
			+ right * 0.30f + fwd * 0.20f;

	// 竿的姿态：**始终斜举在身前**，不去"指着浮漂"。
	// 上一版让竿梢瞄浮漂，结果浮漂一旦落在手边或水面比手低很多，
	// 竿就变成朝下戳地 —— 真人钓鱼也不会把竿指着漂，竿的作用是把线架高。
	// 线自己从竿梢垂到浮漂，那才是钓鱼该有的样子。
	float up = 0.86f;
	float fw = 0.54f;

	if (state == 1) {
		// 待机守候：竿梢随着微风与呼吸轻盈起伏
		float breath = std::sin(swim_phase * 1.6f) * 0.04f;
		up += breath;
		fw -= breath * 0.4f;
	} else if (state == 2) {
		// 咬钩搏斗：竿身弓起后仰，高频剧烈震颤，伴随收线起伏韵律与侧向拽拉！
		float struggle_vibe = std::sin(swim_phase * 24.0f) * 0.08f * (big_fish ? 1.6f : 1.0f);
		float reel_pump = std::sin(catch_prog * 8.0f) * 0.14f;
		float tension_pull = tension * 0.32f;
		up = 1.08f + struggle_vibe + reel_pump + tension_pull;
		fw = 0.50f - tension_pull * 0.40f;
	}

	if (charging) {
		// 蓄力：大幅向后仰起积蓄力量
		up = 0.86f + charge * 0.55f;
		fw = 0.54f - charge * 0.42f;
	}
	if (cast_swing > 0.0f) {
		// 抛竿甩击：强力向前下压抽击，随后柔和回弹
		float whip = std::sin(cast_swing * 3.14159f);
		fw += whip * 0.75f;
		up -= whip * 0.32f;
	}

	Vector3 dir = (fwd * fw + Vector3(0.0f, 1.0f, 0.0f) * up);
	if (state == 2 && fish != nullptr) {
		// 竿梢偏向大鱼挣扎拉扯的方位
		Vector3 to_fish = (fish->get_global_position() - hand).normalized();
		float side_pull = right.dot(to_fish) * 0.28f;
		dir += right * side_pull;
	}
	dir = dir.normalized();
	if (dir.length_squared() < 0.0001f) {
		dir = Vector3(0.0f, 1.0f, 0.0f);
	}
	dir = dir.normalized();
	// 兜底：无论上面怎么算，竿朝下的角度都不许超过约 13°。
	// 竿长 2.45m，再往下压梢部就贴地/入水了。
	const float MIN_DIR_Y = -0.22f;
	if (dir.y < MIN_DIR_Y) {
		dir.y = MIN_DIR_Y;
		dir = dir.normalized();
	}
	if (dir.y > 0.92f) { // 太竖直会让 look_at 退化，观感也不对
		dir.y = 0.92f;
		dir = dir.normalized();
	}
	rod_pivot->look_at_from_position(hand, hand + dir, Vector3(0.0f, 1.0f, 0.0f));

	if (cast_swing > 0.0f) {
		cast_swing = Math::max(cast_swing - (float)get_process_delta_time() * 2.2f, 0.0f);
	}

	// —— 鱼线：竿梢 → 浮漂，10 段、面向相机的薄带 ——
	if (state == 0 || bobber == nullptr || !bobber->is_visible()) {
		line->hide();
		return;
	}
	line->show();
	line->set_global_transform(Transform3D());

	Vector3 tip = rod_tip_world();
	Vector3 bpos = bobber->get_global_position();
	float dist = tip.distance_to(bpos);
	float sag = Math::min(dist * 0.045f, 0.32f); // 线自重下垂（拉紧时几乎是一条直线）

	Camera3D *cam = get_viewport() != nullptr ? get_viewport()->get_camera_3d() : nullptr;
	Vector3 cam_pos = cam != nullptr ? cam->get_global_position() : tip + Vector3(0.0f, 6.0f, 0.0f);

	Ref<ImmediateMesh> im = line->get_mesh();
	im->clear_surfaces();
	im->surface_begin(Mesh::PRIMITIVE_TRIANGLE_STRIP);
	const int SEGS = 10;
	const float HALF_W = 0.017f; // 细丝，但得看得见：0.012 在几米外只有两三个像素，玩家根本找不着
	Vector3 prev = tip;
	for (int i = 0; i <= SEGS; i++) {
		float t = (float)i / (float)SEGS;
		Vector3 p = tip.lerp(bpos, t);
		p.y -= std::sin(t * (float)Math::PI) * sag;
		Vector3 nxt = (i < SEGS) ? tip.lerp(bpos, (float)(i + 1) / (float)SEGS) : p;
		Vector3 seg = nxt - prev;
		if (seg.length_squared() < 1e-9f) {
			seg = bpos - tip;
		}
		if (seg.length_squared() < 1e-9f) {
			seg = Vector3(0.0f, -1.0f, 0.0f);
		}
		seg = seg.normalized();
		Vector3 to_cam = cam_pos - p;
		if (to_cam.length_squared() < 1e-6f) {
			to_cam = Vector3(0.0f, 0.0f, 1.0f);
		}
		to_cam = to_cam.normalized();
		Vector3 side = seg.cross(to_cam);
		if (side.length_squared() < 1e-8f) {
			side = Vector3(1.0f, 0.0f, 0.0f);
		}
		side = side.normalized() * HALF_W;
		im->surface_set_normal(Vector3(0.0f, 1.0f, 0.0f));
		im->surface_add_vertex(p - side);
		im->surface_add_vertex(p + side);
		prev = p;
	}
	im->surface_end();
}

bool FishingSystem::check_near_shore(const Vector3 &p_pos) const {
	// 1. 石桥上垂钓：站在跨湖石拱桥上（沿 X 轴跨湖，桥面高出水面）
	bool on_bridge = (Math::abs(p_pos.z - 26.0f) < 2.5f && p_pos.x > -20.0f && p_pos.x < 8.0f && p_pos.y > 0.0f);
	if (on_bridge) {
		return true;
	}

	// 2. 有高度场就用真实水边判定。
	//
	// **为什么不再用写死的湖心距离**：湖盆是 heightfield 挖出来的
	// （basin = 1 - sstepf(r*0.5, r*1.02, d)，再叠 ±6.5m 的岸线域扭曲），
	// 水面（terrain_height == water_level）实际落在 d ≈ 0.6r 附近，
	// 西北湖大约 36m —— 而老判定带写的是 42~52m。也就是说玩家站在离水
	// 6~16m 的草地上就被判定成"可以钓鱼"，抛出去 5.2m 当然还在草地上。
	// 现在直接问地形：脚边 6m 内有没有"水深够下竿"的地方。
	if (terrain != nullptr) {
		// 站在深水里不能钓；脚底贴着水面（荷叶/浅滩）算例外
		if (terrain->water_depth_at(p_pos.x, p_pos.z) > 0.25f
				&& Math::abs(p_pos.y - water_level) > 0.35f) {
			return false;
		}
		const float NEED_DEPTH = 0.6f;  // 能下竿的最小水深
		const float REACH = 6.0f;       // 站得离水边不超过这么远
		for (int ring = 1; ring <= 6; ring++) {
			float r = (float)ring;
			for (int k = 0; k < 16; k++) {
				float a = (float)k * 0.3926991f; // 2π / 16
				if (terrain->water_depth_at(p_pos.x + std::cos(a) * r,
							p_pos.z + std::sin(a) * r) >= NEED_DEPTH) {
					return true;
				}
			}
		}
		return false;
	}

	// 3. 拿不到地形时的退化路径：老的固定半径判定
	if (p_pos.y <= water_level) {
		return false;
	}
	float d_nw = (Vector2(p_pos.x, p_pos.z) - Vector2(-14.0f, 66.0f)).length();
	float d_ne = (Vector2(p_pos.x, p_pos.z) - Vector2(16.0f, 56.0f)).length();
	float d_south = (Vector2(p_pos.x, p_pos.z) - Vector2(-15.0f, 8.0f)).length();
	float b_dx = (p_pos.x - (-6.0f)) / 13.0f;
	float b_dz = (p_pos.z - 26.0f) / 18.0f;
	float d_bridge = std::sqrt(b_dx * b_dx + b_dz * b_dz);
	if (d_nw >= 34.0f && d_nw <= 46.0f) {
		return true;
	}
	if (d_ne >= 24.0f && d_ne <= 36.0f) {
		return true;
	}
	if (d_south >= 16.0f && d_south <= 27.0f) {
		return true;
	}
	if (d_bridge >= 0.70f && d_bridge <= 1.30f) {
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

void FishingSystem::start_cast(float p_charge) {
	state = 1;
	cast_swing = 1.0f; // 起手一记前挥
	play_sound(snd_cast, -8.0f);
	Vector3 pp = player->get_global_position();
	cast_origin = pp;
	bool on_bridge = (Math::abs(pp.z - 26.0f) < 2.5f && pp.x > -20.0f && pp.x < 8.0f && pp.y > 0.0f);

	Vector3 facing = player->get_facing();
	Vector2 fdir(facing.x, facing.z);
	if (fdir.length_squared() > 0.001f) {
		fdir.normalize();
	} else {
		fdir = Vector2(0.0f, 1.0f);
	}

	// 蓄力 → 抛竿距离。轻点一下抛得近，蓄满抛到最远。
	// 之前是固定 5.2m —— 玩家侧着站、或者岸边比想象的宽，浮漂就落在草地上了。
	const float CAST_MIN = 3.2f;
	const float CAST_MAX = 13.0f;
	float cast_d = CAST_MIN + (CAST_MAX - CAST_MIN) * Math::clamp(p_charge, 0.0f, 1.0f);

	// 候选方向：玩家朝向优先，其次往"最近湖心"方向并拢。
	// 每个方向都沿射线找第一个真在水里的落点，谁先成功用谁。
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
	Vector2 c_dir = to_c.length_squared() > 1e-4f ? to_c.normalized() : Vector2(0.0f, -1.0f);

	Vector2 target;
	bool found = false;
	if (on_bridge) {
		// 桥上：往栏杆外（玩家朝向）为主，水面就在桥下
		float bd = Math::min(cast_d, 8.0f);
		found = find_water_spot(p, fdir, bd, 1.2f, target);
		if (!found) {
			found = find_water_spot(p, c_dir, bd, 1.2f, target);
		}
	} else {
		// 严密扇面扫描：以玩家朝向与最近湖心为基准展开扇面采样
		Vector2 dirs[12];
		dirs[0] = fdir;
		dirs[1] = (fdir * 0.7f + c_dir * 0.3f);
		dirs[2] = (fdir * 0.4f + c_dir * 0.6f);
		dirs[3] = c_dir;
		dirs[4] = fdir.rotated(0.35f);
		dirs[5] = fdir.rotated(-0.35f);
		dirs[6] = c_dir.rotated(0.35f);
		dirs[7] = c_dir.rotated(-0.35f);
		dirs[8] = fdir.rotated(0.70f);
		dirs[9] = fdir.rotated(-0.70f);
		dirs[10] = c_dir.rotated(0.70f);
		dirs[11] = c_dir.rotated(-0.70f);
		for (int i = 0; i < 12 && !found; i++) {
			if (dirs[i].length_squared() < 1e-6f) {
				continue;
			}
			found = find_water_spot(p, dirs[i].normalized(), cast_d, 1.8f, target);
		}
	}

	// 严禁抛到陆地！如果各个方向都没找到足够水深的水域，绝不盲目抛向地面！
	if (!found) {
		state = 0; // 取消抛竿，退回等待
		charging = false;
		charge = 0.0f;
		cast_cooldown = 0.35f;
		if (hint != nullptr) {
			hint->set_text(String::utf8("⚠️ 此处无法下竿，请面朝水深足够的水面！"));
			hint->show();
		}
		return;
	}

	// 最终双重硬核校验：确保目标点水深绝对大于 0.45m（绝对在水面之下）
	if (terrain != nullptr && terrain->water_depth_at(target.x, target.y) < 0.45f) {
		state = 0;
		charging = false;
		charge = 0.0f;
		cast_cooldown = 0.35f;
		if (hint != nullptr) {
			hint->set_text(String::utf8("⚠️ 目标水域过浅，请重新对准湖水！"));
			hint->show();
		}
		return;
	}

	bobber_pos = Vector3(target.x, water_level + 0.05f, target.y);

	// 定鱼：按昼夜定种（金鳞 6% / 昼青鳞红尾 / 夜月影夜鳗），体型档随种
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	bool night_now = (sky != nullptr) ? (sky->get_night_factor() > 0.5f) : false;
	cur_species = pick_species(night_now);
	// 金鳞与大鱼走"难拉"档（金鳞必难拉；昼红尾鲤/夜鳗 40% 大型化）
	big_fish = (cur_species == FISH_JINLIN)
			|| ((cur_species == FISH_HONGWEI || cur_species == FISH_YEMAN) && d01(rng) < 0.40f);
	zone_half = big_fish ? 0.10f : 0.16f;
	catch_need = big_fish ? 5.0f : 3.0f;
	escape_limit = big_fish ? 1.6f : 2.5f;
	// 体长在咬钩时就定死：大型 48~78cm / 常规 16~34cm；金鳞 40~65cm 修长
	if (cur_species == FISH_JINLIN) {
		cur_size_cm = 40.0f + d01(rng) * 25.0f;
	} else {
		cur_size_cm = big_fish ? (48.0f + d01(rng) * 30.0f) : (16.0f + d01(rng) * 18.0f);
	}
	// 鱼影大小按档位 + 按种着色（水下的第一辨识度：红鲤暗紫红/月影冷蓝/夜鳗墨绿/金鳞暗金）
	float fs = big_fish ? 1.5f : 0.8f;
	if (cur_species == FISH_JINLIN) {
		fs = 1.25f; // 金鳞修长居中
	}
	fish->set_scale(Vector3(fs, fs, fs));
	for (int ci = 0; ci < fish->get_child_count(); ci++) {
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(fish->get_child(ci))) {
			Ref<StandardMaterial3D> m = Object::cast_to<StandardMaterial3D>(mi->get_material_override().ptr());
			if (m.is_valid()) {
				m->set_albedo(species_tint(cur_species));
			}
		}
	}

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
		hint->set_text(String::utf8("咬钩了！按住 鼠标左键 控力，保持在绿区！"));
		hint->show();
	}
	bar_bg->show();
	bar_zone->show();
	bar_cur->show();
	prog->show();
}

// —— 鱼种定义 ——
const char *FishingSystem::species_name(int p_species) {
	switch (p_species) {
		case FISH_HONGWEI:
			return "红尾鲤";
		case FISH_YUEYING:
			return "月影鲈";
		case FISH_YEMAN:
			return "夜鳗";
		case FISH_JINLIN:
			return "金鳞鱼";
		case FISH_QINGLIN:
		default:
			return "青鳞鱼";
	}
}

bool FishingSystem::species_nocturnal(int p_species) {
	return p_species == FISH_YUEYING || p_species == FISH_YEMAN;
}

Color FishingSystem::species_tint(int p_species) {
	switch (p_species) {
		case FISH_HONGWEI:
			return Color(0.42f, 0.16f, 0.24f, 0.88f); // 红鲤暗紫红
		case FISH_YUEYING:
			return Color(0.20f, 0.28f, 0.52f, 0.82f); // 月影冷蓝
		case FISH_YEMAN:
			return Color(0.14f, 0.22f, 0.20f, 0.92f); // 夜鳗墨绿
		case FISH_JINLIN:
			return Color(0.62f, 0.50f, 0.14f, 0.95f); // 金鳞暗金
		case FISH_QINGLIN:
		default:
			return Color(0.10f, 0.16f, 0.28f, 0.85f); // 青鳞原剪影蓝
	}
}

// 按昼夜定种：昼池 青鳞62/红尾32/金鳞6，夜池 月影60/夜鳗34/金鳞6。
// 金鳞两池同权——夜里钓到也别惊讶，月下金鳞是这湖的名场面。
int FishingSystem::pick_species(bool p_night) {
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float roll = d01(rng);
	if (roll < 0.06f) {
		return FISH_JINLIN;
	}
	if (p_night) {
		return (d01(rng) < 0.638f) ? FISH_YUEYING : FISH_YEMAN;
	}
	return (d01(rng) < 0.66f) ? FISH_QINGLIN : FISH_HONGWEI;
}

void FishingSystem::finish_caught() {	state = 3;
	state_t = 0.9f; // 跃出动画时长
	caught_count++;
	emit_signal("fish_caught"); // 通知每日待办计分（GameDirector）
	play_sound(snd_caught, -6.0f);
	bar_bg->hide();
	bar_zone->hide();
	bar_cur->hide();
	prog->hide();
	if (hint != nullptr) {
		hint->hide();
	}
	// —— 图鉴入账：按鱼种累计 + 破纪录检测 ——
	bool record = cur_size_cm > rec_best[cur_species];
	rec_count[cur_species]++;
	if (record) {
		rec_best[cur_species] = cur_size_cm;
	}
	save_records();
	if (book_open) {
		refresh_book();
	}
	String text = String::utf8("钓到一条") + String::utf8(species_name(cur_species))
			+ String::utf8(" ") + String::num(cur_size_cm, 1) + String::utf8("cm！（累计 ")
			+ String::num_int64(caught_count) + String::utf8(" 条）");
	if (record) {
		text += String::utf8(" 🏅新纪录！");
	}
	set_result(text, 0.9f);
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
	charging = false;
	charge = 0.0f;
	f_consumed = true;     // 关键：收竿是由按下 F 触发的，标记已消耗，物理松手前绝不进入蓄力
	cast_cooldown = 0.25f; // 短暂保护冷却，防止连击
	if (bobber != nullptr) {
		bobber->hide();
	}
	if (fish != nullptr) {
		fish->hide();
	}
	if (line != nullptr) {
		line->hide(); // 收竿就把线收了
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
	if (charge_bg != nullptr) {
		charge_bg->hide();
		charge_fill->hide();
	}
}

void FishingSystem::tick_idle(float p_delta, bool f_down, bool f_release) {
	// 结算展示计时（如有）
	if (result != nullptr && result->is_visible()) {
		state_t -= p_delta;
		if (state_t <= 0.0f) {
			result->hide();
		}
	}

	Vector3 pp = player->get_global_position();
	near_shore = check_near_shore(pp);

	// 离开岸边：蓄力作废、提示收起
	if (!near_shore) {
		if (hint != nullptr && hint->is_visible()) {
			hint->hide();
		}
		if (charge_bg != nullptr && charge_bg->is_visible()) {
			charge_bg->hide();
			charge_fill->hide();
		}
		charging = false;
		charge = 0.0f;
		return;
	}

	bool on_bridge = (Math::abs(pp.z - 26.0f) < 2.5f && pp.x > -20.0f && pp.x < 8.0f && pp.y > 0.0f);

	// —— 蓄力：按住 F 涨条，松手抛竿 ——
	// 只有当按键未被收竿消耗 (f_consumed == false) 且不在冷却期内，才允许蓄力
	if (f_down && !f_consumed && cast_cooldown <= 0.0f) {
		if (!charging) {
			charging = true;
			charge = 0.0f;
		}
		charge = Math::min(charge + p_delta / CHARGE_TIME, 1.0f);
		if (hint != nullptr) {
			// 满蓄力给个明确反馈，否则玩家不知道还能不能更远
			if (charge >= 0.999f) {
				hint->set_text(String::utf8("蓄满力！松手抛竿"));
			} else {
				hint->set_text(String::utf8("蓄力中……松手抛竿"));
			}
			if (!hint->is_visible()) {
				hint->show();
			}
		}
		if (charge_bg != nullptr) {
			const float bar_l = -180.0f, bar_w = 360.0f;
			charge_bg->show();
			charge_fill->show();
			charge_fill->set_offset(SIDE_LEFT, bar_l);
			charge_fill->set_offset(SIDE_RIGHT, bar_l + charge * bar_w);
			charge_fill->set_offset(SIDE_TOP, -104.0f);
			charge_fill->set_offset(SIDE_BOTTOM, -92.0f);
		}
		return;
	}

	if (f_release) {
		if (charging && !f_consumed && cast_cooldown <= 0.0f) {
			charging = false;
			float c = charge;
			charge = 0.0f;
			if (charge_bg != nullptr) {
				charge_bg->hide();
				charge_fill->hide();
			}
			start_cast(c);
			return;
		}
		charging = false;
		charge = 0.0f;
		if (charge_bg != nullptr && charge_bg->is_visible()) {
			charge_bg->hide();
			charge_fill->hide();
		}
	}

	// 待机提示
	if (hint != nullptr) {
		if (on_bridge) {
			hint->set_text(String::utf8("长按 F 蓄力抛钩 (松开抛出)"));
		} else {
			hint->set_text(String::utf8("长按 F 蓄力抛钩 (松开抛出)"));
		}
		if (!hint->is_visible()) {
			hint->show();
		}
	}
	if (charge_bg != nullptr && charge_bg->is_visible()) {
		charge_bg->hide();
		charge_fill->hide();
	}
}

// 沿 p_dir 从 p_max_d 往回扫，返回第一个"水深够"的落点。
// 从远往近扫是故意的：玩家蓄了多少力就想到多远，落在最远处才算兑现；
// 只有远处是岸、近处才有水时才往前收。
bool FishingSystem::find_water_spot(const Vector2 &p_from, const Vector2 &p_dir,
		float p_max_d, float p_min_d, Vector2 &r_out) const {
	if (terrain == nullptr || p_dir.length_squared() < 1e-6f) {
		return false;
	}
	const float MIN_DEPTH = 0.85f; // 浮漂要真浮起来，鱼影还得在水面下 0.85m
	Vector2 d = p_dir.normalized();
	for (float t = p_max_d; t >= p_min_d; t -= 0.3f) {
		Vector2 q = p_from + d * t;
		if (terrain->water_depth_at(q.x, q.y) >= MIN_DEPTH) {
			r_out = q;
			return true;
		}
	}
	return false;
}

void FishingSystem::tick_waiting(float p_delta, bool f_edge) {
	if (f_edge) { // 收竿
		cancel_fishing();
		return;
	}
	// 走开就自动收竿（相对抛竿点算，不是相对浮漂 —— 蓄满力能抛 13m，
	// 拿浮漂当参照的话玩家刚站好就触发收竿了）
	Vector3 pp = player->get_global_position();
	if (pp.distance_to(cast_origin) > 6.0f) {
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

// —— 鱼获图鉴（B 键）——

void FishingSystem::build_book_ui() {
	book_layer = memnew(CanvasLayer);
	book_layer->set_layer(8); // 在钓鱼 HUD(7) 之上、时钟之下
	add_child(book_layer);

	PanelContainer *panel = memnew(PanelContainer);
	panel->set_name("FishBookPanel");
	panel->set_anchors_preset(Control::PRESET_CENTER_RIGHT);
	panel->set_offset(Side::SIDE_RIGHT, -24.0f);
	panel->add_theme_stylebox_override("panel", uistyle::make_wood_textured(14.0f, 0.96f));
	book_layer->add_child(panel);

	book_label = memnew(Label);
	Ref<Font> cjk = uistyle::load_cjk();
	if (cjk.is_valid()) {
		book_label->add_theme_font_override("font", cjk);
	}
	book_label->add_theme_font_size_override("font_size", 20);
	book_label->add_theme_color_override("font_color", uistyle::wood_text());
	book_label->add_theme_color_override("font_outline_color", uistyle::wood_dark());
	book_label->add_theme_constant_override("outline_size", 2);
	panel->add_child(book_label);
	refresh_book();
}

void FishingSystem::refresh_book() {
	if (book_label == nullptr) {
		return;
	}
	String s = String::utf8("🎣 鱼获图鉴\n");
	for (int sp = 0; sp < FISH_KIND_COUNT; sp++) {
		s += String::utf8("  ");
		if (rec_count[sp] > 0) {
			s += String::utf8(species_name(sp)) + String::utf8(" ×")
					+ String::num_int64(rec_count[sp]);
			if (rec_best[sp] > 0.0f) {
				s += String::utf8("（最长 ") + String::num(rec_best[sp], 1) + String::utf8("cm）");
			}
		} else {
			// 未解锁：只露习性提示，钓到才显形
			s += String::utf8("？？（")
					+ (species_nocturnal(sp) ? String::utf8("夜行") : String::utf8("昼行"))
					+ String::utf8("）");
		}
		s += String("\n");
	}
	s += String::utf8("  合计  ") + String::num_int64(caught_count) + String::utf8(" 条");
	s += String::utf8("\n  （B 键收起）");
	book_label->set_text(s);
}

void FishingSystem::load_records() {
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load("user://fishing_records.cfg") != Error::OK) {
		return; // 没有纪录文件：全新开钓
	}
	// v2 键（按种）优先；v1 旧键（small/big 两档）迁移：青鳞+月影入"常见"、红尾+夜鳗入"次常见"不可分——
	// 旧档只有两档,直接归青鳞/月影（同昼夜常见种）,红尾夜鳗金鳞重新钓
	for (int sp = 0; sp < FISH_KIND_COUNT; sp++) {
		rec_count[sp] = (int)cf->get_value("records", "count_" + String::num_int64(sp), 0);
		rec_best[sp] = (float)cf->get_value("records", "best_" + String::num_int64(sp), 0.0f);
	}
	if (!cf->has_section_key("records", "count_0") && cf->has_section_key("records", "small")) {
		rec_count[FISH_QINGLIN] = (int)cf->get_value("records", "small", 0);
		rec_best[FISH_QINGLIN] = (float)cf->get_value("records", "best_small_cm", 0.0f);
		rec_count[FISH_YUEYING] = (int)cf->get_value("records", "big", 0);
		rec_best[FISH_YUEYING] = (float)cf->get_value("records", "best_big_cm", 0.0f);
	}
}

void FishingSystem::save_records() {
	Ref<ConfigFile> cf;
	cf.instantiate();
	for (int sp = 0; sp < FISH_KIND_COUNT; sp++) {
		cf->set_value("records", "count_" + String::num_int64(sp), rec_count[sp]);
		cf->set_value("records", "best_" + String::num_int64(sp), rec_best[sp]);
	}
	cf->save("user://fishing_records.cfg");
}
