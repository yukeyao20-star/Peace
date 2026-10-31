#include "sunflowers.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Sunflowers::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_flower_count"), &Sunflowers::get_flower_count);
	ClassDB::bind_method(D_METHOD("get_bees_visible"), &Sunflowers::get_bees_visible);
	ClassDB::bind_method(D_METHOD("get_sun_up"), &Sunflowers::get_sun_up);
	ClassDB::bind_method(D_METHOD("get_head_yaw"), &Sunflowers::get_head_yaw);
	ClassDB::bind_method(D_METHOD("get_head_pitch"), &Sunflowers::get_head_pitch);
}

void Sunflowers::_notification(int p_what) {
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
			SkyCycle *sky = (get_tree() != nullptr)
					? Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false))
					: nullptr;
			float target_yaw = 0.0f;
			float target_pitch = 0.0f;
			sun_up = true;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				sun_up = tod > 0.25f && tod < 0.75f;
				if (sun_up) {
					const float k = (tod - 0.25f) / 0.50f;
					target_yaw = -1.15f + 2.30f * k; // 东 → 西
					target_pitch = -0.10f - 0.20f * std::sin(3.14159f * k);
				} else {
					target_yaw = -1.15f; // 夜里慢慢转回东边
					target_pitch = -0.55f; // 垂头
				}
			}
			head_yaw = Math::lerp(head_yaw, target_yaw, Math::min(1.0f, delta * 0.7f));
			head_pitch = Math::lerp(head_pitch, target_pitch, Math::min(1.0f, delta * 0.5f));
			for (int i = 0; i < FLOWER_COUNT; i++) {
				if (heads[i] == nullptr) {
					continue;
				}
				const float sway = std::sin(t * 1.1f + (float)i * 1.3f) * 0.03f;
				heads[i]->set_rotation(Vector3(head_pitch,
						head_yaw + (float)i * 0.05f,
						sway));
			}
			const bool bee_on = sun_up && (sky == nullptr || (sky->get_rain_amount() < 0.3f && sky->get_snow_amount() < 0.2f));
			bees_visible = bee_on ? BEE_COUNT : 0;
			for (int i = 0; i < BEE_COUNT; i++) {
				if (bees[i] == nullptr) {
					continue;
				}
				bees[i]->set_visible(bee_on);
				if (!bee_on) {
					continue;
				}
				const float a = t * 1.3f + (float)i * 2.2f;
				bees[i]->set_position(Vector3(-0.6f + 1.2f * (float)i + std::cos(a) * 0.25f,
						1.30f + 0.08f * std::sin(t * 2.0f + (float)i),
						0.10f + std::sin(a) * 0.20f));
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = std::sin(t * 28.0f + (float)i) * 0.8f;
						wings[i][w]->set_rotation(Vector3(0.0f, 0.0f, (w == 0 ? 1.0f : -1.0f) * flap));
					}
				}
			}
		} break;
		default:
			break;
	}
}

void Sunflowers::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(
				get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}

	Ref<CylinderMesh> stem_mesh;
	stem_mesh.instantiate();
	stem_mesh->set_top_radius(0.020f);
	stem_mesh->set_bottom_radius(0.030f);
	stem_mesh->set_height(1.30f);
	stem_mesh->set_radial_segments(6);
	stem_mesh->set_rings(1);
	stem_mesh->set_material(meshutil::toon_material(toon, Color(0.30f, 0.46f, 0.20f), 0.2f));

	Ref<CylinderMesh> disc_mesh;
	disc_mesh.instantiate();
	disc_mesh->set_top_radius(0.17f);
	disc_mesh->set_bottom_radius(0.17f);
	disc_mesh->set_height(0.055f);
	disc_mesh->set_radial_segments(14);
	disc_mesh->set_rings(1);
	disc_mesh->set_material(meshutil::toon_material(toon, Color(0.34f, 0.22f, 0.10f), 0.2f));

	Ref<BoxMesh> petal_mesh;
	petal_mesh.instantiate();
	petal_mesh->set_size(Vector3(0.11f, 0.045f, 0.012f));
	petal_mesh->set_material(meshutil::toon_material(toon, Color(0.95f, 0.76f, 0.18f), 0.25f));

	const Color leaf_col(0.32f, 0.50f, 0.22f);
	const float xs[FLOWER_COUNT] = { -1.5f, -0.9f, -0.3f, 0.3f, 0.9f, 1.5f };
	for (int i = 0; i < FLOWER_COUNT; i++) {
		const float zoff = 0.25f * std::sin((float)i * 2.1f);
		Node3D *stem = memnew(Node3D);
		stem->set_name(String("Sunflower") + String::num_int64(i));
		stem->set_position(Vector3(xs[i], 0.0f, zoff));
		add_child(stem);
		MeshInstance3D *smi = memnew(MeshInstance3D);
		smi->set_mesh(stem_mesh);
		smi->set_position(Vector3(0.0f, 0.65f, 0.0f));
		stem->add_child(smi);

		for (int l = 0; l < 3; l++) {
			MeshInstance3D *leaf = meshutil::add_sphere(stem, toon, 0.13f,
					Vector3((l % 2 == 0 ? -1.0f : 1.0f) * 0.16f, 0.35f + 0.28f * (float)l, 0.02f), leaf_col);
			leaf->set_scale(Vector3(1.1f, 0.16f, 0.55f));
		}

		Node3D *head = memnew(Node3D);
		head->set_name("Head");
		head->set_position(Vector3(0.0f, 1.32f, 0.02f));
		stem->add_child(head);
		heads[i] = head;
		MeshInstance3D *dmi = memnew(MeshInstance3D);
		dmi->set_mesh(disc_mesh);
		dmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
		head->add_child(dmi);
		for (int p = 0; p < 12; p++) {
			const float a = (float)p * 0.5236f;
			MeshInstance3D *petal = memnew(MeshInstance3D);
			petal->set_mesh(petal_mesh);
			petal->set_position(Vector3(std::cos(a) * 0.22f, std::sin(a) * 0.22f, 0.0f));
			petal->set_rotation(Vector3(0.0f, 0.0f, a));
			head->add_child(petal);
		}
	}

	// 两只小蜂
	const Color black(0.16f, 0.13f, 0.10f);
	for (int i = 0; i < BEE_COUNT; i++) {
		Node3D *bee = memnew(Node3D);
		bee->set_name(String("Bee") + String::num_int64(i));
		bee->set_visible(false);
		add_child(bee);
		bees[i] = bee;
		meshutil::add_box(bee, toon, Vector3(0.030f, 0.022f, 0.055f), Vector3(0.0f, 0.0f, 0.0f), Color(0.92f, 0.76f, 0.22f));
		meshutil::add_box(bee, toon, Vector3(0.032f, 0.024f, 0.010f), Vector3(0.0f, 0.0f, 0.008f), black);
		for (int w = 0; w < 2; w++) {
			Ref<BoxMesh> wing;
			wing.instantiate();
			wing->set_size(Vector3(0.045f, 0.010f, 0.028f));
			wing->set_material(meshutil::toon_material(toon, Color(0.92f, 0.95f, 1.0f), 0.1f));
			MeshInstance3D *wmi = memnew(MeshInstance3D);
			wmi->set_mesh(wing);
			wmi->set_position(Vector3((w == 0 ? -1.0f : 1.0f) * 0.030f, 0.016f, 0.0f));
			bee->add_child(wmi);
			wings[i][w] = wmi;
		}
	}
}
