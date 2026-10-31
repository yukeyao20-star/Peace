#include "paper_boat.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void PaperBoat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &PaperBoat::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &PaperBoat::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &PaperBoat::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &PaperBoat::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &PaperBoat::interact);
	ClassDB::bind_method(D_METHOD("get_active"), &PaperBoat::get_active);
	ClassDB::bind_method(D_METHOD("get_launches"), &PaperBoat::get_launches);
}
String PaperBoat::get_prompt() const { return active ? String::utf8("小船还在漂") : String::utf8("按 E 折一只纸船"); }
Vector3 PaperBoat::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.35f, 0.0f); }
void PaperBoat::interact(const Vector3 &p_player_pos) {
	if (active || boat == nullptr) return;
	active = true;
	launches++;
	drift = 0.0f;
	life = 0.0f;
	boat->set_visible(true);
	boat->set_scale(Vector3(1, 1, 1));
	if (terrain == nullptr && get_tree() != nullptr) terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
	const float water = (terrain != nullptr) ? terrain->get_water_level() : 0.0f;
	origin = get_global_position() + Vector3(0.0f, 0.0f, 0.0f);
	origin.y = water + 0.02f;
	dir = (p_player_pos - get_global_position());
	dir.y = 0.0f;
	if (dir.length_squared() < 0.01f) dir = Vector3(0.0f, 0.0f, -1.0f);
	dir = dir.normalized();
	boat->set_global_position(origin);
}
void PaperBoat::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			if (!active || boat == nullptr) return;
			life += delta;
			drift += delta * 0.35f;
			const float water = (terrain != nullptr) ? terrain->get_water_level() : 0.0f;
			if (boat->get_global_position().y > water - 2.0f) {
				const Vector3 p = origin + dir * drift + Vector3(std::sin(drift * 1.3f) * 0.12f, 0.0f, std::cos(drift * 1.1f) * 0.10f);
				boat->set_global_position(Vector3(p.x, water + 0.02f + 0.01f * std::sin(life * 3.0f), p.z));
				boat->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z) + std::sin(life * 0.8f) * 0.2f, 0.02f * std::sin(life * 2.0f)));
			}
			if (ripple != nullptr) {
				ripple->set_global_position(Vector3(boat->get_global_position().x, water + 0.01f, boat->get_global_position().z));
				const float s = 1.0f + 0.2f * std::sin(life * 2.0f);
				ripple->set_scale(Vector3(s, 1.0f, s));
				ripple->set_visible(true);
			}
			if (life > 18.0f) {
				const float k = Math::max(0.0f, 1.0f - (life - 18.0f) / 2.0f);
				boat->set_scale(Vector3(k, k, k));
				if (life > 20.0f) {
					active = false;
					boat->set_visible(false);
					if (ripple != nullptr) ripple->set_visible(false);
				}
			}
		} break;
		default: break;
	}
}
void PaperBoat::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
	// 岸边的小石台
	Vector3 p = get_global_position();
	p.y = (terrain != nullptr) ? terrain->get_terrain_height(p.x, p.z) : 0.0f;
	set_global_position(p);
	meshutil::add_box(this, toon, Vector3(0.30f, 0.10f, 0.30f), Vector3(0.0f, 0.05f, 0.0f), Color(0.56f, 0.55f, 0.51f));
	boat = memnew(Node3D);
	boat->set_name("PaperBoat");
	boat->set_visible(false);
	add_child(boat);
	Ref<BoxMesh> hull;
	hull.instantiate();
	hull->set_size(Vector3(0.16f, 0.05f, 0.24f));
	Ref<StandardMaterial3D> hm;
	hm.instantiate();
	hm->set_albedo(Color(0.94f, 0.92f, 0.86f));
	hull->set_material(hm);
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(hull);
	boat->add_child(hmi);
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(boat, toon, Vector3(0.16f, 0.01f, 0.20f),
				Vector3((i == 0 ? -1.0f : 1.0f) * 0.07f, 0.04f, 0.0f), Color(0.92f, 0.90f, 0.84f),
				Vector3(0.0f, 0.0f, (i == 0 ? 0.5f : -0.5f)));
	}
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.16f);
	ring->set_outer_radius(0.18f);
	ring->set_rings(16);
	ring->set_ring_segments(3);
	Ref<StandardMaterial3D> rm;
	rm.instantiate();
	rm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	rm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	rm->set_albedo(Color(0.80f, 0.92f, 1.0f, 0.4f));
	rm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ring->set_material(rm);
	ripple = memnew(MeshInstance3D);
	ripple->set_mesh(ring);
	ripple->set_visible(false);
	ripple->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(ripple);
}
