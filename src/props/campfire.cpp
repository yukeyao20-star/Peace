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
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Campfire::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Campfire::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Campfire::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Campfire::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Campfire::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Campfire::interact);
	ClassDB::bind_method(D_METHOD("get_roast_state"), &Campfire::get_roast_state);
	ClassDB::bind_method(D_METHOD("get_roast_time"), &Campfire::get_roast_time);
	ClassDB::bind_method(D_METHOD("get_roast_stage"), &Campfire::get_roast_stage);
	ClassDB::bind_method(D_METHOD("get_golden_count"), &Campfire::get_golden_count);
	ClassDB::bind_method(D_METHOD("get_burnt_count"), &Campfire::get_burnt_count);
	ClassDB::bind_method(D_METHOD("get_roast_visible"), &Campfire::get_roast_visible);
}

int Campfire::get_roast_stage() const {
	if (roast_t < 3.0f) {
		return 0; // 还生
	}
	return roast_t < 6.5f ? 1 : 2; // 金黄 / 焦
}

bool Campfire::get_roast_visible() const {
	return roast_root != nullptr && roast_root->is_visible();
}

String Campfire::get_prompt() const {
	if (roast_state == 1) {
		const int stage = get_roast_stage();
		if (stage == 1) {
			return String::utf8("按 E 吃掉棉花糖");
		}
		return stage == 0 ? String::utf8("按 E 取下棉花糖") : String::utf8("按 E 取下烤糊的棉花糖");
	}
	return String::utf8("按 E 烤棉花糖");
}

Vector3 Campfire::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.35f, 0.0f);
}

float Campfire::get_interact_distance() const {
	return 2.8f;
}

void Campfire::interact(const Vector3 &p_player_pos) {
	if (roast_state == 1) {
		stop_roast();
		return;
	}
	start_roast(p_player_pos);
	boost = 0.9f;
	if (embers != nullptr) {
		embers->restart();
	}
	if (crackle != nullptr) {
		crackle->set_volume_db(-8.0f);
	}
}

void Campfire::start_roast(const Vector3 &p_player_pos) {
	if (roast_root == nullptr) {
		return;
	}
	roast_state = 1;
	roast_t = 0.0f;
	roast_root->set_visible(true);
	if (roast_smoke != nullptr) {
		roast_smoke->set_emitting(false);
	}
	if (marshmallow_mat.is_valid()) {
		marshmallow_mat->set_albedo(Color(0.97f, 0.94f, 0.86f));
	}
	(void)p_player_pos;
	if (get_parent() != nullptr) {
		player_node = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("串上棉花糖 · 在火苗上慢慢转，金黄时按 E 吃掉"), 3.6f);
		}
	}
}

void Campfire::stop_roast() {
	if (roast_state == 0) {
		return;
	}
	const int stage = get_roast_stage();
	const float was = roast_t;
	roast_state = 0;
	roast_t = 0.0f;
	if (roast_root != nullptr) {
		roast_root->set_visible(false);
	}
	if (roast_smoke != nullptr) {
		roast_smoke->set_emitting(false);
	}
	if (eat_voice != nullptr && snd_crunch.is_valid()) {
		eat_voice->set_pitch_scale(stage == 2 ? 0.68f : 1.0f);
		eat_voice->play(0.0f);
	}
	String msg;
	if (stage == 1) {
		golden_count++;
		msg = String::utf8("外焦里软，甜！");
	} else if (stage == 0) {
		msg = String::utf8("还有点生，奶香奶香的");
	} else {
		burnt_count++;
		msg = String::utf8("烤糊了，苦……");
	}
	(void)was;
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("flash_hint", msg, 2.8f);
			if (stage == 1) {
				gd->call("on_marshmallow_roasted");
			}
		}
	}
}

// 每帧把棉花糖摆在火苗上方，木棍朝玩家胸口延伸；颜色按时间从奶白→金黄→焦黑。
void Campfire::update_roast(float p_delta) {
	if (roast_state != 1 || roast_root == nullptr) {
		return;
	}
	roast_t += p_delta;
	const Vector3 fire = get_global_position() + Vector3(0.0f, 1.12f, 0.0f);
	Vector3 chest = fire + Vector3(0.0f, 0.4f, 1.0f);
	if (player_node == nullptr && get_parent() != nullptr) {
		player_node = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
	}
	Vector3 to_player = Vector3(0.0f, 0.0f, 1.0f);
	if (player_node != nullptr) {
		chest = player_node->get_global_position() + Vector3(0.0f, 1.15f, 0.0f);
		to_player = chest - fire;
		if (to_player.length() > 5.0f) {
			// 走远就把棉花糖留在火里，省得举着跟人跑
			roast_state = 0;
			roast_t = 0.0f;
			roast_root->set_visible(false);
			if (roast_smoke != nullptr) {
				roast_smoke->set_emitting(false);
			}
			if (get_parent() != nullptr) {
				Node *gd = get_parent()->get_node_or_null("GameDirector");
				if (gd != nullptr) {
					gd->call("flash_hint", String::utf8("你走远了，棉花糖掉进火里"), 2.4f);
				}
			}
			return;
		}
	}
	if (to_player.length_squared() < 0.04f) {
		to_player = Vector3(0.0f, 0.2f, 1.0f);
	}
	roast_root->set_global_position(fire);
	const Vector3 sdir = to_player.normalized();
	const float slen = Math::clamp(to_player.length() - 0.18f, 0.45f, 1.6f);
	if (stick != nullptr) {
		Transform3D st;
		st.set_basis(Basis(Quaternion(Vector3(0.0f, 1.0f, 0.0f), sdir)).scaled(Vector3(1.0f, slen, 1.0f)));
		st.set_origin(sdir * slen * 0.5f);
		stick->set_transform(st);
	}
	if (marshmallow != nullptr) {
		marshmallow->set_position(Vector3(0.0f, 0.015f * std::sin(t * 5.3f), 0.0f));
		const float puff = 1.0f + 0.10f * Math::clamp(roast_t / 3.0f, 0.0f, 1.0f);
		marshmallow->set_scale(Vector3(puff, puff * 0.9f, puff));
	}
	if (marshmallow_mat.is_valid()) {
		const Color raw(0.97f, 0.94f, 0.86f);
		const Color golden(0.86f, 0.58f, 0.28f);
		const Color burnt(0.17f, 0.12f, 0.09f);
		if (roast_t < 3.0f) {
			marshmallow_mat->set_albedo(raw.lerp(golden, roast_t / 3.0f));
		} else if (roast_t < 6.5f) {
			marshmallow_mat->set_albedo(golden);
		} else {
			marshmallow_mat->set_albedo(golden.lerp(burnt, Math::clamp((roast_t - 6.5f) / 2.0f, 0.0f, 1.0f)));
		}
	}
	if (roast_smoke != nullptr) {
		roast_smoke->set_emitting(roast_t > 5.6f);
	}
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
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			add_to_group("interactables");
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			const float dt = (float)get_process_delta_time();
			update_roast(dt);
			if (boost > 0.0f) {
				boost = Math::max(0.0f, boost - dt);
			}
			float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			wet = Math::lerp(wet, Math::clamp(rain, 0.0f, 1.0f), 1.0f - std::exp(-2.2f * dt));
			float rain_k = wet * 0.55f; // 雨越大，火越小
			// 添柴后 0.9s 内火势冲高再回落（前段猛、后段缓）
			float b = boost / 0.9f;
			float surge = 1.0f + 1.15f * b * b;
			if (light != nullptr) {
				float flicker = 0.92f + 0.06f * std::sin(t * 7.3f) + 0.02f * std::sin(t * 19.1f + 1.7f);
				light->set_param(Light3D::PARAM_ENERGY, 1.55f * flicker * surge * (1.0f - 0.42f * rain_k));
			}
			if (flame != nullptr) {
				float s = 1.0f + 0.06f * std::sin(t * 9.1f) + 0.03f * std::sin(t * 15.7f + 0.8f);
				float shrink = 1.0f - 0.45f * rain_k;
				flame->set_scale(Vector3(s * (1.0f + 0.55f * b) * shrink, (1.0f + 0.10f * std::sin(t * 6.4f)) * (1.0f + 0.8f * b) * shrink, s * (1.0f + 0.55f * b) * shrink));
			}
			if (steam != nullptr) {
				steam->set_emitting(wet > 0.28f);
			}
			if (crackle != nullptr) {
				float target_db = (boost > 0.0f) ? -8.0f - 7.0f * rain_k : -20.0f + 7.0f * rain_k;
				crackle->set_volume_db(Math::lerp((float)crackle->get_volume_db(), target_db, 1.0f - std::exp(-6.0f * dt)));
			}
			if (hiss != nullptr) {
				float target_db = (wet > 0.12f) ? (-17.0f + 8.0f * wet) : -60.0f;
				hiss->set_volume_db(Math::lerp((float)hiss->get_volume_db(), target_db, 1.0f - std::exp(-4.0f * dt)));
				if (wet > 0.12f && !hiss->is_playing()) {
					hiss->play(0.0f);
				}
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

	// 炭床：木柴底部一层暗红发光
	Ref<SphereMesh> bed_mesh;
	bed_mesh.instantiate();
	bed_mesh->set_radius(0.24f);
	bed_mesh->set_height(0.12f);
	bed_mesh->set_radial_segments(10);
	bed_mesh->set_rings(5);
	Ref<StandardMaterial3D> bed_mat;
	bed_mat.instantiate();
	bed_mat->set_albedo(Color(0.22f, 0.09f, 0.04f));
	bed_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	bed_mat->set_emission(Color(1.0f, 0.32f, 0.08f));
	bed_mat->set_emission_energy_multiplier(0.9f);
	bed_mesh->set_material(bed_mat);
	MeshInstance3D *bed = memnew(MeshInstance3D);
	bed->set_name("EmberBed");
	bed->set_mesh(bed_mesh);
	bed->set_position(Vector3(0.0f, 0.10f, 0.0f));
	bed->set_scale(Vector3(1.0f, 0.55f, 1.0f));
	add_child(bed);

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

	// 青烟：一小缕慢慢上升、渐隐
	Ref<SphereMesh> smoke_mesh;
	smoke_mesh.instantiate();
	smoke_mesh->set_radius(0.09f);
	smoke_mesh->set_height(0.18f);
	smoke_mesh->set_radial_segments(6);
	smoke_mesh->set_rings(3);
	Ref<StandardMaterial3D> smoke_mat;
	smoke_mat.instantiate();
	smoke_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	smoke_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	smoke_mat->set_albedo(Color(0.55f, 0.58f, 0.58f, 0.08f));
	smoke_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	smoke_mesh->set_material(smoke_mat);
	CPUParticles3D *smoke = memnew(CPUParticles3D);
	smoke->set_name("Smoke");
	smoke->set_amount(10);
	smoke->set_lifetime(2.8f);
	smoke->set_mesh(smoke_mesh);
	smoke->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	smoke->set_spread(12.0f);
	smoke->set_gravity(Vector3(0.05f, 0.12f, 0.02f));
	smoke->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.22f);
	smoke->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.40f);
	smoke->set_param_min(CPUParticles3D::PARAM_SCALE, 0.8f);
	smoke->set_param_max(CPUParticles3D::PARAM_SCALE, 1.25f);
	smoke->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	smoke->set_emission_sphere_radius(0.10f);
	smoke->set_position(Vector3(0.0f, 0.45f, 0.0f));
	smoke->set_emitting(true);
	add_child(smoke);

	// 火光
	light = memnew(OmniLight3D);
	light->set_name("FireLight");
	light->set_position(Vector3(0.0f, 0.55f, 0.0f));
	light->set_color(Color(1.0f, 0.58f, 0.26f));
	light->set_param(Light3D::PARAM_ENERGY, 1.55f);
	light->set_param(Light3D::PARAM_RANGE, 9.0f);
	light->set_shadow(false);
	add_child(light);

	// 雨中白汽：一小股被雨点蒸起来的水汽
	{
		Ref<SphereMesh> steam_mesh;
		steam_mesh.instantiate();
		steam_mesh->set_radius(0.11f);
		steam_mesh->set_height(0.22f);
		steam_mesh->set_radial_segments(7);
		steam_mesh->set_rings(4);
		Ref<StandardMaterial3D> steam_mat;
		steam_mat.instantiate();
		steam_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		steam_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		steam_mat->set_albedo(Color(0.86f, 0.90f, 0.90f, 0.10f));
		steam_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		steam_mesh->set_material(steam_mat);
		steam = memnew(CPUParticles3D);
		steam->set_name("Steam");
		steam->set_amount(16);
		steam->set_lifetime(1.5f);
		steam->set_mesh(steam_mesh);
		steam->set_direction(Vector3(0.0f, 1.0f, 0.0f));
		steam->set_spread(22.0f);
		steam->set_gravity(Vector3(0.06f, 0.32f, 0.03f));
		steam->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.25f);
		steam->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.55f);
		steam->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
		steam->set_param_max(CPUParticles3D::PARAM_SCALE, 1.8f);
		steam->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
		steam->set_emission_sphere_radius(0.22f);
		steam->set_position(Vector3(0.0f, 0.30f, 0.0f));
		steam->set_emitting(false);
		add_child(steam);
	}

	// 噼啪声
	snd_crackle = make_crackle();
	crackle = memnew(AudioStreamPlayer3D);
	crackle->set_name("Crackle");
	crackle->set_stream(snd_crackle);
	crackle->set_volume_db(-20.0f);
	crackle->set_max_distance(24.0f);
	add_child(crackle);
	crackle->play();

	// 雨落火堆：嗞嗞的白噪声循环
	snd_hiss = make_hiss();
	hiss = memnew(AudioStreamPlayer3D);
	hiss->set_name("Hiss");
	hiss->set_stream(snd_hiss);
	hiss->set_volume_db(-60.0f);
	hiss->set_max_distance(14.0f);
	add_child(hiss);

	build_roast();
}

void Campfire::build_roast() {
	roast_root = memnew(Node3D);
	roast_root->set_name("MarshmallowRoast");
	roast_root->set_visible(false);
	add_child(roast_root);

	Ref<CylinderMesh> stick_mesh;
	stick_mesh.instantiate();
	stick_mesh->set_top_radius(0.012f);
	stick_mesh->set_bottom_radius(0.010f);
	stick_mesh->set_height(1.0f);
	stick_mesh->set_radial_segments(6);
	Ref<StandardMaterial3D> stick_mat;
	stick_mat.instantiate();
	stick_mat->set_albedo(Color(0.42f, 0.28f, 0.15f));
	stick_mat->set_roughness(0.9f);
	stick_mesh->set_material(stick_mat);
	stick = memnew(MeshInstance3D);
	stick->set_name("Stick");
	stick->set_mesh(stick_mesh);
	roast_root->add_child(stick);

	marshmallow_mat.instantiate();
	marshmallow_mat->set_albedo(Color(0.97f, 0.94f, 0.86f));
	marshmallow_mat->set_roughness(0.95f);
	Ref<SphereMesh> mm;
	mm.instantiate();
	mm->set_radius(0.085f);
	mm->set_height(0.17f);
	mm->set_radial_segments(14);
	mm->set_rings(8);
	mm->set_material(Ref<Material>(marshmallow_mat));
	marshmallow = memnew(MeshInstance3D);
	marshmallow->set_name("Marshmallow");
	marshmallow->set_mesh(mm);
	marshmallow->set_scale(Vector3(1.0f, 0.9f, 1.0f));
	roast_root->add_child(marshmallow);

	Ref<SphereMesh> smoke_mesh;
	smoke_mesh.instantiate();
	smoke_mesh->set_radius(0.05f);
	smoke_mesh->set_height(0.10f);
	smoke_mesh->set_radial_segments(6);
	smoke_mesh->set_rings(4);
	Ref<StandardMaterial3D> smoke_mat;
	smoke_mat.instantiate();
	smoke_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	smoke_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	smoke_mat->set_albedo(Color(0.24f, 0.22f, 0.20f, 0.34f));
	smoke_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	smoke_mesh->set_material(smoke_mat);
	roast_smoke = memnew(CPUParticles3D);
	roast_smoke->set_name("RoastSmoke");
	roast_smoke->set_amount(12);
	roast_smoke->set_lifetime(1.3f);
	roast_smoke->set_mesh(smoke_mesh);
	roast_smoke->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	roast_smoke->set_spread(30.0f);
	roast_smoke->set_gravity(Vector3(0.05f, 0.10f, 0.02f));
	roast_smoke->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	roast_smoke->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.24f);
	roast_smoke->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	roast_smoke->set_param_max(CPUParticles3D::PARAM_SCALE, 1.2f);
	roast_smoke->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	roast_smoke->set_emission_sphere_radius(0.05f);
	roast_smoke->set_position(Vector3(0.0f, 0.10f, 0.0f));
	roast_smoke->set_emitting(false);
	roast_root->add_child(roast_smoke);

	snd_crunch = make_crunch();
	eat_voice = memnew(AudioStreamPlayer3D);
	eat_voice->set_name("EatVoice");
	eat_voice->set_stream(snd_crunch);
	eat_voice->set_volume_db(-6.0f);
	eat_voice->set_max_distance(12.0f);
	roast_root->add_child(eat_voice);
}

// 0.22 秒的"咔嚓"：脆壳碎裂的白噪 + 低频闷响，音量快速衰减。
Ref<AudioStreamWAV> Campfire::make_crunch() {
	const int sr = 22050;
	const int n = (int)(0.22f * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(91732u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tsec = (float)i / (float)sr;
		const float env = std::exp(-tsec * 16.0f);
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.18f;
		float samp = (white * 0.55f + lp * 1.4f + std::sin(tsec * 6.2831853f * 140.0f) * 0.15f) * env;
		samp = Math::clamp(samp, -1.0f, 1.0f);
		const int16_t q = (int16_t)(samp * 12000.0f);
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

// 2 秒可循环的嗞声：高通白噪声 + 稀疏爆点，像雨滴砸在炭上
Ref<AudioStreamWAV> Campfire::make_hiss() {
	const int sr = 22050;
	const float dur = 2.0f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(373737u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	std::vector<float> ring(n, 0.0f);
	for (int i = 0; i < 12; i++) {
		int at = (int)(d01(rng) * (float)n);
		for (int j = 0; j < 120; j++) {
			ring[(at + j) % n] += (d01(rng) - 0.5f) * std::exp(-j / 30.0f) * 0.5f;
		}
	}
	for (int i = 0; i < n; i++) {
		float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.25f;
		float hp = white - lp;
		float samp = hp * 0.22f + ring[i] + lp * 0.12f;
		// 首尾交叉淡化，避免循环爆点
		int tail = n - i;
		if (tail < 2205) {
			float k = (float)tail / 2205.0f;
			samp *= k;
		}
		if (i < 2205) {
			float k = (float)i / 2205.0f;
			samp *= k;
		}
		int16_t q = (int16_t)Math::clamp(samp * 9000.0f, -32000.0f, 32000.0f);
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
