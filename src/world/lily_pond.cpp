#include "lily_pond.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../systems/mesh_util.h"
#include "sky_cycle.h"
#include "terrain_builder.h"

using namespace godot;

void LilyPond::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pad_count"), &LilyPond::get_pad_count);
	ClassDB::bind_method(D_METHOD("get_koi_count"), &LilyPond::get_koi_count);
	ClassDB::bind_method(D_METHOD("get_active_jumps"), &LilyPond::get_active_jumps);
}

void LilyPond::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
			}
			if (terrain != nullptr) {
				water_level = terrain->get_water_level();
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			build_pads();
			build_flowers();
			build_koi();
			build_rings();
			build_leaves();
			make_plop();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float dt = (float)get_process_delta_time();
			t += dt;

			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			wet = Math::lerp(wet, Math::clamp(rain, 0.0f, 1.0f), 1.0f - std::exp(-2.0f * dt));

			// 锦鲤绕着自己的水草窝慢慢打转，鱼头朝行进方向；偶尔整条跃出水面
			for (Koi &k : koi) {
				if (k.root == nullptr) {
					continue;
				}
				if (k.jumping) {
					k.jump_t += dt;
					float u = Math::clamp(k.jump_t / k.jump_dur, 0.0f, 1.0f);
					Vector3 p = k.jump_from.lerp(k.jump_to, u);
					p.y = water_level + 0.02f + std::sin(u * 3.1415926f) * 0.52f;
					k.root->set_position(p);
					k.root->set_rotation(Vector3(-std::sin(u * 3.1415926f) * 0.85f, k.root->get_rotation().y, 0.0f));
					if (u >= 1.0f) {
						k.jumping = false;
						spawn_ring(k.jump_to + Vector3(0.0f, water_level + 0.015f - k.jump_to.y, 0.0f));
						if (plop != nullptr) {
							plop->set_position(Vector3(k.jump_to.x, water_level + 0.05f, k.jump_to.z));
							plop->set_pitch_scale(1.12f);
							plop->set_volume_db(-15.0f);
							plop->play(0.0f);
						}
					}
					continue;
				}
				k.phase += k.speed * dt;
				const float px = k.home.x + std::cos(k.phase) * k.radius;
				const float pz = k.home.z + std::sin(k.phase) * k.radius;
				const float py = water_level - 0.24f + std::sin(t * 1.9f + k.bob_phase) * 0.03f;
				k.root->set_position(Vector3(px, py, pz));
				k.root->set_rotation(Vector3(0.0f, -k.phase, 0.0f));
			}

			// 定时挑一条鱼跃水（雨天鱼更活跃）
			jump_t -= dt;
			if (jump_t <= 0.0f && !koi.empty()) {
				Koi &k = koi[rng() % koi.size()];
				if (!k.jumping && k.root != nullptr) {
					Vector3 from = k.root->get_position();
					Vector3 dir(std::cos(k.phase + 1.2f), 0.0f, std::sin(k.phase + 1.2f));
					k.jumping = true;
					k.jump_t = 0.0f;
					k.jump_dur = 0.62f + (float)(rng() % 100u) * 0.0015f;
					k.jump_from = from;
					k.jump_to = from + dir * (0.55f + (float)(rng() % 100u) * 0.0035f);
					k.jump_to.y = water_level - 0.20f;
				}
				jump_t = 18.0f + (float)(rng() % 1000u) * 0.017f - wet * 8.0f;
			}

			// 涟漪：晴天是鱼碰水，雨天则是雨点密密麻麻打在荷叶/水面上
			ring_t -= dt;
			if (ring_t <= 0.0f) {
				if (wet > 0.25f) {
					if (!pad_spots.empty()) {
						const Vector3 &p = pad_spots[rng() % pad_spots.size()];
						spawn_ring(Vector3(p.x, water_level + 0.02f, p.z));
					}
					ring_t = 0.30f + (float)(rng() % 1000u) * 0.0009f;
				} else {
					if (!koi.empty()) {
						const Koi &k = koi[rng() % koi.size()];
						spawn_ring(Vector3(k.home.x + (float)(rng() % 100) * 0.01f - 0.5f,
								water_level + 0.015f,
								k.home.z + (float)(rng() % 100) * 0.008f - 0.4f));
					}
					ring_t = 1.8f + (float)(rng() % 1000u) * 0.0022f;
				}
			}
			for (Ring &r : rings) {
				if (!r.active || r.mi == nullptr) {
					continue;
				}
				r.t += dt;
				float u = r.t / r.dur;
				if (u >= 1.0f) {
					r.active = false;
					r.mi->set_visible(false);
					continue;
				}
				float s = 0.55f + u * 2.6f;
				r.mi->set_scale(Vector3(s, 1.0f, s));
				if (r.mat.is_valid()) {
					r.mat->set_albedo(Color(0.82f, 0.93f, 0.96f, (1.0f - u) * 0.45f));
				}
			}

			// 落叶：很慢地在水面画小圈
			for (Leaf &l : leaves) {
				if (l.mi == nullptr) {
					continue;
				}
				l.phase += dt * l.drift;
				float px = l.home.x + std::sin(l.phase) * 0.34f;
				float pz = l.home.z + std::cos(l.phase * 0.83f) * 0.24f;
				l.mi->set_position(Vector3(px, water_level + 0.02f, pz));
				l.mi->set_rotation(Vector3(0.0f, l.phase * 1.2f, 0.0f));
			}

			plop_t -= dt;
			if (plop_t <= 0.0f && plop != nullptr && !koi.empty()) {
				const Koi &k = koi[rng() % koi.size()];
				plop->set_position(Vector3(k.home.x, water_level + 0.05f, k.home.z));
				plop->set_pitch_scale(0.9f + (float)(rng() % 100u) * 0.0025f);
				plop->set_volume_db(-22.0f);
				plop->play(0.0f);
				plop_t = 7.0f + (float)(rng() % 1000u) * 0.006f;
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (plop != nullptr) {
				plop->stop();
				plop->set_stream(Ref<AudioStream>());
			}
			snd_plop = Ref<AudioStreamWAV>();
		} break;
		default:
			break;
	}
}

void LilyPond::build_pads() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	Ref<CylinderMesh> pad_mesh;
	pad_mesh.instantiate();
	pad_mesh->set_top_radius(1.0f);
	pad_mesh->set_bottom_radius(0.92f);
	pad_mesh->set_height(0.03f);
	pad_mesh->set_radial_segments(11);
	pad_mesh->set_rings(1);
	pad_mesh->set_material(meshutil::toon_material(toon, Color(0.10f, 0.27f, 0.12f), 0.0f));

	const Vector3 centre = get_global_position();
	// 成簇生长：几处"荷叶窝"向四周散开，比均匀撒点更像真的水湾
	const float cluster_x[6] = {-2.3f, -0.4f, 1.5f, 2.4f, -1.8f, 0.4f};
	const float cluster_z[6] = {-1.5f, 1.3f, -1.2f, 1.5f, 1.9f, -0.3f};
	int guard = 0;
	// 优化（第 200 轮）：46 片荷叶合成一个 MultiMesh。
	std::vector<Transform3D> pad_xforms;
	for (int ci = 0; ci < 6 && (int)pad_spots.size() < 46; ci++) {
		int made = 0;
		while (made < 8 && guard < 2200) {
		guard++;
		float ang = (float)(rng() % 628u) * 0.01f;
		float rr = (float)(rng() % 1000u) * 0.0015f;
		Vector3 p(centre.x + cluster_x[ci] + std::cos(ang) * rr, 0.0f, centre.z + cluster_z[ci] + std::sin(ang) * rr);
		if (p.x > -8.3f) {
			continue; // 不压码头
		}
		float depth = (terrain != nullptr) ? terrain->water_depth_at(p.x, p.z) : 1.5f;
		if (depth < 0.5f || depth > 3.4f) {
			continue;
		}
		bool too_close = false;
		for (const Vector3 &q : pad_spots) {
			if (Vector2(q.x - p.x, q.z - p.z).length() < 0.55f) {
				too_close = true;
				break;
			}
		}
		if (too_close) {
			continue;
		}
		float r = 0.26f + (float)(rng() % 100u) * 0.0036f;

		const Basis pb = Basis::from_euler(Vector3(0.0f, (float)(rng() % 628u) * 0.01f, (float)(rng() % 100u) * 0.0006f - 0.03f)).scaled(Vector3(r, 1.0f, r));
		pad_xforms.push_back(Transform3D(pb, Vector3(p.x, water_level + 0.012f, p.z)));
		pad_spots.push_back(Vector3(p.x, water_level + 0.03f, p.z));
		made++;
		}
	}
	Ref<MultiMesh> pads_mm;
	pads_mm.instantiate();
	pads_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	pads_mm->set_mesh(pad_mesh);
	pads_mm->set_instance_count((int)pad_xforms.size());
	for (int i = 0; i < (int)pad_xforms.size(); i++) {
		pads_mm->set_instance_transform(i, pad_xforms[i]);
	}
	MultiMeshInstance3D *pads_mi = memnew(MultiMeshInstance3D);
	pads_mi->set_name("Pads");
	pads_mi->set_multimesh(pads_mm);
	add_child(pads_mi);
}

void LilyPond::build_flowers() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Vector3 centre = get_global_position();
	int flower_i = 0;
	for (const Vector3 &p : pad_spots) {
		if (flower_i >= 8) {
			break;
		}
		// 只在水湾中心一带挑几片叶子开花，避免整片都开
		if (Vector2(p.x - centre.x, p.z - centre.z).length() > 3.2f) {
			continue;
		}
		flower_i++;
		const Color petal_a(1.0f, 0.72f, 0.78f);
		const Color petal_b(0.94f, 0.42f, 0.54f);
		for (int i = 0; i < 8; i++) {
			float a = 6.2831853f * (float)i / 8.0f;
			Vector3 offset(std::sin(a) * 0.055f, 0.115f, std::cos(a) * 0.055f);
			meshutil::add_box(this, toon, Vector3(0.052f, 0.17f, 0.022f),
					Vector3(p.x, p.y, p.z) + offset,
					(i % 2 == 0) ? petal_a : petal_b,
					Vector3(0.38f, a, 0.0f));
		}
		meshutil::add_sphere(this, toon, 0.048f,
				Vector3(p.x, p.y + 0.10f, p.z), Color(0.98f, 0.78f, 0.22f));
	}
}

void LilyPond::build_koi() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	Ref<SphereMesh> body_mesh;
	body_mesh.instantiate();
	body_mesh->set_radius(0.12f);
	body_mesh->set_height(0.24f);
	body_mesh->set_radial_segments(10);
	body_mesh->set_rings(6);
	Ref<BoxMesh> tail_mesh;
	tail_mesh.instantiate();
	tail_mesh->set_size(Vector3(0.16f, 0.012f, 0.11f));

	const Color palette[4] = {
		Color(0.96f, 0.52f, 0.22f),
		Color(0.95f, 0.92f, 0.84f),
		Color(0.90f, 0.40f, 0.18f),
		Color(0.98f, 0.78f, 0.34f),
	};
	const int picks[4] = {3, 14, 25, 36};
	int i = 0;
	for (int idx : picks) {
		if (idx >= (int)pad_spots.size() || i >= 4) {
			break;
		}
		const Vector3 p = pad_spots[idx];
		Koi k;
		k.home = Vector3(p.x, water_level, p.z + 0.25f);
		k.radius = 0.65f + (float)(rng() % 100u) * 0.008f;
		k.phase = (float)(rng() % 628u) * 0.01f;
		k.speed = 0.32f + (float)(rng() % 100u) * 0.0022f;
		k.bob_phase = (float)(rng() % 628u) * 0.01f;

		Node3D *root = memnew(Node3D);
		root->set_name(String("Koi") + String::num_int64(i));
		add_child(root);

		MeshInstance3D *body = memnew(MeshInstance3D);
		body->set_mesh(body_mesh);
		body->set_material_override(meshutil::toon_material(toon, palette[i % 4], 0.5f));
		body->set_scale(Vector3(1.18f, 0.62f, 2.15f));
		root->add_child(body);

		MeshInstance3D *tail = memnew(MeshInstance3D);
		tail->set_mesh(tail_mesh);
		tail->set_material_override(meshutil::toon_material(toon, palette[i % 4], 0.4f));
		tail->set_position(Vector3(0.0f, 0.0f, -0.24f));
		tail->set_rotation(Vector3(0.0f, 0.0f, 0.35f));
		root->add_child(tail);

		root->set_position(Vector3(k.home.x + k.radius, water_level - 0.24f, k.home.z));
		k.root = root;
		koi.push_back(k);
		i++;
	}
}

void LilyPond::build_rings() {
	for (int i = 0; i < 5; i++) {
		Ref<TorusMesh> torus;
		torus.instantiate();
		torus->set_inner_radius(0.16f);
		torus->set_outer_radius(0.21f);
		torus->set_rings(4);
		torus->set_ring_segments(24);

		Ring r;
		r.mat.instantiate();
		r.mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		r.mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		r.mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		r.mat->set_albedo(Color(0.82f, 0.93f, 0.96f, 0.0f));
		torus->set_material(r.mat);

		r.mi = memnew(MeshInstance3D);
		r.mi->set_mesh(torus);
		r.mi->set_visible(false);
		add_child(r.mi);
		rings.push_back(r);
	}
}

void LilyPond::build_leaves() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	Ref<CylinderMesh> leaf_mesh;
	leaf_mesh.instantiate();
	leaf_mesh->set_top_radius(1.0f);
	leaf_mesh->set_bottom_radius(1.0f);
	leaf_mesh->set_height(0.012f);
	leaf_mesh->set_radial_segments(7);
	leaf_mesh->set_rings(1);

	const Color colors[3] = {
		Color(0.72f, 0.44f, 0.18f),
		Color(0.82f, 0.60f, 0.22f),
		Color(0.55f, 0.38f, 0.16f),
	};
	for (int i = 0; i < 14; i++) {
		if (pad_spots.empty()) {
			return;
		}
		const Vector3 &p = pad_spots[rng() % pad_spots.size()];
		Leaf l;
		l.home = Vector3(p.x + (float)(rng() % 100u) * 0.012f - 0.6f,
				water_level, p.z + (float)(rng() % 100u) * 0.012f - 0.6f);
		l.phase = (float)(rng() % 628u) * 0.01f;
		l.drift = 0.08f + (float)(rng() % 100u) * 0.0012f;

		l.mi = memnew(MeshInstance3D);
		l.mi->set_mesh(leaf_mesh);
		l.mi->set_material_override(meshutil::toon_material(toon, colors[i % 3], 0.3f));
		float s = 0.045f + (float)(rng() % 100u) * 0.0005f;
		l.mi->set_scale(Vector3(s, 1.0f, s * 0.7f));
		l.mi->set_position(Vector3(l.home.x, water_level + 0.02f, l.home.z));
		add_child(l.mi);
		leaves.push_back(l);
	}
}

void LilyPond::spawn_ring(const Vector3 &p_pos) {
	for (Ring &r : rings) {
		if (!r.active) {
			r.active = true;
			r.t = 0.0f;
			r.dur = 2.2f + (float)(rng() % 100u) * 0.004f;
			r.mi->set_position(p_pos);
			r.mi->set_scale(Vector3(0.55f, 1.0f, 0.55f));
			r.mi->set_visible(true);
			if (r.mat.is_valid()) {
				r.mat->set_albedo(Color(0.82f, 0.93f, 0.96f, 0.45f));
			}
			return;
		}
	}
}

void LilyPond::make_plop() {
	snd_plop = synth_plop();
	plop = memnew(AudioStreamPlayer3D);
	plop->set_name("KoiPlop");
	plop->set_stream(snd_plop);
	plop->set_volume_db(-22.0f);
	plop->set_max_distance(20.0f);
	add_child(plop);
}

// 0.32s 的小水花：一个快速下滑的低频"啵"，叠一点水面噪声
Ref<AudioStreamWAV> LilyPond::synth_plop() {
	const int sr = 22050;
	const float dur = 0.32f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float u = t / dur;
		float f = 620.0f * std::exp(-6.5f * t) + 180.0f;
		float env = Math::clamp(t / 0.01f, 0.0f, 1.0f) * std::exp(-5.0f * t);
		float body = std::sin(6.2831853f * f * t) * env;
		float splash = (d01(rng) - 0.5f) * std::exp(-13.0f * t) * 0.35f;
		float samp = (body * 0.55f + splash) * (1.0f - 0.2f * u);
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
	return wav;
}
