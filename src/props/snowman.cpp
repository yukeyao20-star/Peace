#include "snowman.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

bool Snowman::get_puff_emitting() const {
	return puff != nullptr && puff->is_emitting();
}

void Snowman::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Snowman::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Snowman::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Snowman::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Snowman::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Snowman::interact);
	ClassDB::bind_method(D_METHOD("get_built"), &Snowman::get_built);
	ClassDB::bind_method(D_METHOD("get_building"), &Snowman::get_building);
	ClassDB::bind_method(D_METHOD("get_crunch_ready"), &Snowman::get_crunch_ready);
	ClassDB::bind_method(D_METHOD("get_puff_emitting"), &Snowman::get_puff_emitting);
}

void Snowman::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
				if (terrain != nullptr) {
					Vector3 p = get_global_position();
					p.y = terrain->get_terrain_height(p.x, p.z) + 0.01f;
					set_global_position(p);
				}
			}
			add_to_group("interactables");
			build();
			make_crunch();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			if (snow < 0.1f) {
				built = false;
				building = false;
				build_t = 0.0f;
				if (pile) {
					pile->set_visible(false);
				}
				if (body) {
					body->set_visible(false);
				}
				return;
			}
			if (pile) {
				pile->set_visible(!built && !building);
			}
			if (building) {
				build_t += delta;
				float pgr = Math::clamp(build_t / 1.8f, 0.0f, 1.0f);
				if (body) {
					body->set_visible(true);
				}
				for (int i = 0; i < 3; i++) {
					float start = (float)i * 0.30f;
					float p = Math::clamp((pgr - start) / 0.35f, 0.0f, 1.0f);
					if (parts[i] != nullptr) {
						float s = p * (1.0f + 0.08f * std::sin(t * 12.0f));
						parts[i]->set_visible(p > 0.01f);
						parts[i]->set_scale(Vector3(s, s, s));
					}
				}
				if (face) {
					face->set_visible(pgr > 0.9f);
				}
				if (pgr >= 1.0f) {
					building = false;
					built = true;
					for (int i = 0; i < 3; i++) {
						if (parts[i]) {
							parts[i]->set_scale(Vector3(1, 1, 1));
						}
					}
					if (face) {
						face->set_visible(true);
					}
				}
			}
		} break;
		default:
			break;
	}
}

String Snowman::get_prompt() const {
	if (built) {
		return String::utf8("按 E 拍拍雪人");
	}
	return String::utf8("按 E 堆雪人");
}

Vector3 Snowman::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.7f, 0.0f);
}

float Snowman::get_interact_distance() const {
	return 2.6f;
}

void Snowman::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (get_parent() != nullptr && sky != nullptr && sky->get_snow_amount() < 0.1f) {
		return;
	}
	if (built) {
		if (puff != nullptr) {
			puff->restart();
		}
		if (crunch != nullptr) {
			crunch->play(0.0f);
		}
		if (get_parent() != nullptr) {
			Node *gd = get_parent()->get_node_or_null("GameDirector");
			if (gd != nullptr) {
				gd->call("flash_hint", String::utf8("雪人朝你笑了笑 · 雪粉扑簌簌落下"), 2.8f);
			}
		}
		return;
	}
	if (building) {
		return;
	}
	building = true;
	build_t = 0.0f;
	if (pile) {
		pile->set_visible(false);
	}
	if (crunch != nullptr) {
		crunch->play(0.0f);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("开始堆雪人 · 滚啊滚，雪球越滚越大"), 3.0f);
		}
	}
}

void Snowman::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color snow_c(0.94f, 0.96f, 0.98f);
	const Color coal(0.12f, 0.11f, 0.10f);
	const Color carrot(0.94f, 0.52f, 0.18f);
	const Color stick(0.42f, 0.29f, 0.16f);
	const Color scarf(0.78f, 0.22f, 0.20f);

	// 雪堆
	pile = memnew(Node3D);
	pile->set_name("SnowPile");
	add_child(pile);
	meshutil::add_sphere(pile, toon, 0.55f, Vector3(0, 0.20f, 0), snow_c)->set_scale(Vector3(1.4f, 0.55f, 1.1f));
	meshutil::add_sphere(pile, toon, 0.35f, Vector3(0.55f, 0.15f, 0.4f), snow_c)->set_scale(Vector3(1.0f, 0.6f, 1.0f));

	// 雪人三段身体
	body = memnew(Node3D);
	body->set_name("SnowmanBody");
	add_child(body);
	body->set_visible(false);
	const float radii[3] = {0.46f, 0.33f, 0.25f};
	const float heights[3] = {0.44f, 1.02f, 1.48f};
	for (int i = 0; i < 3; i++) {
		parts[i] = memnew(MeshInstance3D);
		parts[i]->set_name(String("Ball") + String::num_int64(i));
		Ref<SphereMesh> sm;
		sm.instantiate();
		sm->set_radius(radii[i]);
		sm->set_height(radii[i] * 2.0f);
		sm->set_radial_segments(12);
		sm->set_rings(7);
		sm->set_material(meshutil::toon_material(toon, snow_c, 0.45f));
		parts[i]->set_mesh(sm);
		parts[i]->set_position(Vector3(0.0f, heights[i], 0.0f));
		parts[i]->set_scale(Vector3(0.001f, 0.001f, 0.001f));
		parts[i]->set_visible(false);
		body->add_child(parts[i]);
	}
	// 围巾
	meshutil::add_box(body, toon, Vector3(0.44f, 0.10f, 0.44f), Vector3(0, 1.26f, 0), scarf);
	meshutil::add_box(body, toon, Vector3(0.12f, 0.30f, 0.05f), Vector3(0.18f, 1.10f, 0.18f), scarf);

	// 脸与装饰（随头一起出现）
	face = memnew(Node3D);
	face->set_name("SnowmanFace");
	face->set_position(Vector3(0.0f, 1.48f, 0.0f));
	face->set_visible(false);
	body->add_child(face);
	Ref<CylinderMesh> nose;
	nose.instantiate();
	nose->set_top_radius(0.0f);
	nose->set_bottom_radius(0.045f);
	nose->set_height(0.26f);
	nose->set_radial_segments(6);
	nose->set_rings(1);
	nose->set_material(meshutil::toon_material(toon, carrot, 0.2f));
	MeshInstance3D *nm = memnew(MeshInstance3D);
	nm->set_name("Nose");
	nm->set_mesh(nose);
	nm->set_position(Vector3(0.0f, 0.0f, 0.32f));
	nm->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	face->add_child(nm);
	meshutil::add_sphere(face, toon, 0.035f, Vector3(-0.08f, 0.08f, 0.22f), coal);
	meshutil::add_sphere(face, toon, 0.035f, Vector3(0.08f, 0.08f, 0.22f), coal);
	for (int i = 0; i < 3; i++) {
		meshutil::add_sphere(face, toon, 0.028f, Vector3(0.0f, -0.12f - (float)i * 0.10f, 0.235f), coal);
	}
	// 树枝手臂
	meshutil::add_box(body, toon, Vector3(0.55f, 0.035f, 0.035f), Vector3(-0.55f, 1.10f, 0.0f), stick, Vector3(0.0f, 0.0f, 0.35f));
	meshutil::add_box(body, toon, Vector3(0.55f, 0.035f, 0.035f), Vector3(0.55f, 1.10f, 0.0f), stick, Vector3(0.0f, 0.0f, -0.35f));

	// 拍雪粉
	Ref<SphereMesh> flake;
	flake.instantiate();
	flake->set_radius(0.035f);
	flake->set_height(0.07f);
	flake->set_radial_segments(6);
	flake->set_rings(4);
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fm->set_albedo(snow_c);
	flake->set_material(fm);
	puff = memnew(CPUParticles3D);
	puff->set_name("SnowPuff");
	puff->set_amount(18);
	puff->set_lifetime(1.2f);
	puff->set_one_shot(true);
	puff->set("explosiveness", 0.85f);
	puff->set_mesh(flake);
	puff->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	puff->set_spread(80.0f);
	puff->set_gravity(Vector3(0.0f, -2.4f, 0.0f));
	puff->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.5f);
	puff->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.2f);
	puff->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	puff->set_emission_sphere_radius(0.35f);
	puff->set_position(Vector3(0.0f, 1.1f, 0.0f));
	puff->set_emitting(false);
	add_child(puff);
}

void Snowman::make_crunch() {
	snd_crunch = synth_crunch();
	crunch = memnew(AudioStreamPlayer3D);
	crunch->set_name("SnowCrunch");
	crunch->set_stream(snd_crunch);
	crunch->set_volume_db(-14.0f);
	crunch->set_max_distance(16.0f);
	add_child(crunch);
}

// 0.4 秒的踩雪/滚雪声
Ref<AudioStreamWAV> Snowman::synth_crunch() {
	const int sr = 22050;
	const float dur = 0.42f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float white = ((float)((i * 1103515245 + 12345) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.22f;
		float env = Math::clamp(t / 0.01f, 0.0f, 1.0f) * std::exp(-7.0f * t);
		float samp = (white - lp * 0.5f) * env * 0.6f;
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
