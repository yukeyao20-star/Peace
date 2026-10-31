#include "camp_cat.h"

#include "../world/footprint_manager.h"

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

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

bool CampCat::get_purr_playing() const {
	return purring;
}

bool CampCat::get_at_shelter() const {
	return get_global_position().distance_to(shelter) < 0.45f;
}

bool CampCat::get_hearts_emitting() const {
	return hearts != nullptr && hearts->is_emitting();
}

String CampCat::get_prompt() const {
	return String::utf8("按 E 摸摸猫");
}

Vector3 CampCat::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.35f, 0.0f);
}

float CampCat::get_interact_distance() const {
	return 2.4f;
}

void CampCat::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	pet_t = 2.4f;
	if (hearts != nullptr) {
		hearts->restart();
	}
	if (purr != nullptr) {
		purr->play(0.0f);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("on_cat_petted");
			gd->call("flash_hint", String::utf8("猫咪眯起眼睛 · 呼噜声更响了"), 3.0f);
		}
	}
}

void CampCat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &CampCat::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &CampCat::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &CampCat::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &CampCat::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &CampCat::interact);
	ClassDB::bind_method(D_METHOD("get_pet_time"), &CampCat::get_pet_time);
	ClassDB::bind_method(D_METHOD("get_hearts_emitting"), &CampCat::get_hearts_emitting);
	ClassDB::bind_method(D_METHOD("get_purr_ready"), &CampCat::get_purr_ready);
	ClassDB::bind_method(D_METHOD("get_purr_playing"), &CampCat::get_purr_playing);
	ClassDB::bind_method(D_METHOD("get_at_shelter"), &CampCat::get_at_shelter);
}

void CampCat::_notification(int p_what) {
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
			home = get_global_position();
			if (terrain != nullptr) {
				home.y = terrain->get_terrain_height(home.x, home.z) + 0.01f;
			}
			// 长椅底下是躲雨点（长椅在世界里没有旋转）
			if (get_parent() != nullptr) {
				Node3D *bench = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Bench"));
				if (bench != nullptr) {
					shelter = bench->get_global_position() + Vector3(0.15f, 0.02f, 0.55f);
					if (terrain != nullptr) {
						shelter.y = terrain->get_terrain_height(shelter.x, shelter.z) + 0.01f;
					}
				} else {
					shelter = home;
				}
			} else {
				shelter = home;
			}
			set_global_position(home);
			add_to_group("interactables");
			build();
			make_purr();
			make_hearts();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (pet_t > 0.0f) {
				pet_t = Math::max(0.0f, pet_t - delta);
			}
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			const bool seek_shelter = rain > 0.42f || snow > 0.7f;
			Vector3 want = seek_shelter ? shelter : home;
			Vector3 pos = get_global_position();
			pos = pos.lerp(want, 1.0f - std::exp(-1.7f * delta));
			set_global_position(pos);
			{
				Vector3 pd = pos - last_print;
				pd.y = 0.0f;
				Vector3 to = want - pos;
				to.y = 0.0f;
				if (pd.length() > 0.20f && to.length_squared() > 0.004f && FootprintManager::get_singleton() != nullptr) {
					last_print = pos;
					const float yaw = std::atan2(to.x, to.z);
					const Vector3 side(std::cos(yaw) * 0.055f, 0.0f, -std::sin(yaw) * 0.055f);
					FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_PAW, pos + Vector3(0, 0.01f, 0) + side, yaw, 0.42f);
					FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_PAW, pos + Vector3(0, 0.01f, 0) - side, yaw, 0.42f);
				}
			}

			const Vector3 pp = (player != nullptr) ? player->get_global_position() : Vector3(0, -999, 0);
			const float near = pos.distance_to(pp);
			const bool alert = near < 3.6f || pet_t > 0.0f;
			if (body != nullptr) {
				float breathe = 1.0f + 0.035f * std::sin(t * 1.35f);
				body->set_scale(Vector3(1.0f, breathe, 1.0f));
			}
			if (head != nullptr) {
				float lift = (pet_t > 0.0f) ? -0.50f : (alert ? -0.38f : 0.0f);
				head->set_rotation(Vector3(Math::lerp(head->get_rotation().x, lift, 1.0f - std::exp(-5.0f * delta)),
						std::sin(t * 0.35f) * 0.06f, 0.0f));
			}
			if (tail != nullptr) {
				tail->set_rotation(Vector3(
						std::sin(t * 0.8f) * 0.06f,
						0.0f,
						std::sin(t * 1.15f) * 0.18f + (alert ? 0.12f : 0.0f)));
			}
			if (ear_l != nullptr && ear_r != nullptr) {
				float twitch = alert ? std::sin(t * 9.0f) * 0.10f : 0.0f;
				ear_l->set_rotation(Vector3(0.0f, 0.0f, 0.18f + twitch));
				ear_r->set_rotation(Vector3(0.0f, 0.0f, -0.18f - twitch));
			}

			// 呼噜：玩家蹲在近处才响，走远渐隐
			const bool want_purr = (near < 4.2f || pet_t > 0.0f) && rain < 0.5f;
			purr_fade = Math::lerp(purr_fade, want_purr ? 1.0f : 0.0f, 1.0f - std::exp(-2.5f * delta));
			float boost = (pet_t > 0.0f) ? 1.0f : 0.0f;
			if (purr != nullptr) {
				if (purr_fade > 0.03f) {
					if (!purr->is_playing()) {
						purr->play(0.0f);
					}
					purr->set_volume_db(-30.0f + 10.0f * purr_fade + 4.0f * boost);
					purring = true;
				} else {
					if (purr->is_playing()) {
						purr->stop();
					}
					purring = false;
				}
			}
		} break;
		default:
			break;
	}
}

void CampCat::make_hearts() {
	Ref<SphereMesh> heart;
	heart.instantiate();
	heart->set_radius(0.045f);
	heart->set_height(0.07f);
	heart->set_radial_segments(7);
	heart->set_rings(4);
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_albedo(Color(1.0f, 0.55f, 0.62f, 0.9f));
	mat->set("emission_enabled", true);
	mat->set("emission", Color(1.0f, 0.42f, 0.52f));
	mat->set("emission_energy_multiplier", 1.2f);
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	heart->set_material(mat);
	hearts = memnew(CPUParticles3D);
	hearts->set_name("Hearts");
	hearts->set_amount(10);
	hearts->set_lifetime(1.5f);
	hearts->set_one_shot(true);
	hearts->set("explosiveness", 0.7f);
	hearts->set_mesh(heart);
	hearts->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	hearts->set_spread(28.0f);
	hearts->set_gravity(Vector3(0.05f, 0.18f, 0.02f));
	hearts->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.25f);
	hearts->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.55f);
	hearts->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	hearts->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f);
	hearts->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	hearts->set_emission_sphere_radius(0.12f);
	hearts->set_position(Vector3(0.0f, 0.55f, 0.0f));
	hearts->set_emitting(false);
	add_child(hearts);
}

void CampCat::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color fur(0.85f, 0.50f, 0.22f);
	const Color fur_light(0.95f, 0.70f, 0.42f);
	const Color cream(0.96f, 0.90f, 0.78f);
	const Color stripe(0.55f, 0.28f, 0.10f);
	const Color pink(0.92f, 0.62f, 0.62f);

	// 朝向火堆
	Vector3 to_fire = Vector3(-8.6f, 0.0f, -13.5f) - get_global_position();
	to_fire.y = 0.0f;
	if (to_fire.length_squared() > 0.001f) {
		set_rotation(Vector3(0.0f, std::atan2(to_fire.x, to_fire.z), 0.0f));
	}

	body = memnew(Node3D);
	body->set_name("Body");
	add_child(body);

	MeshInstance3D *torso = memnew(MeshInstance3D);
	Ref<SphereMesh> tm;
	tm.instantiate();
	tm->set_radius(0.20f);
	tm->set_height(0.40f);
	tm->set_radial_segments(14);
	tm->set_rings(8);
	tm->set_material(meshutil::toon_material(toon, fur, 0.4f));
	torso->set_mesh(tm);
	torso->set_position(Vector3(0.0f, 0.15f, 0.0f));
	torso->set_scale(Vector3(1.35f, 0.78f, 0.9f));
	body->add_child(torso);

	// 背上的虎斑
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(body, toon, Vector3(0.30f, 0.012f, 0.032f), Vector3(-0.06f + (float)i * 0.10f, 0.275f, 0.0f), stripe);
	}
	meshutil::add_sphere(body, toon, 0.10f, Vector3(0.0f, 0.13f, 0.21f), cream);

	head = memnew(Node3D);
	head->set_name("Head");
	head->set_position(Vector3(0.0f, 0.20f, 0.27f));
	body->add_child(head);
	meshutil::add_sphere(head, toon, 0.125f, Vector3(0.0f, 0.0f, 0.0f), fur);
	meshutil::add_sphere(head, toon, 0.075f, Vector3(0.0f, -0.015f, 0.07f), cream);
	// 闭着的眼睛：两条细黑缝
	meshutil::add_box(head, toon, Vector3(0.055f, 0.008f, 0.008f), Vector3(-0.045f, 0.02f, 0.105f), Color(0.12f, 0.08f, 0.05f), Vector3(0, 0, 0.08f));
	meshutil::add_box(head, toon, Vector3(0.055f, 0.008f, 0.008f), Vector3(0.045f, 0.02f, 0.105f), Color(0.12f, 0.08f, 0.05f), Vector3(0, 0, -0.08f));
	MeshInstance3D *nose = memnew(MeshInstance3D);
	Ref<SphereMesh> nm;
	nm.instantiate();
	nm->set_radius(0.012f);
	nm->set_height(0.024f);
	nm->set_radial_segments(6);
	nm->set_rings(4);
	nm->set_material(meshutil::toon_material(toon, pink, 0.2f));
	nose->set_mesh(nm);
	nose->set_position(Vector3(0.0f, -0.01f, 0.145f));
	head->add_child(nose);

	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> ear;
		ear.instantiate();
		ear->set_top_radius(0.0f);
		ear->set_bottom_radius(0.045f);
		ear->set_height(0.09f);
		ear->set_radial_segments(5);
		ear->set_rings(1);
		ear->set_material(meshutil::toon_material(toon, fur_light, 0.3f));
		MeshInstance3D *em = memnew(MeshInstance3D);
		em->set_mesh(ear);
		em->set_position(Vector3(i == 0 ? -0.07f : 0.07f, 0.10f, 0.02f));
		em->set_rotation(Vector3(0.0f, 0.0f, i == 0 ? 0.26f : -0.26f));
		head->add_child(em);
		if (i == 0) {
			ear_l = em;
		} else {
			ear_r = em;
		}
	}

	// 前爪
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(body, toon, Vector3(0.06f, 0.045f, 0.14f), Vector3(i == 0 ? -0.11f : 0.11f, 0.035f, 0.19f), cream);
	}
	// 尾巴：绕到身侧的三段
	tail = memnew(Node3D);
	tail->set_name("Tail");
	tail->set_position(Vector3(0.0f, 0.12f, -0.20f));
	body->add_child(tail);
	Ref<SphereMesh> tseg;
	tseg.instantiate();
	tseg->set_radius(0.045f);
	tseg->set_height(0.09f);
	tseg->set_radial_segments(8);
	tseg->set_rings(5);
	tseg->set_material(meshutil::toon_material(toon, fur, 0.35f));
	for (int i = 0; i < 3; i++) {
		MeshInstance3D *seg = memnew(MeshInstance3D);
		seg->set_mesh(tseg);
		seg->set_position(Vector3(0.10f + (float)i * 0.09f, 0.02f - (float)i * 0.008f, -0.07f - (float)i * 0.06f));
		seg->set_scale(Vector3(1.0f, 0.9f, 1.5f));
		tail->add_child(seg);
	}
}

void CampCat::make_purr() {
	snd_purr = synth_purr();
	purr = memnew(AudioStreamPlayer3D);
	purr->set_name("Purr");
	purr->set_stream(snd_purr);
	purr->set_volume_db(-30.0f);
	purr->set_max_distance(9.0f);
	add_child(purr);
}

// 2 秒可循环的低频呼噜：28Hz 起伏 + 一点点毛皮摩擦的噪声
Ref<AudioStreamWAV> CampCat::synth_purr() {
	const int sr = 22050;
	const float dur = 2.0f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float env = 0.55f + 0.45f * std::sin(6.2831853f * 1.0f * t);
		float rumble = std::sin(6.2831853f * 27.5f * t) * 0.5f
				+ std::sin(6.2831853f * 55.0f * t) * 0.25f;
		float white = ((float)((i * 1664525 + 1013904223) & 0x7FFF) / 32767.0f - 0.5f);
		lp += (white - lp) * 0.06f;
		float samp = (rumble * env * 0.6f + lp * 0.25f) * 0.55f;
		// 循环点淡入淡出
		float fade = 1.0f;
		if (i < 1100) {
			fade = (float)i / 1100.0f;
		}
		if (i > n - 1100) {
			fade = (float)(n - i) / 1100.0f;
		}
		int16_t q = (int16_t)Math::clamp(samp * fade * 9000.0f, -32000.0f, 32000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
	wav->set_loop_end(n);
	wav->set_data(bytes);
	return wav;
}
