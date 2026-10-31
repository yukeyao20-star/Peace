#include "shells.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../systems/ui_style.h"
#include "../world/terrain_builder.h"

using namespace godot;

// —————————————————————————— BeachShell ——————————————————————————

void BeachShell::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &BeachShell::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &BeachShell::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &BeachShell::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &BeachShell::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &BeachShell::interact);
}

void BeachShell::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
		add_to_group("interactables");
	}
}

void BeachShell::mark_taken(bool p_taken) {
	taken = p_taken;
	set_visible(!taken);
	if (taken) {
		remove_from_group("interactables");
	} else {
		add_to_group("interactables");
	}
}

String BeachShell::get_prompt() const {
	return taken ? String() : String::utf8("按 E 拾起贝壳");
}

Vector3 BeachShell::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.08f, 0.0f);
}

float BeachShell::get_interact_distance() const {
	return 2.2f;
}

void BeachShell::interact(const Vector3 &p_player_pos) {
	if (!taken && field != nullptr) {
		field->collect(index, p_player_pos);
	}
}

// —————————————————————————— ShellField ——————————————————————————

void ShellField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_collected_mask"), &ShellField::get_collected_mask);
	ClassDB::bind_method(D_METHOD("apply_collected_mask", "mask"), &ShellField::apply_collected_mask);
	ClassDB::bind_method(D_METHOD("get_collected_count"), &ShellField::get_collected_count);
	ClassDB::bind_method(D_METHOD("get_total"), &ShellField::get_total);
	ClassDB::bind_method(D_METHOD("get_has_reward"), &ShellField::get_has_reward);
}

void ShellField::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
			}
			snd_chime = make_chime();
			chime = memnew(AudioStreamPlayer3D);
			chime->set_name("ShellChime");
			chime->set_stream(snd_chime);
			chime->set_volume_db(-8.0f);
			chime->set_max_distance(22.0f);
			add_child(chime);
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (reward != nullptr) {
				reward->set_rotation(Vector3(
						std::sin(t * 1.05f + 0.5f) * 0.035f,
						0.0f,
						std::sin(t * 0.87f) * 0.06f + std::sin(t * 2.3f) * 0.015f));
				reward_t -= delta;
				if (reward_t <= 0.0f) {
					Vector3 pos = reward->get_global_position();
					bool near = true;
					if (get_parent() != nullptr) {
						Node3D *p = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
						if (p != nullptr) {
							near = p->get_global_position().distance_to(pos) < 16.0f;
						}
					}
					if (near && reward_voice != nullptr && !reward_voice->is_playing()) {
						reward_voice->set_global_position(pos);
						reward_voice->set_pitch_scale(0.86f + (float)(rng() % 100u) * 0.0014f);
						reward_voice->set_volume_db(-17.0f);
						reward_voice->play(0.0f);
					}
					reward_t = 8.0f + (float)(rng() % 900u) * 0.009f;
				}
			}
		} break;
		default:
			break;
	}
}

void ShellField::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	Ref<SphereMesh> shell_mesh;
	shell_mesh.instantiate();
	shell_mesh->set_radius(0.18f);
	shell_mesh->set_height(0.36f);
	shell_mesh->set_radial_segments(14);
	shell_mesh->set_rings(7);

	const Color tints[5] = {
		Color(0.96f, 0.88f, 0.76f),
		Color(0.98f, 0.78f, 0.72f),
		Color(0.90f, 0.82f, 0.92f),
		Color(0.98f, 0.93f, 0.80f),
		Color(0.86f, 0.90f, 0.86f),
	};

	const float xs[10] = {-16.2f, -14.4f, -12.7f, -11.0f, -9.3f, -7.3f, -4.2f, -1.0f, 2.6f, 6.0f};
	std::mt19937 rng(20260909u);
	const float water = terrain != nullptr ? terrain->get_water_level() : -0.55f;

	for (int i = 0; i < 10; i++) {
		float x = xs[i];
		float z = -11.6f;
		float ground = water - 0.5f;
		if (terrain != nullptr) {
			// 从浅水线往岸上退，直到脚踩得到干沙
			for (int step = 0; step < 40; step++) {
				ground = terrain->get_terrain_height(x, z);
				if (ground > water + 0.02f) {
					break;
				}
				z -= 0.22f;
			}
		}
		Vector3 pos(x, ground + 0.035f, z);

		BeachShell *shell = memnew(BeachShell);
		shell->set_name(String("Shell") + String::num_int64(i));
		shell->setup(this, i);
		add_child(shell);
		shell->set_global_position(pos);
		shell->set_rotation(Vector3(
				(float)(rng() % 100u) * 0.0015f - 0.04f,
				(float)(rng() % 628u) * 0.01f,
				(float)(rng() % 100u) * 0.0015f - 0.04f));

		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(shell_mesh);
		mi->set_material_override(meshutil::toon_material(toon, tints[i % 5], 0.55f));
		mi->set_scale(Vector3(1.0f, 0.26f, 0.90f));
		shell->add_child(mi);

		// 扇贝的放射状棱：从铰链向外散开的七道浅槽
		for (int r = 0; r < 7; r++) {
			float a = -0.95f + (float)r * 0.316f;
			MeshInstance3D *rib = memnew(MeshInstance3D);
			Ref<BoxMesh> rm;
			rm.instantiate();
			rm->set_size(Vector3(0.012f, 0.012f, 0.30f));
			rm->set_material(meshutil::toon_material(toon, tints[i % 5].darkened(0.14f), 0.2f));
			rib->set_mesh(rm);
			rib->set_position(Vector3(std::sin(a) * 0.075f, 0.045f, std::cos(a) * 0.10f));
			rib->set_rotation(Vector3(0.0f, a, 0.0f));
			shell->add_child(rib);
		}

		// 扇贝的小铰链
		MeshInstance3D *hinge = memnew(MeshInstance3D);
		Ref<SphereMesh> hm;
		hm.instantiate();
		hm->set_radius(0.045f);
		hm->set_height(0.09f);
		hm->set_radial_segments(8);
		hm->set_rings(5);
		hm->set_material(meshutil::toon_material(toon, tints[i % 5].darkened(0.18f), 0.4f));
		hinge->set_mesh(hm);
		hinge->set_scale(Vector3(1.5f, 0.45f, 0.75f));
		hinge->set_position(Vector3(0.0f, 0.015f, -0.175f));
		shell->add_child(hinge);

		shells.push_back(shell);
		collected.push_back(false);
	}
	update_counter();
}

void ShellField::collect(int p_index, const Vector3 &p_player_pos) {
	if (p_index < 0 || p_index >= (int)shells.size() || collected[p_index]) {
		return;
	}
	collected[p_index] = true;
	shells[p_index]->mark_taken(true);
	update_counter();

	const int got = get_collected_count();
	const int total = (int)collected.size();
	if (chime != nullptr) {
		chime->set_global_position(p_player_pos + Vector3(0.0f, 0.8f, 0.0f));
		chime->set_pitch_scale(got >= total ? 1.12f : 1.0f);
		chime->set_volume_db(got >= total ? -5.0f : -8.0f);
		chime->play(0.0f);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			if (got >= total) {
				build_reward();
				gd->call("flash_hint", String::utf8("十枚贝壳集齐了 · 长椅旁挂起一串贝壳风铃"), 5.0f);
			} else {
				gd->call("flash_hint", String::utf8("拾到一枚贝壳（") + String::num_int64(got) + String::utf8(" / ")
						+ String::num_int64(total) + String::utf8("）"), 2.4f);
			}
		}
	}
}

void ShellField::update_counter() {
	if (counter == nullptr) {
		// 顶部左侧小木牌：与目标栏错开一行
		hud = memnew(CanvasLayer);
		hud->set_name("ShellHUD");
		hud->set_layer(6);
		add_child(hud);
		PanelContainer *panel = memnew(PanelContainer);
		panel->set_name("ShellCounterPanel");
		panel->set_position(Vector2(12.0f, 96.0f));
		panel->add_theme_stylebox_override("panel", uistyle::make_wood_textured(9.0f, 0.95f));
		hud->add_child(panel);
		counter = memnew(Label);
		counter->set_name("ShellCounter");
		counter->add_theme_font_size_override("font_size", 18);
		counter->add_theme_color_override("font_color", Color(1.0f, 0.95f, 0.80f));
		counter->add_theme_color_override("font_outline_color", Color(0.12f, 0.08f, 0.04f));
		counter->add_theme_constant_override("outline_size", 4);
		Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
		if (cjk.is_valid()) {
			counter->add_theme_font_override("font", cjk);
		}
		panel->add_child(counter);
	}
	counter->set_text(String::utf8("贝壳  ") + String::num_int64(get_collected_count()) + String::utf8(" / ")
			+ String::num_int64((int)collected.size()));
}

int ShellField::get_collected_count() const {
	int n = 0;
	for (size_t i = 0; i < collected.size(); i++) {
		if (collected[i]) {
			n++;
		}
	}
	return n;
}

String ShellField::get_collected_mask() const {
	String mask;
	for (size_t i = 0; i < collected.size(); i++) {
		mask += String(collected[i] ? "1" : "0");
	}
	return mask;
}

void ShellField::apply_collected_mask(const String &p_mask) {
	if ((int)p_mask.length() != (int)collected.size()) {
		return; // 旧档/缺键：保持全部未拾取
	}
	for (size_t i = 0; i < collected.size(); i++) {
		bool on = p_mask[(int)i] == '1';
		collected[i] = on;
		shells[i]->mark_taken(on);
	}
	restored_mask = true;
	update_counter();
	if (get_collected_count() == (int)collected.size()) {
		build_reward();
	}
}

// 集齐奖励：长椅灯柱旁挂一串贝壳风铃（重复调用只建一次）
void ShellField::build_reward() {
	if (reward != nullptr || get_parent() == nullptr) {
		return;
	}
	Node3D *bench = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Bench"));
	if (bench == nullptr) {
		return;
	}
	Ref<Shader> toon = meshutil::load_toon_shader();
	reward = memnew(Node3D);
	reward->set_name("ShellChime");
	bench->add_child(reward);
	reward->set_position(Vector3(0.55f, 1.12f, 1.02f));

	const Color wood(0.38f, 0.26f, 0.14f);
	meshutil::add_box(reward, toon, Vector3(0.36f, 0.035f, 0.035f), Vector3(0, 0, 0), wood);
	const Color tints[5] = {
		Color(0.96f, 0.88f, 0.76f),
		Color(0.98f, 0.78f, 0.72f),
		Color(0.90f, 0.82f, 0.92f),
		Color(0.98f, 0.93f, 0.80f),
		Color(0.86f, 0.90f, 0.86f),
	};
	for (int i = 0; i < 5; i++) {
		float x = -0.14f + (float)i * 0.07f;
		float len = 0.22f + (float)((i * 7) % 5) * 0.028f;
		meshutil::add_box(reward, toon, Vector3(0.008f, len, 0.008f), Vector3(x, -len * 0.5f - 0.02f, 0), Color(0.72f, 0.66f, 0.52f));
		Ref<SphereMesh> shell_mesh;
		shell_mesh.instantiate();
		shell_mesh->set_radius(0.052f);
		shell_mesh->set_height(0.104f);
		shell_mesh->set_radial_segments(10);
		shell_mesh->set_rings(5);
		shell_mesh->set_material(meshutil::toon_material(toon, tints[i], 0.5f));
		MeshInstance3D *sm = memnew(MeshInstance3D);
		sm->set_name(String("ChimeShell") + String::num_int64(i));
		sm->set_mesh(shell_mesh);
		sm->set_position(Vector3(x, -len - 0.05f, 0));
		sm->set_scale(Vector3(1.0f, 0.5f, 0.9f));
		reward->add_child(sm);
	}
	reward_voice = memnew(AudioStreamPlayer3D);
	reward_voice->set_name("ShellChimeVoice");
	reward_voice->set_stream(snd_chime);
	reward_voice->set_volume_db(-17.0f);
	reward_voice->set_max_distance(22.0f);
	reward->add_child(reward_voice);
}

// 0.55 秒的小风铃：两个纯音先后亮起，尾音叠在一起
Ref<AudioStreamWAV> ShellField::make_chime() {
	const int sr = 22050;
	const float dur = 0.62f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	const float f1 = 1318.5f; // E6
	const float f2 = 1975.5f; // B6
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float a1 = std::exp(-5.5f * t);
		float a2 = t > 0.13f ? std::exp(-5.5f * (t - 0.13f)) : 0.0f;
		float env = Math::clamp(t / 0.006f, 0.0f, 1.0f);
		float samp = (std::sin(6.2831853f * f1 * t) * a1 * 0.5f
				+ std::sin(6.2831853f * f2 * t) * a2 * 0.38f
				+ std::sin(6.2831853f * f1 * 2.0f * t) * a1 * 0.10f) * env;
		int16_t q = (int16_t)Math::clamp(samp * 15000.0f, -32000.0f, 32000.0f);
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
