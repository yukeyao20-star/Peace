#include "scarecrow.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void Scarecrow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Scarecrow::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Scarecrow::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Scarecrow::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Scarecrow::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Scarecrow::interact);
	ClassDB::bind_method(D_METHOD("get_adjust_count"), &Scarecrow::get_adjust_count);
	ClassDB::bind_method(D_METHOD("get_crows_visible"), &Scarecrow::get_crows_visible);
	ClassDB::bind_method(D_METHOD("get_hat_tilt"), &Scarecrow::get_hat_tilt);
	ClassDB::bind_method(D_METHOD("get_crow_angle"), &Scarecrow::get_crow_angle);
}

float Scarecrow::get_hat_tilt() const {
	return (hat != nullptr) ? hat->get_rotation().x : 0.0f;
}

String Scarecrow::get_prompt() const {
	return String::utf8("按 E 把草帽扶正");
}

Vector3 Scarecrow::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.4f, 0.0f);
}

void Scarecrow::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (hat != nullptr) {
		hat->set_rotation(Vector3(hat->get_rotation().x > 0.05f ? 0.0f : 0.24f, 0.0f, 0.0f));
		adjust_count++;
	}
}

void Scarecrow::_notification(int p_what) {
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
			if (sky == nullptr && get_tree() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			}

			// 上半身和袖子随风轻晃
			if (upper != nullptr) {
				upper->set_rotation(Vector3(0.0f, 0.0f,
						std::sin(t * 0.85f) * 0.025f + std::sin(t * 2.1f) * 0.008f));
			}
			for (int i = 0; i < 2; i++) {
				if (sleeves[i] != nullptr) {
					sleeves[i]->set_rotation(Vector3(std::sin(t * 2.3f + (float)i * 1.7f) * 0.35f,
							0.0f, std::sin(t * 1.8f + (float)i) * 0.2f));
				}
			}

			// 乌鸦：白天绕着稻草人转圈，雨雪太猛或夜里回去
			bool show = true;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				show = tod > 0.26f && tod < 0.74f
						&& sky->get_snow_amount() < 0.35f && sky->get_rain_amount() < 0.75f;
			}
			crows_visible = show ? CROW_COUNT : 0;
			if (show) {
				crow_angle += delta * 0.55f;
			}
			for (int i = 0; i < CROW_COUNT; i++) {
				if (crows[i] == nullptr) {
					continue;
				}
				crows[i]->set_visible(show);
				if (!show) {
					continue;
				}
				const float a = crow_angle + (float)i * 3.14159f;
				const float r = 2.15f + 0.20f * std::sin(t * 0.7f + (float)i);
				const float y = 2.45f + 0.15f * std::sin(t * 1.3f + (float)i * 2.0f);
				crows[i]->set_position(Vector3(std::cos(a) * r, y, std::sin(a) * r));
				crows[i]->set_rotation(Vector3(0.0f, -a + 1.5708f, 0.0f));
				for (int w = 0; w < WING_COUNT; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = std::sin(t * 9.0f + (float)i * 1.3f) * 0.65f;
						wings[i][w]->set_rotation(Vector3(0.0f, 0.0f,
								(w == 0 ? 1.0f : -1.0f) * (0.25f + flap)));
					}
				}
			}

			// 偶尔"哑——"一声
			if (show && t - last_caw > next_caw) {
				last_caw = t;
				next_caw = 5.0f + (float)((adjust_count * 7919 + 13) % 3000) / 1000.0f;
				if (caw_voice != nullptr && snd_caw.is_valid()) {
					caw_voice->set_pitch_scale(0.92f + 0.03f * (float)(adjust_count % 5));
					caw_voice->play(0.0f);
				}
			}
		} break;
		default:
			break;
	}
}

void Scarecrow::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.45f, 0.31f, 0.18f);
	const Color straw_col(0.86f, 0.72f, 0.36f);
	const Color cloth(0.42f, 0.52f, 0.58f);

	// 木十字架
	meshutil::add_box(this, toon, Vector3(0.10f, 1.95f, 0.10f), Vector3(0.0f, 0.97f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(1.28f, 0.10f, 0.10f), Vector3(0.0f, 1.36f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.22f, 0.06f, 0.22f), Vector3(0.0f, 0.04f, 0.0f), Color(0.38f, 0.32f, 0.25f));

	// 上半身（会随风轻晃）
	upper = memnew(Node3D);
	upper->set_name("Upper");
	upper->set_position(Vector3(0.0f, 0.92f, 0.0f));
	add_child(upper);

	meshutil::add_box(upper, toon, Vector3(0.56f, 0.58f, 0.26f), Vector3(0.0f, 0.30f, 0.0f), cloth);
	meshutil::add_box(upper, toon, Vector3(0.58f, 0.06f, 0.28f), Vector3(0.0f, 0.02f, 0.0f), Color(0.60f, 0.46f, 0.24f)); // 腰带
	// 补丁
	meshutil::add_box(upper, toon, Vector3(0.14f, 0.14f, 0.02f), Vector3(-0.12f, 0.36f, 0.135f), Color(0.62f, 0.34f, 0.24f));
	meshutil::add_box(upper, toon, Vector3(0.10f, 0.10f, 0.02f), Vector3(0.14f, 0.18f, 0.135f), Color(0.36f, 0.44f, 0.30f));

	// 稻草手与袖子（袖子可摆）
	for (int i = 0; i < 2; i++) {
		const float sgn = (i == 0) ? -1.0f : 1.0f;
		meshutil::add_box(upper, toon, Vector3(0.18f, 0.10f, 0.16f), Vector3(sgn * 0.56f, 0.36f, 0.0f), cloth);
		MeshInstance3D *sl = meshutil::add_box(upper, toon, Vector3(0.34f, 0.20f, 0.16f),
				Vector3(sgn * 0.68f, 0.22f, 0.0f), cloth);
		sleeves[i] = sl;
		// 手腕处露出的稻草
		for (int k = 0; k < 3; k++) {
			meshutil::add_box(upper, toon, Vector3(0.030f, 0.22f, 0.012f),
					Vector3(sgn * 0.86f, 0.12f - 0.03f * (float)k, 0.02f * (float)(k - 1)),
					straw_col, Vector3(0.0f, 0.0f, sgn * (0.45f + 0.12f * (float)k)));
		}
	}

	// 头、眼睛、嘴
	MeshInstance3D *head = meshutil::add_sphere(upper, toon, 0.17f, Vector3(0.0f, 0.76f, 0.0f), straw_col);
	head->set_scale(Vector3(1.0f, 1.05f, 0.95f));
	meshutil::add_sphere(upper, toon, 0.022f, Vector3(-0.065f, 0.80f, 0.145f), Color(0.12f, 0.10f, 0.09f));
	meshutil::add_sphere(upper, toon, 0.022f, Vector3(0.065f, 0.80f, 0.145f), Color(0.12f, 0.10f, 0.09f));
	meshutil::add_box(upper, toon, Vector3(0.11f, 0.016f, 0.02f), Vector3(0.0f, 0.71f, 0.150f), Color(0.30f, 0.20f, 0.14f));

	// 草帽（可扶正）：帽檐 + 帽顶
	hat = memnew(Node3D);
	hat->set_name("Hat");
	hat->set_position(Vector3(0.0f, 0.90f, 0.0f));
	hat->set_rotation(Vector3(0.22f, 0.0f, 0.0f));
	upper->add_child(hat);
	Ref<CylinderMesh> brim;
	brim.instantiate();
	brim->set_top_radius(0.30f);
	brim->set_bottom_radius(0.30f);
	brim->set_height(0.025f);
	brim->set_radial_segments(14);
	brim->set_rings(1);
	brim->set_material(meshutil::toon_material(toon, straw_col, 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(brim);
	hat->add_child(bmi);
	Ref<CylinderMesh> crown;
	crown.instantiate();
	crown->set_top_radius(0.13f);
	crown->set_bottom_radius(0.19f);
	crown->set_height(0.20f);
	crown->set_radial_segments(12);
	crown->set_rings(1);
	crown->set_material(meshutil::toon_material(toon, Color(0.80f, 0.66f, 0.32f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(crown);
	cmi->set_position(Vector3(0.0f, 0.11f, 0.0f));
	hat->add_child(cmi);

	// 脚边飘落的草屑
	Ref<BoxMesh> bit;
	bit.instantiate();
	bit->set_size(Vector3(0.020f, 0.006f, 0.055f));
	Ref<StandardMaterial3D> bm;
	bm.instantiate();
	bm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bm->set_albedo(Color(0.88f, 0.74f, 0.38f));
	bm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	bit->set_material(bm);
	straw = memnew(CPUParticles3D);
	straw->set_name("StrawBits");
	straw->set_amount(14);
	straw->set_lifetime(4.0f);
	straw->set_mesh(bit);
	straw->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	straw->set_emission_box_extents(Vector3(0.45f, 0.20f, 0.30f));
	straw->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	straw->set_spread(70.0f);
	straw->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.02f);
	straw->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	straw->set_gravity(Vector3(0.12f, -0.05f, 0.05f));
	straw->set_param_min(CPUParticles3D::PARAM_ANGULAR_VELOCITY, -90.0f);
	straw->set_param_max(CPUParticles3D::PARAM_ANGULAR_VELOCITY, 90.0f);
	straw->set_position(Vector3(0.0f, 0.15f, 0.0f));
	straw->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(straw);

	// 两只乌鸦
	const Color black(0.10f, 0.10f, 0.12f);
	for (int i = 0; i < CROW_COUNT; i++) {
		Node3D *crow = memnew(Node3D);
		crow->set_name(String("Crow") + String::num_int64(i));
		crow->set_scale(Vector3(1.6f, 1.6f, 1.6f));
		crow->set_position(Vector3(-2.1f, 2.45f, 0.0f));
		add_child(crow);
		crows[i] = crow;

		meshutil::add_box(crow, toon, Vector3(0.09f, 0.07f, 0.22f), Vector3(0.0f, 0.0f, 0.0f), black);
		meshutil::add_sphere(crow, toon, 0.05f, Vector3(0.0f, 0.03f, 0.12f), black);
		meshutil::add_box(crow, toon, Vector3(0.03f, 0.02f, 0.06f), Vector3(0.0f, 0.02f, 0.18f), Color(0.72f, 0.48f, 0.16f));
		meshutil::add_box(crow, toon, Vector3(0.08f, 0.02f, 0.12f), Vector3(0.0f, 0.0f, -0.15f), black);
		for (int w = 0; w < WING_COUNT; w++) {
			const float sgn = (w == 0) ? -1.0f : 1.0f;
			MeshInstance3D *wing = meshutil::add_box(crow, toon, Vector3(0.22f, 0.015f, 0.10f),
					Vector3(sgn * 0.14f, 0.01f, 0.0f), Color(0.13f, 0.13f, 0.15f));
			wings[i][w] = wing;
		}
	}

	// "哑——"的叫声
	build_caw();
	caw_voice = memnew(AudioStreamPlayer3D);
	caw_voice->set_name("CawVoice");
	caw_voice->set_stream(snd_caw);
	caw_voice->set_volume_db(-9.0f);
	caw_voice->set_max_distance(26.0f);
	add_child(caw_voice);
}

void Scarecrow::build_caw() {
	const int sr = 22050;
	const int n = (int)(0.42f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(6120u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float ph = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float f = 760.0f - 260.0f * (tt / 0.42f);
		ph += 6.2831853f * f / (float)sr;
		const float env = std::exp(-tt * 6.5f) * Math::clamp(tt / 0.015f, 0.0f, 1.0f);
		const float sq = std::sin(ph) > 0.0f ? 0.55f : -0.55f;
		const float noise = d01(rng) - 0.5f;
		buf[i] = (sq + std::sin(ph * 2.02f) * 0.25f + noise * 0.25f) * env * 0.6f;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 30000.0f, -30000.0f, 30000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	snd_caw = wav;
}
