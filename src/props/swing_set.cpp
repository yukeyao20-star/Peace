#include "swing_set.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void SwingSet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &SwingSet::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &SwingSet::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &SwingSet::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &SwingSet::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &SwingSet::interact);
	ClassDB::bind_method(D_METHOD("get_angle"), &SwingSet::get_angle);
	ClassDB::bind_method(D_METHOD("is_ridden"), &SwingSet::is_ridden);
	ClassDB::bind_method(D_METHOD("is_creak_ready"), &SwingSet::is_creak_ready);
}

void SwingSet::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
			if (terrain == nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
			}
			if (terrain != nullptr) {
				Vector3 p = get_global_position();
				p.y = terrain->get_terrain_height(p.x, p.z) + 0.02f;
				set_global_position(p);
			}
			add_to_group("interactables");
			build();
			make_creak();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (rider != nullptr && !rider->get_is_sitting()) {
				rider = nullptr;
				amp = 0.0f;
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
				amp = Math::min(0.55f, amp + delta * 0.12f);
			} else {
				amp = Math::max(0.0f, amp - delta * 0.45f);
			}
			// 单摆：角速度由重力回复项驱动，再叠一点阻尼
			vel += (-9.0f * std::sin(angle) - 0.55f * vel) * delta;
			angle += vel * delta;
			// 玩家在荡时补能量，维持振幅；没人时自然停下
			if (rider != nullptr) {
				float want = amp * std::sin(phase);
				phase += delta * 1.55f;
				angle = Math::lerp(angle, want, 1.0f - std::exp(-2.6f * delta));
				vel = vel * 0.92f;
			}
			if (pivot != nullptr) {
				pivot->set_rotation(Vector3(0.0f, 0.0f, angle));
			}
			if (rider != nullptr && pivot != nullptr) {
				Vector3 seat = pivot->get_global_transform().xform(Vector3(0.0f, -1.53f, 0.0f));
				rider->sit_down(seat + Vector3(0.0f, -0.30f, 0.0f), 1.5707964f);
				creak_t -= delta;
				if (creak_t <= 0.0f) {
					if (voice != nullptr && std::fabs(vel) > 0.25f) {
						voice->set_global_position(seat + Vector3(0.0f, 1.4f, 0.0f));
						voice->set_pitch_scale(0.92f + (float)(rand() % 100) * 0.0012f);
						voice->set_volume_db(-19.0f);
						voice->play(0.0f);
					}
					creak_t = 2.6f + (float)(rand() % 100) * 0.012f;
				}
			}
		} break;
		default:
			break;
	}
}

String SwingSet::get_prompt() const {
	return rider != nullptr ? String() : String::utf8("按 E 荡秋千");
}

Vector3 SwingSet::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.15f, 0.0f);
}

float SwingSet::get_interact_distance() const {
	return 2.8f;
}

void SwingSet::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (rider != nullptr || get_parent() == nullptr) {
		return;
	}
	Player *p = Object::cast_to<Player>(get_parent()->get_node_or_null("Player"));
	if (p == nullptr || p->get_is_boating() || p->get_photo_mode()) {
		return;
	}
	rider = p;
	if (pivot != nullptr) {
		Vector3 seat = pivot->get_global_transform().xform(Vector3(0.0f, -1.53f, 0.0f));
		p->sit_down(seat + Vector3(0.0f, -0.30f, 0.0f), 1.5707964f);
	}
	e_prev = true;
	amp = 0.16f;
	phase = 0.0f;
	vel = 0.6f;
	Node *gd = get_parent()->get_node_or_null("GameDirector");
	if (gd != nullptr) {
		gd->call("flash_hint", String::utf8("荡起来了 · 再按 E 下来"), 3.0f);
	}
}

void SwingSet::stand_up() {
	if (rider == nullptr) {
		return;
	}
	Player *p = rider;
	Vector3 out = get_global_position() + Vector3(0.0f, 0.02f, 1.35f);
	p->set_global_position(out);
	p->stand_up();
	p->suppress_interaction(0.4f);
	rider = nullptr;
	amp = 0.0f;
}

void SwingSet::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color bark(0.40f, 0.27f, 0.14f);
	const Color bark_light(0.54f, 0.39f, 0.22f);
	const Color rope(0.78f, 0.70f, 0.52f);

	for (int i = 0; i < 2; i++) {
		float z = (i == 0) ? -1.05f : 1.05f;
		meshutil::add_box(this, toon, Vector3(0.13f, 2.65f, 0.13f), Vector3(0.0f, 1.32f, z), bark);
		// 斜撑
		meshutil::add_box(this, toon, Vector3(0.10f, 1.55f, 0.10f), Vector3(-0.55f, 0.78f, z), bark,
				Vector3(0.0f, 0.0f, 0.62f));
		meshutil::add_box(this, toon, Vector3(0.10f, 1.55f, 0.10f), Vector3(0.55f, 0.78f, z), bark,
				Vector3(0.0f, 0.0f, -0.62f));
	}
	meshutil::add_box(this, toon, Vector3(0.16f, 0.16f, 2.45f), Vector3(0.0f, 2.63f, 0.0f), bark_light);

	pivot = memnew(Node3D);
	pivot->set_name("SwingPivot");
	pivot->set_position(Vector3(0.0f, 2.60f, 0.0f));
	add_child(pivot);

	for (int i = 0; i < 2; i++) {
		float z = (i == 0) ? -0.55f : 0.55f;
		meshutil::add_box(pivot, toon, Vector3(0.025f, 1.52f, 0.025f), Vector3(0.0f, -0.76f, z), rope);
	}
	meshutil::add_box(pivot, toon, Vector3(0.56f, 0.09f, 0.64f), Vector3(0.0f, -1.53f, 0.0f), bark_light);
	meshutil::add_box(pivot, toon, Vector3(0.56f, 0.05f, 0.08f), Vector3(0.0f, -1.46f, -0.30f), bark);
	meshutil::add_box(pivot, toon, Vector3(0.56f, 0.05f, 0.08f), Vector3(0.0f, -1.46f, 0.30f), bark);
}

void SwingSet::make_creak() {
	snd_creak = synth_creak();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("SwingCreak");
	voice->set_stream(snd_creak);
	voice->set_volume_db(-19.0f);
	voice->set_max_distance(22.0f);
	add_child(voice);
}

// 0.45 秒的木轴吱呀：窄带噪声 + 缓慢滑动的音高
Ref<AudioStreamWAV> SwingSet::synth_creak() {
	const int sr = 22050;
	const float dur = 0.45f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1103515245 + 12345) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.08f;
		float f = 320.0f + 180.0f * std::sin(6.2831853f * 1.5f * t);
		float env = Math::clamp(t / 0.03f, 0.0f, 1.0f) * Math::clamp((dur - t) / 0.12f, 0.0f, 1.0f);
		float samp = (std::sin(6.2831853f * f * t) * 0.25f + (white - lp) * 0.8f) * env * 0.5f;
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
