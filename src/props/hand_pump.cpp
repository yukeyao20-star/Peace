#include "hand_pump.h"
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
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void HandPump::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &HandPump::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &HandPump::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &HandPump::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &HandPump::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &HandPump::interact);
	ClassDB::bind_method(D_METHOD("get_pumps"), &HandPump::get_pumps);
	ClassDB::bind_method(D_METHOD("is_flowing"), &HandPump::is_flowing);
	ClassDB::bind_method(D_METHOD("get_water_level"), &HandPump::get_water_level);
}
String HandPump::get_prompt() const { return String::utf8("按 E 压水"); }
Vector3 HandPump::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.85f, 0.0f); }
void HandPump::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	pumps++;
	flow_t = 1.6f;
	water_level = Math::min(1.0f, water_level + 0.22f);
	if (voice != nullptr && snd.is_valid()) voice->play(0.0f);
}
void HandPump::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (flow_t > 0.0f) flow_t = Math::max(0.0f, flow_t - delta);
			const bool flowing = flow_t > 0.0f;
			if (handle != nullptr) {
				const float base = 0.55f;
				const float swing = flowing ? std::sin(flow_t * 26.0f) * 0.42f : 0.0f;
				const float settle = flowing ? 0.0f : std::sin(t * 0.8f) * 0.015f;
				handle->set_rotation(Vector3(0.0f, 0.0f, base + swing + settle));
			}
			if (stream != nullptr) {
				stream->set_visible(flowing);
				if (flowing) {
					const float len = 0.30f + 0.03f * std::sin(t * 31.0f);
					stream->set_scale(Vector3(1.0f, len / 0.30f, 1.0f));
				}
			}
			if (water != nullptr) {
				water->set_visible(true);
				water->set_scale(Vector3(1.0f, 1.0f, 0.45f + 0.55f * water_level));
				water->set_position(Vector3(0.47f, 0.135f + 0.09f * water_level, 0.0f));
			}
			if (ripple != nullptr) {
				const bool rv = flowing && water_level > 0.2f;
				ripple->set_visible(rv);
				if (rv) {
					const float ph = std::fmod(t * 1.6f, 1.0f);
					const float s = 0.35f + 1.4f * ph;
					ripple->set_scale(Vector3(s, 1.0f, s));
					Ref<StandardMaterial3D> rm = ripple->get_material_override();
					if (rm.is_valid()) rm->set_albedo(Color(0.80f, 0.90f, 0.96f, 0.55f * (1.0f - ph)));
				}
			}
			if (!flowing) water_level = Math::max(0.12f, water_level - delta * 0.012f);
		} break;
		default: break;
	}
}
void HandPump::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 1.5707964f, 0.0f));
	// 石基与木槽
	meshutil::add_box(this, toon, Vector3(0.52f, 0.10f, 0.52f), Vector3(0.0f, 0.05f, 0.0f), Color(0.52f, 0.50f, 0.46f));
	meshutil::add_box(this, toon, Vector3(0.62f, 0.12f, 0.34f), Vector3(0.50f, 0.14f, 0.0f), Color(0.50f, 0.35f, 0.22f));
	meshutil::add_box(this, toon, Vector3(0.62f, 0.03f, 0.03f), Vector3(0.50f, 0.20f, 0.155f), Color(0.44f, 0.30f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.62f, 0.03f, 0.03f), Vector3(0.50f, 0.20f, -0.155f), Color(0.44f, 0.30f, 0.20f));
	// 铁泵身
	Ref<CylinderMesh> body;
	body.instantiate();
	body->set_top_radius(0.065f);
	body->set_bottom_radius(0.085f);
	body->set_height(0.64f);
	body->set_radial_segments(10);
	body->set_rings(1);
	body->set_material(meshutil::toon_material(toon, Color(0.24f, 0.28f, 0.26f), 0.45f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(body);
	bmi->set_position(Vector3(0.0f, 0.42f, 0.0f));
	add_child(bmi);
	meshutil::add_box(this, toon, Vector3(0.26f, 0.08f, 0.10f), Vector3(0.13f, 0.70f, 0.0f), Color(0.22f, 0.26f, 0.24f));
	Ref<CylinderMesh> boss;
	boss.instantiate();
	boss->set_top_radius(0.045f);
	boss->set_bottom_radius(0.055f);
	boss->set_height(0.06f);
	boss->set_radial_segments(8);
	boss->set_rings(1);
	boss->set_material(meshutil::toon_material(toon, Color(0.32f, 0.36f, 0.33f), 0.4f));
	MeshInstance3D *bossi = memnew(MeshInstance3D);
	bossi->set_mesh(boss);
	bossi->set_position(Vector3(-0.02f, 0.78f, 0.0f));
	add_child(bossi);
	meshutil::add_box(this, toon, Vector3(0.24f, 0.05f, 0.24f), Vector3(0.0f, 0.10f, 0.0f), Color(0.26f, 0.30f, 0.28f));
	// 出水嘴
	meshutil::add_box(this, toon, Vector3(0.22f, 0.06f, 0.07f), Vector3(0.30f, 0.66f, 0.0f), Color(0.20f, 0.24f, 0.22f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.10f, 0.06f), Vector3(0.40f, 0.62f, 0.0f), Color(0.20f, 0.24f, 0.22f));
	// 压杆（铰链在泵顶后侧）
	handle = memnew(Node3D);
	handle->set_name("Handle");
	handle->set_position(Vector3(-0.02f, 0.78f, 0.0f));
	handle->set_rotation(Vector3(0.0f, 0.0f, 0.55f));
	add_child(handle);
	meshutil::add_box(handle, toon, Vector3(0.46f, 0.035f, 0.035f), Vector3(-0.20f, 0.0f, 0.0f), Color(0.30f, 0.30f, 0.28f));
	meshutil::add_box(handle, toon, Vector3(0.05f, 0.05f, 0.05f), Vector3(-0.44f, 0.02f, 0.0f), Color(0.44f, 0.26f, 0.18f));
	// 水流
	Ref<CylinderMesh> stream_mesh;
	stream_mesh.instantiate();
	stream_mesh->set_top_radius(0.012f);
	stream_mesh->set_bottom_radius(0.018f);
	stream_mesh->set_height(0.30f);
	stream_mesh->set_radial_segments(6);
	stream_mesh->set_rings(1);
	Ref<StandardMaterial3D> sm;
	sm.instantiate();
	sm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	sm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	sm->set_albedo(Color(0.72f, 0.88f, 0.95f, 0.75f));
	sm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	stream_mesh->set_material(sm);
	stream = memnew(MeshInstance3D);
	stream->set_mesh(stream_mesh);
	stream->set_position(Vector3(0.40f, 0.48f, 0.0f));
	stream->set_visible(false);
	add_child(stream);
	// 槽里的水面
	Ref<BoxMesh> wm;
	wm.instantiate();
	wm->set_size(Vector3(0.56f, 0.02f, 0.28f));
	Ref<StandardMaterial3D> wmat;
	wmat.instantiate();
	wmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	wmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wmat->set_albedo(Color(0.52f, 0.76f, 0.86f, 0.80f));
	wm->set_material(wmat);
	water = memnew(MeshInstance3D);
	water->set_mesh(wm);
	water->set_position(Vector3(0.47f, 0.15f, 0.0f));
	add_child(water);
	// 涟漪
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.05f);
	ring->set_outer_radius(0.07f);
	ring->set_rings(12);
	ring->set_ring_segments(3);
	Ref<StandardMaterial3D> rm2;
	rm2.instantiate();
	rm2->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	rm2->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	rm2->set_albedo(Color(0.80f, 0.90f, 0.96f, 0.5f));
	rm2->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ring->set_material(rm2);
	ripple = memnew(MeshInstance3D);
	ripple->set_mesh(ring);
	ripple->set_position(Vector3(0.47f, 0.165f, 0.0f));
	ripple->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	ripple->set_material_override(rm2);
	ripple->set_visible(false);
	add_child(ripple);
	build_sound();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_stream(snd);
	voice->set_unit_size(4.0f);
	voice->set_max_distance(16.0f);
	voice->set_position(Vector3(0.0f, 0.6f, 0.0f));
	add_child(voice);
}
void HandPump::build_sound() {
	const int sr = 22050;
	const int n = sr * 3 / 2;
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(9321u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		if (tt < 0.34f) {
			const float f = 210.0f - 300.0f * tt;
			const float saw = std::sin(6.28318f * f * tt) + 0.35f * std::sin(12.56637f * f * tt);
			const float env = std::sin(3.14159f * tt / 0.34f);
			buf[i] = (saw * 0.22f + (d01(rng) - 0.5f) * 0.08f) * env;
		} else {
			const float wt = tt - 0.34f;
			const float white = d01(rng) - 0.5f;
			lp += (white - lp) * 0.35f;
			const float env = std::exp(-wt * 1.6f) * 0.9f;
			buf[i] = (white - lp * 0.5f) * env * 0.55f;
		}
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 16000.0f, -16000.0f, 16000.0f);
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
