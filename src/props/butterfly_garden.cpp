#include "butterfly_garden.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void ButterflyGarden::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_visible_count"), &ButterflyGarden::get_visible_count);
	ClassDB::bind_method(D_METHOD("get_landed_count"), &ButterflyGarden::get_landed_count);
	ClassDB::bind_method(D_METHOD("get_count"), &ButterflyGarden::get_count);
}
void ButterflyGarden::_notification(int p_what) {
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
			const Vector3 base = get_global_position();
			visible_count = 0;
			landed_count = 0;
			for (int i = 0; i < COUNT; i++) {
				if (butterflies[i] == nullptr) continue;
				state_t[i] += delta;
				Vector3 world = base + pos[i];
				float pd = 999.0f;
				if (player != nullptr) pd = player->get_global_position().distance_to(world);
				if (pd < 1.6f) {
					landed[i] = false;
					const Vector3 away = pos[i] - (player->get_global_position() - base);
					target[i] = pos[i] + Vector3(away.x, 0.4f, away.z).normalized() * 0.9f;
					state_t[i] = 0.0f;
				}
				if (!landed[i] && state_t[i] > 2.2f + 0.7f * (float)i) {
					// 落到附近一朵花上
					const float a = (float)i * 2.1f;
					target[i] = Vector3(0.0f + std::cos(a) * 0.45f, 0.48f, 0.0f + std::sin(a) * 0.30f);
					if (pos[i].distance_to(target[i]) < 0.05f) {
						landed[i] = true;
						state_t[i] = 0.0f;
					}
				} else if (!landed[i]) {
					const float a = t * 0.8f + (float)i * 2.0f;
					target[i] = Vector3(std::cos(a) * 0.8f, 0.9f + 0.2f * std::sin(t * 1.4f + (float)i), std::sin(a) * 0.6f);
				}
				if (landed[i] && state_t[i] > 2.5f) { landed[i] = false; state_t[i] = 0.0f; }
				pos[i] = pos[i].lerp(target[i], Math::min(1.0f, delta * (landed[i] ? 2.5f : 1.2f)));
				butterflies[i]->set_position(pos[i]);
				if (!landed[i]) butterflies[i]->set_rotation(Vector3(0.0f, -t * 1.2f + (float)i, 0.0f));
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = landed[i] ? -1.2f : std::sin(t * 12.0f + (float)i) * 0.9f;
						wings[i][w]->set_rotation(Vector3(0.0f, (w == 0 ? 1.0f : -1.0f) * flap, 0.0f));
					}
				}
				visible_count++;
				if (landed[i]) landed_count++;
			}
		} break;
		default: break;
	}
}
void ButterflyGarden::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	const Color cols[3] = { Color(0.90f, 0.62f, 0.24f), Color(0.86f, 0.44f, 0.50f), Color(0.56f, 0.68f, 0.86f) };
	for (int i = 0; i < COUNT; i++) {
		Node3D *b = memnew(Node3D);
		b->set_name(String("Butterfly") + String::num_int64(i));
		pos[i] = Vector3((float)i * 0.4f - 0.4f, 0.8f, 0.2f);
		b->set_position(pos[i]);
		add_child(b);
		butterflies[i] = b;
		meshutil::add_box(b, toon, Vector3(0.010f, 0.010f, 0.060f), Vector3(), Color(0.20f, 0.16f, 0.12f));
		for (int w = 0; w < 2; w++) {
			Ref<PlaneMesh> wing;
			wing.instantiate();
			wing->set_size(Vector2(0.075f, 0.06f));
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			mat->set_albedo(Color(cols[i].r, cols[i].g, cols[i].b, 0.9f));
			mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			wing->set_material(mat);
			MeshInstance3D *wmi = memnew(MeshInstance3D);
			wmi->set_mesh(wing);
			wmi->set_position(Vector3((w == 0 ? 1.0f : -1.0f) * 0.038f, 0.0f, 0.0f));
			wmi->set_rotation(Vector3(-1.5707964f, 0.0f, 0.0f));
			b->add_child(wmi);
			wings[i][w] = wmi;
		}
	}
}
