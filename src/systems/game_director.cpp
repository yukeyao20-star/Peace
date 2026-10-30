#include "game_director.h"

#include "../actors/farm_animal.h"
#include "../interact/berry_bush.h"
#include "../interact/chest.h"
#include "../interact/farm_plot.h"
#include "../interact/well.h"
#include "../props/soccer_ball.h"
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "fishing_system.h"
#include "ui_style.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>

#include <random>

using namespace godot;

namespace {

constexpr int BERRY_TARGET = 6;       // 第 1 天目标采摘数（园里 3 丛 × 45s 重长）
constexpr float HINT_SECONDS = 10.0f; // 操作提示显示时长（老玩家常规淡出）
constexpr float HINT_FADE_INTERACT = 3.0f; // 驻留结束后：1s 全显 + 2s 渐隐
constexpr float DONE_SECONDS = 8.0f;  // 当日完成结算大字显示时长
constexpr float NEWDAY_SECONDS = 4.0f; // 新一天发布横幅显示时长

// —— 目标方向指示（世界空间标记）——
// 钓鱼引导点：湖心（FishingSystem::lake_center 同源固定值，地形固定种子；
// 仅指路用不参与玩法判定——玩家走近湖岸后由"按 F 抛竿"提示接管）
const Vector2 LAKE_CENTER(-6.0f, 26.0f);
const float FISH_WATER_Y = -0.55f;   // 与 FishingSystem 的 water_level 一致
const float MARKER_HIDE_DIST = 5.0f; // 贴近目标即收牌（交互悬浮提示接管）
const float FISH_HIDE_DIST = 12.0f;  // 钓鱼：接近湖岸即收（近岸提示接管）

float marker_height(int p_type) {
	switch (p_type) {
		case GameDirector::TASK_WELL:
			return 2.6f; // 井台约 2m 高，牌悬其上
		case GameDirector::TASK_CHEST:
			return 1.9f;
		case GameDirector::TASK_BERRY:
			return 2.0f;
		case GameDirector::TASK_GOAL:
			return 1.5f; // 球心上方
		case GameDirector::TASK_FISH:
			return 1.6f; // 水面上方
		case GameDirector::TASK_HARVEST:
			return 1.7f; // 菜畦上方
		case GameDirector::TASK_FEED:
			return 1.9f; // 动物头顶
		default:
			return 2.0f;
	}
}

const char *task_name(int p_type) {
	switch (p_type) {
		case GameDirector::TASK_BERRY:
			return "采浆果";
		case GameDirector::TASK_FISH:
			return "钓鱼";
		case GameDirector::TASK_GOAL:
			return "足球进球";
		case GameDirector::TASK_WELL:
			return "打井水";
		case GameDirector::TASK_CHEST:
			return "开木箱";
		case GameDirector::TASK_HARVEST:
			return "照看菜畦";
		case GameDirector::TASK_FEED:
			return "喂动物";
		default:
			return "?";
	}
}

} // namespace

void GameDirector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_berry_picked"), &GameDirector::on_berry_picked);
	ClassDB::bind_method(D_METHOD("on_fish_caught"), &GameDirector::on_fish_caught);
	ClassDB::bind_method(D_METHOD("on_goal_scored"), &GameDirector::on_goal_scored);
	ClassDB::bind_method(D_METHOD("on_water_drawn"), &GameDirector::on_water_drawn);
	ClassDB::bind_method(D_METHOD("on_chest_opened"), &GameDirector::on_chest_opened);
	ClassDB::bind_method(D_METHOD("on_chest_looted"), &GameDirector::on_chest_looted);
	ClassDB::bind_method(D_METHOD("on_plot_harvested"), &GameDirector::on_plot_harvested);
	ClassDB::bind_method(D_METHOD("on_animal_fed"), &GameDirector::on_animal_fed);
	ClassDB::bind_method(D_METHOD("get_count"), &GameDirector::get_count);
	ClassDB::bind_method(D_METHOD("get_target"), &GameDirector::get_target);
	ClassDB::bind_method(D_METHOD("is_done"), &GameDirector::is_done);
	ClassDB::bind_method(D_METHOD("get_current_day"), &GameDirector::get_current_day);
	// —— 存档恢复用 ——
	ClassDB::bind_method(D_METHOD("get_daily_serial"), &GameDirector::get_daily_serial);
	ClassDB::bind_method(D_METHOD("apply_daily_serial", "p_serial"), &GameDirector::apply_daily_serial);
	ClassDB::bind_method(D_METHOD("get_farm_serial"), &GameDirector::get_farm_serial);
	ClassDB::bind_method(D_METHOD("apply_farm_serial", "p_serial"), &GameDirector::apply_farm_serial);
}

void GameDirector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成 HUD
			}
			build_ui();
			// 初始第 1 天清单。仅当清单为空时才滚（若 SaveSystem 先于本节点 READY
			// 已 apply_daily_serial 恢复过存档，这里不能再覆盖回第 1 天）。
			if (tasks.empty()) {
				roll_day_tasks(1);
			}
			hint_t = HINT_SECONDS;
			// 新档（第 1 天且零进度）：操作提示驻留到玩家完成首次交互（ROADMAP #3②）；
			// 有进度的老玩家由 apply_daily_serial 翻回常规 10s 淡出。
			hint_persistent = (cur_day <= 1 && count == 0);
			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (goal_player != nullptr) {
				goal_player->stop();
				goal_player->set_stream(Ref<AudioStream>());
			}
			snd_goal = Ref<AudioStream>();
			if (pick_player != nullptr) {
				pick_player->stop();
				pick_player->set_stream(Ref<AudioStream>());
			}
			snd_pick = Ref<AudioStream>();
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();
			if (!connected) {
				connect_bushes(); // 首帧浆果丛都已 READY，稳稳连上
			}
			if (!farm_spawned) {
				spawn_farm_plots(); // 园内菜畦（锚定园内浆果丛；锚点没就绪下一帧再试）
			}
			if (!sources_connected) {
				connect_sources(); // 钓鱼/足球/菜畦在各自 READY 后才出现，逐帧补连
			}
			check_day_rollover(); // 自然午夜或床上睡觉跨天 → 换新清单
			// 目标方向指示（世界空间标记；目标失效就近重解析，贴近自动收牌）
			marker_t += (float)delta;
			update_goal_marker();
			// 操作提示淡出（整块玻璃药丸一起淡，避免字消失了底板还留着）；
			// 驻留中（新档首次交互前）不计时。
			if (hint_panel != nullptr && !hint_persistent && hint_t > 0.0f) {
				hint_t -= (float)delta;
				float a = Math::clamp(hint_t / 2.0f, 0.0f, 1.0f); // 最后 2 秒渐隐
				hint_panel->set_modulate(Color(1, 1, 1, a));
				if (hint_t <= 0.0f) {
					hint_panel->set_visible(false);
				}
			}
			// 中央大字淡出（当日完成 / 新一天发布共用）
			if (done_t > 0.0f) {
				done_t -= (float)delta;
				float a = Math::clamp(done_t / 2.0f, 0.0f, 1.0f);
				center_label->set_modulate(Color(1, 1, 1, a));
				if (done_t <= 0.0f) {
					center_label->set_visible(false);
				}
			}
		} break;
		default:
			break;
	}
}

Ref<Font> GameDirector::load_cjk_font() const {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	return Ref<Font>(Object::cast_to<Font>(res.ptr()));
}

void GameDirector::build_ui() {
	Ref<Font> font = load_cjk_font();

	// —— 左上角：每日待办清单（玻璃底板，内容自适应）——
	PanelContainer *goal_panel = memnew(PanelContainer);
	goal_panel->set_name("GoalPanel");
	goal_panel->set_position(Vector2(12.0f, 10.0f));
	goal_panel->add_theme_stylebox_override("panel", uistyle::make_glass(14.0f, 0.55f));
	add_child(goal_panel);

	goal_label = memnew(Label);
	goal_label->set_name("GoalLabel");
	if (font.is_valid()) {
		goal_label->add_theme_font_override("font", font);
	}
	goal_label->add_theme_font_size_override("font_size", 21);
	goal_label->add_theme_color_override("font_color", uistyle::text_cream());
	goal_label->add_theme_color_override("font_outline_color", uistyle::outline_ink());
	goal_label->add_theme_constant_override("outline_size", 6);
	goal_panel->add_child(goal_label);

	// —— 底部：操作提示（玻璃药丸，10 秒淡出）——
	hint_panel = memnew(PanelContainer);
	hint_panel->set_name("HintPanel");
	hint_panel->set_anchors_preset(Control::PRESET_CENTER_BOTTOM);
	hint_panel->set_offset(Side::SIDE_BOTTOM, -16.0f);
	hint_panel->add_theme_stylebox_override("panel", uistyle::make_glass(16.0f, 0.45f));
	add_child(hint_panel);

	hint_label = memnew(Label);
	hint_label->set_name("HintLabel");
	hint_label->set_text(String::utf8("WASD 移动 · 空格 跳跃 · E 互动（开门/开窗/开箱/打水/采浆果）"));
	hint_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	if (font.is_valid()) {
		hint_label->add_theme_font_override("font", font);
	}
	hint_label->add_theme_font_size_override("font_size", 17);
	hint_label->add_theme_color_override("font_color", uistyle::text_soft());
	hint_label->add_theme_color_override("font_outline_color", uistyle::outline_ink());
	hint_label->add_theme_constant_override("outline_size", 5);
	hint_panel->add_child(hint_label);

	// —— 中央：结算大字（达成/新一天时显示，初始隐藏） ——
	center_label = memnew(Label);
	center_label->set_name("DoneLabel");
	center_label->set_text(String::utf8("今日待办全部完成！\n\n回床上睡一觉，迎接新的一天"));
	center_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	center_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	center_label->set_anchors_preset(Control::PRESET_FULL_RECT);
	center_label->set_visible(false);
	if (font.is_valid()) {
		center_label->add_theme_font_override("font", font);
	}
	center_label->add_theme_font_size_override("font_size", 44);
	center_label->add_theme_color_override("font_color", Color(1.0f, 0.92f, 0.55f));
	center_label->add_theme_color_override("font_outline_color", Color(0.1f, 0.06f, 0.02f));
	center_label->add_theme_constant_override("outline_size", 14);
	add_child(center_label);

	// —— 进度与达成音效 ——
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr && rl->exists("res://assets/audio/goal_shimmer.mp3")) {
		Ref<Resource> r = rl->load("res://assets/audio/goal_shimmer.mp3");
		if (r.is_valid()) {
			snd_goal = Object::cast_to<AudioStream>(r.ptr());
		}
	}
	goal_player = memnew(AudioStreamPlayer);
	goal_player->set_name("GoalShimmer");
	if (snd_goal.is_valid()) {
		goal_player->set_stream(snd_goal);
	}
	goal_player->set_volume_db(-6.0f);
	goal_player->set_bus(StringName("SFX"));
	add_child(goal_player);

	if (rl != nullptr && rl->exists("res://assets/audio/organic_chime.mp3")) {
		Ref<Resource> r = rl->load("res://assets/audio/organic_chime.mp3");
		if (r.is_valid()) {
			snd_pick = Object::cast_to<AudioStream>(r.ptr());
		}
	}
	pick_player = memnew(AudioStreamPlayer);
	pick_player->set_name("PickChime");
	if (snd_pick.is_valid()) {
		pick_player->set_stream(snd_pick);
	}
	pick_player->set_volume_db(-8.0f);
	pick_player->set_bus(StringName("SFX"));
	add_child(pick_player);
}

void GameDirector::connect_bushes() {
	if (get_tree() == nullptr) {
		return;
	}
	// 浆果丛都是运行时生成的：READY 后才进组，这里按帧重试直到连上至少一丛。
	// （园子里/林缘的丛数量固定，首帧即可全部就位；连上后不再重复连接。）
	Array bushes = get_tree()->get_nodes_in_group(StringName("berry_bushes"));
	if (bushes.size() == 0) {
		return; // 本帧还没生成好，下一帧再试
	}
	for (int i = 0; i < bushes.size(); i++) {
		Node *b = Object::cast_to<Node>(bushes[i]);
		if (b != nullptr && !b->is_connected("berry_picked", Callable(this, "on_berry_picked"))) {
			b->connect("berry_picked", Callable(this, "on_berry_picked"));
		}
	}
	connected = true;
}

void GameDirector::connect_sources() {
	if (get_tree() == nullptr) {
		return;
	}
	Node *root = get_tree()->get_current_scene();
	if (root == nullptr && get_tree()->get_root() != nullptr) {
		root = get_tree()->get_root(); // Window 继承自 Node
	}
	if (root == nullptr) {
		return;
	}
	// 钓鱼与足球按名找（FishingSystem 在场景里，SoccerBall 由 SoccerField READY 时生成）
	Node *fishing = root->find_child("FishingSystem", true, false);
	if (fishing != nullptr && !fishing->is_connected("fish_caught", Callable(this, "on_fish_caught"))) {
		fishing->connect("fish_caught", Callable(this, "on_fish_caught"));
	}
	Node *ball = root->find_child("SoccerBall", true, false);
	if (ball != nullptr && !ball->is_connected("goal_scored", Callable(this, "on_goal_scored"))) {
		ball->connect("goal_scored", Callable(this, "on_goal_scored"));
	}
	// 井/箱走 interactables 组按类型 cast 连接
	bool saw_well = false;
	bool saw_chest = false;
	Array inters = get_tree()->get_nodes_in_group(StringName("interactables"));
	for (int i = 0; i < inters.size(); i++) {
		Node *n = Object::cast_to<Node>(inters[i]);
		if (n == nullptr) {
			continue;
		}
		Well *w = Object::cast_to<Well>(n);
		if (w != nullptr) {
			saw_well = true;
			if (!w->is_connected("water_drawn", Callable(this, "on_water_drawn"))) {
				w->connect("water_drawn", Callable(this, "on_water_drawn"));
			}
		}
		Chest *c = Object::cast_to<Chest>(n);
		if (c != nullptr) {
			saw_chest = true;
			if (!c->is_connected("chest_opened", Callable(this, "on_chest_opened"))) {
				c->connect("chest_opened", Callable(this, "on_chest_opened"));
			}
			if (!c->is_connected("chest_looted", Callable(this, "on_chest_looted"))) {
				c->connect("chest_looted", Callable(this, "on_chest_looted"));
			}
		}
	}
	// 菜畦收获走 farm_plots 组（berry_picked 按颗计浆果；crop_harvested 按茬计"照看菜畦"）
	Array plots = get_tree()->get_nodes_in_group(StringName("farm_plots"));
	for (int i = 0; i < plots.size(); i++) {
		Node *p = Object::cast_to<Node>(plots[i]);
		if (p != nullptr) {
			if (!p->is_connected("berry_picked", Callable(this, "on_berry_picked"))) {
				p->connect("berry_picked", Callable(this, "on_berry_picked"));
			}
			if (!p->is_connected("crop_harvested", Callable(this, "on_plot_harvested"))) {
				p->connect("crop_harvested", Callable(this, "on_plot_harvested"));
			}
		}
	}
	// 农场动物喂食（"farm_animals" 组，tscn 节点在导演之后 READY，逐帧补连）
	bool saw_animal = false;
	Array animals = get_tree()->get_nodes_in_group(StringName("farm_animals"));
	for (int i = 0; i < animals.size(); i++) {
		Node *a = Object::cast_to<Node>(animals[i]);
		if (a != nullptr) {
			saw_animal = true;
			if (!a->is_connected("animal_fed", Callable(this, "on_animal_fed"))) {
				a->connect("animal_fed", Callable(this, "on_animal_fed"));
			}
		}
	}
	// 四路+菜畦+动物都连上才停（场景固定都有；缺谁下一帧再补）
	if (fishing != nullptr && ball != nullptr && saw_well && saw_chest && farm_spawned && saw_animal) {
		sources_connected = true;
	}
}

void GameDirector::spawn_farm_plots() {
	if (get_tree() == nullptr || get_parent() == nullptr) {
		return;
	}
	// 锚点：园内第一丛浆果（挂在 Homestead 下的那几丛）
	Array bushes = get_tree()->get_nodes_in_group(StringName("berry_bushes"));
	Vector3 anchor;
	bool found = false;
	for (int i = 0; i < bushes.size(); i++) {
		Node3D *b = Object::cast_to<Node3D>(bushes[i]);
		if (b != nullptr && b->get_parent() != nullptr
				&& b->get_parent()->get_name() == StringName("Homestead")) {
			anchor = b->get_global_position();
			found = true;
			break;
		}
	}
	if (!found) {
		return; // 本帧园内丛还没 READY，下一帧再试
	}
	// 菜畦整体搬到宅基地东侧开阔地（用户反馈：挤在门口让房前过于拥挤）——
	// 2×2 保持，中心从门口 (+2.6,+2.0) 外推到 (+7.2,+0.6)，留足院门-门廊动线。
	// 注意：GameDirector 是 CanvasLayer，3D 节点必须挂到场景根，不能 add 到自己名下。
	static const Vector2 offs[4] = {
		Vector2(6.4f, 0.0f), Vector2(7.8f, 0.0f), Vector2(6.4f, 1.2f), Vector2(7.8f, 1.2f)
	};
	for (int i = 0; i < 4; i++) {
		FarmPlot *plot = memnew(FarmPlot);
		plot->set_name("FarmPlot");
		plot->set_position(anchor + Vector3(offs[i].x, 0.0f, offs[i].y));
		get_parent()->add_child(plot);
	}

	// 菜园木牌：立在菜园西侧入口，牌面朝西对着宅院方向——"菜园"二字一望即知
	{
		Ref<Shader> shader = meshutil::load_toon_shader();
		Node3D *sign = memnew(Node3D);
		sign->set_name("FarmSign");
		sign->set_position(anchor + Vector3(5.5f, 0.0f, 0.6f));
		sign->set_rotation(Vector3(0.0f, Math::PI * 0.5f, 0.0f)); // +Z 牌面朝西(宅院来向)
		get_parent()->add_child(sign);
		const Color wood_post(0.42f, 0.31f, 0.19f);
		const Color wood_board(0.58f, 0.43f, 0.27f);
		meshutil::add_box(sign, shader, Vector3(0.09f, 0.95f, 0.09f),
				Vector3(0.0f, 0.47f, 0.0f), wood_post); // 立柱
		meshutil::add_box(sign, shader, Vector3(0.78f, 0.36f, 0.06f),
				Vector3(0.0f, 1.02f, 0.0f), wood_board); // 牌面
		Label3D *board_text = memnew(Label3D);
		board_text->set_text(String::utf8("菜园"));
		board_text->set_pixel_size(0.0042f);
		board_text->set_font_size(48);
		board_text->set_outline_size(10);
		board_text->set_modulate(Color(0.32f, 0.22f, 0.10f)); // 木刻深棕字
		board_text->set_position(Vector3(0.0f, 1.02f, 0.045f));
		Ref<Font> cjk = uistyle::load_cjk();
		if (cjk.is_valid()) {
			board_text->set_font(cjk);
		}
		sign->add_child(board_text);
	}
	farm_spawned = true;
	// 存档里的菜畦状态回放（SaveSystem 恢复先于菜畦生成时在这里补上）
	if (!pending_farm_serial.is_empty()) {
		apply_farm_serial_to_plots(pending_farm_serial);
		pending_farm_serial = String();
	}
}

void GameDirector::apply_farm_serial_to_plots(const String &p_serial) {
	if (p_serial.is_empty() || get_tree() == nullptr) {
		return;
	}
	// 顺序=生成顺序（组内序即场景树加入序，确定性布局下两轮一致）
	Array plots = get_tree()->get_nodes_in_group(StringName("farm_plots"));
	PackedStringArray entries = p_serial.split(";", true);
	const int n = Math::min((int)plots.size(), (int)entries.size());
	for (int i = 0; i < n; i++) {
		FarmPlot *plot = Object::cast_to<FarmPlot>(plots[i]);
		if (plot != nullptr) {
			plot->apply_plot_serial(entries[i]);
		}
	}
}

void GameDirector::check_day_rollover() {
	if (get_parent() == nullptr) {
		return;
	}
	SkyCycle *sc = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
	if (sc == nullptr) {
		return;
	}
	int d = sc->get_day_count();
	if (d == cur_day) {
		return;
	}
	roll_day_tasks(d);
	if (suppress_announce) {
		suppress_announce = false; // 只吞读档当天的这一次
	} else {
		announce_center(String::utf8("第 ") + String::num_int64(d) + String::utf8(" 天 · 新的待办已发布"), NEWDAY_SECONDS);
	}
}

void GameDirector::roll_day_tasks(int p_day) {
	cur_day = Math::max(p_day, 1);
	tasks.clear();
	day_done = false;
	if (cur_day <= 1) {
		// 第 1 天：新手引导，行为与旧版一致（采浆果 ×6）
		DailyTask t;
		t.type = TASK_BERRY;
		t.target = BERRY_TARGET;
		t.progress = 0;
		tasks.push_back(t);
	} else {
		// 第 2 天起：浆果恒在（3~5 颗），另从 钓鱼/进球/井水/木箱/菜畦/喂动物
		// 六类中抽 2 类。以天数为种子确定性生成——读档重进同一天清单一致。
		std::mt19937 rng((unsigned)(cur_day * 2654435761u));
		DailyTask berry;
		berry.type = TASK_BERRY;
		berry.target = 3 + (int)(rng() % 3);
		tasks.push_back(berry);
		int others[6] = { TASK_FISH, TASK_GOAL, TASK_WELL, TASK_CHEST, TASK_HARVEST, TASK_FEED };
		for (int i = 5; i > 0; i--) { // Fisher-Yates 洗牌
			int j = (int)(rng() % (unsigned)(i + 1));
			int tmp = others[i];
			others[i] = others[j];
			others[j] = tmp;
		}
		for (int i = 0; i < 2; i++) {
			DailyTask t;
			t.type = others[i];
			t.target = (others[i] == TASK_FISH) ? (1 + (int)(rng() % 2)) : 1; // 钓鱼 1~2 条
			tasks.push_back(t);
		}
	}
	sync_legacy_fields();
	refresh_goal();
}

void GameDirector::sync_legacy_fields() {
	// 旧字段镜像：count/target/done 始终对齐"当日浆果任务"（无浆果任务时保持 0/6）
	for (const DailyTask &t : tasks) {
		if (t.type == TASK_BERRY) {
			target = t.target;
			count = t.progress;
			break;
		}
	}
	bool all_done = !tasks.empty();
	for (const DailyTask &t : tasks) {
		if (t.progress < t.target) {
			all_done = false;
			break;
		}
	}
	day_done = all_done;
	done = all_done;
}

void GameDirector::refresh_goal() {
	if (goal_label == nullptr) {
		return;
	}
	String s = String::utf8("今日待办 · 第 ") + String::num_int64(cur_day) + String::utf8(" 天");
	for (const DailyTask &t : tasks) {
		bool fin = t.progress >= t.target;
		s += String::utf8("\n  ");
		s += fin ? String::utf8("✓ ") : String::utf8("· ");
		s += String::utf8(task_name(t.type)) + String(" ")
				+ String::num_int64(Math::min(t.progress, t.target))
				+ String("/") + String::num_int64(t.target);
	}
	if (day_done) {
		s += String::utf8("\n✨ 全部完成 · 睡一觉迎接明天");
	}
	goal_label->set_text(s);
}

void GameDirector::ensure_goal_marker() {
	if (goal_marker != nullptr || get_parent() == nullptr) {
		return;
	}
	Ref<Font> font = load_cjk_font();
	if (font.is_null()) {
		return; // 没有 CJK 字体就不立牌（世界空间豆腐块比没有更糟）
	}
	Label3D *m = memnew(Label3D);
	m->set_name("GoalMarker");
	m->set_font(font);
	m->set_font_size(110);
	m->set_outline_size(26);
	m->set_modulate(Color(1.0f, 0.9f, 0.55f, 0.95f));
	m->set_outline_modulate(Color(0.12f, 0.07f, 0.02f, 0.92f));
	m->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	m->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true); // 引导牌穿树冠/屋顶可见，远处也能指路
	m->set_draw_flag(Label3D::FLAG_SHADED, false);
	m->set_visible(false);
	// CanvasLayer 装不下 3D 节点，挂到场景根（同菜畦套路）
	get_parent()->add_child(m);
	goal_marker = m;
}

Node3D *GameDirector::resolve_marker_target(int p_type) {
	if (get_tree() == nullptr) {
		return nullptr;
	}
	Node *root = get_tree()->get_current_scene();
	if (root == nullptr && get_tree()->get_root() != nullptr) {
		root = get_tree()->get_root(); // Window 继承自 Node
	}
	if (root == nullptr) {
		return nullptr;
	}
	if (p_type == TASK_BERRY) {
		// 就近挑一丛果满的（get_prompt 为空 = 采空重长中，不指）
		Node3D *player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
		const Vector3 ppos = (player != nullptr) ? player->get_global_position() : Vector3();
		Array bushes = get_tree()->get_nodes_in_group(StringName("berry_bushes"));
		BerryBush *best = nullptr;
		float best_d = 0.0f;
		for (int i = 0; i < bushes.size(); i++) {
			BerryBush *b = Object::cast_to<BerryBush>(bushes[i]);
			if (b == nullptr || b->get_prompt().is_empty()) {
				continue;
			}
			float d = (b->get_global_position() - ppos).length_squared();
			if (best == nullptr || d < best_d) {
				best = b;
				best_d = d;
			}
		}
		return best;
	}
	if (p_type == TASK_WELL || p_type == TASK_CHEST) {
		Array inters = get_tree()->get_nodes_in_group(StringName("interactables"));
		for (int i = 0; i < inters.size(); i++) {
			Node *n = Object::cast_to<Node>(inters[i]);
			if (p_type == TASK_WELL && Object::cast_to<Well>(n) != nullptr) {
				return Object::cast_to<Well>(n);
			}
			if (p_type == TASK_CHEST && Object::cast_to<Chest>(n) != nullptr) {
				return Object::cast_to<Chest>(n);
			}
		}
		return nullptr;
	}
	if (p_type == TASK_GOAL) {
		// 足球由 SoccerField READY 时生成，标记跟着球走
		return Object::cast_to<Node3D>(root->find_child("SoccerBall", true, false));
	}
	if (p_type == TASK_HARVEST) {
		// 就近挑一块菜畦（优先已成熟的：挂果可收）
		Node3D *player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
		const Vector3 ppos = (player != nullptr) ? player->get_global_position() : Vector3();
		FarmPlot *best = nullptr;
		FarmPlot *best_ripe = nullptr;
		float best_d = 0.0f;
		float best_ripe_d = 0.0f;
		Array plots = get_tree()->get_nodes_in_group(StringName("farm_plots"));
		for (int i = 0; i < plots.size(); i++) {
			FarmPlot *p = Object::cast_to<FarmPlot>(plots[i]);
			if (p == nullptr) {
				continue;
			}
			float d = (p->get_global_position() - ppos).length_squared();
			if (p->get_state() == FarmPlot::STATE_RIPE) {
				if (best_ripe == nullptr || d < best_ripe_d) {
					best_ripe = p;
					best_ripe_d = d;
				}
			} else if (best == nullptr || d < best_d) {
				best = p;
				best_d = d;
			}
		}
		return best_ripe != nullptr ? best_ripe : best;
	}
	if (p_type == TASK_FEED) {
		// 就近挑一只农场动物
		Node3D *player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
		const Vector3 ppos = (player != nullptr) ? player->get_global_position() : Vector3();
		Node3D *best = nullptr;
		float best_d = 0.0f;
		Array animals = get_tree()->get_nodes_in_group(StringName("farm_animals"));
		for (int i = 0; i < animals.size(); i++) {
			Node3D *a = Object::cast_to<Node3D>(animals[i]);
			if (a == nullptr) {
				continue;
			}
			float d = (a->get_global_position() - ppos).length_squared();
			if (best == nullptr || d < best_d) {
				best = a;
				best_d = d;
			}
		}
		return best;
	}
	return nullptr; // 钓鱼无静态目标节点（竿在玩家手上）→ update_goal_marker 特判指湖心
}

void GameDirector::update_goal_marker() {
	ensure_goal_marker();
	if (goal_marker == nullptr) {
		return;
	}
	// 当前目标 = 清单里第一个未完成项；全部完成 → 收牌
	int want_type = -1;
	for (const DailyTask &t : tasks) {
		if (t.progress < t.target) {
			want_type = t.type;
			break;
		}
	}
	if (want_type < 0) {
		marker_task_type = -1;
		marker_target_path = NodePath();
		goal_marker->set_visible(false);
		return;
	}
	Vector3 target_pos;
	bool have_target = false;
	if (want_type == TASK_FISH) {
		// 指湖心：玩家走近湖岸，"按 F 抛竿"提示自然接管
		if (marker_task_type != want_type) {
			goal_marker->set_text(String::utf8("▼ ") + String::utf8(task_name(want_type)));
		}
		target_pos = Vector3(LAKE_CENTER.x, FISH_WATER_Y, LAKE_CENTER.y);
		have_target = true;
	} else {
		// 目标变更或失效（浆果丛采空 / 节点不在了）→ 重新解析
		Node3D *target = nullptr;
		if (marker_task_type == want_type && !marker_target_path.is_empty()) {
			target = Object::cast_to<Node3D>(get_parent()->get_node_or_null(marker_target_path));
			if (target != nullptr && want_type == TASK_BERRY) {
				BerryBush *b = Object::cast_to<BerryBush>(target);
				if (b == nullptr || b->get_prompt().is_empty()) {
					target = nullptr; // 本丛采空进重长 → 就近换下一丛
				}
			}
		}
		if (target == nullptr) {
			target = resolve_marker_target(want_type);
			marker_target_path = (target != nullptr) ? target->get_path() : NodePath();
			goal_marker->set_text(String::utf8("▼ ") + String::utf8(task_name(want_type)));
		}
		if (target != nullptr) {
			target_pos = target->get_global_position();
			have_target = true;
		}
	}
	marker_task_type = want_type;
	if (!have_target) {
		goal_marker->set_visible(false);
		return;
	}
	// 玩家贴近目标 → 收牌（交互悬浮提示已接管）；Player 路径缓存，免逐帧全树扫
	Node3D *player = nullptr;
	if (get_tree() != nullptr && get_tree()->get_root() != nullptr) {
		if (!player_path.is_empty()) {
			player = Object::cast_to<Node3D>(get_tree()->get_root()->get_node_or_null(player_path));
		}
		if (player == nullptr) {
			player = Object::cast_to<Node3D>(get_tree()->get_root()->find_child("Player", true, false));
			player_path = (player != nullptr) ? player->get_path() : NodePath();
		}
	}
	float hide_d = (want_type == TASK_FISH) ? FISH_HIDE_DIST : MARKER_HIDE_DIST;
	if (player != nullptr && (player->get_global_position() - target_pos).length() < hide_d) {
		goal_marker->set_visible(false);
		return;
	}
	goal_marker->set_visible(true);
	target_pos.y += marker_height(want_type) + Math::sin(marker_t * 2.2f) * 0.15f;
	goal_marker->set_global_position(target_pos);
}

void GameDirector::note_first_interaction() {
	if (hint_persistent) {
		hint_persistent = false;
		hint_t = HINT_FADE_INTERACT; // 走既有淡出曲线：1s 全显 + 2s 渐隐
	}
}

void GameDirector::announce_center(const String &p_text, float p_seconds) {
	if (center_label == nullptr) {
		return;
	}
	center_label->set_text(p_text);
	center_label->set_modulate(Color(1, 1, 1, 1));
	center_label->set_visible(true);
	done_t = p_seconds;
}

void GameDirector::mark_task_progress(int p_type) {
	if (day_done) {
		return; // 当日已结算，继续玩不再计数
	}
	bool hit = false;
	for (DailyTask &t : tasks) {
		if (t.type == p_type && t.progress < t.target) {
			t.progress++;
			hit = true;
			break;
		}
	}
	if (!hit) {
		return; // 今日清单里没有这类（或已满），静默忽略
	}
	sync_legacy_fields();
	refresh_goal();
	// 检查当日是否全部完成
	bool all_done = true;
	for (const DailyTask &t : tasks) {
		if (t.progress < t.target) {
			all_done = false;
			break;
		}
	}
	if (all_done) {
		day_done = true;
		done = true;
		announce_center(String::utf8("第 ") + String::num_int64(cur_day)
				+ String::utf8(" 天 · 今日待办全部完成！\n\n回床上睡一觉，迎接新的一天"),
				DONE_SECONDS);
		if (goal_player != nullptr && snd_goal.is_valid()) {
			goal_player->play();
		}
	} else {
		if (pick_player != nullptr && snd_pick.is_valid()) {
			pick_player->play();
		}
	}
}

void GameDirector::on_berry_picked() {
	note_first_interaction();
	mark_task_progress(TASK_BERRY);
}

void GameDirector::on_fish_caught() {
	note_first_interaction();
	mark_task_progress(TASK_FISH);
}

void GameDirector::on_goal_scored() {
	note_first_interaction();
	mark_task_progress(TASK_GOAL);
}

void GameDirector::on_water_drawn() {
	note_first_interaction();
	mark_task_progress(TASK_WELL);
}

void GameDirector::on_chest_opened() {
	note_first_interaction();
	mark_task_progress(TASK_CHEST);
}

void GameDirector::on_plot_harvested() {
	note_first_interaction();
	mark_task_progress(TASK_HARVEST);
}

void GameDirector::on_animal_fed() {
	note_first_interaction();
	mark_task_progress(TASK_FEED);
}

void GameDirector::on_chest_looted() {
	note_first_interaction();
	// 一次性彩蛋公告（木箱首次打开翻出野餐毯）：
	// 若这一箱正好补完当日清单，mark_task_progress 的结算横幅会随后覆盖本条
	announce_center(String::utf8("从木箱里翻出一条野餐毯！\n\n· 已摆在木箱旁 ·"), 4.5f);
}

String GameDirector::get_daily_serial() const {
	// "day:type,progress,target;type,progress,target;..."
	String s = String::num_int64(cur_day);
	for (const DailyTask &t : tasks) {
		s += String(";") + String::num_int64(t.type) + String(",")
				+ String::num_int64(t.progress) + String(",")
				+ String::num_int64(t.target);
	}
	return s;
}

void GameDirector::apply_daily_serial(const String &p_serial) {
	if (p_serial.is_empty()) {
		return;
	}
	PackedStringArray parts = p_serial.split(";", false);
	if (parts.size() < 1) {
		return;
	}
	int day = parts[0].to_int();
	std::vector<DailyTask> parsed;
	for (int i = 1; i < parts.size(); i++) {
		PackedStringArray f = parts[i].split(",", false);
		if (f.size() != 3) {
			continue; // 坏段跳过，不整体失败
		}
		DailyTask t;
		t.type = f[0].to_int();
		t.progress = f[1].to_int();
		t.target = f[2].to_int();
		if (t.type < 0 || t.type >= TASK_KIND_COUNT || t.target < 1) {
			continue;
		}
		t.progress = Math::clamp(t.progress, 0, t.target);
		parsed.push_back(t);
	}
	if (parsed.empty()) {
		return; // 一条有效任务都没有 → 保持默认第 1 天清单
	}
	cur_day = Math::max(day, 1);
	tasks = parsed;
	suppress_announce = true; // 读档恢复当天，别弹"新的一天"
	sync_legacy_fields();
	// 有进度的老玩家（day>1 或已有进度）：操作提示不驻留，走常规 10s 淡出
	if (cur_day > 1 || count > 0) {
		hint_persistent = false;
		hint_t = HINT_SECONDS;
	}
	refresh_goal();
}

String GameDirector::get_farm_serial() const {
	// "state,进度%,浇灌;state,进度%,浇灌;..." ×4（顺序=farm_plots 组序=生成顺序）。
	// 运行期扫描组并刷新快照；退出拆树期组/树已不可用 → 回退最近一次成功快照，
	// 否则退出兜底存档会把空串写进存档覆盖掉好数据（实测踩过）。
	if (farm_spawned && get_tree() != nullptr) {
		Array plots = get_tree()->get_nodes_in_group(StringName("farm_plots"));
		String s;
		for (int i = 0; i < plots.size(); i++) {
			const FarmPlot *plot = Object::cast_to<FarmPlot>(plots[i]);
			if (plot == nullptr) {
				continue;
			}
			if (!s.is_empty()) {
				s += String(";");
			}
			s += plot->get_plot_serial();
		}
		if (!s.is_empty()) {
			last_farm_serial = s; // 四块齐整的聚合结果才值得缓存
			return s;
		}
	}
	return last_farm_serial;
}

void GameDirector::apply_farm_serial(const String &p_serial) {
	if (p_serial.is_empty()) {
		return;
	}
	if (!farm_spawned) {
		// 菜畦还没生成（锚点浆果丛未 READY）：先存着，spawn_farm_plots 末尾回放
		pending_farm_serial = p_serial;
		return;
	}
	apply_farm_serial_to_plots(p_serial);
}
