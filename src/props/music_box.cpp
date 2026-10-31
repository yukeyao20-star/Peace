#include "music_box.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"

using namespace godot;

void MusicBox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &MusicBox::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &MusicBox::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &MusicBox::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &MusicBox::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &MusicBox::interact);
	ClassDB::bind_method(D_METHOD("get_playing"), &MusicBox::get_playing);
	ClassDB::bind_method(D_METHOD("get_tune_ready"), &MusicBox::get_tune_ready);
	ClassDB::bind_method(D_METHOD("get_note_count"), &MusicBox::get_note_count);
	ClassDB::bind_method(D_METHOD("get_crank_angle"), &MusicBox::get_crank_angle);
}

float MusicBox::get_crank_angle() const {
	return (crank != nullptr) ? crank->get_rotation().x : 0.0f;
}

String MusicBox::get_prompt() const {
	return playing ? String::utf8("按 E 停下八音盒") : String::utf8("按 E 给八音盒上弦");
}

Vector3 MusicBox::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.55f, 0.0f);
}

void MusicBox::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (playing) {
		playing = false;
		if (voice != nullptr) {
			voice->stop();
		}
		if (notes_fx != nullptr) {
			notes_fx->set_emitting(false);
		}
	} else {
		playing = true;
		if (voice != nullptr && tune.is_valid()) {
			voice->play(0.0f);
		}
		if (notes_fx != nullptr) {
			notes_fx->restart();
			notes_fx->set_emitting(true);
		}
		if (get_parent() != nullptr) {
			Node *gd = get_parent()->get_node_or_null("GameDirector");
			if (gd != nullptr) {
				gd->call("flash_hint", String::utf8("上弦 · 八音盒叮叮咚咚地唱起来"), 3.0f);
				gd->call("on_music_box_played");
			}
		}
	}
}

void MusicBox::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (playing) {
				t += delta;
				if (crank != nullptr) {
					crank->set_rotation(Vector3(t * 7.0f, 0.0f, 0.0f));
				}
			}
		} break;
		default:
			break;
	}
}

void MusicBox::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.42f, 0.28f, 0.15f);
	const Color wood_light(0.58f, 0.42f, 0.24f);
	const Color brass(0.78f, 0.62f, 0.28f);

	// 小凳
	meshutil::add_box(this, toon, Vector3(0.16f, 0.30f, 0.16f), Vector3(0.0f, 0.15f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.46f, 0.04f, 0.40f), Vector3(0.0f, 0.32f, 0.0f), wood_light);

	// 盒身与盖
	meshutil::add_box(this, toon, Vector3(0.34f, 0.20f, 0.26f), Vector3(0.0f, 0.44f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(0.36f, 0.035f, 0.28f), Vector3(0.0f, 0.557f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.30f, 0.02f, 0.22f), Vector3(0.0f, 0.575f, 0.0f), Color(0.30f, 0.20f, 0.11f));

	// 露在前脸的一点黄铜滚筒
	Ref<CylinderMesh> drum;
	drum.instantiate();
	drum->set_top_radius(0.055f);
	drum->set_bottom_radius(0.055f);
	drum->set_height(0.28f);
	drum->set_radial_segments(12);
	drum->set_rings(1);
	drum->set_material(meshutil::toon_material(toon, brass, 0.5f));
	MeshInstance3D *dmi = memnew(MeshInstance3D);
	dmi->set_name("Drum");
	dmi->set_mesh(drum);
	dmi->set_position(Vector3(0.0f, 0.445f, 0.132f));
	dmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(dmi);
	// 滚筒上的小钉
	for (int i = 0; i < 10; i++) {
		const float a = (float)i * 0.628f;
		meshutil::add_sphere(this, toon, 0.008f,
				Vector3(-0.12f + (float)i * 0.027f, 0.445f + std::cos(a) * 0.055f, 0.132f + std::sin(a) * 0.055f),
				Color(0.62f, 0.50f, 0.24f));
	}

	// 摇柄：绕 X 轴转
	crank = memnew(Node3D);
	crank->set_name("Crank");
	crank->set_position(Vector3(0.21f, 0.47f, 0.0f));
	add_child(crank);
	meshutil::add_box(crank, toon, Vector3(0.14f, 0.02f, 0.02f), Vector3(0.07f, 0.0f, 0.0f), brass);
	meshutil::add_box(crank, toon, Vector3(0.025f, 0.10f, 0.025f), Vector3(0.14f, 0.05f, 0.0f), brass);

	// 飘起的金色音符（小光点）
	Ref<SphereMesh> mote;
	mote.instantiate();
	mote->set_radius(0.018f);
	mote->set_height(0.036f);
	mote->set_radial_segments(5);
	mote->set_rings(3);
	Ref<StandardMaterial3D> mm;
	mm.instantiate();
	mm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mm->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	mm->set_albedo(Color(1.0f, 0.88f, 0.52f, 0.85f));
	mm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	mote->set_material(mm);
	notes_fx = memnew(CPUParticles3D);
	notes_fx->set_name("NoteMotes");
	notes_fx->set_amount(14);
	notes_fx->set_lifetime(1.8f);
	notes_fx->set_mesh(mote);
	notes_fx->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	notes_fx->set_spread(28.0f);
	notes_fx->set_gravity(Vector3(0.03f, 0.12f, 0.02f));
	notes_fx->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	notes_fx->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.26f);
	notes_fx->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	notes_fx->set_emission_sphere_radius(0.12f);
	notes_fx->set_position(Vector3(0.0f, 0.62f, 0.0f));
	notes_fx->set_emitting(false);
	add_child(notes_fx);

	tune = compose_tune();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("MusicVoice");
	voice->set_stream(tune);
	voice->set_volume_db(-11.0f);
	voice->set_unit_size(9.0f);
	voice->set_max_distance(28.0f);
	add_child(voice);
}

// 16 小节的五声音阶小曲：LCG 随机游走 + 最大跳进 ±2 + 前半上行/后半回落的拱形轮廓。
Ref<AudioStreamWAV> MusicBox::compose_tune() {
	const int sr = 22050;
	const float step = 0.3125f; // 96 BPM 八分音符
	const int steps = 32;       // 10 秒
	const int n = (int)(step * (float)steps * (float)sr);
	std::vector<float> buf(n, 0.0f);

	static const int penta[5] = { 0, 2, 4, 7, 9 };
	uint32_t seed = 20261101u;
	auto lcg = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return (seed >> 8) & 0xFFFFu;
	};
	auto frnd = [&lcg]() { return (float)lcg() / 65535.0f; };

	int idx = 6;
	note_count = 0;
	for (int i = 0; i < steps; i++) {
		if (frnd() < 0.12f) {
			continue; // 留白
		}
		int leap = (int)(lcg() % 5u) - 2; // -2..2 级
		// 拱形轮廓：前半更容易上行，后半更容易回落
		if (i < steps / 2) {
			if (frnd() > 0.72f) leap += 1;
		} else {
			if (frnd() > 0.72f) leap -= 1;
		}
		idx = Math::clamp(idx + leap, 0, 9);
		const int midi = 60 + penta[idx % 5] + 12 * (idx / 5);
		const float freq = 440.0f * std::pow(2.0f, ((float)midi - 69.0f) / 12.0f);
		const int start = (int)((float)i * step * (float)sr);
		const float dur = step * (((lcg() % 2u) != 0u) ? 2.0f : 1.0f);
		const int len = (int)(dur * (float)sr);
		for (int s = 0; s < len && start + s < n; s++) {
			const float tt = (float)s / (float)sr;
			const float env = std::exp(-tt * 4.5f) * Math::clamp(tt / 0.006f, 0.0f, 1.0f);
			const float w = std::sin(6.2831853f * freq * tt)
					+ std::sin(6.2831853f * freq * 2.0f * tt) * 0.35f
					+ std::sin(6.2831853f * freq * 3.0f * tt) * 0.12f;
			buf[start + s] += w * env * 0.22f;
		}
		note_count++;
	}

	float peak = 0.0001f;
	for (float v : buf) {
		peak = Math::max(peak, std::abs(v));
	}
	const float gain = 0.72f / peak;
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *wd = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float v = buf[i] * gain;
		const int tail = n - i;
		if (tail < 1400) {
			v *= (float)tail / 1400.0f; // 循环接缝淡出
		}
		const int16_t q = (int16_t)Math::clamp(v * 32000.0f, -32000.0f, 32000.0f);
		wd[i * 2] = (uint8_t)(q & 0xFF);
		wd[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
	wav->set_loop_begin(0);
	wav->set_loop_end(n);
	return wav;
}
