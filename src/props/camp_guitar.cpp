#include "camp_guitar.h"
#include <cmath>
#include <vector>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
using namespace godot;

void CampGuitar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &CampGuitar::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &CampGuitar::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &CampGuitar::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &CampGuitar::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &CampGuitar::interact);
	ClassDB::bind_method(D_METHOD("get_playing"), &CampGuitar::get_playing);
	ClassDB::bind_method(D_METHOD("get_times"), &CampGuitar::get_times);
	ClassDB::bind_method(D_METHOD("get_tune_ready"), &CampGuitar::get_tune_ready);
}
String CampGuitar::get_prompt() const { return playing ? String::utf8("按 E 停下吉他") : String::utf8("按 E 弹一段吉他"); }
Vector3 CampGuitar::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.45f, 0.0f); }
void CampGuitar::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	playing = !playing;
	if (voice != nullptr) {
		if (playing) { voice->play(0.0f); times++; }
		else voice->stop();
	}
	if (notes != nullptr) notes->set_emitting(playing);
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", playing ? String::utf8("木吉他 · 四个和弦慢慢转") : String::utf8("吉他声停了 · 火堆还在响"), 2.6f);
	}
}
void CampGuitar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS:
			t += (float)get_process_delta_time();
			if (strings != nullptr && playing) strings->set_position(Vector3(0.0f, 0.0f, std::sin(t * 22.0f) * 0.0015f));
			break;
		default: break;
	}
}
void CampGuitar::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 琴身
	Node3D *g = memnew(Node3D);
	g->set_name("Guitar");
	g->set_position(Vector3(0.0f, 0.35f, 0.0f));
	g->set_rotation(Vector3(-0.35f, 0.4f, 0.12f));
	add_child(g);
	MeshInstance3D *lower = meshutil::add_sphere(g, toon, 0.20f, Vector3(0.0f, 0.0f, 0.0f), Color(0.58f, 0.34f, 0.18f));
	lower->set_scale(Vector3(1.0f, 0.35f, 1.2f));
	MeshInstance3D *upper = meshutil::add_sphere(g, toon, 0.15f, Vector3(0.0f, 0.0f, -0.22f), Color(0.54f, 0.31f, 0.16f));
	upper->set_scale(Vector3(1.0f, 0.35f, 1.1f));
	// 音孔
	Ref<CylinderMesh> hole;
	hole.instantiate();
	hole->set_top_radius(0.05f);
	hole->set_bottom_radius(0.05f);
	hole->set_height(0.005f);
	hole->set_radial_segments(10);
	hole->set_rings(1);
	Ref<StandardMaterial3D> hm;
	hm.instantiate();
	hm->set_albedo(Color(0.08f, 0.06f, 0.04f));
	hole->set_material(hm);
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(hole);
	hmi->set_position(Vector3(0.0f, 0.075f, 0.02f));
	g->add_child(hmi);
	// 琴颈与琴头
	meshutil::add_box(g, toon, Vector3(0.07f, 0.04f, 0.75f), Vector3(0.0f, 0.03f, -0.58f), Color(0.44f, 0.28f, 0.16f));
	meshutil::add_box(g, toon, Vector3(0.09f, 0.05f, 0.16f), Vector3(0.0f, 0.03f, -0.98f), Color(0.30f, 0.22f, 0.14f));
	// 琴弦（会颤的节点）
	strings = memnew(Node3D);
	strings->set_name("Strings");
	strings->set_position(Vector3(0.0f, 0.08f, 0.02f));
	g->add_child(strings);
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(strings, toon, Vector3(0.004f, 0.004f, 0.85f), Vector3(-0.015f + 0.015f * (float)i, 0.0f, -0.44f),
				Color(0.86f, 0.84f, 0.76f));
	}
	// 音符粒子
	Ref<SphereMesh> mote;
	mote.instantiate();
	mote->set_radius(0.016f);
	mote->set_height(0.032f);
	mote->set_radial_segments(5);
	mote->set_rings(3);
	Ref<StandardMaterial3D> mm;
	mm.instantiate();
	mm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mm->set_albedo(Color(1.0f, 0.86f, 0.50f, 0.8f));
	mm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	mote->set_material(mm);
	notes = memnew(CPUParticles3D);
	notes->set_name("NoteMotes");
	notes->set_amount(12);
	notes->set_lifetime(2.0f);
	notes->set_mesh(mote);
	notes->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	notes->set_spread(25.0f);
	notes->set_gravity(Vector3(0.02f, 0.10f, 0.0f));
	notes->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.08f);
	notes->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.22f);
	notes->set_position(Vector3(0.0f, 0.45f, 0.0f));
	notes->set_emitting(false);
	add_child(notes);
	// 程序化和弦
	tune = compose();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("GuitarVoice");
	voice->set_stream(tune);
	voice->set_volume_db(-11.0f);
	voice->set_unit_size(8.0f);
	voice->set_max_distance(26.0f);
	add_child(voice);
}
Ref<AudioStreamWAV> CampGuitar::compose() {
	const int sr = 22050;
	const float chord_len = 4.0f;
	const int chords = 4;
	const int n = (int)(chord_len * (float)chords * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	const float freqs[4][4] = {
		{ 130.81f, 164.81f, 196.00f, 261.63f }, // C
		{ 98.00f, 123.47f, 146.83f, 196.00f },  // G
		{ 110.00f, 130.81f, 164.81f, 220.00f }, // Am
		{ 87.31f, 110.00f, 130.81f, 174.61f },  // F
	};
	for (int c = 0; c < chords; c++) {
		const int cstart = (int)((float)c * chord_len * (float)sr);
		for (int p = 0; p < 4; p++) {
			const int start = cstart + (int)((float)p * 0.92f * (float)sr);
			const float dur = 1.6f;
			const int len = (int)(dur * (float)sr);
			for (int s = 0; s < len; s++) {
				const float tt = (float)s / (float)sr;
				const float env = std::exp(-tt * 3.0f) * Math::clamp(tt / 0.005f, 0.0f, 1.0f);
				float v = 0.0f;
				for (int note = 0; note < 4; note++) {
					const float f = freqs[c][note];
					// 轻微拨弦错开
					const float delay = (float)note * 0.015f;
					if (tt < delay) continue;
					const float td = tt - delay;
					v += (std::sin(6.2831853f * f * td) + 0.4f * std::sin(6.2831853f * f * 2.0f * td) + 0.15f * std::sin(6.2831853f * f * 3.0f * td)) * std::exp(-td * 1.8f);
				}
				const int idx = start + s;
				if (idx < n) buf[idx] += v * env * 0.06f;
			}
		}
	}
	float peak = 0.0001f;
	for (float v : buf) peak = Math::max(peak, std::abs(v));
	const float gain = 0.72f / peak;
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float v = buf[i] * gain;
		const int tail = n - i;
		if (tail < 2000) v *= (float)tail / 2000.0f;
		const int16_t q = (int16_t)Math::clamp(v * 32000.0f, -32000.0f, 32000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
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
