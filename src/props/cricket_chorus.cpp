#include "cricket_chorus.h"
#include <cmath>
#include <random>
#include <vector>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../world/sky_cycle.h"
using namespace godot;

void CricketChorus::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_night"), &CricketChorus::get_night);
	ClassDB::bind_method(D_METHOD("get_hushed"), &CricketChorus::get_hushed);
	ClassDB::bind_method(D_METHOD("get_chirps"), &CricketChorus::get_chirps);
}
void CricketChorus::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) break;
			if (player == nullptr) player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			if (sky == nullptr) sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			night = sky != nullptr && (sky->get_time_of_day() < 0.24f || sky->get_time_of_day() > 0.76f);
			hushed = player != nullptr && player->get_global_position().distance_to(get_global_position()) < 2.6f;
			const bool can = night && !hushed && (sky == nullptr || sky->get_snow_amount() < 0.35f);
			for (int i = 0; i < COUNT; i++) {
				timers[i] -= delta;
				if (can && timers[i] <= 0.0f) {
					timers[i] = 1.6f + 0.9f * (float)i + 0.4f * std::sin((float)chirps + (float)i);
					if (voices[i] != nullptr && snd.is_valid()) {
						voices[i]->set_pitch_scale(0.96f + 0.05f * (float)((chirps + i) % 4));
						voices[i]->play(0.0f);
						chirps++;
					}
				}
			}
		} break;
		default: break;
	}
}
void CricketChorus::build() {
	build_sound();
	const Vector3 offsets[COUNT] = { Vector3(0.4f, 0.1f, 0.2f), Vector3(-0.5f, 0.1f, 0.5f), Vector3(0.1f, 0.1f, -0.6f) };
	for (int i = 0; i < COUNT; i++) {
		voices[i] = memnew(AudioStreamPlayer3D);
		voices[i]->set_name(String("Cricket") + String::num_int64(i));
		voices[i]->set_stream(snd);
		voices[i]->set_volume_db(-16.0f - 2.0f * (float)i);
		voices[i]->set_max_distance(22.0f);
		voices[i]->set_position(offsets[i]);
		add_child(voices[i]);
	}
}
void CricketChorus::build_sound() {
	const int sr = 22050;
	const int n = (int)(0.18f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(5511u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float ph = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		// 三个短脉冲
		float env = 0.0f;
		for (int p = 0; p < 3; p++) {
			const float center = 0.025f + 0.055f * (float)p;
			env += std::exp(-std::fabs(tt - center) * 160.0f);
		}
		const float f = 4200.0f + 300.0f * std::sin(tt * 40.0f);
		ph += 6.2831853f * f / (float)sr;
		buf[i] = std::sin(ph) * env * 0.5f + (d01(rng) - 0.5f) * env * 0.08f;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 22000.0f, -22000.0f, 22000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	snd = wav;
}
