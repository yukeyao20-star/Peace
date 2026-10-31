#include "ice_hole.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void IceHole::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_ice_visible"), &IceHole::get_ice_visible);
	ClassDB::bind_method(D_METHOD("get_mist_emitting"), &IceHole::get_mist_emitting);
	ClassDB::bind_method(D_METHOD("get_fish_count"), &IceHole::get_fish_count);
	ClassDB::bind_method(D_METHOD("get_fishing"), &IceHole::get_fishing);
	ClassDB::bind_method(D_METHOD("get_rumble_count"), &IceHole::get_rumble_count);
	ClassDB::bind_method(D_METHOD("get_crack_alpha"), &IceHole::get_crack_alpha);
	ClassDB::bind_method(D_METHOD("get_ice_sound_ready"), &IceHole::get_ice_sound_ready);
	ClassDB::bind_method(D_METHOD("get_ripple_scale"), &IceHole::get_ripple_scale);
}

bool IceHole::get_mist_emitting() const {
	return mist != nullptr && mist->is_emitting();
}

float IceHole::get_ripple_scale() const {
	return (ripple != nullptr) ? ripple->get_scale().x : 0.0f;
}

float IceHole::get_crack_alpha() const {
	return (crack_mat.is_valid()) ? crack_mat->get_albedo().a : 0.0f;
}

void IceHole::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			ice = Math::lerp(ice, snow > 0.84f ? 1.0f : 0.0f, 1.0f - std::exp(-0.9f * delta));
			set_visible(ice > 0.15f);
			if (ice <= 0.15f) {
				return;
			}
			if (mist != nullptr) {
				mist->set_emitting(ice > 0.5f);
			}
			fishing_mix = Math::lerp(fishing_mix, fishing ? 1.0f : 0.0f, 1.0f - std::exp(-2.4f * delta));
			// 鱼影绕洞口缓慢兜圈，偶尔换向
			for (size_t i = 0; i < fish.size(); i++) {
				const float a = t * (0.35f + 0.06f * (float)i) + fish_phase[i];
				const float r = fish_radius[i] * (1.0f - 0.55f * fishing_mix); // 有人下竿时鱼影聚到洞口
				const float y = -0.03f + 0.02f * fishing_mix - 0.01f * std::sin(t * 1.7f + fish_phase[i]);
				Node3D *f = fish[i];
				f->set_position(Vector3(std::cos(a) * r, y, std::sin(a) * r));
				const Vector3 tangent(-std::sin(a), 0.0f, std::cos(a));
				f->set_rotation(Vector3(0.0f, std::atan2(tangent.x, tangent.z), 0.0f));
			}
			// 涟漪：由内向外扩一圈再隐去
			if (ripple != nullptr && ripple_mat.is_valid()) {
				const float u = std::fmod(t, 2.8f) / 2.8f;
				const float s = 0.55f + 2.0f * u;
				ripple->set_scale(Vector3(s, 1.0f, s));
				ripple_mat->set_albedo(Color(0.88f, 0.96f, 1.0f, 0.42f * (1.0f - u) * ice));
			}
			// 冰鸣：封冻的湖面偶尔低低地响一声，冰上裂开一道细纹
			rumble_t -= delta;
			if (rumble_t <= 0.0f) {
				const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
				const float r = (float)((rumble_count * 7919 + 13) % 1000) / 1000.0f;
				rumble_t = (night > 0.5f) ? (8.0f + r * 8.0f) : (16.0f + r * 14.0f);
				rumble_count++;
				crack_t = 2.4f;
				if (crack_a != nullptr) {
					const float a = (float)((rumble_count * 47) % 360) * 0.01745f;
					crack_a->set_position(Vector3(std::cos(a) * 0.55f, 0.012f, std::sin(a) * 0.55f));
					crack_a->set_rotation(Vector3(0.0f, a + 0.4f, 0.0f));
				}
				if (crack_b != nullptr) {
					const float a = (float)((rumble_count * 83 + 120) % 360) * 0.01745f;
					crack_b->set_position(Vector3(std::cos(a) * 0.42f, 0.012f, std::sin(a) * 0.42f));
					crack_b->set_rotation(Vector3(0.0f, a - 0.7f, 0.0f));
				}
				if (ice_voice != nullptr && snd_ice.is_valid()) {
					ice_voice->set_pitch_scale(0.9f + 0.06f * (float)(rumble_count % 4));
					ice_voice->play(0.0f);
				}
			}
			if (crack_t > 0.0f) {
				crack_t = Math::max(0.0f, crack_t - delta);
				const float u = crack_t / 2.4f;
				const float alpha = std::sin(u * 3.14159f) * 0.7f * ice;
				if (crack_mat.is_valid()) {
					crack_mat->set_albedo(Color(0.92f, 0.97f, 1.0f, alpha));
				}
				if (crack_a != nullptr) {
					crack_a->set_visible(alpha > 0.02f);
				}
				if (crack_b != nullptr) {
					crack_b->set_visible(alpha > 0.02f);
				}
			} else {
				if (crack_a != nullptr) crack_a->set_visible(false);
				if (crack_b != nullptr) crack_b->set_visible(false);
			}
		} break;
		default:
			break;
	}
}

void IceHole::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();

	// —— 洞口：深色水面 ——
	Ref<CylinderMesh> water;
	water.instantiate();
	water->set_top_radius(0.52f);
	water->set_bottom_radius(0.52f);
	water->set_height(0.02f);
	water->set_radial_segments(28);
	water->set_rings(1);
	Ref<StandardMaterial3D> water_mat;
	water_mat.instantiate();
	water_mat->set_albedo(Color(0.03f, 0.09f, 0.12f, 1.0f));
	water_mat->set_roughness(0.12f);
	water_mat->set_metallic(0.25f);
	water->set_material(Ref<Material>(water_mat));
	MeshInstance3D *wmi = memnew(MeshInstance3D);
	wmi->set_name("DarkWater");
	wmi->set_mesh(water);
	wmi->set_position(Vector3(0.0f, -0.08f, 0.0f));
	add_child(wmi);

	// —— 冰缘：一圈乳白厚冰 ——
	Ref<TorusMesh> rim;
	rim.instantiate();
	rim->set_inner_radius(0.50f);
	rim->set_outer_radius(0.64f);
	rim->set_rings(30);
	rim->set_ring_segments(6);
	rim->set_material(meshutil::toon_material(toon, Color(0.80f, 0.90f, 1.0f), 0.55f));
	MeshInstance3D *rmi = memnew(MeshInstance3D);
	rmi->set_name("IceRim");
	rmi->set_mesh(rim);
	rmi->set_scale(Vector3(1.0f, 0.45f, 1.0f));
	rmi->set_position(Vector3(0.0f, 0.012f, 0.0f));
	add_child(rmi);

	// —— 碎冰：沿洞口外缘斜插几片 ——
	for (int i = 0; i < 9; i++) {
		const float a = (float)i * 0.698f + 0.3f;
		const float r = 0.62f + 0.03f * std::sin((float)i * 2.1f);
		MeshInstance3D *shard = meshutil::add_box(this, toon, Vector3(0.07f, 0.13f, 0.045f),
				Vector3(std::cos(a) * r, 0.03f, std::sin(a) * r),
				Color(0.84f, 0.93f, 1.0f),
				Vector3(0.25f + 0.15f * std::sin((float)i), -a, 0.2f * std::cos((float)i * 1.7f)));
		shard->set_name(String("IceShard") + String::num_int64(i));
	}

	// —— 洞口下的鱼影 ——
	Ref<SphereMesh> fish_mesh;
	fish_mesh.instantiate();
	fish_mesh->set_radius(0.085f);
	fish_mesh->set_height(0.17f);
	fish_mesh->set_radial_segments(10);
	fish_mesh->set_rings(5);
	Ref<StandardMaterial3D> fish_mat;
	fish_mat.instantiate();
	fish_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fish_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	fish_mat->set_albedo(Color(0.10f, 0.16f, 0.19f, 0.85f));
	fish_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	fish_mesh->set_material(fish_mat);
	for (int i = 0; i < 3; i++) {
		Node3D *root = memnew(Node3D);
		root->set_name(String("FishShadow") + String::num_int64(i));
		MeshInstance3D *body = memnew(MeshInstance3D);
		body->set_mesh(fish_mesh);
		body->set_scale(Vector3(1.6f, 0.5f, 0.6f));
		root->add_child(body);
		// 小尾鳍，游动时更容易看出方向
		MeshInstance3D *tail = meshutil::add_box(root, toon, Vector3(0.07f, 0.035f, 0.02f), Vector3(0.0f, 0.0f, -0.11f), Color(0.10f, 0.16f, 0.19f));
		tail->set_name("Tail");
		add_child(root);
		fish.push_back(root);
		fish_phase.push_back((float)i * 2.4f);
		fish_radius.push_back(0.22f + 0.07f * (float)i);
	}

	// —— 寒气 ——
	Ref<SphereMesh> mist_mesh;
	mist_mesh.instantiate();
	mist_mesh->set_radius(0.07f);
	mist_mesh->set_height(0.14f);
	mist_mesh->set_radial_segments(6);
	mist_mesh->set_rings(3);
	Ref<StandardMaterial3D> mist_mat;
	mist_mat.instantiate();
	mist_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mist_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mist_mat->set_albedo(Color(0.88f, 0.94f, 1.0f, 0.13f));
	mist_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	mist_mesh->set_material(mist_mat);
	mist = memnew(CPUParticles3D);
	mist->set_name("HoleMist");
	mist->set_amount(12);
	mist->set_lifetime(2.4f);
	mist->set_mesh(mist_mesh);
	mist->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	mist->set_spread(26.0f);
	mist->set_gravity(Vector3(0.01f, 0.05f, 0.0f));
	mist->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	mist->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.22f);
	mist->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	mist->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
	mist->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	mist->set_emission_sphere_radius(0.28f);
	mist->set_position(Vector3(0.0f, 0.06f, 0.0f));
	mist->set_emitting(false);
	add_child(mist);

	// —— 涟漪圈 ——
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.24f);
	ring->set_outer_radius(0.28f);
	ring->set_rings(28);
	ring->set_ring_segments(5);
	ripple_mat.instantiate();
	ripple_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	ripple_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	ripple_mat->set_albedo(Color(0.88f, 0.96f, 1.0f, 0.0f));
	ripple_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ring->set_material(Ref<Material>(ripple_mat));
	ripple = memnew(MeshInstance3D);
	ripple->set_name("Ripple");
	ripple->set_mesh(ring);
	ripple->set_scale(Vector3(0.55f, 1.0f, 0.55f));
	ripple->set_position(Vector3(0.0f, 0.016f, 0.0f));
	add_child(ripple);

	// —— 冰鸣裂纹：两道细白线，响的时候才出现 ——
	crack_mat.instantiate();
	crack_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	crack_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	crack_mat->set_albedo(Color(0.92f, 0.97f, 1.0f, 0.0f));
	crack_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	Ref<BoxMesh> crack_mesh;
	crack_mesh.instantiate();
	crack_mesh->set_size(Vector3(1.9f, 0.006f, 0.035f));
	crack_mesh->set_material(Ref<Material>(crack_mat));
	crack_a = memnew(MeshInstance3D);
	crack_a->set_name("IceCrackA");
	crack_a->set_mesh(crack_mesh);
	crack_a->set_visible(false);
	add_child(crack_a);
	Ref<BoxMesh> crack_mesh_b;
	crack_mesh_b.instantiate();
	crack_mesh_b->set_size(Vector3(1.3f, 0.006f, 0.025f));
	crack_mesh_b->set_material(Ref<Material>(crack_mat));
	crack_b = memnew(MeshInstance3D);
	crack_b->set_name("IceCrackB");
	crack_b->set_mesh(crack_mesh_b);
	crack_b->set_visible(false);
	add_child(crack_b);

	// 冰鸣声：低频的冰层呻吟
	snd_ice = make_ice_groan();
	ice_voice = memnew(AudioStreamPlayer3D);
	ice_voice->set_name("IceVoice");
	ice_voice->set_stream(snd_ice);
	ice_voice->set_volume_db(-12.0f);
	ice_voice->set_unit_size(12.0f);
	ice_voice->set_max_distance(60.0f);
	ice_voice->set_position(Vector3(0.0f, 0.1f, 0.0f));
	add_child(ice_voice);
}

// 2.6 秒的冰层呻吟：低沉的正弦滑音 + 一段被包络塑形的噪声，
// 像远处厚冰慢慢裂开又合拢。
Ref<AudioStreamWAV> IceHole::make_ice_groan() {
	const int sr = 22050;
	const int n = (int)(2.6f * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(86420u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float phase = 0.0f;
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tsec = (float)i / (float)sr;
		const float u = tsec / 2.6f;
		const float env = std::sin(u * 3.14159f);
		const float freq = 72.0f - 22.0f * u + 6.0f * std::sin(tsec * 9.0f);
		phase += 6.2831853f * freq / (float)sr;
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.06f;
		float samp = (std::sin(phase) * 0.7f + std::sin(phase * 2.01f) * 0.2f + lp * 1.6f) * env;
		samp = Math::clamp(samp, -1.0f, 1.0f);
		const int16_t q = (int16_t)(samp * 8000.0f);
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
