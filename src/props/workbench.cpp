#include "workbench.h"
#include <cmath>
#include <random>
#include <vector>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void Workbench::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Workbench::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Workbench::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Workbench::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Workbench::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Workbench::interact);
	ClassDB::bind_method(D_METHOD("get_uses"), &Workbench::get_uses);
	ClassDB::bind_method(D_METHOD("is_working"), &Workbench::is_working);
}
String Workbench::get_prompt() const { return String::utf8("按 E 推两下刨子"); }
Vector3 Workbench::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.95f, 0.0f); }
void Workbench::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	uses++;
	work_t = 1.0f;
	if (voice != nullptr && snd.is_valid()) voice->play(0.0f);
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("推了两下刨子 · 刨花打着卷落在脚边"), 2.8f);
	}
}
void Workbench::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (work_t > 0.0f) work_t = Math::max(0.0f, work_t - delta);
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (plane != nullptr) {
				const float push = work_t > 0.0f ? std::sin(work_t * 20.0f) * 0.16f : 0.0f;
				plane->set_position(Vector3(-0.28f + push, 0.985f, 0.06f));
			}
			if (shavings != nullptr) shavings->set_rotation(Vector3(0.0f, std::sin(t * 1.4f) * 0.05f, 0.0f));
			if (snow_cap != nullptr) snow_cap->set_visible(sky != nullptr && sky->get_snow_amount() > 0.35f);
		} break;
		default: break;
	}
}
void Workbench::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, -0.7f, 0.0f));
	// 台面与桌腿
	meshutil::add_box(this, toon, Vector3(1.70f, 0.10f, 0.62f), Vector3(0.0f, 0.91f, 0.0f), Color(0.56f, 0.40f, 0.24f));
	for (int i = 0; i < 4; i++) {
		const float x = (i % 2 == 0) ? -0.72f : 0.72f;
		const float z = (i < 2) ? -0.24f : 0.24f;
		meshutil::add_box(this, toon, Vector3(0.09f, 0.88f, 0.09f), Vector3(x, 0.44f, z), Color(0.46f, 0.32f, 0.20f));
	}
	meshutil::add_box(this, toon, Vector3(1.52f, 0.06f, 0.06f), Vector3(0.0f, 0.26f, -0.24f), Color(0.44f, 0.31f, 0.19f));
	meshutil::add_box(this, toon, Vector3(1.52f, 0.06f, 0.06f), Vector3(0.0f, 0.26f, 0.24f), Color(0.44f, 0.31f, 0.19f));
	// 台钳（左端）
	meshutil::add_box(this, toon, Vector3(0.26f, 0.12f, 0.18f), Vector3(-0.62f, 1.02f, 0.12f), Color(0.36f, 0.38f, 0.38f));
	meshutil::add_box(this, toon, Vector3(0.06f, 0.14f, 0.22f), Vector3(-0.47f, 1.02f, 0.12f), Color(0.30f, 0.32f, 0.32f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.02f, 0.02f), Vector3(-0.40f, 0.99f, 0.12f), Color(0.44f, 0.44f, 0.42f), Vector3(0.0f, 0.0f, 0.3f));
	// 手刨
	plane = memnew(Node3D);
	plane->set_name("Plane");
	plane->set_position(Vector3(-0.28f, 0.985f, 0.06f));
	add_child(plane);
	meshutil::add_box(plane, toon, Vector3(0.30f, 0.07f, 0.09f), Vector3(0.0f, 0.0f, 0.0f), Color(0.52f, 0.36f, 0.22f));
	meshutil::add_box(plane, toon, Vector3(0.03f, 0.10f, 0.02f), Vector3(0.03f, 0.06f, 0.0f), Color(0.60f, 0.62f, 0.62f), Vector3(0.0f, 0.0f, -0.5f));
	meshutil::add_box(plane, toon, Vector3(0.05f, 0.07f, 0.025f), Vector3(-0.10f, 0.07f, 0.0f), Color(0.34f, 0.26f, 0.18f));
	// 锯子靠在台腿边
	meshutil::add_box(this, toon, Vector3(0.02f, 0.62f, 0.11f), Vector3(0.60f, 0.55f, 0.36f), Color(0.66f, 0.68f, 0.68f), Vector3(0.14f, 0.0f, 0.06f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.14f, 0.10f), Vector3(0.66f, 0.90f, 0.36f), Color(0.42f, 0.28f, 0.18f), Vector3(0.14f, 0.0f, 0.06f));
	// 刨花：桌面上几卷 + 地上
	shavings = memnew(Node3D);
	shavings->set_name("Shavings");
	shavings->set_position(Vector3(0.10f, 0.97f, -0.02f));
	add_child(shavings);
	for (int i = 0; i < 5; i++) {
		const float a = (float)i * 1.3f;
		meshutil::add_box(shavings, toon, Vector3(0.12f, 0.012f, 0.030f), Vector3(std::cos(a) * 0.10f, 0.01f * (float)(i % 2), std::sin(a) * 0.08f), Color(0.80f, 0.64f, 0.40f), Vector3(0.0f, a, 0.12f));
	}
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(this, toon, Vector3(0.10f, 0.01f, 0.026f), Vector3(-0.5f + 0.22f * (float)i, 0.03f, 0.48f + 0.06f * (float)(i % 2)), Color(0.76f, 0.60f, 0.38f), Vector3(0.0f, 0.4f * (float)i, 0.1f));
	}
	// 雪帽
	snow_cap = meshutil::add_box(this, toon, Vector3(1.72f, 0.03f, 0.64f), Vector3(0.0f, 0.975f, 0.0f), Color(0.94f, 0.96f, 0.98f));
	snow_cap->set_visible(false);
	build_sound();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_stream(snd);
	voice->set_unit_size(3.0f);
	voice->set_max_distance(12.0f);
	voice->set_position(Vector3(0.0f, 0.9f, 0.0f));
	add_child(voice);
}
void Workbench::build_sound() {
	const int sr = 22050;
	const int n = sr / 2;
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(6613u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.18f;
		const float stroke = 0.6f + 0.4f * std::sin(6.28318f * 3.5f * tt);
		const float env = std::sin(3.14159f * tt / 0.5f);
		buf[i] = (white - lp * 0.7f) * stroke * env * 0.6f;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 13000.0f, -13000.0f, 13000.0f);
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
