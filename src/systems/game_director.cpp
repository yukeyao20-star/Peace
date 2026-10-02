#include "game_director.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>

using namespace godot;

namespace {

constexpr int BERRY_TARGET = 6;       // 目标采摘数（园里 3 丛 × 45s 重长）
constexpr float HINT_SECONDS = 10.0f; // 操作提示显示时长
constexpr float DONE_SECONDS = 8.0f;  // 结算大字显示时长

} // namespace

void GameDirector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_berry_picked"), &GameDirector::on_berry_picked);
	ClassDB::bind_method(D_METHOD("get_count"), &GameDirector::get_count);
	ClassDB::bind_method(D_METHOD("get_target"), &GameDirector::get_target);
	ClassDB::bind_method(D_METHOD("is_done"), &GameDirector::is_done);
	// —— 存档恢复用 ——
	ClassDB::bind_method(D_METHOD("set_count", "p_count"), &GameDirector::set_count);
	ClassDB::bind_method(D_METHOD("set_done", "p_done"), &GameDirector::set_done);
}

void GameDirector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成 HUD
			}
			build_ui();
			hint_t = HINT_SECONDS;
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
			// 操作提示淡出
			if (hint_label != nullptr && hint_t > 0.0f) {
				hint_t -= (float)delta;
				float a = Math::clamp(hint_t / 2.0f, 0.0f, 1.0f); // 最后 2 秒渐隐
				hint_label->set_modulate(Color(1, 1, 1, a));
				if (hint_t <= 0.0f) {
					hint_label->set_visible(false);
				}
			}
			// 结算大字淡出
			if (done && done_t > 0.0f) {
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

	// —— 左上角：目标进度 ——
	goal_label = memnew(Label);
	goal_label->set_name("GoalLabel");
	goal_label->set_position(Vector2(16.0f, 12.0f));
	if (font.is_valid()) {
		goal_label->add_theme_font_override("font", font);
	}
	goal_label->add_theme_font_size_override("font_size", 22);
	goal_label->add_theme_color_override("font_color", Color(1.0f, 0.97f, 0.88f));
	goal_label->add_theme_color_override("font_outline_color", Color(0.08f, 0.08f, 0.10f));
	goal_label->add_theme_constant_override("outline_size", 8);
	add_child(goal_label);
	refresh_goal(); // 初始文案统一由 refresh_goal 生成（避免 0/10 硬编码漂移）

	// —— 底部：操作提示（10 秒淡出） ——
	hint_label = memnew(Label);
	hint_label->set_name("HintLabel");
	hint_label->set_text(String::utf8("WASD 移动 · 空格 跳跃 · E 互动（开门/开窗/开箱/打水/采浆果）"));
	hint_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	hint_label->set_anchors_preset(Control::PRESET_BOTTOM_WIDE);
	hint_label->set_offset(Side::SIDE_TOP, -46.0f);
	hint_label->set_offset(Side::SIDE_BOTTOM, -14.0f);
	if (font.is_valid()) {
		hint_label->add_theme_font_override("font", font);
	}
	hint_label->add_theme_font_size_override("font_size", 17);
	hint_label->add_theme_color_override("font_color", Color(0.95f, 0.95f, 0.92f));
	hint_label->add_theme_color_override("font_outline_color", Color(0.05f, 0.05f, 0.08f));
	hint_label->add_theme_constant_override("outline_size", 6);
	add_child(hint_label);

	// —— 中央：结算大字（达成时显示，初始隐藏） ——
	center_label = memnew(Label);
	center_label->set_name("DoneLabel");
	center_label->set_text(String::utf8("目标达成！浆果收集完成\n\n这座小岛任你继续探索"));
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

	// —— 采摘与达成音效 ——
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

void GameDirector::refresh_goal() {
	if (goal_label != nullptr) {
		goal_label->set_text(String::utf8("目标 · 采浆果 ") + String::num_int64(Math::min(count, target))
				+ String("/") + String::num_int64(target) + (done ? String::utf8("  ✓") : String()));
	}
}

void GameDirector::on_berry_picked() {
	if (done) {
		return; // 达成后继续采不再计数
	}
	count++;
	refresh_goal();
	if (count >= target && center_label != nullptr) {
		done = true;
		done_t = DONE_SECONDS;
		center_label->set_modulate(Color(1, 1, 1, 1));
		center_label->set_visible(true);
		if (goal_player != nullptr && snd_goal.is_valid()) {
			goal_player->play();
		}
	} else {
		if (pick_player != nullptr && snd_pick.is_valid()) {
			pick_player->play();
		}
	}
}
