#include "birdbath.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
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

void Birdbath::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Birdbath::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Birdbath::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Birdbath::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Birdbath::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Birdbath::interact);
	ClassDB::bind_method(D_METHOD("get_water_level"), &Birdbath::get_water_level);
	ClassDB::bind_method(D_METHOD("get_birds_visible"), &Birdbath::get_birds_visible);
	ClassDB::bind_method(D_METHOD("get_refills"), &Birdbath::get_refills);
	ClassDB::bind_method(D_METHOD("get_ice_on"), &Birdbath::get_ice_on);
}

String Birdbath::get_prompt() const {
	return water_level < 0.35f ? String::utf8("按 E 给鸟浴盆添水") : String::utf8("按 E 换一盆清水");
}

Vector3 Birdbath::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.72f, 0.0f);
}

void Birdbath::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	water_level = 1.0f;
	refills++;
	spawn_ripple();
	ripple_age[1] = 0.12f;
	if (splash_voice != nullptr && snd_splash.is_valid()) {
		splash_voice->play(0.0f);
	}
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("添了一盆清水 · 水面又亮起来"), 2.8f);
		}
	}
}

void Birdbath::spawn_ripple() {
	ripple_age[ripple_next] = 0.0f;
	ripple_next = (ripple_next + 1) % 2;
}

void Birdbath::_notification(int p_what) {
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
			const float delta = (float)get_process_delta_time();
			t += delta;
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}
			if (sky == nullptr) {
				sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			}
			// 水慢慢蒸发
			if (ice_on == false) {
				water_level = Math::max(0.12f, water_level - delta * 0.0035f);
			}
			ice_on = sky != nullptr && sky->get_snow_amount() > 0.5f;
			if (water != nullptr) {
				water->set_visible(!ice_on && water_level > 0.13f);
			}
			if (ice != nullptr) {
				ice->set_visible(ice_on);
			}

			const bool day = sky == nullptr || (sky->get_time_of_day() > 0.27f && sky->get_time_of_day() < 0.73f);
			const bool can_bird = day && !ice_on && water_level > 0.3f
					&& (sky == nullptr || (sky->get_rain_amount() < 0.4f && sky->get_snow_amount() < 0.3f));
			bird_timer -= delta;
			birds_visible = 0;
			for (int i = 0; i < BIRD_COUNT; i++) {
				if (birds[i] == nullptr) {
					continue;
				}
				float d = 999.0f;
				if (player != nullptr) {
					d = player->get_global_position().distance_to(get_global_position() + Vector3(0.0f, 0.75f, 0.0f));
				}
				if (d < 2.3f) {
					bird_on_ground[i] = false;
					bird_timer = 2.5f;
				} else if (bird_timer <= 0.0f && can_bird) {
					bird_on_ground[i] = true;
				}
				if (!can_bird) {
					bird_on_ground[i] = false;
				}
				const bool vis = can_bird && bird_on_ground[i];
				birds[i]->set_visible(vis);
				if (!vis) {
					continue;
				}
				birds_visible++;
				const float a = (float)i * 3.14159f;
				const float bob = 0.02f * std::max(0.0f, std::sin(t * 3.0f + (float)i));
				birds[i]->set_position(Vector3(std::cos(a) * 0.30f, 0.78f + bob, std::sin(a) * 0.30f));
				birds[i]->set_rotation(Vector3(0.25f + 0.20f * std::max(0.0f, std::sin(t * 3.2f + (float)i)),
						-a + 1.5708f, 0.0f));
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = std::max(0.0f, std::sin(t * 5.0f + (float)i)) * 0.5f;
						wings[i][w]->set_rotation(Vector3(0.0f, 0.0f, (w == 0 ? 1.0f : -1.0f) * flap));
					}
				}
				// 偶尔啄水，激起涟漪
				if (std::fmod(t + (float)i * 1.7f, 4.0f) < delta && water_level > 0.3f) {
					spawn_ripple();
				}
			}
			if (bird_timer <= 0.0f) {
				bird_timer = 4.0f;
			}

			// 涟漪
			for (int i = 0; i < 2; i++) {
				if (ripples[i] == nullptr) {
					continue;
				}
				if (ripple_age[i] >= 1.0f) {
					ripples[i]->set_visible(false);
					continue;
				}
				ripple_age[i] += delta * 1.3f;
				const float age = Math::min(ripple_age[i], 1.0f);
				const float s = 0.35f + age * 1.2f;
				ripples[i]->set_visible(!ice_on);
				ripples[i]->set_scale(Vector3(s, 1.0f, s));
				if (ripple_mats[i].is_valid()) {
					Color c = ripple_mats[i]->get_albedo();
					c.a = (1.0f - age) * 0.45f;
					ripple_mats[i]->set_albedo(c);
				}
			}
		} break;
		default:
			break;
	}
}

void Birdbath::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color stone(0.54f, 0.52f, 0.48f);

	// 石座与盆
	Ref<CylinderMesh> base;
	base.instantiate();
	base->set_top_radius(0.30f);
	base->set_bottom_radius(0.34f);
	base->set_height(0.10f);
	base->set_radial_segments(12);
	base->set_rings(1);
	base->set_material(meshutil::toon_material(toon, stone, 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(base);
	bmi->set_position(Vector3(0.0f, 0.05f, 0.0f));
	add_child(bmi);
	Ref<CylinderMesh> pedestal;
	pedestal.instantiate();
	pedestal->set_top_radius(0.14f);
	pedestal->set_bottom_radius(0.20f);
	pedestal->set_height(0.55f);
	pedestal->set_radial_segments(12);
	pedestal->set_rings(1);
	pedestal->set_material(meshutil::toon_material(toon, stone, 0.2f));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_mesh(pedestal);
	pmi->set_position(Vector3(0.0f, 0.37f, 0.0f));
	add_child(pmi);
	Ref<CylinderMesh> bowl;
	bowl.instantiate();
	bowl->set_top_radius(0.40f);
	bowl->set_bottom_radius(0.24f);
	bowl->set_height(0.14f);
	bowl->set_radial_segments(16);
	bowl->set_rings(1);
	bowl->set_material(meshutil::toon_material(toon, Color(0.50f, 0.48f, 0.44f), 0.2f));
	MeshInstance3D *bw = memnew(MeshInstance3D);
	bw->set_mesh(bowl);
	bw->set_position(Vector3(0.0f, 0.71f, 0.0f));
	add_child(bw);

	// 水面与薄冰
	Ref<CylinderMesh> water_mesh;
	water_mesh.instantiate();
	water_mesh->set_top_radius(0.33f);
	water_mesh->set_bottom_radius(0.33f);
	water_mesh->set_height(0.012f);
	water_mesh->set_radial_segments(16);
	water_mesh->set_rings(1);
	Ref<StandardMaterial3D> wmat;
	wmat.instantiate();
	wmat->set_albedo(Color(0.30f, 0.52f, 0.62f, 0.85f));
	wmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wmat->set_roughness(0.12f);
	water_mesh->set_material(wmat);
	water = memnew(MeshInstance3D);
	water->set_mesh(water_mesh);
	water->set_position(Vector3(0.0f, 0.755f, 0.0f));
	add_child(water);

	Ref<CylinderMesh> ice_mesh;
	ice_mesh.instantiate();
	ice_mesh->set_top_radius(0.33f);
	ice_mesh->set_bottom_radius(0.33f);
	ice_mesh->set_height(0.014f);
	ice_mesh->set_radial_segments(16);
	ice_mesh->set_rings(1);
	Ref<StandardMaterial3D> imat;
	imat.instantiate();
	imat->set_albedo(Color(0.86f, 0.92f, 0.96f, 0.95f));
	imat->set_roughness(0.35f);
	ice_mesh->set_material(imat);
	ice = memnew(MeshInstance3D);
	ice->set_mesh(ice_mesh);
	ice->set_position(Vector3(0.0f, 0.762f, 0.0f));
	ice->set_visible(false);
	add_child(ice);

	// 两圈涟漪
	for (int i = 0; i < 2; i++) {
		Ref<TorusMesh> ring;
		ring.instantiate();
		ring->set_inner_radius(0.16f);
		ring->set_outer_radius(0.18f);
		ring->set_rings(20);
		ring->set_ring_segments(3);
		ripple_mats[i].instantiate();
		ripple_mats[i]->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		ripple_mats[i]->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		ripple_mats[i]->set_albedo(Color(0.80f, 0.92f, 1.0f, 0.45f));
		ripple_mats[i]->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		ring->set_material(ripple_mats[i]);
		MeshInstance3D *rm = memnew(MeshInstance3D);
		rm->set_mesh(ring);
		rm->set_position(Vector3(0.0f, 0.766f, 0.0f));
		rm->set_visible(false);
		add_child(rm);
		ripples[i] = rm;
	}

	// 两只小鸟
	for (int i = 0; i < BIRD_COUNT; i++) {
		Node3D *bird = memnew(Node3D);
		bird->set_name(String("BathBird") + String::num_int64(i));
		bird->set_visible(false);
		add_child(bird);
		birds[i] = bird;
		const Color brown(0.56f, 0.42f, 0.28f);
		MeshInstance3D *body = meshutil::add_sphere(bird, toon, 0.055f, Vector3(0.0f, 0.05f, 0.0f), brown);
		body->set_scale(Vector3(1.0f, 0.9f, 1.3f));
		meshutil::add_sphere(bird, toon, 0.034f, Vector3(0.0f, 0.10f, 0.05f), Color(0.64f, 0.50f, 0.34f));
		meshutil::add_box(bird, toon, Vector3(0.018f, 0.012f, 0.028f), Vector3(0.0f, 0.095f, 0.088f), Color(0.86f, 0.64f, 0.24f));
		meshutil::add_box(bird, toon, Vector3(0.10f, 0.008f, 0.05f), Vector3(0.0f, 0.035f, -0.075f), Color(0.42f, 0.30f, 0.20f), Vector3(-0.3f, 0.0f, 0.0f));
		for (int w = 0; w < 2; w++) {
			MeshInstance3D *wing = meshutil::add_box(bird, toon, Vector3(0.11f, 0.008f, 0.055f),
					Vector3((w == 0 ? -1.0f : 1.0f) * 0.055f, 0.06f, 0.0f), Color(0.48f, 0.34f, 0.22f));
			wings[i][w] = wing;
		}
	}

	build_splash();
	splash_voice = memnew(AudioStreamPlayer3D);
	splash_voice->set_name("SplashVoice");
	splash_voice->set_stream(snd_splash);
	splash_voice->set_volume_db(-8.0f);
	splash_voice->set_max_distance(14.0f);
	add_child(splash_voice);
}

void Birdbath::build_splash() {
	const int sr = 22050;
	const int n = (int)(0.45f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(6621u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float env = std::exp(-tt * 9.0f) * Math::clamp(tt / 0.01f, 0.0f, 1.0f);
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.12f;
		buf[i] = (lp * 2.0f + std::sin(6.2831853f * 120.0f * tt) * 0.3f) * env;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 26000.0f, -26000.0f, 26000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	snd_splash = wav;
}
