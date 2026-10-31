#include "sprinkler.h"
#include <cmath>
#include <random>
#include <vector>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void Sprinkler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Sprinkler::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Sprinkler::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Sprinkler::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Sprinkler::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Sprinkler::interact);
	ClassDB::bind_method(D_METHOD("get_on"), &Sprinkler::get_on);
	ClassDB::bind_method(D_METHOD("get_uses"), &Sprinkler::get_uses);
}
String Sprinkler::get_prompt() const { return on ? String::utf8("按 E 关掉洒水器") : String::utf8("按 E 打开洒水器"); }
Vector3 Sprinkler::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.5f, 0.0f); }
void Sprinkler::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	on = !on;
	if (on) { on_t = 22.0f; uses++; }
	if (spray != nullptr) spray->set_emitting(on);
	if (voice != nullptr) { if (on) voice->play(0.0f); else voice->stop(); }
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", on ? String::utf8("洒水器转起来 · 水珠在阳光里发亮") : String::utf8("洒水器停了 · 叶子上还挂着水"), 2.6f);
	}
}
void Sprinkler::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (on) {
				on_t -= delta;
				if (head != nullptr) head->set_rotation(Vector3(0.0f, head->get_rotation().y + delta * 2.2f, 0.0f));
				if (on_t <= 0.0f) {
					on = false;
					if (spray != nullptr) spray->set_emitting(false);
					if (voice != nullptr) voice->stop();
				}
			}
		} break;
		default: break;
	}
}
void Sprinkler::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 三脚小架
	meshutil::add_box(this, toon, Vector3(0.06f, 0.40f, 0.06f), Vector3(0.0f, 0.20f, 0.0f), Color(0.50f, 0.52f, 0.54f));
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.094f;
		meshutil::add_box(this, toon, Vector3(0.03f, 0.30f, 0.03f),
				Vector3(std::cos(a) * 0.10f, 0.14f, std::sin(a) * 0.10f), Color(0.40f, 0.42f, 0.44f),
				Vector3(std::sin(a) * 0.35f, 0.0f, -std::cos(a) * 0.35f));
	}
	// 旋转喷头
	head = memnew(Node3D);
	head->set_name("SprinklerHead");
	head->set_position(Vector3(0.0f, 0.44f, 0.0f));
	add_child(head);
	Ref<CylinderMesh> hub;
	hub.instantiate();
	hub->set_top_radius(0.05f);
	hub->set_bottom_radius(0.06f);
	hub->set_height(0.07f);
	hub->set_radial_segments(8);
	hub->set_rings(1);
	hub->set_material(meshutil::toon_material(toon, Color(0.62f, 0.58f, 0.30f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(hub);
	head->add_child(hmi);
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(head, toon, Vector3(0.30f, 0.025f, 0.025f), Vector3((i == 0 ? 0.15f : -0.15f), 0.02f, 0.0f), Color(0.66f, 0.62f, 0.32f));
	}
	// 水花粒子
	Ref<SphereMesh> drop;
	drop.instantiate();
	drop->set_radius(0.012f);
	drop->set_height(0.024f);
	drop->set_radial_segments(4);
	drop->set_rings(2);
	Ref<StandardMaterial3D> dm;
	dm.instantiate();
	dm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dm->set_albedo(Color(0.80f, 0.92f, 1.0f, 0.55f));
	dm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	drop->set_material(dm);
	spray = memnew(CPUParticles3D);
	spray->set_name("Spray");
	spray->set_amount(70);
	spray->set_lifetime(1.1f);
	spray->set_mesh(drop);
	spray->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	spray->set_emission_sphere_radius(0.06f);
	spray->set_direction(Vector3(0.0f, 0.35f, 0.0f));
	spray->set_spread(80.0f);
	spray->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.2f);
	spray->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 2.2f);
	spray->set_gravity(Vector3(0.0f, -3.2f, 0.0f));
	spray->set_position(Vector3(0.0f, 0.47f, 0.0f));
	spray->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	spray->set_emitting(false);
	add_child(spray);
	build_sound();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("SprayVoice");
	voice->set_stream(snd);
	voice->set_volume_db(-15.0f);
	voice->set_max_distance(18.0f);
	add_child(voice);
}
void Sprinkler::build_sound() {
	const int sr = 22050;
	const int n = sr * 2;
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(4411u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.25f;
		const float env = 0.7f + 0.3f * std::sin(tt * 12.0f);
		buf[i] = (white - lp) * 0.5f * env;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 18000.0f, -18000.0f, 18000.0f);
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
	snd = wav;
}
