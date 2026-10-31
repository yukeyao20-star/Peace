#include "leaf_drift.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void LeafDrift::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_leaf_amount"), &LeafDrift::get_leaf_amount);
	ClassDB::bind_method(D_METHOD("get_pile_count"), &LeafDrift::get_pile_count);
	ClassDB::bind_method(D_METHOD("get_emitting"), &LeafDrift::get_emitting);
	ClassDB::bind_method(D_METHOD("get_piles_hidden"), &LeafDrift::get_piles_hidden);
	ClassDB::bind_method(D_METHOD("get_wet_level"), &LeafDrift::get_wet_level);
	ClassDB::bind_method(D_METHOD("get_snow_cover"), &LeafDrift::get_snow_cover);
	ClassDB::bind_method(D_METHOD("get_kick_count"), &LeafDrift::get_kick_count);
}

int LeafDrift::get_leaf_amount() const {
	return (fall_fx != nullptr) ? fall_fx->get_amount() : 0;
}

bool LeafDrift::get_emitting() const {
	return fall_fx != nullptr && fall_fx->is_emitting();
}

void LeafDrift::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;

			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}
			SkyCycle *sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			wet = Math::move_toward(wet, Math::clamp(rain, 0.0f, 1.0f), delta * 0.4f);
			snow_sm = Math::move_toward(snow_sm, Math::clamp(snow, 0.0f, 1.0f), delta * 0.25f);

			if (fall_fx != nullptr) {
				// 雪天不再飘叶；发射盒跟着玩家，走到哪都有一小片秋色。
				const bool emit = snow_sm < 0.35f;
				if (fall_fx->is_emitting() != emit) {
					fall_fx->set_emitting(emit);
				}
				if (player != nullptr) {
					fall_fx->set_global_position(player->get_global_position() + Vector3(0.0f, 5.0f, 0.0f));
				}
				// 阵风：重力方向缓慢摆动，叶子就一阵一阵地斜着飘。
				fall_fx->set_gravity(Vector3(
						0.20f + 0.15f * std::sin(t * 0.41f),
						-0.32f,
						0.12f + 0.10f * std::sin(t * 0.27f + 1.3f)));
			}

			// 叶堆：雪盖住时收起来；潮湿时颜色变深。
			piles_hidden = snow_sm > 0.55f;
			if (pile_mm != nullptr) {
				pile_mm->set_visible(!piles_hidden);
			}
			if (pile_mat.is_valid()) {
				pile_mat->set_albedo(Color(1.0f, 1.0f, 1.0f).lerp(Color(0.58f, 0.56f, 0.52f), wet));
			}

			// 快步走过叶堆：踢起一小蓬叶子 + 沙沙声，每堆各自冷却。
			CharacterBody3D *pb = Object::cast_to<CharacterBody3D>(player);
			const float hspeed = (pb != nullptr)
					? Vector2(pb->get_velocity().x, pb->get_velocity().z).length()
					: 0.0f;
			for (int i = 0; i < PILE_COUNT; i++) {
				if (kick_cool[i] > 0.0f) {
					kick_cool[i] = Math::max(0.0f, kick_cool[i] - delta);
					continue;
				}
				if (piles_hidden || pb == nullptr || hspeed < 1.2f) {
					continue;
				}
				const Vector3 pp = pb->get_global_position();
				if (Vector2(pp.x - pile_pos[i].x, pp.z - pile_pos[i].z).length() < 0.95f) {
					kick_pile(i);
				}
			}
		} break;
		default:
			break;
	}
}

void LeafDrift::kick_pile(int p_index) {
	kick_cool[p_index] = 1.8f;
	kick_count++;
	if (kick_fx != nullptr) {
		kick_fx->set_global_position(pile_pos[p_index] + Vector3(0.0f, 0.05f, 0.0f));
		kick_fx->restart();
		kick_fx->set_emitting(true);
	}
	if (rustle != nullptr && snd_rustle.is_valid()) {
		rustle->set_global_position(pile_pos[p_index] + Vector3(0.0f, 0.08f, 0.0f));
		rustle->set_pitch_scale(0.92f + 0.16f * (float)((kick_count * 37) % 10) / 10.0f);
		rustle->play(0.0f);
	}
}

void LeafDrift::build_leaf_mesh() {
	// 菱形叶片（四点两三角），比矩形更像叶子。
	PackedVector3Array verts;
	verts.push_back(Vector3(0.0f, 0.075f, 0.0f));
	verts.push_back(Vector3(0.050f, 0.0f, 0.0f));
	verts.push_back(Vector3(0.0f, -0.075f, 0.0f));
	verts.push_back(Vector3(-0.050f, 0.0f, 0.0f));
	PackedVector3Array normals;
	for (int i = 0; i < 4; i++) {
		normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
	}
	PackedInt32Array idx;
	idx.push_back(0);
	idx.push_back(1);
	idx.push_back(2);
	idx.push_back(0);
	idx.push_back(2);
	idx.push_back(3);
	PackedColorArray vcols;
	for (int i = 0; i < 4; i++) {
		vcols.push_back(Color(1, 1, 1)); // 粒子色由顶点色 × 粒子颜色共同决定，缺顶点色会变黑
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_COLOR] = vcols;
	arrays[Mesh::ARRAY_INDEX] = idx;
	Ref<ArrayMesh> am;
	am.instantiate();
	am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	leaf_mesh = am;

	leaf_mat.instantiate();
	leaf_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
	leaf_mat->set_albedo(Color(0.86f, 0.52f, 0.20f));
	leaf_mat->set_roughness(0.85f);
	leaf_mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	leaf_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	am->surface_set_material(0, leaf_mat);
}

void LeafDrift::build() {
	build_leaf_mesh();

	// 秋天的五色渐变：金、橙、锈红、褐、浅黄
	Ref<Gradient> ramp;
	ramp.instantiate();
	PackedFloat32Array offs;
	offs.push_back(0.0f);
	offs.push_back(0.28f);
	offs.push_back(0.55f);
	offs.push_back(0.80f);
	offs.push_back(1.0f);
	PackedColorArray cols;
	cols.push_back(Color(0.92f, 0.70f, 0.22f));
	cols.push_back(Color(0.86f, 0.46f, 0.16f));
	cols.push_back(Color(0.66f, 0.27f, 0.12f));
	cols.push_back(Color(0.48f, 0.30f, 0.14f));
	cols.push_back(Color(0.95f, 0.82f, 0.38f));
	ramp->set_offsets(offs);
	ramp->set_colors(cols);

	fall_fx = memnew(CPUParticles3D);
	fall_fx->set_name("FallingLeaves");
	fall_fx->set_amount(160);
	fall_fx->set_lifetime(14.0f);
	fall_fx->set_pre_process_time(7.0f);
	fall_fx->set_use_local_coordinates(false);
	fall_fx->set_mesh(leaf_mesh);
	fall_fx->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	fall_fx->set_emission_box_extents(Vector3(9.0f, 0.4f, 9.0f));
	fall_fx->set_visibility_aabb(AABB(Vector3(-18.0f, -14.0f, -18.0f), Vector3(36.0f, 22.0f, 36.0f)));
	fall_fx->set_direction(Vector3(0.0f, -1.0f, 0.0f));
	fall_fx->set_spread(24.0f);
	fall_fx->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.05f);
	fall_fx->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.30f);
	fall_fx->set_gravity(Vector3(0.2f, -0.22f, 0.12f));
	fall_fx->set_param_min(CPUParticles3D::PARAM_DAMPING, 0.10f);
	fall_fx->set_param_max(CPUParticles3D::PARAM_DAMPING, 0.18f);
	fall_fx->set_param_min(CPUParticles3D::PARAM_SCALE, 0.9f);
	fall_fx->set_param_max(CPUParticles3D::PARAM_SCALE, 1.7f);
	fall_fx->set_param_min(CPUParticles3D::PARAM_ANGULAR_VELOCITY, -160.0f);
	fall_fx->set_param_max(CPUParticles3D::PARAM_ANGULAR_VELOCITY, 160.0f);
	fall_fx->set_particle_flag(CPUParticles3D::PARTICLE_FLAG_ROTATE_Y, true);
	fall_fx->set_color_ramp(ramp);
	add_child(fall_fx);

	// 踢叶堆时的一蓬叶子
	kick_fx = memnew(CPUParticles3D);
	kick_fx->set_name("KickBurst");
	kick_fx->set_amount(18);
	kick_fx->set_lifetime(1.5f);
	kick_fx->set_one_shot(true);
	kick_fx->set_explosiveness_ratio(0.85f);
	kick_fx->set_use_local_coordinates(false);
	kick_fx->set_mesh(leaf_mesh);
	kick_fx->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	kick_fx->set_emission_sphere_radius(0.30f);
	kick_fx->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	kick_fx->set_spread(65.0f);
	kick_fx->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.7f);
	kick_fx->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.9f);
	kick_fx->set_gravity(Vector3(0.0f, -2.4f, 0.0f));
	kick_fx->set_param_min(CPUParticles3D::PARAM_ANGULAR_VELOCITY, -520.0f);
	kick_fx->set_param_max(CPUParticles3D::PARAM_ANGULAR_VELOCITY, 520.0f);
	kick_fx->set_particle_flag(CPUParticles3D::PARTICLE_FLAG_ROTATE_Y, true);
	kick_fx->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	kick_fx->set_param_max(CPUParticles3D::PARAM_SCALE, 1.1f);
	kick_fx->set_color_ramp(ramp);
	kick_fx->set_emitting(false);
	add_child(kick_fx);

	// 三小堆干叶
	TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(
			(get_tree() != nullptr) ? get_tree()->get_root()->find_child("Terrain", true, false) : nullptr);
	const Vector3 bases[PILE_COUNT] = {
		Vector3(8.20f, 0.0f, 13.20f),
		Vector3(12.60f, 0.0f, 15.60f),
		Vector3(14.20f, 0.0f, 11.80f),
	};
	for (int i = 0; i < PILE_COUNT; i++) {
		pile_pos[i] = bases[i];
		if (terrain != nullptr) {
			pile_pos[i].y = terrain->get_terrain_height(bases[i].x, bases[i].z);
		}
	}

	pile_mat.instantiate();
	pile_mat->set_albedo(Color(1, 1, 1));
	pile_mat->set_roughness(0.9f);
	pile_mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	pile_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	Ref<ArrayMesh> pile_mesh_res = Object::cast_to<ArrayMesh>(leaf_mesh->duplicate().ptr());
	pile_mesh_res->surface_set_material(0, pile_mat);

	pile_mesh.instantiate();
	pile_mesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	pile_mesh->set_use_colors(true);
	pile_mesh->set_mesh(pile_mesh_res);
	pile_mesh->set_instance_count(PILE_COUNT * PILE_LEAVES);
	std::mt19937 rng(90210u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	const Color palette[5] = {
		Color(0.90f, 0.62f, 0.20f),
		Color(0.82f, 0.40f, 0.14f),
		Color(0.60f, 0.24f, 0.10f),
		Color(0.46f, 0.30f, 0.14f),
		Color(0.93f, 0.78f, 0.34f),
	};
	for (int p = 0; p < PILE_COUNT; p++) {
		for (int j = 0; j < PILE_LEAVES; j++) {
			const int k = p * PILE_LEAVES + j;
			const float ang = d01(rng) * 6.28318f;
			const float rad = 0.10f + 0.28f * std::sqrt(d01(rng));
			const Vector3 off(std::cos(ang) * rad, 0.0f, std::sin(ang) * rad);
			const float yaw = d01(rng) * 6.28318f;
			const float tilt = (d01(rng) - 0.5f) * 0.5f;
			const Basis b = Basis(Vector3(0.0f, 1.0f, 0.0f), yaw)
					* Basis(Vector3(1.0f, 0.0f, 0.0f), -1.5707964f + tilt);
			const Transform3D xf(b, pile_pos[p] + off + Vector3(0.0f, 0.012f + 0.018f * d01(rng), 0.0f));
			pile_mesh->set_instance_transform(k, xf);
			pile_mesh->set_instance_color(k, palette[(int)(d01(rng) * 5.0f) % 5]);
		}
	}
	pile_mm = memnew(MultiMeshInstance3D);
	pile_mm->set_name("LeafPiles");
	pile_mm->set_multimesh(pile_mesh);
	add_child(pile_mm);

	snd_rustle = make_crunch();
	rustle = memnew(AudioStreamPlayer3D);
	rustle->set_name("LeafRustle");
	rustle->set_stream(snd_rustle);
	rustle->set_volume_db(-7.0f);
	rustle->set_unit_size(4.0f);
	rustle->set_max_distance(16.0f);
	add_child(rustle);
}

// 0.35 秒干叶脆响：高通噪声 + 几下不规则碎裂。
Ref<AudioStreamWAV> LeafDrift::make_crunch() {
	const int sr = 22050;
	const int n = (int)(0.35f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(9917u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.30f;
		const float hp = white - lp;
		const float crackle = 0.55f + 0.45f * std::sin(tt * 210.0f + std::sin(tt * 53.0f) * 3.0f);
		const float env = std::exp(-tt * 13.0f) * Math::clamp(tt / 0.006f, 0.0f, 1.0f);
		buf[i] = hp * crackle * env * 1.6f;
	}
	float peak = 0.0001f;
	for (float v : buf) {
		peak = Math::max(peak, std::abs(v));
	}
	const float gain = 0.72f / peak;
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * gain * 32000.0f, -32000.0f, 32000.0f);
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
