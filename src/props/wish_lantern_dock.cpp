#include "wish_lantern_dock.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

int WishLanternDock::get_active_count() const {
	int n = 0;
	for (const Released &r : released) {
		if (r.life > 0.0f && r.root != nullptr && r.root->is_visible()) {
			n++;
		}
	}
	return n;
}

float WishLanternDock::get_light_energy() const {
	for (const Released &r : released) {
		if (r.light != nullptr && r.root != nullptr && r.root->is_visible()) {
			return r.light->get_param(Light3D::PARAM_ENERGY);
		}
	}
	return 0.0f;
}

Vector3 WishLanternDock::get_released_pos(int p_index) const {
	if (p_index < 0 || p_index >= (int)released.size() || released[p_index].root == nullptr) {
		return Vector3();
	}
	return released[p_index].root->get_global_position();
}

void WishLanternDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &WishLanternDock::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &WishLanternDock::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &WishLanternDock::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &WishLanternDock::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &WishLanternDock::interact);
	ClassDB::bind_method(D_METHOD("get_active_count"), &WishLanternDock::get_active_count);
	ClassDB::bind_method(D_METHOD("get_light_energy"), &WishLanternDock::get_light_energy);
	ClassDB::bind_method(D_METHOD("get_released_pos", "index"), &WishLanternDock::get_released_pos);
}

void WishLanternDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			if (terrain != nullptr) {
				water_level = terrain->get_water_level();
			}
			add_to_group("interactables");
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (cooldown > 0.0f) {
				cooldown = Math::max(0.0f, cooldown - delta);
			}
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float on = Math::clamp((night - 0.25f) / 0.4f, 0.0f, 1.0f);
			for (Released &r : released) {
				if (r.root == nullptr || r.life <= 0.0f) {
					continue;
				}
				r.life -= delta;
				Vector3 p = r.root->get_global_position();
				p += r.dir * (0.22f * delta);
				p.y = water_level + 0.03f + std::sin(t * 1.6f + r.phase) * 0.025f;
				r.root->set_global_position(p);
				r.root->set_rotation(Vector3(std::sin(t * 1.1f + r.phase) * 0.03f, t * 0.25f + r.phase, std::sin(t * 0.9f + r.phase) * 0.03f));
				float flick = 0.9f + 0.08f * std::sin(t * 6.5f + r.phase);
				if (r.light != nullptr) {
					r.light->set_param(Light3D::PARAM_ENERGY, 0.34f * on * flick);
					r.light->set_visible(on > 0.03f);
				}
				if (r.mat.is_valid()) {
					r.mat->set("emission_energy_multiplier", 0.18f + 1.8f * on);
				}
				if (r.life <= 0.0f) {
					r.root->set_visible(false);
				}
			}
		} break;
		default:
			break;
	}
}

String WishLanternDock::get_prompt() const {
	return String::utf8("按 E 放一盏河灯");
}

Vector3 WishLanternDock::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.45f, 0.0f);
}

float WishLanternDock::get_interact_distance() const {
	return 2.6f;
}

void WishLanternDock::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (cooldown > 0.0f || get_active_count() >= 4) {
		return;
	}
	release();
	cooldown = 5.0f;
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("on_wish_lantern");
			gd->call("flash_hint", String::utf8("放下一盏河灯 · 愿今夜好梦"), 3.2f);
		}
	}
}

void WishLanternDock::release() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color paper_c(0.98f, 0.88f, 0.66f);
	const Color wood(0.38f, 0.24f, 0.12f);
	const Color warm(1.0f, 0.72f, 0.35f);

	Released r;
	r.root = memnew(Node3D);
	r.root->set_name(String("WishLantern") + String::num_int64((int)released.size()));
	add_child(r.root);
	Vector3 start(get_global_position().x, water_level + 0.03f, get_global_position().z + 1.0f);
	r.root->set_global_position(start);
	r.dir = Vector3(0.05f, 0.0f, 1.0f).normalized();
	r.life = 45.0f;
	r.phase = (float)(released.size() * 37 % 100) * 0.06f;

	Ref<BoxMesh> paper;
	paper.instantiate();
	paper->set_size(Vector3(0.24f, 0.26f, 0.24f));
	r.mat.instantiate();
	r.mat->set_albedo(paper_c);
	r.mat->set("emission_enabled", true);
	r.mat->set("emission", warm);
	r.mat->set("emission_energy_multiplier", 0.18f);
	paper->set_material(Ref<Material>(r.mat));
	r.paper = memnew(MeshInstance3D);
	r.paper->set_mesh(paper);
	r.paper->set_position(Vector3(0, 0.16f, 0));
	r.root->add_child(r.paper);

	meshutil::add_box(r.root, toon, Vector3(0.27f, 0.03f, 0.27f), Vector3(0, 0.30f, 0), wood);
	meshutil::add_box(r.root, toon, Vector3(0.27f, 0.03f, 0.27f), Vector3(0, 0.03f, 0), wood);
	Ref<SphereMesh> flame;
	flame.instantiate();
	flame->set_radius(0.035f);
	flame->set_height(0.07f);
	flame->set_radial_segments(6);
	flame->set_rings(4);
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fm->set_albedo(Color(1.0f, 0.85f, 0.45f));
	flame->set_material(fm);
	MeshInstance3D *fl = memnew(MeshInstance3D);
	fl->set_mesh(flame);
	fl->set_position(Vector3(0, 0.14f, 0));
	r.root->add_child(fl);

	r.light = memnew(OmniLight3D);
	r.light->set_name("WishLight");
	r.light->set_position(Vector3(0, 0.18f, 0));
	r.light->set_color(Color(1.0f, 0.70f, 0.36f));
	r.light->set_param(Light3D::PARAM_RANGE, 3.4f);
	r.light->set_param(Light3D::PARAM_ENERGY, 0.0f);
	r.light->set_visible(false);
	r.root->add_child(r.light);

	released.push_back(r);
}

void WishLanternDock::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color paper_c(0.94f, 0.86f, 0.66f);
	const Color wood(0.40f, 0.27f, 0.14f);
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.22f, 0.17f, 0.22f), Vector3(0.0f, 0.09f + (float)i * 0.17f, 0.0f), paper_c);
		meshutil::add_box(this, toon, Vector3(0.25f, 0.025f, 0.25f), Vector3(0.0f, 0.18f + (float)i * 0.17f, 0.0f), wood);
	}
}
