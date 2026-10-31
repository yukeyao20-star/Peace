#include "dandelion_field.h"

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
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

int DandelionField::get_bare_count() const {
	int n = 0;
	for (const Plant &p : plants) {
		if (p.bare) {
			n++;
		}
	}
	return n;
}

bool DandelionField::get_ambient_enabled() const {
	return ambient != nullptr && ambient->is_emitting();
}

void DandelionField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_plant_count"), &DandelionField::get_plant_count);
	ClassDB::bind_method(D_METHOD("get_bare_count"), &DandelionField::get_bare_count);
	ClassDB::bind_method(D_METHOD("get_ambient_enabled"), &DandelionField::get_ambient_enabled);
	ClassDB::bind_method(D_METHOD("get_puff_ready"), &DandelionField::get_puff_ready);
}

void DandelionField::_notification(int p_what) {
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
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			build();
			build_ambient();
			make_puff();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float wet = (sky != nullptr) ? Math::clamp(sky->get_rain_amount() + sky->get_snow_amount() * 0.8f, 0.0f, 1.0f) : 0.0f;
			if (ambient != nullptr) {
				ambient->set_emitting(night < 0.35f && wet < 0.35f);
			}
			Player *pl = Object::cast_to<Player>(player);
			const Vector3 pp = (player != nullptr) ? player->get_global_position() : Vector3(0, -999, 0);
			const Vector3 pv = (pl != nullptr) ? pl->get_velocity() : Vector3();
			const float hspd = Vector2(pv.x, pv.z).length();
			burst_cd -= delta;
			for (Plant &p : plants) {
				if (p.puff == nullptr) {
					continue;
				}
				if (p.bare) {
					p.regrow_t -= delta;
					if (p.regrow_t <= 0.0f) {
						p.bare = false;
						p.grow_t = 0.0f;
						p.puff->set_visible(true);
					}
					continue;
				}
				if (p.grow_t < 1.0f) {
					p.grow_t = Math::min(1.0f, p.grow_t + delta * 0.45f);
					float s = p.grow_t * (1.0f + 0.08f * std::sin(t * 2.0f + p.phase));
					p.puff->set_scale(Vector3(s, s, s));
				}
				// 玩家擦过：绒球噗地散开
				if (night < 0.35f && wet < 0.4f && burst_cd <= 0.0f && hspd > 0.35f
						&& Vector2(pp.x - p.pos.x, pp.z - p.pos.z).length() < 0.85f
						&& std::fabs(pp.y - p.pos.y) < 1.2f) {
					p.bare = true;
					p.regrow_t = 12.0f + (float)(rng() % 300u) * 0.01f;
					p.puff->set_visible(false);
					if (burst != nullptr) {
						burst->set_global_position(p.pos + Vector3(0, 0.30f, 0));
						burst->restart();
					}
					if (voice != nullptr) {
						voice->set_global_position(p.pos + Vector3(0, 0.35f, 0));
						voice->set_pitch_scale(0.92f + (float)(rng() % 100u) * 0.002f);
						voice->set_volume_db(-16.0f);
						voice->play(0.0f);
					}
					burst_cd = 0.4f;
				}
			}
		} break;
		default:
			break;
	}
}

void DandelionField::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color stem_c(0.32f, 0.48f, 0.20f);
	const Color leaf_c(0.36f, 0.52f, 0.22f);
	const Color white(0.97f, 0.97f, 0.92f);
	const Color yellow(0.98f, 0.84f, 0.28f);

	const Vector3 centers[3] = {Vector3(-1.0f, 0, -21.0f), Vector3(5.5f, 0, -18.5f), Vector3(12.0f, 0, -10.0f)};
	int made = 0;
	int guard = 0;
	while (made < 18 && guard < 900) {
		guard++;
		const Vector3 &c = centers[rng() % 3];
		float a = (float)(rng() % 628u) * 0.01f;
		float r = (float)(rng() % 1000u) * 0.0035f;
		Vector3 pos(c.x + std::cos(a) * r, 0.0f, c.z + std::sin(a) * r);
		float depth = (terrain != nullptr) ? terrain->water_depth_at(pos.x, pos.z) : -1.0f;
		if (depth > -0.05f) {
			continue;
		}
		if (terrain != nullptr) {
			pos.y = terrain->get_terrain_height(pos.x, pos.z);
		}

		Plant p;
		p.pos = pos;
		p.phase = (float)(rng() % 628u) * 0.01f;
		p.root = memnew(Node3D);
		p.root->set_name(String("Dandelion") + String::num_int64(made));
		add_child(p.root);
		p.root->set_global_position(pos);
		p.root->set_rotation(Vector3(0.0f, (float)(rng() % 628u) * 0.01f, 0.0f));

		meshutil::add_box(p.root, toon, Vector3(0.012f, 0.27f, 0.012f), Vector3(0, 0.135f, 0), stem_c);
		for (int l = 0; l < 3; l++) {
			float la = 6.2831853f * (float)l / 3.0f + p.phase;
			meshutil::add_box(p.root, toon, Vector3(0.10f, 0.012f, 0.035f),
					Vector3(std::cos(la) * 0.055f, 0.04f, std::sin(la) * 0.055f), leaf_c,
					Vector3(0.0f, la, 0.28f));
		}

		Ref<SphereMesh> puff_mesh;
		puff_mesh.instantiate();
		puff_mesh->set_radius(0.085f);
		puff_mesh->set_height(0.17f);
		puff_mesh->set_radial_segments(10);
		puff_mesh->set_rings(6);
		bool flower = (rng() % 4u) == 0;
		puff_mesh->set_material(meshutil::toon_material(toon, flower ? yellow : white, 0.55f));
		p.puff = memnew(MeshInstance3D);
		p.puff->set_name("Puff");
		p.puff->set_mesh(puff_mesh);
		p.puff->set_position(Vector3(0.0f, 0.30f, 0.0f));
		p.root->add_child(p.puff);
		plants.push_back(p);
		made++;
	}
}

void DandelionField::build_ambient() {
	Ref<BoxMesh> seed;
	seed.instantiate();
	seed->set_size(Vector3(0.020f, 0.020f, 0.006f));
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_albedo(Color(1.0f, 1.0f, 0.96f, 0.85f));
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	seed->set_material(mat);

	ambient = memnew(CPUParticles3D);
	ambient->set_name("SeedDrift");
	ambient->set_amount(70);
	ambient->set_lifetime(7.0f);
	ambient->set_mesh(seed);
	ambient->set_direction(Vector3(0.7f, 0.15f, 0.25f));
	ambient->set_spread(35.0f);
	ambient->set_gravity(Vector3(0.10f, 0.012f, 0.035f));
	ambient->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.18f);
	ambient->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.45f);
	ambient->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	ambient->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f);
	ambient->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	ambient->set_emission_box_extents(Vector3(7.0f, 0.8f, 6.0f));
	ambient->set_position(Vector3(4.0f, 1.0f, -17.0f));
	add_child(ambient);

	burst = memnew(CPUParticles3D);
	burst->set_name("SeedBurst");
	burst->set_amount(16);
	burst->set_one_shot(true);
	burst->set("explosiveness", 0.9f);
	burst->set_lifetime(1.8f);
	burst->set_mesh(seed);
	burst->set_direction(Vector3(0.0f, 0.8f, 0.0f));
	burst->set_spread(70.0f);
	burst->set_gravity(Vector3(0.08f, 0.03f, 0.03f));
	burst->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.5f);
	burst->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.2f);
	burst->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	burst->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
	burst->set_emitting(false);
	add_child(burst);
}

void DandelionField::make_puff() {
	snd_puff = synth_puff();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("PuffVoice");
	voice->set_stream(snd_puff);
	voice->set_volume_db(-16.0f);
	voice->set_max_distance(16.0f);
	add_child(voice);
}

// 0.28 秒的"噗"：一团被吹散的细碎气流
Ref<AudioStreamWAV> DandelionField::synth_puff() {
	const int sr = 22050;
	const float dur = 0.28f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1664525 + 1013904223) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.18f;
		float env = Math::clamp(t / 0.008f, 0.0f, 1.0f) * std::exp(-9.0f * t);
		float samp = (white - lp) * env * 0.6f;
		int16_t q = (int16_t)Math::clamp(samp * 12000.0f, -32000.0f, 32000.0f);
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
