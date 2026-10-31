#include "lake_jump_fx.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

void LakeJumpFx::_bind_methods() {
}

void LakeJumpFx::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build_fish();
			build_ring();
			// 落水声（有本地音频就播，缺失时静默——音频素材不入库，各机本地保留）
			ResourceLoader *rl = ResourceLoader::get_singleton();
			if (rl != nullptr && rl->exists("res://assets/audio/water_splash_gentle.wav")) {
				Ref<Resource> res = rl->load("res://assets/audio/water_splash_gentle.wav");
				Ref<AudioStream> stream = Ref<AudioStream>(Object::cast_to<AudioStream>(res.ptr()));
				if (stream.is_valid()) {
					splash = memnew(AudioStreamPlayer3D);
					splash->set_name("SplashVoice");
					splash->set_stream(stream);
					splash->set_volume_db(-6.0f);
					splash->set_max_distance(30.0f);
					add_child(splash);
				}
			}
			schedule_next();
			// 调试用：ZCODE_FORCE_JUMP=1 时首次跃水提前到 0.5s（探针截图用，不影响正常游玩）
			if (OS::get_singleton()->get_environment("ZCODE_FORCE_JUMP") == "1") {
				timer = 0.5f;
			}
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			double dt = get_process_delta_time();
			float delta = (float)dt;
			if (player == nullptr && get_parent() != nullptr) {
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			if (!leaping) {
				timer -= delta;
				if (timer <= 0.0f) {
					// 只在玩家附近上演，避免远处空演
					bool near = true;
					if (player != nullptr) {
						near = player->get_global_position().distance_to(
								Vector3(lake_center.x, water_level, lake_center.y)) < 55.0f;
					}
					if (near) {
						static std::mt19937 rng(20261031u);
						auto rnd = [&](float lo, float hi) {
							std::uniform_real_distribution<float> d(lo, hi);
							return d(rng);
						};
						float a = rnd(0.0f, 6.2831853f);
						float r = rnd(2.5f, 11.5f);
						leap_start = Vector3(lake_center.x + std::cos(a) * r,
								water_level - 0.22f,
								lake_center.y + std::sin(a) * r);
						leap_end = leap_start + Vector3(rnd(-0.8f, 0.8f), 0.0f, rnd(-0.8f, 0.8f));
						leap_h = rnd(0.45f, 0.68f);
						leap_dur = rnd(0.7f, 0.95f);
						leap_t = 0.0f;
						leaping = true;
						if (fish != nullptr) {
							fish->set_visible(true);
						}
					}
					schedule_next();
				}
			}
			if (leaping && fish != nullptr) {
				leap_t += delta;
				float u = Math::clamp(leap_t / leap_dur, 0.0f, 1.0f);
				Vector3 p = leap_start.lerp(leap_end, u);
				p.y += 4.0f * leap_h * u * (1.0f - u);
				float u2 = Math::min(u + 0.04f, 1.0f);
				Vector3 p2 = leap_start.lerp(leap_end, u2);
				p2.y += 4.0f * leap_h * u2 * (1.0f - u2);
				fish->set_position(p);
				if (p.distance_to(p2) > 0.001f) {
					fish->look_at(p2, Vector3(0.0f, 1.0f, 0.0f));
				}
				if (u >= 1.0f) {
					leaping = false;
					fish->set_visible(false);
					ring_pos = Vector3(leap_end.x, water_level + 0.012f, leap_end.z);
					ring_t = 0.0f;
					ring_active = true;
					if (ring != nullptr) {
						ring->set_visible(true);
						ring->set_position(ring_pos);
					}
					if (splash != nullptr) {
						splash->set_global_position(ring_pos);
						splash->play();
					}
				}
			}
			if (ring_active && ring != nullptr) {
				ring_t += delta;
				float u = Math::clamp(ring_t / ring_dur, 0.0f, 1.0f);
				float s = 0.55f + 1.9f * u;
				ring->set_scale(Vector3(s, 1.0f, s));
				Ref<StandardMaterial3D> mat = Ref<StandardMaterial3D>(
						Object::cast_to<StandardMaterial3D>(ring_mat.ptr()));
				if (mat.is_valid()) {
					Color c = mat->get_albedo();
					c.a = 0.62f * (1.0f - u);
					mat->set_albedo(c);
				}
				if (u >= 1.0f) {
					ring_active = false;
					ring->set_visible(false);
				}
			}
		} break;
		default:
			break;
	}
}

void LakeJumpFx::schedule_next() {
	// 调试：ZCODE_JUMP_LOOP=1 时每 1.5s 一跳（探针截图用；正常游玩不受影响）
	if (OS::get_singleton()->get_environment("ZCODE_JUMP_LOOP") == "1") {
		timer = 1.5f;
		return;
	}
	static std::mt19937 rng(771023u);
	std::uniform_real_distribution<float> d(8.0f, 17.0f);
	timer = d(rng);
}

void LakeJumpFx::build_fish() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color body_c(0.70f, 0.79f, 0.88f);
	fish = memnew(Node3D);
	fish->set_name("JumpingFish");
	add_child(fish);

	Ref<CapsuleMesh> body;
	body.instantiate();
	body->set_radius(0.032f);
	body->set_height(0.30f);
	body->set_radial_segments(10);
	body->set_rings(4);
	body->set_material(meshutil::toon_material(toon, body_c));
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(body);
	// 胶囊默认沿 Y，转成沿 Z（前进方向）
	Transform3D t;
	Basis b;
	b.set_euler_scale(Vector3(1.5707964f, 0.0f, 0.0f), Vector3(1, 1, 1));
	t.set_basis(b);
	mi->set_transform(t);
	fish->add_child(mi);

	// 尾鳍：小圆锥朝后
	Ref<CylinderMesh> tail;
	tail.instantiate();
	tail->set_bottom_radius(0.002f);
	tail->set_top_radius(0.035f);
	tail->set_height(0.055f);
	tail->set_radial_segments(6);
	tail->set_material(meshutil::toon_material(toon, Color(0.58f, 0.67f, 0.78f)));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(tail);
	tmi->set_position(Vector3(0.0f, 0.0f, 0.155f));
	tmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	fish->add_child(tmi);

	fish->set_visible(false);
}

void LakeJumpFx::build_ring() {
	Ref<TorusMesh> torus;
	torus.instantiate();
	torus->set_inner_radius(0.20f);
	torus->set_outer_radius(0.27f);
	torus->set_rings(8);
	torus->set_ring_segments(24);

	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_albedo(Color(0.95f, 0.98f, 1.0f, 0.62f));
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ring_mat = mat;
	torus->set_material(mat);

	ring = memnew(MeshInstance3D);
	ring->set_name("SplashRing");
	ring->set_mesh(torus);
	ring->set_visible(false);
	add_child(ring);
}
