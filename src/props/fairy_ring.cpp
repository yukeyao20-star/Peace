#include "fairy_ring.h"

#include <cmath>

#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void FairyRing::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &FairyRing::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &FairyRing::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &FairyRing::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &FairyRing::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &FairyRing::interact);
	ClassDB::bind_method(D_METHOD("get_mushroom_count"), &FairyRing::get_mushroom_count);
	ClassDB::bind_method(D_METHOD("get_picked_count"), &FairyRing::get_picked_count);
	ClassDB::bind_method(D_METHOD("get_glow_visible"), &FairyRing::get_glow_visible);
	ClassDB::bind_method(D_METHOD("get_spores_on"), &FairyRing::get_spores_on);
}

String FairyRing::get_prompt() const {
	return String::utf8("按 E 采一朵蘑菇");
}

Vector3 FairyRing::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.25f, 0.0f);
}

void FairyRing::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	for (int i = 0; i < COUNT; i++) {
		if (!picked[i]) {
			picked[i] = true;
			if (mushrooms[i] != nullptr) {
				mushrooms[i]->set_visible(false);
			}
			picked_count++;
			break;
		}
	}
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("采了一朵小蘑菇 · 过一会儿还会冒出来"), 2.8f);
		}
	}
}

void FairyRing::_notification(int p_what) {
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
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			}
			bool night = false;
			float spore_want = 0.0f;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				night = tod < 0.24f || tod > 0.76f;
				if (tod > 0.20f && tod < 0.34f) {
					spore_want = std::sin(3.14159f * (tod - 0.20f) / 0.14f) * 0.8f;
				}
			}
			glow_visible = 0;
			for (int i = 0; i < COUNT; i++) {
				if (glow[i] == nullptr) {
					continue;
				}
				const bool vis = night && !picked[i];
				glow[i]->set_visible(vis);
				if (vis) {
					glow_visible++;
					const float pulse = 0.75f + 0.25f * std::sin(t * 1.6f + (float)i * 1.1f);
					glow[i]->set_scale(Vector3(pulse, pulse, pulse));
				}
			}
			spores_on = spore_want > 0.2f;
			if (spores != nullptr && spores->is_emitting() != spores_on) {
				spores->set_emitting(spores_on);
			}
			// 采走的蘑菇 25 秒后长回来一朵
			respawn_t += delta;
			if (respawn_t > 25.0f) {
				respawn_t = 0.0f;
				for (int i = 0; i < COUNT; i++) {
					if (picked[i]) {
						picked[i] = false;
						if (mushrooms[i] != nullptr) {
							mushrooms[i]->set_visible(true);
						}
						break;
					}
				}
			}
		} break;
		default:
			break;
	}
}

void FairyRing::build() {
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
	stem_mesh->set_top_radius(0.022f);
	stem_mesh->set_bottom_radius(0.030f);
	stem_mesh->set_height(0.13f);
	stem_mesh->set_radial_segments(7);
	stem_mesh->set_rings(1);
	stem_mesh->set_material(meshutil::toon_material(toon, Color(0.90f, 0.84f, 0.68f), 0.2f));

	Ref<SphereMesh> cap_mesh;
	cap_mesh.instantiate();
	cap_mesh->set_radius(0.115f);
	cap_mesh->set_height(0.23f);
	cap_mesh->set_radial_segments(12);
	cap_mesh->set_rings(6);
	cap_mesh->set_material(meshutil::toon_material(toon, Color(0.64f, 0.26f, 0.22f), 0.3f));

	Ref<StandardMaterial3D> glow_mat;
	glow_mat.instantiate();
	glow_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	glow_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	glow_mat->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	glow_mat->set_albedo(Color(1.0f, 0.72f, 0.42f, 0.35f));
	glow_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	Ref<SphereMesh> glow_mesh;
	glow_mesh.instantiate();
	glow_mesh->set_radius(0.15f);
	glow_mesh->set_height(0.30f);
	glow_mesh->set_radial_segments(10);
	glow_mesh->set_rings(5);
	glow_mesh->set_material(glow_mat);

	for (int i = 0; i < COUNT; i++) {
		const float a = (float)i * 0.7854f;
		const float rad = 1.25f + 0.12f * std::sin((float)i * 2.3f);
		Node3D *m = memnew(Node3D);
		m->set_name(String("Mushroom") + String::num_int64(i));
		m->set_position(Vector3(std::cos(a) * rad, 0.0f, std::sin(a) * rad));
		m->set_scale(Vector3(0.85f + 0.2f * std::sin((float)i), 1.0f, 0.85f + 0.2f * std::sin((float)i * 1.7f)));
		add_child(m);
		mushrooms[i] = m;

		MeshInstance3D *smi = memnew(MeshInstance3D);
		smi->set_mesh(stem_mesh);
		smi->set_position(Vector3(0.0f, 0.065f, 0.0f));
		m->add_child(smi);
		MeshInstance3D *cmi = memnew(MeshInstance3D);
		cmi->set_mesh(cap_mesh);
		cmi->set_position(Vector3(0.0f, 0.145f, 0.0f));
		cmi->set_scale(Vector3(1.0f, 0.55f, 1.0f));
		m->add_child(cmi);
		// 伞盖上的白点
		for (int s = 0; s < 3; s++) {
			const float sa = (float)s * 2.1f + (float)i;
			meshutil::add_sphere(m, toon, 0.018f,
					Vector3(std::cos(sa) * 0.06f, 0.165f, std::sin(sa) * 0.06f),
					Color(0.95f, 0.92f, 0.84f));
		}
		MeshInstance3D *g = memnew(MeshInstance3D);
		g->set_mesh(glow_mesh);
		g->set_position(Vector3(0.0f, 0.17f, 0.0f));
		g->set_scale(Vector3(1.0f, 0.6f, 1.0f));
		g->set_visible(false);
		g->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		m->add_child(g);
		glow[i] = g;
	}

	// 清晨的孢子
	Ref<SphereMesh> spore_mesh;
	spore_mesh.instantiate();
	spore_mesh->set_radius(0.010f);
	spore_mesh->set_height(0.020f);
	spore_mesh->set_radial_segments(4);
	spore_mesh->set_rings(2);
	Ref<StandardMaterial3D> sm;
	sm.instantiate();
	sm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	sm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	sm->set_albedo(Color(0.95f, 0.90f, 0.72f, 0.55f));
	sm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	spore_mesh->set_material(sm);
	spores = memnew(CPUParticles3D);
	spores->set_name("Spores");
	spores->set_amount(26);
	spores->set_lifetime(7.0f);
	spores->set_mesh(spore_mesh);
	spores->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	spores->set_emission_box_extents(Vector3(1.3f, 0.10f, 1.3f));
	spores->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	spores->set_spread(55.0f);
	spores->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.02f);
	spores->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.09f);
	spores->set_gravity(Vector3(0.02f, 0.015f, 0.01f));
	spores->set_position(Vector3(0.0f, 0.16f, 0.0f));
	spores->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	spores->set_emitting(false);
	add_child(spores);
}
