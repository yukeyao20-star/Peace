#include "snail_patch.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void SnailPatch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_snail_count"), &SnailPatch::get_snail_count);
	ClassDB::bind_method(D_METHOD("get_emerge_count"), &SnailPatch::get_emerge_count);
	ClassDB::bind_method(D_METHOD("get_retract_count"), &SnailPatch::get_retract_count);
	ClassDB::bind_method(D_METHOD("get_trail_active"), &SnailPatch::get_trail_active);
	ClassDB::bind_method(D_METHOD("get_active"), &SnailPatch::get_active);
}

void SnailPatch::_notification(int p_what) {
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

			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}
			if (terrain == nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(root->find_child("Terrain", true, false));
			}
			SkyCycle *sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;

			// 雨后出没：带一点回差，免得雨量在小雨阈值附近来回闪。
			if (!active && rain > 0.30f && snow < 0.2f) {
				active = true;
			} else if (active && (rain < 0.15f || snow > 0.3f)) {
				active = false;
			}

			emerge_count = 0;
			retract_count = 0;
			for (int i = 0; i < SNAIL_COUNT; i++) {
				Snail &s = snails[i];
				if (s.root == nullptr) {
					continue;
				}
				const float target = active ? 1.0f : 0.0f;
				s.emerge = Math::move_toward(s.emerge, target, delta * (active ? 0.7f : 0.45f));
				if (s.emerge < 0.02f) {
					s.root->set_visible(false);
					continue;
				}
				s.root->set_visible(true);

				const float base_scale = 0.35f + 0.65f * s.emerge;
				s.root->set_scale(Vector3(base_scale, base_scale, base_scale));

				// 人一凑近就缩回壳里；走远再探出来。
				float want_retract = 0.0f;
				if (player != nullptr) {
					const Vector3 pp = player->get_global_position();
					const float d = Vector2(pp.x - s.pos.x, pp.z - s.pos.z).length();
					if (d < 1.6f) {
						want_retract = 1.0f;
					}
				}
				s.retract = Math::move_toward(s.retract, want_retract, delta * 3.5f);
				if (s.retract > 0.5f) {
					retract_count++;
				}

				bool moving = false;
				if (active && s.emerge > 0.6f && s.retract < 0.25f) {
					// 慢吞吞地走：朝向轻轻摆动，离窝太远就慢慢转回去。
					s.heading += std::sin(t * 0.31f + s.phase) * 0.45f * delta;
					const Vector3 away(s.home.x - s.pos.x, 0.0f, s.home.z - s.pos.z);
					if (away.length() > 1.3f) {
						const float want = std::atan2(away.x, away.z);
						float diff = want - s.heading;
						while (diff > 3.14159f) {
							diff -= 6.28318f;
						}
						while (diff < -3.14159f) {
							diff += 6.28318f;
						}
						s.heading += Math::clamp(diff, -1.4f * delta, 1.4f * delta);
					}
					const float speed = 0.055f + 0.02f * std::sin(s.phase * 1.7f);
					s.pos.x += std::sin(s.heading) * speed * delta;
					s.pos.z += std::cos(s.heading) * speed * delta;
					s.trail_acc += speed * delta;
					if (s.trail_acc > 0.16f) {
						s.trail_acc = 0.0f;
						const Vector3 back(std::sin(s.heading), 0.0f, std::cos(s.heading));
						add_trail_segment(s.pos - back * 0.07f, s.heading);
					}
					moving = true;
				}

				s.pos.y = ground_height(s.pos.x, s.pos.z);
				s.root->set_position(s.pos);
				s.root->set_rotation(Vector3(0.0f, s.heading, 0.0f));

				// 身子伸缩：缩回壳里时前后/高度一起收；走路时慢慢一伸一缩。
				if (s.body != nullptr) {
					const float stretch = moving ? (1.0f + 0.12f * std::sin(t * 3.2f + s.phase)) : 1.0f;
					s.body->set_scale(Vector3(1.0f, 1.0f - 0.45f * s.retract,
							1.0f * stretch * (1.0f - 0.55f * s.retract)));
					s.body->set_position(Vector3(0.0f, 0.0f, 0.045f - 0.06f * s.retract));
				}

				if (s.emerge > 0.5f) {
					emerge_count++;
				}
			}

			// 黏液痕：MultiMesh 薄片带，按存放顺序逐片淡出。
			trail_active = 0;
			if (trail_mesh.is_valid()) {
				for (int i = 0; i < TRAIL_SLOTS; i++) {
					if (trail_age[i] < trail_life[i]) {
						trail_age[i] += delta;
					}
					if (trail_age[i] >= trail_life[i]) {
						trail_mesh->set_instance_transform(i, Transform3D(Basis().scaled(Vector3()), Vector3()));
						trail_mesh->set_instance_color(i, Color(1, 1, 1, 0));
						continue;
					}
					const float f = 1.0f - trail_age[i] / trail_life[i];
					const Basis b(Vector3(0.0f, 1.0f, 0.0f), trail_yaw[i]);
					Transform3D xf(b, trail_pos[i] + Vector3(0.0f, 0.006f, 0.0f));
					trail_mesh->set_instance_transform(i, xf);
					trail_mesh->set_instance_color(i, Color(0.70f, 0.86f, 0.84f, 0.20f * f * f));
					trail_active++;
				}
			}
		} break;
		default:
			break;
	}
}

float SnailPatch::ground_height(float p_x, float p_z) {
	if (terrain == nullptr && get_tree() != nullptr) {
		terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
	}
	return (terrain != nullptr) ? terrain->get_terrain_height(p_x, p_z) : 0.0f;
}

void SnailPatch::add_trail_segment(const Vector3 &p_pos, float p_yaw) {
	const int i = trail_next;
	trail_pos[i] = p_pos;
	trail_yaw[i] = p_yaw;
	trail_age[i] = 0.0f;
	trail_life[i] = 20.0f;
	trail_next = (trail_next + 1) % TRAIL_SLOTS;
}

void SnailPatch::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Vector3 base(10.6f, 0.0f, 14.0f); // SnailPatch 节点放在原点，子节点坐标即世界坐标
	const Vector3 offsets[SNAIL_COUNT] = {
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.7f, 0.0f, 0.35f),
		Vector3(-0.6f, 0.0f, 0.5f),
		Vector3(0.25f, 0.0f, -0.55f),
	};

	for (int i = 0; i < SNAIL_COUNT; i++) {
		Snail &s = snails[i];
		s.home = base + offsets[i];
		s.home.y = ground_height(s.home.x, s.home.z);
		s.pos = s.home;
		s.heading = (float)i * 1.7f;
		s.phase = (float)i * 1.9f;

		s.root = memnew(Node3D);
		s.root->set_name(String("Snail") + String::num_int64(i));
		s.root->set_position(s.pos);
		s.root->set_rotation(Vector3(0.0f, s.heading, 0.0f));
		s.root->set_visible(false);
		add_child(s.root);

		// 壳：几颗琥珀色小球叠成一个小螺旋
		meshutil::add_sphere(s.root, toon, 0.055f, Vector3(0.0f, 0.055f, -0.025f), Color(0.62f, 0.42f, 0.20f));
		meshutil::add_sphere(s.root, toon, 0.038f, Vector3(0.0f, 0.082f, -0.008f), Color(0.72f, 0.50f, 0.24f));
		meshutil::add_sphere(s.root, toon, 0.024f, Vector3(0.0f, 0.099f, 0.008f), Color(0.82f, 0.60f, 0.30f));
		meshutil::add_sphere(s.root, toon, 0.012f, Vector3(0.0f, 0.108f, 0.020f), Color(0.90f, 0.72f, 0.42f));

		// 软软的身子：拉长的椭球脚、圆脑袋、两根向前探的触角
		s.body = memnew(Node3D);
		s.body->set_name("Body");
		s.body->set_position(Vector3(0.0f, 0.0f, 0.045f));
		s.root->add_child(s.body);
		MeshInstance3D *foot = meshutil::add_sphere(s.body, toon, 0.06f, Vector3(0.0f, 0.012f, 0.0f), Color(0.88f, 0.80f, 0.66f));
		foot->set_scale(Vector3(0.42f, 0.22f, 1.02f));
		MeshInstance3D *head = meshutil::add_sphere(s.body, toon, 0.034f, Vector3(0.0f, 0.021f, 0.058f), Color(0.92f, 0.85f, 0.72f));
		head->set_scale(Vector3(0.95f, 0.9f, 1.0f));
		meshutil::add_box(s.body, toon, Vector3(0.005f, 0.038f, 0.005f), Vector3(-0.012f, 0.050f, 0.074f),
				Color(0.86f, 0.78f, 0.64f), Vector3(0.5f, 0.0f, 0.0f));
		meshutil::add_box(s.body, toon, Vector3(0.005f, 0.038f, 0.005f), Vector3(0.012f, 0.050f, 0.074f),
				Color(0.86f, 0.78f, 0.64f), Vector3(0.5f, 0.0f, 0.0f));
		meshutil::add_sphere(s.body, toon, 0.0050f, Vector3(-0.012f, 0.066f, 0.091f), Color(0.16f, 0.12f, 0.09f));
		meshutil::add_sphere(s.body, toon, 0.0050f, Vector3(0.012f, 0.066f, 0.091f), Color(0.16f, 0.12f, 0.09f));
	}

	// 黏液痕：一片片朝上的薄方片，颜色带 Alpha 逐片淡出。
	Ref<PlaneMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(0.055f, 0.16f));
	Ref<StandardMaterial3D> slime_mat;
	slime_mat.instantiate();
	slime_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	slime_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	slime_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	slime_mat->set_albedo(Color(1.0f, 1.0f, 1.0f, 1.0f));
	slime_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	quad->set_material(slime_mat);

	trail_mesh.instantiate();
	trail_mesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	trail_mesh->set_use_colors(true);
	trail_mesh->set_mesh(quad);
	trail_mesh->set_instance_count(TRAIL_SLOTS);
	for (int i = 0; i < TRAIL_SLOTS; i++) {
		trail_age[i] = 999.0f;
		trail_life[i] = 20.0f;
		trail_mesh->set_instance_transform(i, Transform3D(Basis().scaled(Vector3()), Vector3()));
		trail_mesh->set_instance_color(i, Color(1, 1, 1, 0));
	}
	trail_mm = memnew(MultiMeshInstance3D);
	trail_mm->set_name("SlimeTrails");
	trail_mm->set_multimesh(trail_mesh);
	add_child(trail_mm);
}
