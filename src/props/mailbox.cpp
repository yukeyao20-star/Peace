#include "mailbox.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../systems/ui_style.h"
#include "../world/sky_cycle.h"

using namespace godot;

void Mailbox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Mailbox::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Mailbox::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Mailbox::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Mailbox::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Mailbox::interact);
	ClassDB::bind_method(D_METHOD("get_ui_open"), &Mailbox::get_ui_open);
	ClassDB::bind_method(D_METHOD("get_read_today"), &Mailbox::get_read_today);
	ClassDB::bind_method(D_METHOD("get_letter_day"), &Mailbox::get_letter_day);
	ClassDB::bind_method(D_METHOD("get_read_day"), &Mailbox::get_read_day);
	ClassDB::bind_method(D_METHOD("set_read_day", "day"), &Mailbox::set_read_day);
}

String Mailbox::get_prompt() const {
	if (ui_open) {
		return String::utf8("按 E 收起信");
	}
	if (read_today) {
		return String::utf8("按 E 看看空信箱");
	}
	return String::utf8("按 E 读信");
}

Vector3 Mailbox::get_interact_pos() const {
	return get_global_transform().xform(Vector3(0.0f, 1.0f, 0.3f));
}

void Mailbox::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (ui_open) {
		close_ui();
		return;
	}
	if (read_today) {
		if (get_parent() != nullptr && get_parent()->get_parent() != nullptr) {
			Node *gd = get_parent()->get_parent()->get_node_or_null("GameDirector");
			if (gd != nullptr) {
				gd->call("flash_hint", String::utf8("信箱空了 · 明天再来吧"), 2.4f);
			}
		}
		return;
	}
	open_ui();
}

void Mailbox::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			Node *root = (get_parent() != nullptr) ? get_parent()->get_parent() : nullptr;
			if (root != nullptr) {
				sky = Object::cast_to<SkyCycle>(root->get_node_or_null("SkyCycle"));
				player = Object::cast_to<Node3D>(root->get_node_or_null("Player"));
			}
			add_to_group("interactables");
			build();
			build_ui();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (sky != nullptr) {
				const int day = sky->get_day_count();
				if (day != day_seen) {
					day_seen = day;
					read_today = (day == read_day);
					if (envelope != nullptr) {
						envelope->set_visible(!read_today);
					}
				}
			}
			door_t = Math::lerp(door_t, door_target, 1.0f - std::exp(-9.0f * delta));
			if (door != nullptr) {
				door->set_rotation(Vector3(1.45f * door_t, 0.0f, 0.0f));
			}
			if (flag_pivot != nullptr) {
				const float target = read_today ? -1.15f : 0.0f;
				const Vector3 r = flag_pivot->get_rotation();
				flag_pivot->set_rotation(Vector3(r.x, r.y, Math::lerp(r.z, target, 1.0f - std::exp(-6.0f * delta))));
			}
			if (drifts != nullptr) {
				const float snow_now = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
				drifts->set_visible(snow_now > 0.2f);
				drifts->set_scale(Vector3(1.0f, 0.35f + 0.65f * Math::clamp(snow_now, 0.0f, 1.0f), 1.0f));
			}
			if (ui_open && player != nullptr) {
				if (player->get_global_position().distance_to(get_global_position()) > 3.2f) {
					close_ui();
				}
			}
		} break;
		default:
			break;
	}
}

void Mailbox::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.44f, 0.31f, 0.18f);
	const Color wood_light(0.58f, 0.42f, 0.24f);
	const Color iron(0.24f, 0.23f, 0.22f);

	meshutil::add_box(this, toon, Vector3(0.10f, 0.95f, 0.10f), Vector3(0.0f, 0.47f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.32f, 0.23f, 0.40f), Vector3(0.0f, 1.02f, 0.0f), wood_light);
	// 箱顶小铜帽
	meshutil::add_box(this, toon, Vector3(0.34f, 0.03f, 0.42f), Vector3(0.0f, 1.145f, 0.0f), Color(0.42f, 0.36f, 0.28f));

	// 箱门：底部铰链，向外放下
	door = memnew(Node3D);
	door->set_name("Door");
	door->set_position(Vector3(0.0f, 0.92f, 0.185f));
	add_child(door);
	meshutil::add_box(door, toon, Vector3(0.28f, 0.19f, 0.025f), Vector3(0.0f, 0.095f, 0.0f), wood);
	meshutil::add_box(door, toon, Vector3(0.05f, 0.05f, 0.02f), Vector3(0.0f, 0.17f, 0.02f), iron);

	// 箱里的信封：没读时露出一角
	envelope = meshutil::add_box(this, toon, Vector3(0.23f, 0.135f, 0.018f), Vector3(0.0f, 1.01f, 0.145f), Color(0.94f, 0.91f, 0.83f));

	// 小红旗：有信时立着，读完倒下
	flag_pivot = memnew(Node3D);
	flag_pivot->set_name("Flag");
	flag_pivot->set_position(Vector3(0.185f, 1.09f, 0.05f));
	add_child(flag_pivot);
	meshutil::add_box(flag_pivot, toon, Vector3(0.028f, 0.18f, 0.028f), Vector3(0.0f, 0.08f, 0.0f), Color(0.78f, 0.20f, 0.16f));
	meshutil::add_box(flag_pivot, toon, Vector3(0.10f, 0.035f, 0.02f), Vector3(0.06f, 0.15f, 0.0f), Color(0.78f, 0.20f, 0.16f));

	snd_paper = make_paper();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("PaperVoice");
	voice->set_stream(snd_paper);
	voice->set_volume_db(-8.0f);
	voice->set_max_distance(10.0f);
	add_child(voice);

	// —— 冬日：信箱柱脚的积雪堆 ——
	drifts = memnew(Node3D);
	drifts->set_name("SnowDrifts");
	add_child(drifts);
	MeshInstance3D *d0 = meshutil::add_sphere(drifts, toon, 0.30f, Vector3(0.02f, 0.02f, 0.02f), Color(0.92f, 0.95f, 1.0f));
	d0->set_scale(Vector3(1.0f, 0.5f, 0.9f));
	MeshInstance3D *d1 = meshutil::add_sphere(drifts, toon, 0.22f, Vector3(0.28f, 0.02f, -0.12f), Color(0.92f, 0.95f, 1.0f));
	d1->set_scale(Vector3(0.9f, 0.45f, 0.8f));
	drifts->set_visible(false);
}

// 0.3 秒的纸声：几簇细碎的高频噪声，像把信纸从信封里抽出来。
Ref<AudioStreamWAV> Mailbox::make_paper() {
	const int sr = 22050;
	const int n = (int)(0.3f * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(70707u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float hp = 0.0f;
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tsec = (float)i / (float)sr;
		const float envelope = std::sin(Math::clamp(tsec / 0.3f, 0.0f, 1.0f) * 3.14159f);
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.35f;
		hp = white - lp;
		const float crackle = (std::sin(tsec * 900.0f + std::sin(tsec * 3100.0f) * 2.0f) > 0.85f) ? (d01(rng) - 0.5f) : 0.0f;
		float samp = (hp * 0.5f + crackle * 0.8f) * envelope;
		samp = Math::clamp(samp, -1.0f, 1.0f);
		const int16_t q = (int16_t)(samp * 9000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}

void Mailbox::build_ui() {
	ui_layer = memnew(CanvasLayer);
	ui_layer->set_name("LetterLayer");
	ui_layer->set_layer(12);
	add_child(ui_layer);

	ui_root = memnew(Control);
	ui_root->set_name("LetterRoot");
	ui_root->set_anchors_preset(Control::PRESET_FULL_RECT);
	ui_root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	ui_root->set_visible(false);
	ui_layer->add_child(ui_root);

	ColorRect *dim = memnew(ColorRect);
	dim->set_color(Color(0.02f, 0.02f, 0.02f, 0.30f));
	dim->set_anchors_preset(Control::PRESET_FULL_RECT);
	dim->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	ui_root->add_child(dim);

	CenterContainer *center = memnew(CenterContainer);
	center->set_anchors_preset(Control::PRESET_FULL_RECT);
	center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	ui_root->add_child(center);

	PanelContainer *panel = memnew(PanelContainer);
	panel->set_custom_minimum_size(Vector2(560.0f, 0.0f));
	panel->add_theme_stylebox_override("panel", uistyle::make_wood_tiled());
	center->add_child(panel);

	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 22);
	margin->add_theme_constant_override("margin_right", 22);
	margin->add_theme_constant_override("margin_top", 18);
	margin->add_theme_constant_override("margin_bottom", 16);
	panel->add_child(margin);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->add_theme_constant_override("separation", 12);
	margin->add_child(vbox);

	Ref<Font> font = uistyle::load_cjk();
	ui_title = memnew(Label);
	ui_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	uistyle::dress_wood_label(ui_title, 22, font);
	vbox->add_child(ui_title);

	PanelContainer *paper = memnew(PanelContainer);
	Ref<StyleBoxFlat> paper_sb;
	paper_sb.instantiate();
	paper_sb->set_bg_color(uistyle::paper_cream());
	paper_sb->set_corner_radius_all(8);
	paper_sb->set_border_color(Color(0.72f, 0.62f, 0.46f));
	paper_sb->set_border_width_all(1);
	paper_sb->set_content_margin_all(22.0f);
	paper_sb->set_shadow_color(Color(0.0f, 0.0f, 0.0f, 0.25f));
	paper_sb->set_shadow_size(8);
	paper->add_theme_stylebox_override("panel", paper_sb);
	vbox->add_child(paper);

	VBoxContainer *pvb = memnew(VBoxContainer);
	pvb->add_theme_constant_override("separation", 14);
	paper->add_child(pvb);

	ui_body = memnew(Label);
	ui_body->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	ui_body->set_custom_minimum_size(Vector2(470.0f, 150.0f));
	ui_body->set_vertical_alignment(VERTICAL_ALIGNMENT_TOP);
	uistyle::dress_label(ui_body, 18, uistyle::ink_brown(), font, 0);
	pvb->add_child(ui_body);

	ui_sign = memnew(Label);
	ui_sign->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	ui_sign->set_text(String::utf8("—— 湖畔的风"));
	uistyle::dress_label(ui_sign, 15, Color(0.38f, 0.29f, 0.20f), font, 0);
	pvb->add_child(ui_sign);

	Label *hint = memnew(Label);
	hint->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	hint->set_text(String::utf8("按 E 收起信"));
	uistyle::dress_label(hint, 15, uistyle::chalk_gold(), font, 1);
	vbox->add_child(hint);
}

String Mailbox::letter_for(int p_day, int p_weather) const {
	static const char *lines[8] = {
		"今天湖面结了一层薄冰，踩上去会轻轻响。火堆边留了位置给你。",
		"早上有雾，码头上的灯还没灭。慢慢走，不着急。",
		"松鼠把松果藏到了柴堆后面。别告诉它，我看见啦。",
		"夜里星星很亮，北斗的勺子正对着湖心。",
		"炉子上的水开了，屋里都是木头和热茶的味道。",
		"院子里的雪又厚了一点，信箱差点打不开。",
		"昨天有颗流星划过。许愿这种事，要趁早。",
		"湖边的风把风筝线吹得嗡嗡响。记得收线，也记得抬头。",
	};
	int idx = (p_day * 7 + p_weather * 3) % 8;
	if (idx < 0) {
		idx += 8;
	}
	return String::utf8(lines[idx]);
}

void Mailbox::open_ui() {
	ui_open = true;
	read_today = true;
	read_day = day_seen;
	door_target = 1.0f;
	if (envelope != nullptr) {
		envelope->set_visible(false);
	}
	if (voice != nullptr && snd_paper.is_valid()) {
		voice->play(0.0f);
	}
	if (ui_root != nullptr) {
		const int day = (sky != nullptr) ? sky->get_day_count() : 1;
		const int weather = (sky != nullptr) ? sky->get_weather() : 0;
		if (ui_title != nullptr) {
			ui_title->set_text(String::utf8("湖畔来信 · 第 ") + String::num_int64(day) + String::utf8(" 天"));
		}
		if (ui_body != nullptr) {
			ui_body->set_text(String::utf8("见字如面。\n\n") + letter_for(day, weather) + String::utf8("\n\n今天也慢慢过。"));
		}
		ui_root->set_visible(true);
	}
	if (get_parent() != nullptr && get_parent()->get_parent() != nullptr) {
		Node *gd = get_parent()->get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("on_letter_read");
		}
	}
}

void Mailbox::close_ui() {
	if (!ui_open) {
		return;
	}
	ui_open = false;
	door_target = 0.0f;
	if (voice != nullptr && snd_paper.is_valid()) {
		voice->play(0.0f);
	}
	if (ui_root != nullptr) {
		ui_root->set_visible(false);
	}
}
