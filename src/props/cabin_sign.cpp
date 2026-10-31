#include "cabin_sign.h"
#include <cmath>
#include <random>
#include <vector>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
using namespace godot;

void CabinSign::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &CabinSign::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &CabinSign::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &CabinSign::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &CabinSign::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &CabinSign::interact);
	ClassDB::bind_method(D_METHOD("get_knocks"), &CabinSign::get_knocks);
	ClassDB::bind_method(D_METHOD("is_knocking"), &CabinSign::is_knocking);
}
String CabinSign::get_prompt() const { return String::utf8("按 E 叩一叩门"); }
void CabinSign::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	knocks++;
	knock_t = 0.7f;
	if (voice != nullptr && snd.is_valid()) voice->play(0.0f);
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("笃、笃 · 门里像有人应了一声"), 2.6f);
	}
}
void CabinSign::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (knock_t > 0.0f) knock_t = Math::max(0.0f, knock_t - delta);
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (knocker != nullptr) {
				const float sway = knock_t > 0.0f ? std::sin(knock_t * 34.0f) * knock_t * 0.35f : 0.0f;
				knocker->set_rotation(Vector3(sway + std::sin(t * 0.9f) * 0.01f, 0.0f, 0.0f));
			}
			if (snow_cap != nullptr) snow_cap->set_visible(sky != nullptr && sky->get_snow_amount() > 0.35f);
		} break;
		default: break;
	}
}
void CabinSign::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 门楣木牌
	meshutil::add_box(this, toon, Vector3(0.88f, 0.26f, 0.045f), Vector3(0.0f, 1.07f, -0.05f), Color(0.58f, 0.42f, 0.26f));
	meshutil::add_box(this, toon, Vector3(0.80f, 0.20f, 0.010f), Vector3(0.0f, 1.07f, -0.026f), Color(0.56f, 0.40f, 0.24f));
	meshutil::add_box(this, toon, Vector3(0.02f, 0.02f, 0.02f), Vector3(-0.36f, 1.07f, -0.026f), Color(0.32f, 0.30f, 0.26f));
	meshutil::add_box(this, toon, Vector3(0.02f, 0.02f, 0.02f), Vector3(0.36f, 1.07f, -0.026f), Color(0.32f, 0.30f, 0.26f));
	Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	Label3D *label = memnew(Label3D);
	label->set_name("SignText");
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_DISABLED);
	label->set_pixel_size(0.0032f);
	label->set_font_size(56);
	label->set_text(String::utf8("平 安"));
	label->set_modulate(Color(0.92f, 0.86f, 0.70f));
	label->set_outline_modulate(Color(0.24f, 0.17f, 0.10f));
	label->set_outline_size(6);
	label->set_position(Vector3(0.0f, 1.07f, -0.018f));
	if (cjk.is_valid()) label->set_font(cjk);
	add_child(label);
	// 铜门环：底板 + 可摆动的圆环
	meshutil::add_box(this, toon, Vector3(0.13f, 0.17f, 0.025f), Vector3(0.30f, -0.16f, 0.02f), Color(0.52f, 0.44f, 0.24f));
	knocker = memnew(Node3D);
	knocker->set_name("Knocker");
	knocker->set_position(Vector3(0.30f, -0.18f, 0.05f));
	add_child(knocker);
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.045f);
	ring->set_outer_radius(0.060f);
	ring->set_rings(14);
	ring->set_ring_segments(4);
	ring->set_material(meshutil::toon_material(toon, Color(0.72f, 0.60f, 0.30f), 0.15f));
	MeshInstance3D *rmi = memnew(MeshInstance3D);
	rmi->set_mesh(ring);
	rmi->set_position(Vector3(0.0f, -0.06f, 0.0f));
	rmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	knocker->add_child(rmi);
	// 雪帽
	snow_cap = meshutil::add_box(this, toon, Vector3(0.90f, 0.03f, 0.08f), Vector3(0.0f, 1.215f, -0.05f), Color(0.94f, 0.96f, 0.98f));
	snow_cap->set_visible(false);
	build_sound();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_stream(snd);
	voice->set_unit_size(3.0f);
	voice->set_max_distance(12.0f);
	voice->set_position(Vector3(0.0f, 0.0f, 0.05f));
	add_child(voice);
}
void CabinSign::build_sound() {
	const int sr = 22050;
	const int n = sr * 6 / 10;
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(2207u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	const float hits[2] = { 0.0f, 0.17f };
	for (int h = 0; h < 2; h++) {
		const int start = (int)(hits[h] * (float)sr);
		const int len = (int)(0.16f * (float)sr);
		for (int i = 0; i < len && start + i < n; i++) {
			const float tt = (float)i / (float)sr;
			const float env = std::exp(-tt * 26.0f);
			const float thump = std::sin(6.28318f * (105.0f - 40.0f * tt) * tt);
			const float click = (d01(rng) - 0.5f) * std::exp(-tt * 90.0f);
			buf[start + i] += (thump * 0.55f + click * 0.35f) * env;
		}
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 15000.0f, -15000.0f, 15000.0f);
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
