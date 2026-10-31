#include "stew_pot.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void StewPot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &StewPot::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &StewPot::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &StewPot::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &StewPot::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &StewPot::interact);
	ClassDB::bind_method(D_METHOD("get_stir_time"), &StewPot::get_stir_time);
	ClassDB::bind_method(D_METHOD("get_steam_emitting"), &StewPot::get_steam_emitting);
	ClassDB::bind_method(D_METHOD("get_stir_ready"), &StewPot::get_stir_ready);
}

bool StewPot::get_steam_emitting() const {
	return steam != nullptr && steam->is_emitting();
}

void StewPot::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
			}
			if (terrain != nullptr) {
				Vector3 p = get_global_position();
				p.y = terrain->get_terrain_height(p.x, p.z) + 0.02f;
				set_global_position(p);
			}
			add_to_group("interactables");
			build();
			snd_stir = make_stir();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("StirVoice");
			voice->set_stream(snd_stir);
			voice->set_volume_db(-15.0f);
			voice->set_max_distance(16.0f);
			add_child(voice);
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (stir_t > 0.0f) {
				stir_t = Math::max(0.0f, stir_t - delta);
			}
			if (soup != nullptr) {
				float speed = (stir_t > 0.0f) ? 3.2f : 0.35f;
				soup->set_rotation(Vector3(0.0f, soup->get_rotation().y + delta * speed, 0.0f));
			}
			if (bubbles != nullptr) {
				bubbles->set_emitting(true);
				bubbles->set_speed_scale(stir_t > 0.0f ? 2.2f : 1.0f);
			}
		} break;
		default:
			break;
	}
}

String StewPot::get_prompt() const {
	return String::utf8("按 E 搅一搅汤");
}

Vector3 StewPot::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.35f, 0.0f);
}

float StewPot::get_interact_distance() const {
	return 2.4f;
}

void StewPot::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	stir_t = 2.6f;
	if (steam != nullptr) {
		steam->restart();
	}
	if (bubbles != nullptr) {
		bubbles->restart();
	}
	if (voice != nullptr) {
		voice->set_global_position(get_global_position() + Vector3(0.0f, 0.4f, 0.0f));
		voice->set_pitch_scale(0.95f + (float)(rand() % 100) * 0.0015f);
		voice->set_volume_db(-14.0f);
		voice->play(0.0f);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("汤搅匀了 · 热气一下涌起来"), 2.8f);
		}
	}
}

void StewPot::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color iron(0.20f, 0.21f, 0.22f);
	const Color iron_light(0.34f, 0.35f, 0.36f);
	const Color soup_c(0.86f, 0.45f, 0.18f);
	const Color stone(0.38f, 0.37f, 0.35f);

	// 三块支锅石
	for (int i = 0; i < 3; i++) {
		float a = 6.2831853f * (float)i / 3.0f + 0.4f;
		meshutil::add_box(this, toon, Vector3(0.20f, 0.12f, 0.18f),
				Vector3(std::cos(a) * 0.30f, 0.06f, std::sin(a) * 0.30f), stone);
	}

	// 锅身与锅沿
	Ref<CylinderMesh> pot;
	pot.instantiate();
	pot->set_top_radius(0.30f);
	pot->set_bottom_radius(0.22f);
	pot->set_height(0.34f);
	pot->set_radial_segments(18);
	pot->set_rings(2);
	pot->set_material(meshutil::toon_material(toon, iron, 0.35f));
	MeshInstance3D *pot_mi = memnew(MeshInstance3D);
	pot_mi->set_name("Cauldron");
	pot_mi->set_mesh(pot);
	pot_mi->set_position(Vector3(0.0f, 0.28f, 0.0f));
	add_child(pot_mi);
	Ref<TorusMesh> rim;
	rim.instantiate();
	rim->set_inner_radius(0.27f);
	rim->set_outer_radius(0.32f);
	rim->set_rings(20);
	rim->set_ring_segments(4);
	rim->set_material(meshutil::toon_material(toon, iron_light, 0.4f));
	MeshInstance3D *rim_mi = memnew(MeshInstance3D);
	rim_mi->set_name("Rim");
	rim_mi->set_mesh(rim);
	rim_mi->set_position(Vector3(0.0f, 0.45f, 0.0f));
	add_child(rim_mi);

	// 浓汤表面
	Ref<CylinderMesh> soup_mesh;
	soup_mesh.instantiate();
	soup_mesh->set_top_radius(0.27f);
	soup_mesh->set_bottom_radius(0.27f);
	soup_mesh->set_height(0.015f);
	soup_mesh->set_radial_segments(20);
	soup_mesh->set_rings(1);
	Ref<StandardMaterial3D> soup_mat;
	soup_mat.instantiate();
	soup_mat->set_albedo(soup_c);
	soup_mat->set("emission_enabled", true);
	soup_mat->set("emission", Color(0.85f, 0.38f, 0.12f));
	soup_mat->set("emission_energy_multiplier", 0.55f);
	soup_mesh->set_material(Ref<Material>(soup_mat));
	soup = memnew(MeshInstance3D);
	soup->set_name("Soup");
	soup->set_mesh(soup_mesh);
	soup->set_position(Vector3(0.0f, 0.44f, 0.0f));
	add_child(soup);

	// 锅把
	Ref<TorusMesh> handle;
	handle.instantiate();
	handle->set_inner_radius(0.28f);
	handle->set_outer_radius(0.30f);
	handle->set_rings(16);
	handle->set_ring_segments(4);
	handle->set_material(meshutil::toon_material(toon, iron_light, 0.3f));
	MeshInstance3D *handle_mi = memnew(MeshInstance3D);
	handle_mi->set_name("Handle");
	handle_mi->set_mesh(handle);
	handle_mi->set_position(Vector3(0.0f, 0.46f, 0.0f));
	handle_mi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	handle_mi->set_scale(Vector3(1.15f, 1.0f, 1.15f));
	add_child(handle_mi);

	// 热气
	Ref<SphereMesh> puff;
	puff.instantiate();
	puff->set_radius(0.07f);
	puff->set_height(0.14f);
	puff->set_radial_segments(7);
	puff->set_rings(4);
	Ref<StandardMaterial3D> steam_mat;
	steam_mat.instantiate();
	steam_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	steam_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	steam_mat->set_albedo(Color(0.92f, 0.90f, 0.86f, 0.10f));
	steam_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	puff->set_material(steam_mat);
	steam = memnew(CPUParticles3D);
	steam->set_name("Steam");
	steam->set_amount(14);
	steam->set_lifetime(1.7f);
	steam->set_mesh(puff);
	steam->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	steam->set_spread(18.0f);
	steam->set_gravity(Vector3(0.04f, 0.22f, 0.02f));
	steam->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.18f);
	steam->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.38f);
	steam->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	steam->set_param_max(CPUParticles3D::PARAM_SCALE, 1.7f);
	steam->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	steam->set_emission_sphere_radius(0.16f);
	steam->set_position(Vector3(0.0f, 0.50f, 0.0f));
	steam->set_emitting(true);
	add_child(steam);

	// 汤泡
	Ref<SphereMesh> bubble;
	bubble.instantiate();
	bubble->set_radius(0.022f);
	bubble->set_height(0.044f);
	bubble->set_radial_segments(6);
	bubble->set_rings(4);
	Ref<StandardMaterial3D> bubble_mat;
	bubble_mat.instantiate();
	bubble_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bubble_mat->set_albedo(Color(1.0f, 0.72f, 0.30f, 0.9f));
	bubble_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	bubble->set_material(bubble_mat);
	bubbles = memnew(CPUParticles3D);
	bubbles->set_name("Bubbles");
	bubbles->set_amount(10);
	bubbles->set_lifetime(0.55f);
	bubbles->set_mesh(bubble);
	bubbles->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	bubbles->set_spread(25.0f);
	bubbles->set_gravity(Vector3(0.0f, 0.15f, 0.0f));
	bubbles->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.05f);
	bubbles->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.18f);
	bubbles->set_param_min(CPUParticles3D::PARAM_SCALE, 0.5f);
	bubbles->set_param_max(CPUParticles3D::PARAM_SCALE, 1.2f);
	bubbles->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	bubbles->set_emission_sphere_radius(0.22f);
	bubbles->set_position(Vector3(0.0f, 0.44f, 0.0f));
	bubbles->set_emitting(true);
	add_child(bubbles);
}

// 0.5 秒搅汤：木勺刮锅的闷响 + 汤汁打旋的水声
Ref<AudioStreamWAV> StewPot::make_stir() {
	const int sr = 22050;
	const float dur = 0.5f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1103515245 + 12345) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.15f;
		float scrape = std::sin(6.2831853f * 210.0f * t) * std::exp(-7.0f * t) * 0.20f;
		float swirl = (white - lp) * (0.35f + 0.25f * std::sin(6.2831853f * 3.0f * t)) * 0.30f;
		float env = Math::clamp(t / 0.02f, 0.0f, 1.0f) * Math::clamp((dur - t) / 0.12f, 0.0f, 1.0f);
		float samp = (scrape + swirl) * env;
		int16_t q = (int16_t)Math::clamp(samp * 11000.0f, -32000.0f, 32000.0f);
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
