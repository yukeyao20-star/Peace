#include "campfire.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Campfire::_bind_methods() {
}

void Campfire::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			TerrainBuilder *terrain = nullptr;
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
			}
			if (terrain != nullptr) {
				Vector3 p = get_global_position();
				p.y = terrain->get_terrain_height(p.x, p.z) + 0.03f;
				set_global_position(p);
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (light != nullptr) {
				float flicker = 0.92f + 0.06f * std::sin(t * 7.3f) + 0.02f * std::sin(t * 19.1f + 1.7f);
				light->set_param(Light3D::PARAM_ENERGY, 1.55f * flicker);
			}
		} break;
		default:
			break;
	}
}

void Campfire::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color stone_c(0.38f, 0.37f, 0.35f);
	const Color stone_dark(0.28f, 0.27f, 0.26f);
	const Color wood_c(0.36f, 0.23f, 0.12f);
	const Color wood_light(0.52f, 0.36f, 0.20f);

	// 石圈
	std::mt19937 rng(747474u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < 9; i++) {
		float a = (float)i / 9.0f * 6.2831853f + d01(rng) * 0.15f;
		float r = 0.52f + d01(rng) * 0.12f;
		Ref<SphereMesh> rock;
		rock.instantiate();
		rock->set_radius(0.11f + d01(rng) * 0.05f);
		rock->set_height(0.16f);
		rock->set_radial_segments(7);
		rock->set_rings(4);
		rock->set_material(meshutil::toon_material(toon, (i % 2 == 0) ? stone_c : stone_dark));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(rock);
		mi->set_position(Vector3(std::cos(a) * r, 0.06f, std::sin(a) * r));
		mi->set_scale(Vector3(1.0f, 0.65f, 1.0f));
		mi->set_rotation(Vector3(0.0f, a, 0.0f));
		add_child(mi);
	}

	// 交叉柴火
	for (int i = 0; i < 4; i++) {
		Ref<CylinderMesh> log;
		log.instantiate();
		log->set_top_radius(0.055f);
		log->set_bottom_radius(0.065f);
		log->set_height(0.85f);
		log->set_radial_segments(7);
		log->set_material(meshutil::toon_material(toon, (i % 2 == 0) ? wood_c : wood_light));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(log);
		float a = (float)i / 4.0f * 3.1415926f;
		mi->set_position(Vector3(0.0f, 0.16f + (float)i * 0.02f, 0.0f));
		mi->set_rotation(Vector3(1.5707964f, a, 0.0f));
		add_child(mi);
	}

	// 火焰
	Ref<SphereMesh> flame_mesh;
	flame_mesh.instantiate();
	flame_mesh->set_radius(0.075f);
	flame_mesh->set_height(0.15f);
	flame_mesh->set_radial_segments(6);
	flame_mesh->set_rings(3);
	Ref<StandardMaterial3D> flame_mat;
	flame_mat.instantiate();
	flame_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	flame_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	flame_mat->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	flame_mat->set_albedo(Color(1.0f, 0.55f, 0.15f, 0.9f));
	flame_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	flame_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	flame_mesh->set_material(flame_mat);
	Ref<Gradient> grad;
	grad.instantiate();
	grad->add_point(0.0f, Color(1.0f, 0.85f, 0.35f, 1.0f));
	grad->add_point(0.55f, Color(1.0f, 0.45f, 0.10f, 0.8f));
	grad->add_point(1.0f, Color(0.5f, 0.12f, 0.03f, 0.0f));
	flame = memnew(CPUParticles3D);
	flame->set_name("Flame");
	flame->set_amount(26);
	flame->set_lifetime(0.85f);
	flame->set_mesh(flame_mesh);
	flame->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	flame->set_spread(14.0f);
	flame->set_gravity(Vector3(0.0f, 0.35f, 0.0f));
	flame->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.32f);
	flame->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.62f);
	flame->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	flame->set_param_max(CPUParticles3D::PARAM_SCALE, 1.6f);
	flame->set_color_ramp(grad);
	flame->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	flame->set_emission_sphere_radius(0.14f);
	flame->set_position(Vector3(0.0f, 0.18f, 0.0f));
	flame->set_emitting(true);
	add_child(flame);

	// 火星
	Ref<SphereMesh> ember_mesh;
	ember_mesh.instantiate();
	ember_mesh->set_radius(0.014f);
	ember_mesh->set_height(0.028f);
	ember_mesh->set_radial_segments(5);
	ember_mesh->set_rings(3);
	Ref<StandardMaterial3D> ember_mat;
	ember_mat.instantiate();
	ember_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	ember_mat->set_albedo(Color(1.0f, 0.72f, 0.28f, 0.9f));
	ember_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ember_mesh->set_material(ember_mat);
	embers = memnew(CPUParticles3D);
	embers->set_name("Embers");
	embers->set_amount(14);
	embers->set_lifetime(1.8f);
	embers->set_mesh(ember_mesh);
	embers->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	embers->set_spread(26.0f);
	embers->set_gravity(Vector3(0.0f, 0.18f, 0.0f));
	embers->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.5f);
	embers->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.1f);
	embers->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	embers->set_emission_sphere_radius(0.12f);
	embers->set_position(Vector3(0.0f, 0.22f, 0.0f));
	embers->set_emitting(true);
	add_child(embers);

	// 火光
	light = memnew(OmniLight3D);
	light->set_name("FireLight");
	light->set_position(Vector3(0.0f, 0.55f, 0.0f));
	light->set_color(Color(1.0f, 0.58f, 0.26f));
	light->set_param(Light3D::PARAM_ENERGY, 1.55f);
	light->set_param(Light3D::PARAM_RANGE, 9.0f);
	light->set_shadow(false);
	add_child(light);

	// 噼啪声
	snd_crackle = make_crackle();
	crackle = memnew(AudioStreamPlayer3D);
	crackle->set_name("Crackle");
	crackle->set_stream(snd_crackle);
	crackle->set_volume_db(-20.0f);
	crackle->set_max_distance(24.0f);
	add_child(crackle);
	crackle->play();
}

// 柴火噼啪：4 秒可循环，稀疏短爆 + 低频底噪
Ref<AudioStreamWAV> Campfire::make_crackle() {
	const int sr = 22050;
	const float dur = 4.0f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(828282u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	std::vector<float> pops(n, 0.0f);
	for (int i = 0; i < 26; i++) {
		int at = (int)(d01(rng) * (float)(n - 2000));
		float amp = 0.4f + d01(rng) * 0.6f;
		for (int k = 0; k < 1200; k++) {
			float env = std::exp(-(float)k / 260.0f) * ((k % 2 == 0) ? 1.0f : -0.7f);
			pops[at + k] += amp * env * 0.35f;
		}
	}
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		lp += 0.05f * ((d01(rng) - 0.5f) - lp);
		float samp = lp * 0.8f + pops[i];
		float edge = Math::min(1.0f, Math::min((float)i / (float)sr / 0.05f,
				(dur - (float)i / (float)sr) / 0.05f));
		samp *= Math::clamp(edge, 0.0f, 1.0f);
		int16_t q = (int16_t)Math::clamp(samp * 14000.0f, -32000.0f, 32000.0f);
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
