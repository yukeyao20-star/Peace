#include "paper_plane.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void PaperPlane::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &PaperPlane::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &PaperPlane::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &PaperPlane::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &PaperPlane::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &PaperPlane::interact);
	ClassDB::bind_method(D_METHOD("get_flights"), &PaperPlane::get_flights);
	ClassDB::bind_method(D_METHOD("get_active"), &PaperPlane::get_active);
	ClassDB::bind_method(D_METHOD("get_plane_pos"), &PaperPlane::get_plane_pos);
}

String PaperPlane::get_prompt() const {
	return active ? String::utf8("纸飞机还在飞") : String::utf8("按 E 折一架纸飞机");
}

Vector3 PaperPlane::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.55f, 0.0f);
}

Vector3 PaperPlane::get_plane_pos() const {
	return (plane != nullptr) ? plane->get_position() : Vector3();
}

void PaperPlane::interact(const Vector3 &p_player_pos) {
	if (active || (rest_t > 0.0f && rest_t < 0.8f)) {
		return;
	}
	if (player == nullptr || plane == nullptr) {
		return;
	}
	active = true;
	flights++;
	flight_t = 0.0f;
	Vector3 f = Object::cast_to<Player>(player) != nullptr
			? Object::cast_to<Player>(player)->get_facing()
			: Vector3(0.0f, 0.0f, 1.0f);
	f.y = 0.0f;
	if (f.length_squared() < 0.01f) {
		f = Vector3(0.0f, 0.0f, 1.0f);
	}
	f = f.normalized();
	plane->set_global_position(p_player_pos + f * 0.7f + Vector3(0.0f, 1.25f, 0.0f));
	vel = (f * 6.0f + Vector3(0.0f, 1.6f, 0.0f));
	if (sky != nullptr) {
		vel += Vector3(0.0f, 0.0f, 0.0f);
	}
	plane->set_visible(true);
}

void PaperPlane::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}
			if (sky == nullptr) {
				sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			}
			if (!active || plane == nullptr) {
				if (rest_t > 0.0f) {
					rest_t += delta;
					if (rest_t > 6.0f && plane != nullptr) {
						plane->set_visible(false);
						rest_t = 0.0f;
					}
				}
				break;
			}
			// 简化的纸飞机滑翔：缓降 + 水平阻尼 + 微风侧移
			flight_t += delta;
			vel.y -= 2.4f * delta;
			if (vel.y < -1.1f) {
				vel.y = -1.1f;
			}
			vel.x *= (1.0f - 0.10f * delta);
			vel.z *= (1.0f - 0.10f * delta);
			if (sky != nullptr) {
				const float wind = 0.20f + 0.60f * sky->get_rain_amount();
				vel.x += std::sin(flight_t * 1.7f) * wind * delta;
				vel.z += std::cos(flight_t * 1.3f) * wind * 0.6f * delta;
			}
			Vector3 p = plane->get_global_position() + vel * delta;
			const float ground = [&]() {
				TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(root->find_child("Terrain", true, false));
				return (terrain != nullptr) ? terrain->get_terrain_height(p.x, p.z) : 0.0f;
			}();
			if (p.y <= ground + 0.04f) {
				p.y = ground + 0.04f;
				plane->set_global_position(p);
				plane->set_rotation(Vector3(0.0f, plane->get_rotation().y, 0.0f));
				active = false;
				rest_t = 0.001f;
				break;
			}
			plane->set_global_position(p);
			// 机头朝速度方向
			const Vector3 dir = vel.normalized();
			plane->set_rotation(Vector3(-std::asin(Math::clamp(dir.y, -1.0f, 1.0f)), std::atan2(dir.x, dir.z), 0.0f));
			if (flight_t > 14.0f) {
				active = false;
				rest_t = 0.001f;
			}
		} break;
		default:
			break;
	}
}

void PaperPlane::build() {
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

	// 一张小木凳与一叠便签
	meshutil::add_box(this, toon, Vector3(0.36f, 0.40f, 0.36f), Vector3(0.0f, 0.20f, 0.0f), Color(0.50f, 0.34f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.42f, 0.04f, 0.42f), Vector3(0.0f, 0.42f, 0.0f), Color(0.58f, 0.42f, 0.26f));
	meshutil::add_box(this, toon, Vector3(0.24f, 0.03f, 0.18f), Vector3(0.0f, 0.455f, 0.0f), Color(0.94f, 0.92f, 0.86f));

	// 纸飞机模型
	plane = memnew(Node3D);
	plane->set_name("Plane");
	plane->set_visible(false);
	add_child(plane);
	Ref<BoxMesh> wing;
	wing.instantiate();
	wing->set_size(Vector3(0.30f, 0.006f, 0.16f));
	wing->set_material(meshutil::toon_material(toon, Color(0.95f, 0.94f, 0.88f), 0.2f));
	for (int i = 0; i < 2; i++) {
		MeshInstance3D *w = memnew(MeshInstance3D);
		w->set_mesh(wing);
		w->set_position(Vector3((i == 0 ? -1.0f : 1.0f) * 0.10f, 0.0f, 0.0f));
		w->set_rotation(Vector3(0.0f, (i == 0 ? 1.0f : -1.0f) * 0.28f, (i == 0 ? 1.0f : -1.0f) * 0.12f));
		w->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		plane->add_child(w);
	}
	meshutil::add_box(plane, toon, Vector3(0.035f, 0.012f, 0.26f), Vector3(0.0f, 0.002f, 0.02f), Color(0.92f, 0.90f, 0.84f));
	meshutil::add_box(plane, toon, Vector3(0.10f, 0.006f, 0.05f), Vector3(0.0f, 0.03f, -0.12f), Color(0.90f, 0.88f, 0.82f));
}
