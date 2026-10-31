#include "owl_perch.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void OwlPerch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_owl_count"), &OwlPerch::get_owl_count);
	ClassDB::bind_method(D_METHOD("get_visible_count"), &OwlPerch::get_visible_count);
	ClassDB::bind_method(D_METHOD("get_hoot_ready"), &OwlPerch::get_hoot_ready);
}

int OwlPerch::get_visible_count() const {
	int n = 0;
	for (const Owl &o : owls) {
		if (o.root != nullptr && o.root->is_visible()) {
			n++;
		}
	}
	return n;
}

void OwlPerch::_notification(int p_what) {
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
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			snd_hoot = make_hoot();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("OwlVoice");
			voice->set_stream(snd_hoot);
			voice->set_volume_db(-14.0f);
			voice->set_max_distance(32.0f);
			add_child(voice);
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			const bool show = night > 0.52f && rain < 0.25f && snow < 0.25f;
			const Vector3 ppos = player != nullptr ? player->get_global_position() : Vector3(0, -999, 0);

			for (Owl &o : owls) {
				if (o.root == nullptr) {
					continue;
				}
				if (o.root->is_visible() != show) {
					o.root->set_visible(show);
				}
				if (!show) {
					continue;
				}
				const Vector3 op = o.root->get_global_position();
				// 头追着玩家转（只转有限角度，避免整颗头拧成麻花）
				if (o.head != nullptr) {
					float want = 0.0f;
					Vector3 to = ppos - op;
					to.y = 0.0f;
					if (to.length() < 16.0f && to.length_squared() > 0.01f) {
						float ang = std::atan2(to.x, to.z);
						want = Math::clamp(Math::angle_difference(o.root->get_rotation().y, ang), -0.95f, 0.95f);
					}
					o.yaw = Math::lerp(o.yaw, want, 1.0f - std::exp(-4.5f * delta));
					o.head->set_rotation(Vector3(
							std::sin(t * 0.7f + o.phase) * 0.05f,
							o.yaw,
							std::sin(t * 0.5f + o.phase) * 0.04f));
				}
				// 眨眼：眼睑用眼睛的纵向缩放模拟
				o.blink_t -= delta;
				float eye_scale_y = 1.0f;
				if (o.blink_t <= 0.0f) {
					o.blink_phase = 0.0f;
					o.blink_t = 3.2f + (float)(rng() % 500u) * 0.008f;
				}
				if (o.blink_phase >= 0.0f) {
					o.blink_phase += delta;
					if (o.blink_phase < 0.09f) {
						eye_scale_y = 0.12f;
					} else if (o.blink_phase < 0.18f) {
						eye_scale_y = 0.35f;
					} else {
						o.blink_phase = -1.0f;
					}
				}
				if (o.eye_l != nullptr) {
					o.eye_l->set_scale(Vector3(1.0f, eye_scale_y, 1.0f));
				}
				if (o.eye_r != nullptr) {
					o.eye_r->set_scale(Vector3(1.0f, eye_scale_y, 1.0f));
				}

				// 夜里近处偶尔叫两声
				o.hoot_t -= delta;
				if (o.hoot_t <= 0.0f) {
					if (voice != nullptr && op.distance_to(ppos) < 26.0f) {
						voice->set_global_position(op + Vector3(0.0f, 1.0f, 0.0f));
						voice->set_pitch_scale(0.92f + (float)(rng() % 100u) * 0.0016f);
						voice->set_volume_db(-14.0f);
						voice->play(0.0f);
					}
					o.hoot_t = 12.0f + (float)(rng() % 900u) * 0.011f;
				}
			}
		} break;
		default:
			break;
	}
}

void OwlPerch::build() {
	owls.clear();
	const Vector3 spots[2] = {Vector3(-17.2f, 0.0f, -12.6f), Vector3(7.8f, 0.0f, -13.8f)};
	for (int i = 0; i < 2; i++) {
		Vector3 home = spots[i];
		if (terrain != nullptr) {
			home.y = terrain->get_terrain_height(home.x, home.z);
		}
		build_owl(i, home, 300u + (unsigned)i * 97u);
	}
}

void OwlPerch::build_owl(int p_index, const Vector3 &p_home, unsigned p_seed) {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color bark(0.34f, 0.23f, 0.13f);
	const Color bark_light(0.48f, 0.34f, 0.20f);
	const Color feather(0.44f, 0.31f, 0.19f);
	const Color feather_light(0.62f, 0.48f, 0.31f);
	const Color cream(0.86f, 0.76f, 0.58f);
	const Color eye_y(0.96f, 0.78f, 0.20f);
	const Color dark(0.08f, 0.06f, 0.045f);

	// 栖木：一根木桩 + 一根横杆
	meshutil::add_box(this, toon, Vector3(0.14f, 1.65f, 0.14f), p_home + Vector3(0.0f, 0.82f, 0.0f), bark);
	meshutil::add_box(this, toon, Vector3(0.62f, 0.09f, 0.12f), p_home + Vector3(0.0f, 1.63f, 0.0f), bark_light);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.22f, 0.10f), p_home + Vector3(-0.24f, 1.48f, 0.0f), bark);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.22f, 0.10f), p_home + Vector3(0.24f, 1.48f, 0.0f), bark);

	Owl o;
	o.home = p_home + Vector3(0.0f, 1.67f, 0.0f);
	o.phase = (float)(p_seed % 100u) * 0.06f;
	o.blink_t = 2.0f + (float)(p_seed % 40u) * 0.05f;
	o.hoot_t = 5.0f + (float)(p_seed % 60u) * 0.1f;

	Node3D *root = memnew(Node3D);
	root->set_name(String("Owl") + String::num_int64(p_index));
	add_child(root);
	root->set_global_position(o.home);
	root->set_rotation(Vector3(0.0f, (float)(p_seed % 628u) * 0.01f, 0.0f));
	root->set_visible(false);
	o.root = root;

	// 身体（微微前倾的蛋形），胸口浅色羽毛
	MeshInstance3D *body = memnew(MeshInstance3D);
	Ref<SphereMesh> body_mesh;
	body_mesh.instantiate();
	body_mesh->set_radius(0.24f);
	body_mesh->set_height(0.48f);
	body_mesh->set_radial_segments(14);
	body_mesh->set_rings(8);
	body_mesh->set_material(meshutil::toon_material(toon, feather, 0.4f));
	body->set_mesh(body_mesh);
	body->set_position(Vector3(0.0f, 0.25f, 0.0f));
	body->set_scale(Vector3(0.92f, 1.18f, 0.88f));
	root->add_child(body);
	meshutil::add_sphere(root, toon, 0.15f, Vector3(0.0f, 0.21f, 0.13f), cream);

	// 两侧收起的翅膀
	for (int i = 0; i < 2; i++) {
		MeshInstance3D *wing = memnew(MeshInstance3D);
		Ref<SphereMesh> wm;
		wm.instantiate();
		wm->set_radius(0.11f);
		wm->set_height(0.22f);
		wm->set_radial_segments(10);
		wm->set_rings(6);
		wm->set_material(meshutil::toon_material(toon, feather_light, 0.35f));
		wing->set_mesh(wm);
		wing->set_position(Vector3(i == 0 ? -0.18f : 0.18f, 0.26f, 0.02f));
		wing->set_scale(Vector3(0.75f, 1.5f, 0.8f));
		root->add_child(wing);
	}

	// 头（可转动）
	Node3D *head = memnew(Node3D);
	head->set_name("Head");
	head->set_position(Vector3(0.0f, 0.55f, 0.02f));
	root->add_child(head);
	o.head = head;
	meshutil::add_sphere(head, toon, 0.18f, Vector3(0.0f, 0.0f, 0.0f), feather);

	// 耳朵/角羽
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> ear;
		ear.instantiate();
		ear->set_top_radius(0.0f);
		ear->set_bottom_radius(0.045f);
		ear->set_height(0.12f);
		ear->set_radial_segments(6);
		ear->set_rings(1);
		ear->set_material(meshutil::toon_material(toon, feather, 0.3f));
		MeshInstance3D *em = memnew(MeshInstance3D);
		em->set_mesh(ear);
		em->set_position(Vector3(i == 0 ? -0.10f : 0.10f, 0.16f, 0.0f));
		em->set_rotation(Vector3(0.0f, 0.0f, i == 0 ? 0.26f : -0.26f));
		head->add_child(em);
	}

	// 大眼睛：一圈黄 + 黑瞳
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> eye;
		eye.instantiate();
		eye->set_top_radius(0.062f);
		eye->set_bottom_radius(0.062f);
		eye->set_height(0.02f);
		eye->set_radial_segments(12);
		eye->set_rings(1);
		eye->set_material(meshutil::toon_material(toon, eye_y, 0.1f));
		MeshInstance3D *em = memnew(MeshInstance3D);
		em->set_mesh(eye);
		em->set_position(Vector3(i == 0 ? -0.075f : 0.075f, 0.035f, 0.155f));
		em->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
		head->add_child(em);
		if (i == 0) {
			o.eye_l = em;
		} else {
			o.eye_r = em;
		}
		meshutil::add_sphere(head, toon, 0.026f, Vector3(i == 0 ? -0.075f : 0.075f, 0.035f, 0.172f), dark);
	}
	// 喙
	Ref<CylinderMesh> beak;
	beak.instantiate();
	beak->set_top_radius(0.0f);
	beak->set_bottom_radius(0.035f);
	beak->set_height(0.09f);
	beak->set_radial_segments(5);
	beak->set_rings(1);
	beak->set_material(meshutil::toon_material(toon, Color(0.72f, 0.55f, 0.22f), 0.2f));
	MeshInstance3D *bm = memnew(MeshInstance3D);
	bm->set_mesh(beak);
	bm->set_position(Vector3(0.0f, -0.015f, 0.19f));
	bm->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	head->add_child(bm);

	owls.push_back(o);
}

// 1.5 秒两声低沉的“咕—咕”，带轻微的喉音
Ref<AudioStreamWAV> OwlPerch::make_hoot() {
	const int sr = 22050;
	const float dur = 1.5f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int b = 0; b < 2; b++) {
			float bt = t - b * 0.62f;
			if (bt < 0.0f || bt > 0.46f) {
				continue;
			}
			float u = bt / 0.46f;
			float f = 345.0f - 22.0f * u + std::sin(6.2831853f * 5.0f * bt) * 4.0f;
			float env = Math::clamp(bt / 0.06f, 0.0f, 1.0f) * Math::clamp((0.46f - bt) / 0.18f, 0.0f, 1.0f);
			samp += (std::sin(6.2831853f * f * bt) + 0.32f * std::sin(6.2831853f * f * 2.0f * bt)) * env * 0.42f;
		}
		int16_t q = (int16_t)Math::clamp(samp * 13000.0f, -32000.0f, 32000.0f);
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
