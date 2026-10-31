#include "floating_lanterns.h"

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

int FloatingLanterns::get_lit_count() const {
	int n = 0;
	for (const Lantern &l : lanterns) {
		if (l.light != nullptr && l.light->is_visible()) {
			n++;
		}
	}
	return n;
}

float FloatingLanterns::get_light_energy() const {
	for (const Lantern &l : lanterns) {
		if (l.light != nullptr) {
			return l.light->get_param(Light3D::PARAM_ENERGY);
		}
	}
	return 0.0f;
}

Vector3 FloatingLanterns::get_lantern_pos(int p_index) const {
	if (p_index < 0 || p_index >= (int)lanterns.size() || lanterns[p_index].root == nullptr) {
		return Vector3();
	}
	return lanterns[p_index].root->get_global_position();
}

void FloatingLanterns::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_lantern_count"), &FloatingLanterns::get_lantern_count);
	ClassDB::bind_method(D_METHOD("get_lit_count"), &FloatingLanterns::get_lit_count);
	ClassDB::bind_method(D_METHOD("get_light_energy"), &FloatingLanterns::get_light_energy);
	ClassDB::bind_method(D_METHOD("get_lantern_pos", "index"), &FloatingLanterns::get_lantern_pos);
}

void FloatingLanterns::_notification(int p_what) {
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
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float on = Math::clamp((night - 0.25f) / 0.4f, 0.0f, 1.0f) * (1.0f - 0.55f * rain);
			for (Lantern &l : lanterns) {
				if (l.root == nullptr) {
					continue;
				}
				l.phase += l.speed * delta;
				Vector3 p = l.anchor + Vector3(
						std::cos(l.phase) * l.radius,
						std::sin(t * 1.4f + l.bob_phase) * 0.025f,
						std::sin(l.phase * 0.8f) * l.radius * 0.7f);
				p.y = water_level + 0.035f + std::sin(t * 1.4f + l.bob_phase) * 0.025f;
				l.root->set_global_position(p);
				l.root->set_rotation(Vector3(
						std::sin(t * 0.9f + l.bob_phase) * 0.04f,
						l.phase * 0.6f,
						std::sin(t * 1.1f + l.bob_phase) * 0.035f));
				if (l.light != nullptr) {
					float flick = 0.88f + 0.08f * std::sin(t * 6.0f + l.bob_phase) + 0.04f * std::sin(t * 15.0f + l.bob_phase);
					l.light->set_param(Light3D::PARAM_ENERGY, 0.30f * on * flick);
					l.light->set_visible(on > 0.03f);
				}
				if (l.mat.is_valid()) {
					l.mat->set("emission_energy_multiplier", 0.18f + 1.7f * on);
				}
			}
		} break;
		default:
			break;
	}
}

void FloatingLanterns::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color paper_c(0.98f, 0.88f, 0.66f);
	const Color wood(0.38f, 0.24f, 0.12f);
	const Color warm(1.0f, 0.72f, 0.35f);

	// 浅水湾里挑六个点
	const float spots[6][2] = {
		{-11.5f, -6.0f}, {-9.6f, -4.5f}, {-12.8f, -8.5f},
		{-7.4f, -5.2f}, {-5.2f, -4.0f}, {-10.2f, -9.5f},
	};
	for (int i = 0; i < 6; i++) {
		Vector3 anchor(spots[i][0], water_level, spots[i][1]);
		if (terrain != nullptr && terrain->water_depth_at(anchor.x, anchor.z) < 0.5f) {
			// 水太浅就往外挪一点，保证灯漂得起来
			anchor.z += 1.5f;
		}
		Lantern l;
		l.anchor = anchor;
		l.phase = (float)(rng() % 628u) * 0.01f;
		l.radius = 0.35f + (float)(rng() % 100u) * 0.003f;
		l.speed = 0.10f + (float)(rng() % 100u) * 0.0008f;
		l.bob_phase = (float)(rng() % 628u) * 0.01f;

		l.root = memnew(Node3D);
		l.root->set_name(String("FloatingLantern") + String::num_int64(i));
		add_child(l.root);
		l.root->set_global_position(anchor);

		// 纸灯身
		Ref<BoxMesh> paper;
		paper.instantiate();
		paper->set_size(Vector3(0.24f, 0.26f, 0.24f));
		l.mat.instantiate();
		l.mat->set_albedo(paper_c);
		l.mat->set("emission_enabled", true);
		l.mat->set("emission", warm);
		l.mat->set("emission_energy_multiplier", 0.18f);
		paper->set_material(Ref<Material>(l.mat));
		l.paper = memnew(MeshInstance3D);
		l.paper->set_name("Paper");
		l.paper->set_mesh(paper);
		l.paper->set_position(Vector3(0, 0.16f, 0));
		l.root->add_child(l.paper);

		// 上下木盖
		meshutil::add_box(l.root, toon, Vector3(0.27f, 0.03f, 0.27f), Vector3(0, 0.30f, 0), wood);
		meshutil::add_box(l.root, toon, Vector3(0.27f, 0.03f, 0.27f), Vector3(0, 0.03f, 0), wood);
		// 灯芯
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
		l.root->add_child(fl);

		l.light = memnew(OmniLight3D);
		l.light->set_name("LanternLight");
		l.light->set_position(Vector3(0, 0.18f, 0));
		l.light->set_color(Color(1.0f, 0.70f, 0.36f));
		l.light->set_param(Light3D::PARAM_RANGE, 3.2f);
		l.light->set_param(Light3D::PARAM_ENERGY, 0.0f);
		l.light->set_visible(false);
		l.root->add_child(l.light);

		lanterns.push_back(l);
	}
}
