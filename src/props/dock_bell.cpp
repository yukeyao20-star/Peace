#include "dock_bell.h"
#include <cmath>
#include <random>
#include <vector>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void DockBell::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &DockBell::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &DockBell::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &DockBell::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &DockBell::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &DockBell::interact);
	ClassDB::bind_method(D_METHOD("get_rings"), &DockBell::get_rings);
	ClassDB::bind_method(D_METHOD("get_ringing"), &DockBell::get_ringing);
}
String DockBell::get_prompt() const { return String::utf8("按 E 摇一下铜铃"); }
Vector3 DockBell::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 1.25f, 0.0f); }
void DockBell::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	rings++;
	ring_t = 2.4f;
	if (voice != nullptr && snd.is_valid()) voice->play(0.0f);
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("铜铃在水面上荡开 · 远处有鸟惊起"), 2.8f);
	}
}
void DockBell::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (ring_t > 0.0f) {
				ring_t -= delta;
				if (bell != nullptr) bell->set_rotation(Vector3(0.0f, 0.0f, std::sin(ring_t * 18.0f) * ring_t * 0.25f));
			} else if (bell != nullptr) {
				bell->set_rotation(Vector3());
			}
		} break;
		default: break;
	}
}
void DockBell::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	meshutil::add_box(this, toon, Vector3(0.12f, 1.50f, 0.12f), Vector3(0.0f, 0.75f, 0.0f), Color(0.50f, 0.35f, 0.21f));
	meshutil::add_box(this, toon, Vector3(0.44f, 0.08f, 0.08f), Vector3(0.12f, 1.45f, 0.0f), Color(0.46f, 0.32f, 0.20f));
	bell = memnew(Node3D);
	bell->set_name("Bell");
	bell->set_position(Vector3(0.30f, 1.42f, 0.0f));
	add_child(bell);
	Ref<CylinderMesh> bell_mesh;
	bell_mesh.instantiate();
	bell_mesh->set_top_radius(0.03f);
	bell_mesh->set_bottom_radius(0.09f);
	bell_mesh->set_height(0.14f);
	bell_mesh->set_radial_segments(10);
	bell_mesh->set_rings(1);
	bell_mesh->set_material(meshutil::toon_material(toon, Color(0.78f, 0.62f, 0.28f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(bell_mesh);
	bell->add_child(bmi);
	bmi->set_position(Vector3(0.0f, -0.08f, 0.0f));
	meshutil::add_sphere(bell, toon, 0.02f, Vector3(0.0f, -0.16f, 0.0f), Color(0.60f, 0.46f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.02f, 0.30f, 0.02f), Vector3(0.30f, 1.22f, 0.0f), Color(0.70f, 0.62f, 0.44f));
	build_sound();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("BellVoice");
	voice->set_stream(snd);
	voice->set_volume_db(-6.0f);
	voice->set_max_distance(60.0f);
	voice->set_position(Vector3(0.30f, 1.35f, 0.0f));
	add_child(voice);
}
void DockBell::build_sound() {
	const int sr = 22050;
	const int n = (int)(2.2f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	// 钟声：几个不成整数倍的分音
	const float partials[4] = { 620.0f, 930.0f, 1480.0f, 2210.0f };
	const float amps[4] = { 0.5f, 0.3f, 0.16f, 0.08f };
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float env = std::exp(-tt * 1.8f) * Math::clamp(tt / 0.005f, 0.0f, 1.0f);
		float v = 0.0f;
		for (int p = 0; p < 4; p++) v += std::sin(6.2831853f * partials[p] * tt) * amps[p] * std::exp(-tt * (1.0f + 0.5f * (float)p));
		buf[i] = v * env;
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
