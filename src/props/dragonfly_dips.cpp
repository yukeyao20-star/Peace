#include "dragonfly_dips.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void DragonflyDips::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_visible_count"), &DragonflyDips::get_visible_count);
	ClassDB::bind_method(D_METHOD("get_dips"), &DragonflyDips::get_dips);
}
void DragonflyDips::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) break;
			if (player == nullptr) player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			if (sky == nullptr) sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			if (terrain == nullptr) terrain = Object::cast_to<TerrainBuilder>(root->find_child("Terrain", true, false));
			const float water = (terrain != nullptr) ? terrain->get_water_level() : 0.0f;
			bool active = true;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				active = tod > 0.29f && tod < 0.70f && sky->get_rain_amount() < 0.3f && sky->get_snow_amount() < 0.3f;
			}
			visible_count = active ? COUNT : 0;
			for (int i = 0; i < COUNT; i++) {
				if (flies[i] == nullptr) continue;
				flies[i]->set_visible(active);
				if (!active) continue;
				const float a = t * (0.9f + 0.15f * (float)i) + phase[i];
				const float r = 1.6f + 0.4f * (float)i;
				const float dip = std::fmod(t + phase[i] * 2.0f, 4.0f);
				float y = water + 0.55f + 0.12f * std::sin(t * 3.0f + phase[i]);
				if (dip < 0.25f) {
					y = water + 0.02f;
					if (dip < delta + 0.001f) dips++;
					ripple_age[i] = 0.0f;
				}
				flies[i]->set_position(Vector3(std::cos(a) * r, y, std::sin(a) * r));
				flies[i]->set_rotation(Vector3(0.0f, -a + 1.5708f, 0.0f));
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						wings[i][w]->set_rotation(Vector3(0.0f, 0.0f, (w == 0 ? 1.0f : -1.0f) * (0.3f + std::sin(t * 26.0f + (float)i) * 0.5f)));
					}
				}
				// 涟漪
				if (ripples[i] != nullptr) {
					if (ripple_age[i] < 1.0f) {
						ripple_age[i] += delta * 1.4f;
						const float age = Math::min(ripple_age[i], 1.0f);
						const float s = 0.35f + age * 1.1f;
						ripples[i]->set_visible(true);
						ripples[i]->set_scale(Vector3(s, 1.0f, s));
						ripples[i]->set_position(Vector3(std::cos(a) * r, water + 0.015f, std::sin(a) * r));
					} else {
						ripples[i]->set_visible(false);
					}
				}
			}
		} break;
		default: break;
	}
}
void DragonflyDips::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
	}
	Ref<TorusMesh> ring_mesh;
	ring_mesh.instantiate();
	ring_mesh->set_inner_radius(0.20f);
	ring_mesh->set_outer_radius(0.23f);
	ring_mesh->set_rings(16);
	ring_mesh->set_ring_segments(3);
	Ref<StandardMaterial3D> rm;
	rm.instantiate();
	rm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	rm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	rm->set_albedo(Color(0.80f, 0.92f, 1.0f, 0.45f));
	rm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ring_mesh->set_material(rm);
	for (int i = 0; i < COUNT; i++) {
		Node3D *f = memnew(Node3D);
		f->set_name(String("Dragonfly") + String::num_int64(i));
		add_child(f);
		flies[i] = f;
		meshutil::add_box(f, toon, Vector3(0.020f, 0.020f, 0.16f), Vector3(), Color(0.24f, 0.46f, 0.62f));
		meshutil::add_sphere(f, toon, 0.028f, Vector3(0.0f, 0.0f, -0.10f), Color(0.30f, 0.52f, 0.66f));
		for (int w = 0; w < 2; w++) {
			MeshInstance3D *wing = meshutil::add_box(f, toon, Vector3(0.16f, 0.008f, 0.045f),
					Vector3((w == 0 ? 1.0f : -1.0f) * 0.08f, 0.01f, 0.0f), Color(0.82f, 0.90f, 0.94f));
			wings[i][w] = wing;
		}
		MeshInstance3D *r = memnew(MeshInstance3D);
		r->set_mesh(ring_mesh);
		r->set_visible(false);
		add_child(r);
		ripples[i] = r;
	}
}
