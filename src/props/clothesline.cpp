#include "clothesline.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Clothesline::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Clothesline::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Clothesline::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Clothesline::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Clothesline::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Clothesline::interact);
	ClassDB::bind_method(D_METHOD("get_clothes_hung"), &Clothesline::get_clothes_hung);
	ClassDB::bind_method(D_METHOD("get_wet_level"), &Clothesline::get_wet_level);
	ClassDB::bind_method(D_METHOD("get_snow_level"), &Clothesline::get_snow_level);
	ClassDB::bind_method(D_METHOD("get_drips_emitting"), &Clothesline::get_drips_emitting);
}

bool Clothesline::get_drips_emitting() const {
	return drips != nullptr && drips->is_emitting();
}

String Clothesline::get_prompt() const {
	return hung ? String::utf8("按 E 把衣服收进木盆") : String::utf8("按 E 把衣服晾回绳上");
}

Vector3 Clothesline::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.2f, 0.0f);
}

void Clothesline::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	hung = !hung;
	for (int i = 0; i < GARMENT_COUNT; i++) {
		if (hangers[i] != nullptr) {
			hangers[i]->set_visible(hung);
		}
	}
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) {
			gd->call("flash_hint", hung ? String::utf8("衣服晾回绳上 · 风一吹轻轻晃")
										: String::utf8("把衣服收进木盆 · 等天晴再晾"),
					3.0f);
		}
	}
}

float Clothesline::rope_y(float p_x) const {
	const float k = p_x / 1.5f;
	return 1.50f - 0.16f * (1.0f - k * k);
}

void Clothesline::_notification(int p_what) {
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

			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (sky == nullptr) {
				sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			}
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			wet = Math::move_toward(wet, Math::clamp(rain, 0.0f, 1.0f), delta * 0.35f);
			snow_lvl = Math::move_toward(snow_lvl, Math::clamp(snow, 0.0f, 1.0f), delta * 0.22f);

			// 衣物随风轻摆：挂点不动，底边摆幅最大。雪天布料变僵，摆得更小。
			const float sway = 0.085f * (1.0f - 0.5f * snow_lvl);
			for (int i = 0; i < GARMENT_COUNT; i++) {
				if (hangers[i] == nullptr || !hangers[i]->is_visible()) {
					continue;
				}
				const float ph = phases[i];
				hangers[i]->set_rotation(Vector3(
						std::sin(t * 1.15f + ph) * sway + std::sin(t * 2.7f + ph * 1.7f) * sway * 0.30f,
						std::sin(t * 0.53f + ph * 0.6f) * sway * 0.35f,
						std::sin(t * 0.87f + ph * 1.3f) * sway * 0.55f));
			}

			// 淋湿的布料颜色加深，衣角开始滴水。
			if (std::fabs(wet - last_wet_draw) > 0.008f) {
				last_wet_draw = wet;
				for (int i = 0; i < GARMENT_COUNT; i++) {
					if (cloth_mats[i].is_valid()) {
						cloth_mats[i]->set_shader_parameter("albedo", cloth_dry[i].lerp(cloth_dry[i].darkened(0.42f), wet));
					}
				}
			}
			const bool drip_on = hung && wet > 0.35f;
			if (drips != nullptr && drips->is_emitting() != drip_on) {
				drips->set_emitting(drip_on);
			}
			if (drips != nullptr) {
				drips->set_global_position(get_global_position() + Vector3(0.0f, 0.85f, 0.0f));
			}

			// 雪落在绳上：一节一节的薄雪帽。
			if (snow_multimesh.is_valid()) {
				snow_multimesh->set_visible_instance_count(snow_lvl > 0.42f ? ROPE_SEGMENTS + 1 : 0);
			}
		} break;
		default:
			break;
	}
}

void Clothesline::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();

	// 落地：按地形抬到草面上
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(
				get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}

	const Color wood(0.48f, 0.33f, 0.19f);
	meshutil::add_box(this, toon, Vector3(0.09f, 1.62f, 0.09f), Vector3(-1.5f, 0.81f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.09f, 1.62f, 0.09f), Vector3(1.5f, 0.81f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.16f, 0.05f, 0.16f), Vector3(-1.5f, 1.60f, 0.0f), Color(0.40f, 0.27f, 0.16f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.05f, 0.16f), Vector3(1.5f, 1.60f, 0.0f), Color(0.40f, 0.27f, 0.16f));

	// 麻绳：11 个采样点，10 段小圆柱拼成下垂的弧
	rope_mat = meshutil::toon_material(toon, Color(0.62f, 0.54f, 0.38f), 0.2f);
	Ref<CylinderMesh> seg_mesh;
	seg_mesh.instantiate();
	seg_mesh->set_top_radius(0.010f);
	seg_mesh->set_bottom_radius(0.010f);
	seg_mesh->set_height(1.0f);
	seg_mesh->set_radial_segments(5);
	seg_mesh->set_rings(1);
	seg_mesh->set_material(rope_mat);

	snow_mat = meshutil::toon_material(toon, Color(0.90f, 0.95f, 1.0f), 0.15f);
	Ref<BoxMesh> snow_mesh;
	snow_mesh.instantiate();
	snow_mesh->set_size(Vector3(0.032f, 0.012f, 0.032f));
	snow_mesh->set_material(snow_mat);

	rope_multimesh.instantiate();
	rope_multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	rope_multimesh->set_mesh(seg_mesh);
	rope_multimesh->set_instance_count(ROPE_SEGMENTS);
	snow_multimesh.instantiate();
	snow_multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	snow_multimesh->set_mesh(snow_mesh);
	snow_multimesh->set_instance_count(ROPE_SEGMENTS + 1);

	Vector3 pts[ROPE_SEGMENTS + 1];
	for (int i = 0; i <= ROPE_SEGMENTS; i++) {
		const float x = -1.5f + 3.0f * (float)i / (float)ROPE_SEGMENTS;
		pts[i] = Vector3(x, rope_y(x), 0.0f);
		snow_multimesh->set_instance_transform(i, Transform3D(Basis(), pts[i] + Vector3(0.0f, 0.014f, 0.0f)));
	}
	for (int i = 0; i < ROPE_SEGMENTS; i++) {
		const Vector3 d = pts[i + 1] - pts[i];
		const float len = d.length();
		const float ang = std::atan2(-d.x, d.y);
		const Basis b = Basis(Vector3(0.0f, 0.0f, 1.0f), ang).scaled(Vector3(1.0f, len, 1.0f));
		rope_multimesh->set_instance_transform(i, Transform3D(b, (pts[i] + pts[i + 1]) * 0.5f));
	}
	rope_mm = memnew(MultiMeshInstance3D);
	rope_mm->set_name("Rope");
	rope_mm->set_multimesh(rope_multimesh);
	rope_mm->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(rope_mm);

	snow_mm = memnew(MultiMeshInstance3D);
	snow_mm->set_name("RopeSnow");
	snow_mm->set_multimesh(snow_multimesh);
	snow_mm->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(snow_mm);
	snow_multimesh->set_visible_instance_count(0);

	// 四件衣物
	const float xs[GARMENT_COUNT] = { -0.95f, -0.30f, 0.35f, 0.95f };
	const Color colors[GARMENT_COUNT] = {
		Color(0.92f, 0.90f, 0.84f), // 白衬衫
		Color(0.55f, 0.72f, 0.82f), // 蓝毛巾
		Color(0.88f, 0.72f, 0.35f), // 米黄小巾
		Color(0.70f, 0.22f, 0.18f), // 红围巾
	};
	for (int i = 0; i < GARMENT_COUNT; i++) {
		line_x[i] = xs[i];
		phases[i] = (float)i * 1.37f;
		cloth_dry[i] = colors[i];
		cloth_mats[i] = meshutil::toon_material(toon, colors[i], 0.25f);

		Node3D *h = memnew(Node3D);
		h->set_name(String("Garment") + String::num_int64(i));
		h->set_position(Vector3(xs[i], rope_y(xs[i]) - 0.006f, 0.0f));
		add_child(h);
		hangers[i] = h;

		auto part = [&](const Vector3 &size, const Vector3 &pos) {
			Ref<BoxMesh> bm;
			bm.instantiate();
			bm->set_size(size);
			bm->set_material(cloth_mats[i]);
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(bm);
			mi->set_position(pos);
			mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			h->add_child(mi);
		};

		if (i == 0) { // 衬衫
			part(Vector3(0.38f, 0.42f, 0.03f), Vector3(0.0f, -0.235f, 0.0f));
			part(Vector3(0.11f, 0.26f, 0.03f), Vector3(-0.235f, -0.16f, 0.0f));
			part(Vector3(0.11f, 0.26f, 0.03f), Vector3(0.235f, -0.16f, 0.0f));
		} else if (i == 1) { // 大毛巾
			part(Vector3(0.44f, 0.56f, 0.026f), Vector3(0.0f, -0.30f, 0.0f));
		} else if (i == 2) { // 小方巾
			part(Vector3(0.30f, 0.40f, 0.024f), Vector3(0.0f, -0.22f, 0.0f));
		} else { // 围巾
			part(Vector3(0.10f, 0.54f, 0.024f), Vector3(0.0f, -0.29f, 0.0f));
			part(Vector3(0.22f, 0.09f, 0.022f), Vector3(0.07f, -0.53f, 0.0f));
		}

		// 两只木夹子
		meshutil::add_box(h, toon, Vector3(0.028f, 0.075f, 0.030f), Vector3(-0.10f, -0.010f, 0.012f), Color(0.60f, 0.44f, 0.26f));
		meshutil::add_box(h, toon, Vector3(0.028f, 0.075f, 0.030f), Vector3(0.10f, -0.010f, 0.012f), Color(0.60f, 0.44f, 0.26f));
	}

	// 衣角滴水
	Ref<SphereMesh> drop;
	drop.instantiate();
	drop->set_radius(0.011f);
	drop->set_height(0.022f);
	drop->set_radial_segments(5);
	drop->set_rings(3);
	Ref<StandardMaterial3D> dm;
	dm.instantiate();
	dm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dm->set_albedo(Color(0.72f, 0.86f, 0.95f, 0.7f));
	dm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	drop->set_material(dm);
	drips = memnew(CPUParticles3D);
	drips->set_name("Drips");
	drips->set_amount(26);
	drips->set_lifetime(1.4f);
	drips->set_mesh(drop);
	drips->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	drips->set_emission_box_extents(Vector3(1.35f, 0.18f, 0.10f));
	drips->set_direction(Vector3(0.0f, -1.0f, 0.0f));
	drips->set_spread(8.0f);
	drips->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.05f);
	drips->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.20f);
	drips->set_gravity(Vector3(0.0f, -2.8f, 0.0f));
	drips->set_param_min(CPUParticles3D::PARAM_SCALE, 0.8f);
	drips->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f);
	drips->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	drips->set_emitting(false);
	add_child(drips);
}
