#include "fish_drying_rack.h"
#include <cmath>
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

void FishDryingRack::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &FishDryingRack::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &FishDryingRack::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &FishDryingRack::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &FishDryingRack::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &FishDryingRack::interact);
	ClassDB::bind_method(D_METHOD("get_fish_count"), &FishDryingRack::get_fish_count);
	ClassDB::bind_method(D_METHOD("get_flips"), &FishDryingRack::get_flips);
	ClassDB::bind_method(D_METHOD("is_covered"), &FishDryingRack::is_covered);
	ClassDB::bind_method(D_METHOD("is_gull_visible"), &FishDryingRack::is_gull_visible);
}
bool FishDryingRack::is_gull_visible() const { return gull != nullptr && gull->is_visible(); }
String FishDryingRack::get_prompt() const { return String::utf8("按 E 翻一翻鱼干"); }
Vector3 FishDryingRack::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 1.25f, 0.0f); }
void FishDryingRack::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	flips++;
	for (int i = 0; i < FISH_COUNT; i++) {
		if (fish[i] != nullptr) fish[i]->set_rotation(Vector3(0.0f, 0.0f, 0.35f));
	}
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("把鱼干翻了个面 · 湖风一吹，腥香里带点松木味"), 2.8f);
	}
}
void FishDryingRack::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			const float rain = sky != nullptr ? sky->get_rain_amount() : 0.0f;
			covered = rain > 0.3f;
			if (cover != nullptr) cover->set_visible(covered);
			for (int i = 0; i < FISH_COUNT; i++) {
				if (fish[i] == nullptr) continue;
				const float base = fish[i]->get_rotation().z;
				const float target = 0.0f;
				const float cur = Math::move_toward(base, target, delta * 0.9f);
				fish[i]->set_rotation(Vector3(0.0f, 0.0f, cur + std::sin(t * 1.6f + (float)i * 1.2f) * 0.06f));
			}
			if (gull != nullptr) {
				const bool show = sky != nullptr && sky->get_snow_amount() < 0.3f && rain < 0.5f;
				gull->set_visible(show);
				if (show) {
					const float a = t * 0.55f;
					gull->set_position(Vector3(std::cos(a) * 1.4f, 2.6f + 0.15f * std::sin(t * 1.7f), std::sin(a) * 1.4f));
					gull->set_rotation(Vector3(0.0f, -a, std::sin(t * 6.0f) * 0.18f));
				}
			}
		} break;
		default: break;
	}
}
void FishDryingRack::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 0.6f, 0.0f));
	// 两组交叉木架
	for (int s = 0; s < 2; s++) {
		const float x = s == 0 ? -0.85f : 0.85f;
		for (int i = 0; i < 2; i++) {
			const float z = i == 0 ? -0.32f : 0.32f;
			meshutil::add_box(this, toon, Vector3(0.06f, 1.62f, 0.06f), Vector3(x, 0.80f, z * 0.5f), Color(0.50f, 0.36f, 0.22f), Vector3(z > 0.0f ? -0.22f : 0.22f, 0.0f, 0.0f));
		}
	}
	meshutil::add_box(this, toon, Vector3(1.95f, 0.06f, 0.06f), Vector3(0.0f, 1.55f, 0.0f), Color(0.46f, 0.33f, 0.20f));
	// 麻绳：两股细绳从横杆垂下
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(this, toon, Vector3(1.80f, 0.010f, 0.010f), Vector3(0.0f, 1.46f - 0.03f * (float)i, 0.0f), Color(0.62f, 0.54f, 0.40f));
	}
	// 四条鱼干
	Ref<SphereMesh> body_mesh;
	body_mesh.instantiate();
	body_mesh->set_radius(0.09f);
	body_mesh->set_height(0.20f);
	body_mesh->set_radial_segments(8);
	body_mesh->set_rings(4);
	body_mesh->set_material(meshutil::toon_material(toon, Color(0.56f, 0.61f, 0.64f), 0.25f));
	Ref<BoxMesh> tail_mesh;
	tail_mesh.instantiate();
	tail_mesh->set_size(Vector3(0.035f, 0.11f, 0.06f));
	tail_mesh->set_material(meshutil::toon_material(toon, Color(0.50f, 0.55f, 0.58f), 0.25f));
	for (int i = 0; i < FISH_COUNT; i++) {
		const float x = -0.62f + 0.41f * (float)i;
		Node3D *f = memnew(Node3D);
		f->set_name(String("Fish") + String::num_int64(i));
		f->set_position(Vector3(x, 1.40f, 0.01f));
		add_child(f);
		fish[i] = f;
		MeshInstance3D *bmi = memnew(MeshInstance3D);
		bmi->set_mesh(body_mesh);
		bmi->set_position(Vector3(0.0f, -0.12f, 0.0f));
		bmi->set_scale(Vector3(0.64f, 1.05f, 0.40f));
		f->add_child(bmi);
		for (int w = 0; w < 2; w++) {
			MeshInstance3D *tmi = memnew(MeshInstance3D);
			tmi->set_mesh(tail_mesh);
			tmi->set_position(Vector3(w == 0 ? -0.035f : 0.035f, -0.27f, 0.0f));
			tmi->set_rotation(Vector3(0.0f, 0.0f, w == 0 ? 0.75f : -0.75f));
			f->add_child(tmi);
		}
		meshutil::add_box(f, toon, Vector3(0.012f, 0.07f, 0.04f), Vector3(0.0f, -0.13f, -0.055f), Color(0.48f, 0.53f, 0.56f), Vector3(0.35f, 0.0f, 0.0f));
		meshutil::add_sphere(f, toon, 0.012f, Vector3(0.0f, -0.06f, 0.038f), Color(0.12f, 0.12f, 0.11f));
		meshutil::add_box(f, toon, Vector3(0.012f, 0.13f, 0.012f), Vector3(0.0f, 0.06f, 0.0f), Color(0.60f, 0.54f, 0.42f));
	}
	// 雨布（收在横杆上，雨天才展开）
	cover = meshutil::add_box(this, toon, Vector3(2.05f, 0.02f, 0.95f), Vector3(0.0f, 1.50f, 0.0f), Color(0.66f, 0.58f, 0.42f), Vector3(0.0f, 0.0f, 0.0f));
	cover->set_visible(false);
	// 海鸥
	gull = memnew(Node3D);
	gull->set_name("Gull");
	gull->set_position(Vector3(0.0f, 2.6f, 0.0f));
	gull->set_visible(false);
	add_child(gull);
	meshutil::add_sphere(gull, toon, 0.10f, Vector3(), Color(0.94f, 0.94f, 0.90f))->set_scale(Vector3(1.5f, 0.7f, 0.7f));
	meshutil::add_box(gull, toon, Vector3(0.34f, 0.02f, 0.11f), Vector3(-0.20f, 0.04f, 0.0f), Color(0.88f, 0.88f, 0.84f), Vector3(0.0f, 0.0f, 0.18f));
	meshutil::add_box(gull, toon, Vector3(0.34f, 0.02f, 0.11f), Vector3(0.20f, 0.04f, 0.0f), Color(0.88f, 0.88f, 0.84f), Vector3(0.0f, 0.0f, -0.18f));
	meshutil::add_box(gull, toon, Vector3(0.07f, 0.012f, 0.010f), Vector3(0.16f, 0.0f, 0.0f), Color(0.94f, 0.70f, 0.26f));
}
