#include "rocking_chair.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"

using namespace godot;

void RockingChair::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &RockingChair::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &RockingChair::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &RockingChair::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &RockingChair::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &RockingChair::interact);
	ClassDB::bind_method(D_METHOD("is_ridden"), &RockingChair::is_ridden);
	ClassDB::bind_method(D_METHOD("get_angle"), &RockingChair::get_angle);
	ClassDB::bind_method(D_METHOD("get_creak_ready"), &RockingChair::get_creak_ready);
}

void RockingChair::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
			build();
			make_creak();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (rider != nullptr && !rider->get_is_sitting()) {
				rider = nullptr;
				e_prev = Input::get_singleton()->is_key_pressed(KEY_E);
				return;
			}
			if (rider != nullptr) {
				bool e = Input::get_singleton()->is_key_pressed(KEY_E);
				if (e && !e_prev) {
					stand_up();
					e_prev = e;
					return;
				}
				e_prev = e;
			}
			float target = (rider != nullptr) ? std::sin(t * 1.55f) * 0.075f : 0.0f;
			angle = Math::lerp(angle, target, 1.0f - std::exp(-3.0f * delta));
			if (rocker != nullptr) {
				rocker->set_rotation(Vector3(angle, 0.0f, 0.0f));
			}
			if (rider != nullptr && rocker != nullptr) {
				Vector3 seat = rocker->get_global_transform().xform(Vector3(0.0f, 0.52f, 0.0f));
				rider->sit_down(seat, get_global_rotation().y);
				creak_t -= delta;
				if (creak_t <= 0.0f) {
					if (voice != nullptr && std::fabs(angle) > 0.02f) {
						voice->set_global_position(get_global_position() + Vector3(0.0f, 0.5f, 0.0f));
						voice->set_pitch_scale(0.92f + (float)(rand() % 100) * 0.0015f);
						voice->set_volume_db(-21.0f);
						voice->play(0.0f);
					}
					creak_t = 3.2f + (float)(rand() % 100) * 0.02f;
				}
			}
		} break;
		default:
			break;
	}
}

String RockingChair::get_prompt() const {
	return rider != nullptr ? String() : String::utf8("按 E 坐摇椅");
}

Vector3 RockingChair::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.7f, 0.0f);
}

float RockingChair::get_interact_distance() const {
	return 2.4f;
}

void RockingChair::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (rider != nullptr || get_parent() == nullptr) {
		return;
	}
	Player *p = Object::cast_to<Player>(get_parent()->get_node_or_null("Player"));
	if (p == nullptr || p->get_is_boating() || p->get_photo_mode() || p->get_is_lying()) {
		return;
	}
	rider = p;
	e_prev = true;
	if (rocker != nullptr) {
		Vector3 seat = rocker->get_global_transform().xform(Vector3(0.0f, 0.52f, 0.0f));
		p->sit_down(seat, get_global_rotation().y);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("摇椅慢慢晃 · 再按 E 起身"), 3.0f);
		}
	}
}

void RockingChair::stand_up() {
	if (rider == nullptr) {
		return;
	}
	Player *p = rider;
	Vector3 out = get_global_position() + Basis(Vector3(0.0f, 1.0f, 0.0f), get_global_rotation().y).xform(Vector3(0.0f, 0.02f, 1.0f));
	p->set_global_position(out);
	p->stand_up();
	p->suppress_interaction(0.4f);
	rider = nullptr;
}

void RockingChair::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.44f, 0.30f, 0.16f);
	const Color wood_light(0.58f, 0.42f, 0.24f);
	const Color wood_dark(0.32f, 0.21f, 0.11f);

	rocker = memnew(Node3D);
	rocker->set_name("Rocker");
	add_child(rocker);

	// 两道弧形摇杆（用长条木近似）
	for (int i = 0; i < 2; i++) {
		float x = (i == 0) ? -0.25f : 0.25f;
		meshutil::add_box(rocker, toon, Vector3(0.07f, 0.035f, 0.90f), Vector3(x, 0.035f, 0.0f), wood_dark);
	}
	// 四条腿
	for (int i = 0; i < 4; i++) {
		float x = (i < 2) ? -0.22f : 0.22f;
		float z = (i % 2 == 0) ? 0.18f : -0.18f;
		meshutil::add_box(rocker, toon, Vector3(0.055f, 0.42f, 0.055f), Vector3(x, 0.24f, z), wood);
	}
	// 坐面与靠背
	meshutil::add_box(rocker, toon, Vector3(0.50f, 0.06f, 0.44f), Vector3(0.0f, 0.45f, 0.0f), wood_light);
	meshutil::add_box(rocker, toon, Vector3(0.50f, 0.58f, 0.06f), Vector3(0.0f, 0.74f, -0.21f), wood_light, Vector3(-0.12f, 0.0f, 0.0f));
	meshutil::add_box(rocker, toon, Vector3(0.50f, 0.07f, 0.05f), Vector3(0.0f, 1.02f, -0.25f), wood);
	// 扶手
	for (int i = 0; i < 2; i++) {
		float x = (i == 0) ? -0.27f : 0.27f;
		meshutil::add_box(rocker, toon, Vector3(0.055f, 0.05f, 0.38f), Vector3(x, 0.58f, -0.02f), wood);
		meshutil::add_box(rocker, toon, Vector3(0.045f, 0.16f, 0.045f), Vector3(x, 0.50f, 0.16f), wood_dark);
	}
	// 靠背横条
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(rocker, toon, Vector3(0.46f, 0.05f, 0.035f), Vector3(0.0f, 0.68f + (float)i * 0.15f, -0.225f), wood);
	}
}

void RockingChair::make_creak() {
	snd_creak = synth_creak();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("ChairCreak");
	voice->set_stream(snd_creak);
	voice->set_volume_db(-21.0f);
	voice->set_max_distance(18.0f);
	add_child(voice);
}

// 0.4 秒的木头摇动吱呀
Ref<AudioStreamWAV> RockingChair::synth_creak() {
	const int sr = 22050;
	const float dur = 0.4f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1103515245 + 12345) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.07f;
		float f = 300.0f + 120.0f * std::sin(6.2831853f * 1.8f * t);
		float env = Math::clamp(t / 0.03f, 0.0f, 1.0f) * Math::clamp((dur - t) / 0.12f, 0.0f, 1.0f);
		float samp = (std::sin(6.2831853f * f * t) * 0.22f + (white - lp) * 0.7f) * env * 0.5f;
		int16_t q = (int16_t)Math::clamp(samp * 10000.0f, -32000.0f, 32000.0f);
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
