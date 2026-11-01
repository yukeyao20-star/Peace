#include "fairy_ring.h"

#include <cmath>

#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
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
			update_mushroom(i);
			update_glow(i, false);
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
				const bool vis = night && !picked[i];
				update_glow(i, vis);
				if (vis) glow_visible++;
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
						update_mushroom(i);
						update_glow(i, false);
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

	// 优化（第 190 轮）：菌柄、伞盖、白点、光晕各合成一个 MultiMesh。
	Ref<SphereMesh> spot_mesh;
	spot_mesh.instantiate();
	spot_mesh->set_radius(0.018f);
	spot_mesh->set_height(0.036f);
	spot_mesh->set_radial_segments(6);
	spot_mesh->set_rings(3);
	spot_mesh->set_material(meshutil::toon_material(toon, Color(0.95f, 0.92f, 0.84f), 0.2f));
	stem_mm.instantiate();
	stem_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	stem_mm->set_mesh(stem_mesh);
	stem_mm->set_instance_count(COUNT);
	cap_mm.instantiate();
	cap_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	cap_mm->set_mesh(cap_mesh);
	cap_mm->set_instance_count(COUNT);
	spot_mm.instantiate();
	spot_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	spot_mm->set_mesh(spot_mesh);
	spot_mm->set_instance_count(COUNT * 3);
	glow_mm.instantiate();
	glow_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	glow_mm->set_mesh(glow_mesh);
	glow_mm->set_instance_count(COUNT);
	MultiMeshInstance3D *stem_mi = memnew(MultiMeshInstance3D);
	stem_mi->set_name("Stems");
	stem_mi->set_multimesh(stem_mm);
	add_child(stem_mi);
	MultiMeshInstance3D *cap_mi = memnew(MultiMeshInstance3D);
	cap_mi->set_name("Caps");
	cap_mi->set_multimesh(cap_mm);
	add_child(cap_mi);
	MultiMeshInstance3D *spot_mi = memnew(MultiMeshInstance3D);
	spot_mi->set_name("Spots");
	spot_mi->set_multimesh(spot_mm);
	add_child(spot_mi);
	MultiMeshInstance3D *glow_mi = memnew(MultiMeshInstance3D);
	glow_mi->set_name("Glows");
	glow_mi->set_multimesh(glow_mm);
	glow_mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(glow_mi);
	for (int i = 0; i < COUNT; i++) {
		const float a = (float)i * 0.7854f;
		const float rad = 1.25f + 0.12f * std::sin((float)i * 2.3f);
		mush_pos[i] = Vector3(std::cos(a) * rad, 0.0f, std::sin(a) * rad);
		mush_sx[i] = 0.85f + 0.2f * std::sin((float)i);
		mush_sz[i] = 0.85f + 0.2f * std::sin((float)i * 1.7f);
		update_mushroom(i);
		update_glow(i, false);
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

void FairyRing::update_mushroom(int p_index) {
	const Basis hull = Basis().scaled(Vector3(mush_sx[p_index], 1.0f, mush_sz[p_index]));
	if (picked[p_index]) {
		const Transform3D hidden(Basis().scaled(Vector3(0.0f, 0.0f, 0.0f)), mush_pos[p_index]);
		stem_mm->set_instance_transform(p_index, hidden);
		cap_mm->set_instance_transform(p_index, hidden);
		for (int sp = 0; sp < 3; sp++) spot_mm->set_instance_transform(p_index * 3 + sp, hidden);
		return;
	}
	const Transform3D h(hull, mush_pos[p_index]);
	stem_mm->set_instance_transform(p_index, h * Transform3D(Basis(), Vector3(0.0f, 0.065f, 0.0f)));
	cap_mm->set_instance_transform(p_index, h * Transform3D(Basis().scaled(Vector3(1.0f, 0.55f, 1.0f)), Vector3(0.0f, 0.145f, 0.0f)));
	for (int sp = 0; sp < 3; sp++) {
		const float sa = (float)sp * 2.1f + (float)p_index;
		spot_mm->set_instance_transform(p_index * 3 + sp, h * Transform3D(Basis(), Vector3(std::cos(sa) * 0.06f, 0.165f, std::sin(sa) * 0.06f)));
	}
}

void FairyRing::update_glow(int p_index, bool p_on) {
	if (!glow_mm.is_valid()) return;
	if (!p_on) {
		glow_mm->set_instance_transform(p_index, Transform3D(Basis().scaled(Vector3(0.0f, 0.0f, 0.0f)), mush_pos[p_index]));
		return;
	}
	const float pulse = 0.75f + 0.25f * std::sin(t * 1.6f + (float)p_index * 1.1f);
	const Basis hull = Basis().scaled(Vector3(mush_sx[p_index], 1.0f, mush_sz[p_index]));
	const Transform3D h(hull, mush_pos[p_index]);
	const Basis g = Basis().scaled(Vector3(pulse, pulse * 0.6f, pulse));
	glow_mm->set_instance_transform(p_index, h * Transform3D(g, Vector3(0.0f, 0.17f, 0.0f)));
}
