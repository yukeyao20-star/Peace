#include "rain_chain.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void RainChain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cup_count"), &RainChain::get_cup_count);
	ClassDB::bind_method(D_METHOD("get_bead_visible"), &RainChain::get_bead_visible);
	ClassDB::bind_method(D_METHOD("get_wet_level"), &RainChain::get_wet_level);
	ClassDB::bind_method(D_METHOD("get_frost_level"), &RainChain::get_frost_level);
	ClassDB::bind_method(D_METHOD("get_sway_amp"), &RainChain::get_sway_amp);
	ClassDB::bind_method(D_METHOD("get_active_ripples"), &RainChain::get_active_ripples);
	ClassDB::bind_method(D_METHOD("get_trickle_playing"), &RainChain::get_trickle_playing);
}

bool RainChain::get_trickle_playing() const {
	return trickle != nullptr && trickle->is_playing();
}

void RainChain::_notification(int p_what) {
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

			if (sky == nullptr && get_tree() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			}
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			wet = Math::move_toward(wet, Math::clamp(rain, 0.0f, 1.0f), delta * 0.7f);
			const float frost_target = Math::clamp((snow - 0.2f) / 0.8f, 0.0f, 1.0f) * (1.0f - wet);
			frost_lvl = Math::move_toward(frost_lvl, frost_target, delta * 0.45f);

			// 单摆式的轻摆：雨越大风越足，摆幅越大；干的链条只留一点呼吸感。
			sway_amp = 0.006f + wet * 0.055f;
			if (chain_root != nullptr) {
				chain_root->set_rotation(Vector3(
						std::sin(t * 0.9f) * sway_amp * 0.55f,
						0.0f,
						std::sin(t * 0.63f + 1.3f) * sway_amp));
			}

			// 水珠：雨强决定同时可见的数量；每颗沿杯沿螺旋下滑，到底落进石钵。
			bead_visible = (wet > 0.12f) ? Math::min(BEAD_COUNT, 2 + (int)(wet * 7.0f)) : 0;
			const float bead_speed = 0.22f + 0.18f * wet;
			if (beads_multimesh.is_valid()) {
				beads_multimesh->set_visible_instance_count(bead_visible);
			}
			for (int i = 0; i < bead_visible; i++) {
				bead_phase[i] += delta * bead_speed;
				if (bead_phase[i] >= 1.0f) {
					bead_phase[i] -= 1.0f;
					spawn_ripple();
				}
				const float p = bead_phase[i];
				const float a = p * 14.0f + (float)i * 0.9f;
				const float r = 0.052f + 0.012f * std::sin(p * 9.0f + (float)i);
				if (beads_multimesh.is_valid()) {
					beads_multimesh->set_instance_transform(i, Transform3D(Basis(),
							Vector3(std::cos(a) * r, -0.13f - 2.23f * p, std::sin(a) * r)));
				}
			}

			// 石钵涟漪：三圈轮换，从落点向外扩、渐隐。
			active_ripples = 0;
			for (int i = 0; i < RIPPLE_COUNT; i++) {
				if (ripples[i] == nullptr) {
					continue;
				}
				if (ripple_age[i] >= 1.0f) {
					ripples[i]->set_visible(false);
					continue;
				}
				ripple_age[i] += delta * 1.5f;
				const float age = Math::min(ripple_age[i], 1.0f);
				const float s = 0.35f + age * 1.25f;
				ripples[i]->set_visible(true);
				ripples[i]->set_scale(Vector3(s, 1.0f, s));
				if (ripple_mats[i].is_valid()) {
					Color c = ripple_mats[i]->get_albedo();
					c.a = (1.0f - age) * 0.55f;
					ripple_mats[i]->set_albedo(c);
				}
				if (ripple_age[i] < 1.0f) {
					active_ripples++;
				}
			}

			// 雪天：杯口浅浅一层白霜；化雪后慢慢退掉。
			const bool frost_on = frost_lvl > 0.32f;
			if (caps_multimesh.is_valid()) {
				caps_multimesh->set_visible_instance_count(frost_on ? CUP_COUNT : 0);
			}
			if (copper_mat.is_valid() && std::fabs(wet - last_wet_draw) > 0.004f) {
				last_wet_draw = wet;
				const Color dry(0.55f, 0.34f, 0.18f);
				const Color soaked(0.34f, 0.21f, 0.12f);
				copper_mat->set_albedo(dry.lerp(soaked, wet));
			}

			// 流水声：雨起渐入、雨停渐出；音量随雨强。
			if (trickle != nullptr && snd_trickle.is_valid()) {
				if (wet > 0.06f) {
					if (!trickle->is_playing()) {
						trickle->play(0.0f);
					}
					trickle->set_volume_db(Math::lerp(-34.0f, -13.0f, wet));
				} else if (trickle->is_playing()) {
					trickle->stop();
				}
			}
		} break;
		default:
			break;
	}
}

void RainChain::spawn_ripple() {
	ripple_age[ripple_next] = 0.0f;
	ripple_next = (ripple_next + 1) % RIPPLE_COUNT;
}

void RainChain::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();

	copper_mat.instantiate();
	copper_mat->set_albedo(Color(0.55f, 0.34f, 0.18f));
	copper_mat->set_metallic(0.55f);
	copper_mat->set_roughness(0.42f);

	chain_root = memnew(Node3D);
	chain_root->set_name("ChainRoot");
	add_child(chain_root);

	// 顶上的挂环
	meshutil::add_box(chain_root, toon, Vector3(0.10f, 0.025f, 0.10f), Vector3(0.0f, 0.0f, 0.0f), Color(0.30f, 0.24f, 0.17f));
	Ref<CylinderMesh> top_link;
	top_link.instantiate();
	top_link->set_top_radius(0.008f);
	top_link->set_bottom_radius(0.008f);
	top_link->set_height(0.12f);
	top_link->set_radial_segments(6);
	top_link->set_rings(1);
	top_link->set_material(copper_mat);
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(top_link);
	tmi->set_position(Vector3(0.0f, -0.06f, 0.0f));
	chain_root->add_child(tmi);

	// 优化（第 110 轮）：铜杯/杯孔/链节/霜帽/水珠各合成一个 MultiMesh，
	// 原来 48 个 MeshInstance3D 变成 5 次 draw call，也不再参与阴影 pass。
	Ref<CylinderMesh> cup_mesh;
	cup_mesh.instantiate();
	cup_mesh->set_top_radius(0.085f);
	cup_mesh->set_bottom_radius(0.045f);
	cup_mesh->set_height(0.075f);
	cup_mesh->set_radial_segments(12);
	cup_mesh->set_rings(1);
	cup_mesh->set_material(copper_mat);

	Ref<CylinderMesh> hole_mesh;
	hole_mesh.instantiate();
	hole_mesh->set_top_radius(0.030f);
	hole_mesh->set_bottom_radius(0.030f);
	hole_mesh->set_height(0.006f);
	hole_mesh->set_radial_segments(8);
	hole_mesh->set_rings(1);
	Ref<StandardMaterial3D> hole_mat;
	hole_mat.instantiate();
	hole_mat->set_albedo(Color(0.12f, 0.10f, 0.08f));
	hole_mat->set_roughness(0.7f);
	hole_mesh->set_material(hole_mat);

	frost_mat.instantiate();
	frost_mat->set_albedo(Color(0.90f, 0.95f, 1.0f));
	frost_mat->set_roughness(0.55f);
	Ref<CylinderMesh> cap_mesh;
	cap_mesh.instantiate();
	cap_mesh->set_top_radius(0.090f);
	cap_mesh->set_bottom_radius(0.086f);
	cap_mesh->set_height(0.018f);
	cap_mesh->set_radial_segments(12);
	cap_mesh->set_rings(1);
	cap_mesh->set_material(frost_mat);

	Ref<CylinderMesh> link_mesh;
	link_mesh.instantiate();
	link_mesh->set_top_radius(0.007f);
	link_mesh->set_bottom_radius(0.007f);
	link_mesh->set_height(0.16f);
	link_mesh->set_radial_segments(6);
	link_mesh->set_rings(1);
	link_mesh->set_material(copper_mat);

	std::vector<Transform3D> cup_xf(CUP_COUNT);
	std::vector<Transform3D> hole_xf(CUP_COUNT);
	std::vector<Transform3D> cap_xf(CUP_COUNT);
	std::vector<Transform3D> link_xf(CUP_COUNT);
	for (int i = 0; i < CUP_COUNT; i++) {
		const float y = -0.20f - 0.24f * (float)i;
		cup_xf[i] = Transform3D(Basis(Vector3(0.0f, 1.0f, 0.0f), (float)i * 0.47f), Vector3(0.0f, y, 0.0f));
		hole_xf[i] = Transform3D(Basis(), Vector3(0.0f, y + 0.030f, 0.0f));
		cap_xf[i] = Transform3D(Basis(), Vector3(0.0f, y + 0.040f, 0.0f));
		link_xf[i] = Transform3D(Basis(), Vector3(0.0f, y - 0.12f, 0.0f));
	}

	auto make_group = [&](const String &name, const Ref<Mesh> &mesh, const std::vector<Transform3D> &xfs,
							 Ref<MultiMesh> &out) -> MultiMeshInstance3D * {
		out.instantiate();
		out->set_transform_format(MultiMesh::TRANSFORM_3D);
		out->set_mesh(mesh);
		out->set_instance_count((int)xfs.size());
		for (size_t k = 0; k < xfs.size(); k++) {
			out->set_instance_transform((int)k, xfs[k]);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_name(name);
		mmi->set_multimesh(out);
		mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		chain_root->add_child(mmi);
		return mmi;
	};

	cups_mm = make_group("Cups", cup_mesh, cup_xf, cups_multimesh);
	holes_mm = make_group("CupHoles", hole_mesh, hole_xf, holes_multimesh);
	caps_mm = make_group("FrostCaps", cap_mesh, cap_xf, caps_multimesh);
	links_mm = make_group("Links", link_mesh, link_xf, links_multimesh);
	caps_multimesh->set_visible_instance_count(0);

	// 水珠：小小的、微微发亮，沿杯沿下滑
	Ref<SphereMesh> drop;
	drop.instantiate();
	drop->set_radius(0.014f);
	drop->set_height(0.028f);
	drop->set_radial_segments(6);
	drop->set_rings(3);
	bead_mat.instantiate();
	bead_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bead_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	bead_mat->set_albedo(Color(0.74f, 0.88f, 0.98f, 0.88f));
	bead_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	drop->set_material(bead_mat);
	std::vector<Transform3D> bead_xf(BEAD_COUNT);
	for (int i = 0; i < BEAD_COUNT; i++) {
		bead_phase[i] = (float)i / (float)BEAD_COUNT;
		bead_xf[i] = Transform3D(Basis(), Vector3(0.0f, -0.13f - 2.23f * bead_phase[i], 0.0f));
	}
	beads_mm = make_group("Beads", drop, bead_xf, beads_multimesh);
	beads_multimesh->set_visible_instance_count(0);

	// 接水的石钵
	Ref<CylinderMesh> basin;
	basin.instantiate();
	basin->set_top_radius(0.26f);
	basin->set_bottom_radius(0.20f);
	basin->set_height(0.10f);
	basin->set_radial_segments(16);
	basin->set_rings(1);
	Ref<StandardMaterial3D> stone;
	stone.instantiate();
	stone->set_albedo(Color(0.52f, 0.50f, 0.45f));
	stone->set_roughness(0.88f);
	basin->set_material(stone);
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(basin);
	bmi->set_position(Vector3(0.0f, -2.55f, 0.0f));
	add_child(bmi);

	Ref<CylinderMesh> water;
	water.instantiate();
	water->set_top_radius(0.225f);
	water->set_bottom_radius(0.225f);
	water->set_height(0.010f);
	water->set_radial_segments(16);
	water->set_rings(1);
	basin_water_mat.instantiate();
	basin_water_mat->set_albedo(Color(0.30f, 0.44f, 0.50f, 0.62f));
	basin_water_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	basin_water_mat->set_roughness(0.16f);
	water->set_material(basin_water_mat);
	MeshInstance3D *wmi = memnew(MeshInstance3D);
	wmi->set_mesh(water);
	wmi->set_position(Vector3(0.0f, -2.495f, 0.0f));
	add_child(wmi);

	// 三圈涟漪（扁环），落水时轮换播放
	for (int i = 0; i < RIPPLE_COUNT; i++) {
		Ref<TorusMesh> ring;
		ring.instantiate();
		ring->set_inner_radius(0.155f);
		ring->set_outer_radius(0.175f);
		ring->set_rings(20);
		ring->set_ring_segments(4);
		ripple_mats[i].instantiate();
		ripple_mats[i]->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		ripple_mats[i]->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		ripple_mats[i]->set_albedo(Color(0.80f, 0.92f, 0.98f, 0.5f));
		ripple_mats[i]->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		ring->set_material(ripple_mats[i]);
		MeshInstance3D *rmi = memnew(MeshInstance3D);
		rmi->set_mesh(ring);
		rmi->set_position(Vector3(0.0f, -2.487f, 0.0f));
		rmi->set_visible(false);
		add_child(rmi);
		ripples[i] = rmi;
		ripple_age[i] = 1.0f;
	}

	snd_trickle = make_trickle();
	trickle = memnew(AudioStreamPlayer3D);
	trickle->set_name("TrickleVoice");
	trickle->set_stream(snd_trickle);
	trickle->set_volume_db(-34.0f);
	trickle->set_unit_size(6.0f);
	trickle->set_max_distance(22.0f);
	add_child(trickle);
}

// 4 秒程序化流水 loop：低通噪声 + 慢速起伏 + 零星水滴 plink。
Ref<AudioStreamWAV> RainChain::make_trickle() {
	const int sr = 22050;
	const int n = sr * 4;
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(7106u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);

	// 预先撒几颗水滴：指数衰减的短正弦，音高随机。
	struct Plink {
		int start;
		float freq;
		float amp;
	};
	std::vector<Plink> plinks;
	for (int i = 0; i < 14; i++) {
		Plink pk;
		pk.start = (int)(d01(rng) * (float)(n - 2000));
		pk.freq = 1600.0f + d01(rng) * 2100.0f;
		pk.amp = 0.10f + d01(rng) * 0.14f;
		plinks.push_back(pk);
	}

	float lp = 0.0f;
	float lp2 = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.13f;
		lp2 += (lp - lp2) * 0.05f;
		// 水流忽大忽小：两个不同频率的慢 LFO
		const float gurgle = 0.55f + 0.28f * std::sin(6.2831853f * 0.47f * tt + 0.8f)
				+ 0.17f * std::sin(6.2831853f * 1.13f * tt + 2.1f);
		float s = (lp * 1.6f + lp2 * 2.2f) * Math::clamp(gurgle, 0.15f, 1.0f);
		for (const Plink &pk : plinks) {
			const int off = i - pk.start;
			if (off >= 0 && off < 2000) {
				const float dt = (float)off / (float)sr;
				s += std::sin(6.2831853f * pk.freq * dt) * std::exp(-dt * 22.0f) * pk.amp;
			}
		}
		buf[i] = s;
	}

	float peak = 0.0001f;
	for (float v : buf) {
		peak = Math::max(peak, std::abs(v));
	}
	const float gain = 0.62f / peak;
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float v = buf[i] * gain;
		const int tail = n - i;
		if (tail < 1600) {
			v *= (float)tail / 1600.0f; // 循环接缝淡出
		}
		const int16_t q = (int16_t)Math::clamp(v * 32000.0f, -32000.0f, 32000.0f);
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
