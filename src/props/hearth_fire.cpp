#include "hearth_fire.h"

#include <cmath>
#include <random>
#include <vector>

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
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"

using namespace godot;

void HearthFire::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &HearthFire::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &HearthFire::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &HearthFire::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &HearthFire::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &HearthFire::interact);
	ClassDB::bind_method(D_METHOD("get_light_energy"), &HearthFire::get_light_energy);
	ClassDB::bind_method(D_METHOD("get_boost"), &HearthFire::get_boost);
	ClassDB::bind_method(D_METHOD("get_flame_emitting"), &HearthFire::get_flame_emitting);
	ClassDB::bind_method(D_METHOD("get_steam_emitting"), &HearthFire::get_steam_emitting);
}

float HearthFire::get_light_energy() const {
	return (light != nullptr) ? (float)light->get_param(Light3D::PARAM_ENERGY) : 0.0f;
}

bool HearthFire::get_flame_emitting() const {
	return flame != nullptr && flame->is_emitting();
}

bool HearthFire::get_steam_emitting() const {
	return steam != nullptr && steam->is_emitting();
}

String HearthFire::get_prompt() const {
	return String::utf8("按 E 添柴");
}

Vector3 HearthFire::get_interact_pos() const {
	return get_global_transform().xform(Vector3(-3.05f, 0.7f, -0.6f));
}

void HearthFire::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	boost = 1.3f;
	if (sparks != nullptr) {
		sparks->restart();
	}
	if (crackle != nullptr) {
		crackle->set_volume_db(-7.0f);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_parent() != nullptr
				? get_parent()->get_parent()->get_node_or_null("GameDirector")
				: nullptr;
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("添了根柴 · 屋里更暖了"), 2.8f);
		}
	}
}

void HearthFire::_notification(int p_what) {
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
			const float dt = (float)get_process_delta_time();
			t += dt;
			if (boost > 0.0f) {
				boost = Math::max(0.0f, boost - dt);
			}
			const float b = boost / 1.3f;
			const float flicker = 0.93f + 0.05f * std::sin(t * 8.1f) + 0.02f * std::sin(t * 21.3f + 1.1f);
			if (light != nullptr) {
				light->set_param(Light3D::PARAM_ENERGY, 0.38f * flicker * (1.0f + 0.85f * b));
			}
			if (flame != nullptr) {
				const float s = 1.0f + 0.07f * std::sin(t * 7.7f) + 0.04f * std::sin(t * 17.2f + 0.6f);
				flame->set_scale(Vector3(s * (1.0f + 0.35f * b), (1.0f + 0.10f * std::sin(t * 6.1f)) * (1.0f + 0.55f * b), s * (1.0f + 0.35f * b)));
			}
			if (crackle != nullptr) {
				const float target_db = (boost > 0.0f) ? -7.0f : -17.0f;
				crackle->set_volume_db(Math::lerp((float)crackle->get_volume_db(), target_db, 1.0f - std::exp(-5.0f * dt)));
			}
		} break;
		default:
			break;
	}
}

void HearthFire::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();

	// —— 火苗：小尺度、贴着柴堆往上舔 ——
	Ref<SphereMesh> flame_mesh;
	flame_mesh.instantiate();
	flame_mesh->set_radius(0.055f);
	flame_mesh->set_height(0.11f);
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
	grad->add_point(0.0f, Color(1.0f, 0.80f, 0.34f, 0.75f));
	grad->add_point(0.55f, Color(1.0f, 0.42f, 0.09f, 0.55f));
	grad->add_point(1.0f, Color(0.5f, 0.12f, 0.03f, 0.0f));
	flame = memnew(CPUParticles3D);
	flame->set_name("Flame");
	flame->set_amount(16);
	flame->set_lifetime(0.75f);
	flame->set_mesh(flame_mesh);
	flame->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	flame->set_spread(13.0f);
	flame->set_gravity(Vector3(0.0f, 0.30f, 0.0f));
	flame->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.22f);
	flame->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.48f);
	flame->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	flame->set_param_max(CPUParticles3D::PARAM_SCALE, 1.5f);
	flame->set_color_ramp(grad);
	flame->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	flame->set_emission_sphere_radius(0.08f);
	flame->set_position(Vector3(-3.45f, 0.50f, -0.60f));
	flame->set_emitting(true);
	add_child(flame);

	// —— 火星：偶尔蹦起来一点 ——
	Ref<SphereMesh> spark_mesh;
	spark_mesh.instantiate();
	spark_mesh->set_radius(0.012f);
	spark_mesh->set_height(0.024f);
	spark_mesh->set_radial_segments(5);
	spark_mesh->set_rings(3);
	Ref<StandardMaterial3D> spark_mat;
	spark_mat.instantiate();
	spark_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	spark_mat->set_albedo(Color(1.0f, 0.72f, 0.28f, 0.9f));
	spark_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	spark_mesh->set_material(spark_mat);
	sparks = memnew(CPUParticles3D);
	sparks->set_name("Sparks");
	sparks->set_amount(8);
	sparks->set_lifetime(1.3f);
	sparks->set_mesh(spark_mesh);
	sparks->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	sparks->set_spread(24.0f);
	sparks->set_gravity(Vector3(0.0f, 0.12f, 0.0f));
	sparks->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.35f);
	sparks->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.75f);
	sparks->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	sparks->set_emission_sphere_radius(0.10f);
	sparks->set_position(Vector3(-3.45f, 0.52f, -0.60f));
	sparks->set_emitting(true);
	add_child(sparks);

	// —— 暖光：照亮半个屋子，夜里最明显 ——
	light = memnew(OmniLight3D);
	light->set_name("FireLight");
	light->set_position(Vector3(-2.55f, 1.00f, -0.60f));
	light->set_color(Color(1.0f, 0.66f, 0.36f));
	light->set_param(Light3D::PARAM_ENERGY, 0.38f);
	light->set_param(Light3D::PARAM_RANGE, 5.2f);
	light->set_shadow(false);
	add_child(light);

	// —— 吊在火上的铜水壶 ——
	const Color brass(0.60f, 0.33f, 0.14f);
	MeshInstance3D *pot = meshutil::add_sphere(this, toon, 0.16f, Vector3(-3.20f, 1.02f, -0.62f), brass);
	pot->set_scale(Vector3(1.0f, 0.82f, 1.0f));
	meshutil::add_sphere(this, toon, 0.055f, Vector3(-3.20f, 1.155f, -0.62f), Color(0.50f, 0.28f, 0.12f));
	meshutil::add_sphere(this, toon, 0.022f, Vector3(-3.20f, 1.205f, -0.62f), Color(0.42f, 0.24f, 0.10f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.035f, 0.035f), Vector3(-3.055f, 1.065f, -0.62f), brass, Vector3(0.0f, 0.0f, -0.38f));

	Ref<TorusMesh> handle;
	handle.instantiate();
	handle->set_inner_radius(0.075f);
	handle->set_outer_radius(0.10f);
	handle->set_rings(20);
	handle->set_ring_segments(6);
	handle->set_material(meshutil::toon_material(toon, Color(0.30f, 0.26f, 0.22f), 0.4f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_name("KettleHandle");
	hmi->set_mesh(handle);
	hmi->set_position(Vector3(-3.20f, 1.195f, -0.62f));
	hmi->set_rotation(Vector3(1.5708f, 0.0f, 0.0f));
	add_child(hmi);

	// 从壁炉上槛垂下来的铁链
	Ref<CylinderMesh> chain;
	chain.instantiate();
	chain->set_top_radius(0.009f);
	chain->set_bottom_radius(0.009f);
	chain->set_height(0.52f);
	chain->set_radial_segments(5);
	chain->set_material(meshutil::toon_material(toon, Color(0.20f, 0.19f, 0.18f), 0.3f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_name("KettleChain");
	cmi->set_mesh(chain);
	cmi->set_position(Vector3(-3.20f, 1.47f, -0.62f));
	add_child(cmi);

	// —— 水壶嘴冒出的热气 ——
	Ref<SphereMesh> steam_mesh;
	steam_mesh.instantiate();
	steam_mesh->set_radius(0.055f);
	steam_mesh->set_height(0.11f);
	steam_mesh->set_radial_segments(6);
	steam_mesh->set_rings(3);
	Ref<StandardMaterial3D> steam_mat;
	steam_mat.instantiate();
	steam_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	steam_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	steam_mat->set_albedo(Color(0.90f, 0.94f, 0.95f, 0.16f));
	steam_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	steam_mesh->set_material(steam_mat);
	steam = memnew(CPUParticles3D);
	steam->set_name("KettleSteam");
	steam->set_amount(14);
	steam->set_lifetime(2.2f);
	steam->set_mesh(steam_mesh);
	steam->set_direction(Vector3(-0.25f, 1.0f, 0.0f));
	steam->set_spread(14.0f);
	steam->set_gravity(Vector3(-0.02f, 0.10f, 0.0f));
	steam->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.12f);
	steam->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.26f);
	steam->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	steam->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f);
	steam->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	steam->set_emission_sphere_radius(0.05f);
	steam->set_position(Vector3(-3.04f, 1.10f, -0.62f));
	steam->set_emitting(true);
	add_child(steam);

	// —— 柴火噼啪 ——
	snd_crackle = make_crackle();
	crackle = memnew(AudioStreamPlayer3D);
	crackle->set_name("Crackle");
	crackle->set_stream(snd_crackle);
	crackle->set_volume_db(-17.0f);
	crackle->set_unit_size(4.0f);
	crackle->set_max_distance(12.0f);
	crackle->set_position(Vector3(-3.35f, 0.75f, -0.60f));
	add_child(crackle);
	crackle->play(0.0f);
}

// 2.2 秒可循环的柴火声：低沉噪声底 + 稀疏的"啪"声，首尾交叉淡入避免接缝。
Ref<AudioStreamWAV> HearthFire::make_crackle() {
	const int sr = 22050;
	const int n = (int)(2.2f * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(55221u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	std::vector<float> pops(n, 0.0f);
	for (int i = 0; i < 16; i++) {
		const int at = (int)(d01(rng) * (float)n);
		const float amp = 0.25f + 0.55f * d01(rng);
		for (int j = 0; j < 200; j++) {
			pops[(at + j) % n] += (d01(rng) - 0.5f) * std::exp(-j / 26.0f) * amp;
		}
	}
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.08f;
		float samp = lp * 0.9f + pops[i];
		const int tail = n - i;
		if (tail < 2205) {
			samp *= (float)tail / 2205.0f;
		}
		if (i < 2205) {
			samp *= (float)i / 2205.0f;
		}
		const int16_t q = (int16_t)Math::clamp(samp * 11000.0f, -32000.0f, 32000.0f);
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
