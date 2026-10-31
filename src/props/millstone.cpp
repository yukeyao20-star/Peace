#include "millstone.h"
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

void Millstone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Millstone::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Millstone::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Millstone::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Millstone::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Millstone::interact);
	ClassDB::bind_method(D_METHOD("get_grinding"), &Millstone::get_grinding);
	ClassDB::bind_method(D_METHOD("get_flour_level"), &Millstone::get_flour_level);
	ClassDB::bind_method(D_METHOD("get_grinds"), &Millstone::get_grinds);
}
String Millstone::get_prompt() const { return grinding ? String::utf8("石轮吱呀吱呀地转…") : String::utf8("按 E 推一推石磨"); }
Vector3 Millstone::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.7f, 0.0f); }
void Millstone::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (grinding) return;
	grinding = true;
	grind_t = 0.0f;
	grinds++;
	if (voice != nullptr && snd.is_valid()) voice->play(0.0f);
}
void Millstone::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (grinding) {
				grind_t += delta;
				if (wheel != nullptr) wheel->set_rotation(Vector3(wheel->get_rotation().x, wheel->get_rotation().y, wheel->get_rotation().z - delta * 5.0f));
				if (crank != nullptr) crank->set_rotation(Vector3(0.0f, 0.0f, std::sin(grind_t * 5.0f) * 0.6f));
				if (grind_t > 2.0f) {
					grinding = false;
					flour_level = Math::min(1.0f, flour_level + 0.25f);
					if (flour != nullptr) flour->set_scale(Vector3(0.4f + 0.6f * flour_level, 0.4f + 0.6f * flour_level, 0.4f + 0.6f * flour_level));
					if (get_tree() != nullptr) {
						Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
						if (gd != nullptr) gd->call("flash_hint", String::utf8("磨出一些面粉 · 槽里积了一小堆"), 2.6f);
					}
				}
			}
		} break;
		default: break;
	}
}
void Millstone::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 木架
	const Color wood(0.50f, 0.35f, 0.21f);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.90f, 0.10f), Vector3(-0.34f, 0.45f, -0.20f), wood);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.90f, 0.10f), Vector3(0.34f, 0.45f, -0.20f), wood);
	// 石轮
	Ref<CylinderMesh> wheel_mesh;
	wheel_mesh.instantiate();
	wheel_mesh->set_top_radius(0.38f);
	wheel_mesh->set_bottom_radius(0.38f);
	wheel_mesh->set_height(0.12f);
	wheel_mesh->set_radial_segments(16);
	wheel_mesh->set_rings(1);
	wheel_mesh->set_material(meshutil::toon_material(toon, Color(0.56f, 0.54f, 0.50f), 0.2f));
	wheel = memnew(Node3D);
	wheel->set_name("StoneWheel");
	wheel->set_position(Vector3(0.0f, 0.55f, 0.0f));
	wheel->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(wheel);
	MeshInstance3D *wmi = memnew(MeshInstance3D);
	wmi->set_mesh(wheel_mesh);
	wheel->add_child(wmi);
	// 轴与摇柄
	meshutil::add_box(this, toon, Vector3(0.10f, 0.10f, 0.55f), Vector3(0.0f, 0.55f, 0.0f), Color(0.42f, 0.40f, 0.36f));
	crank = memnew(Node3D);
	crank->set_name("Crank");
	crank->set_position(Vector3(0.0f, 0.55f, 0.30f));
	add_child(crank);
	meshutil::add_box(crank, toon, Vector3(0.05f, 0.05f, 0.20f), Vector3(0.0f, 0.10f, 0.0f), Color(0.46f, 0.32f, 0.20f));
	// 木槽
	meshutil::add_box(this, toon, Vector3(0.70f, 0.10f, 0.30f), Vector3(0.0f, 0.15f, 0.45f), Color(0.54f, 0.38f, 0.24f));
	// 面粉堆
	Ref<SphereMesh> fm;
	fm.instantiate();
	fm->set_radius(0.16f);
	fm->set_height(0.20f);
	fm->set_radial_segments(8);
	fm->set_rings(4);
	fm->set_material(meshutil::toon_material(toon, Color(0.95f, 0.93f, 0.87f), 0.3f));
	flour = memnew(MeshInstance3D);
	flour->set_mesh(fm);
	flour->set_position(Vector3(0.0f, 0.22f, 0.45f));
	flour->set_scale(Vector3(0.4f, 0.4f, 0.4f));
	add_child(flour);
	build_sound();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("GrindVoice");
	voice->set_stream(snd);
	voice->set_volume_db(-10.0f);
	voice->set_max_distance(16.0f);
	add_child(voice);
}
void Millstone::build_sound() {
	const int sr = 22050;
	const int n = (int)(2.0f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(7137u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	float ph = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float env = Math::clamp(tt / 0.1f, 0.0f, 1.0f) * Math::clamp((2.0f - tt) / 0.2f, 0.0f, 1.0f);
		const float f = 95.0f + 20.0f * std::sin(tt * 18.0f);
		ph += 6.2831853f * f / (float)sr;
		lp += ((d01(rng) - 0.5f) - lp) * 0.08f;
		buf[i] = (std::sin(ph) * 0.35f + lp * 1.5f) * env * (0.7f + 0.3f * std::sin(tt * 6.0f));
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 26000.0f, -26000.0f, 26000.0f);
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
