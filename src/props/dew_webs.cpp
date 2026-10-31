#include "dew_webs.h"

#include <cmath>

#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
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

void DewWebs::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_web_count"), &DewWebs::get_web_count);
	ClassDB::bind_method(D_METHOD("get_dew_visible"), &DewWebs::get_dew_visible);
	ClassDB::bind_method(D_METHOD("get_spider_out"), &DewWebs::get_spider_out);
	ClassDB::bind_method(D_METHOD("get_webs_visible"), &DewWebs::get_webs_visible);
}

void DewWebs::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
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
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}
			if (sky == nullptr) {
				sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			}

			float rain = 0.0f;
			float target_dew = 0.0f;
			if (sky != nullptr) {
				rain = sky->get_rain_amount();
				const float tod = sky->get_time_of_day();
				if (tod > 0.22f && tod < 0.38f) {
					target_dew = std::sin(3.14159f * (tod - 0.22f) / 0.16f);
				}
				target_dew *= Math::clamp(1.0f - rain, 0.0f, 1.0f);
			}
			dew = Math::move_toward(dew, Math::clamp(target_dew, 0.0f, 1.0f), delta * 0.5f);

			// 暴雨打烂网，雨停后重新织
			const bool want_web = rain < 0.55f;
			if (want_web != webs_visible) {
				webs_visible = want_web;
				for (int i = 0; i < WEB_COUNT; i++) {
					if (webs[i] != nullptr) {
						webs[i]->set_visible(want_web);
					}
				}
				if (spider != nullptr) {
					spider->set_visible(want_web);
				}
			}

			// 风里轻晃
			for (int i = 0; i < WEB_COUNT; i++) {
				if (webs[i] != nullptr) {
					webs[i]->set_rotation(Vector3(
							std::sin(t * 0.9f + (float)i) * 0.02f,
							0.0f,
							std::sin(t * 1.3f + (float)i * 1.7f) * 0.025f));
				}
			}

			// 露珠
			dew_visible = 0;
			for (int w = 0; w < WEB_COUNT; w++) {
				for (int d = 0; d < DEW_PER_WEB; d++) {
					MeshInstance3D *drop = dewdrops[w][d];
					if (drop == nullptr) {
						continue;
					}
					const bool vis = webs_visible && dew > 0.08f;
					drop->set_visible(vis);
					if (vis) {
						const float s = 0.4f + 0.6f * dew;
						drop->set_scale(Vector3(s, s, s));
						dew_visible++;
					}
				}
			}

			// 蜘蛛：人靠近缩到网角
			spider_out = false;
			if (spider != nullptr && webs_visible) {
				float d = 999.0f;
				if (player != nullptr) {
					const Vector3 wp = webs[0] != nullptr ? webs[0]->get_global_position() : get_global_position();
					d = player->get_global_position().distance_to(wp);
				}
				const Vector3 target = (d < 2.2f) ? Vector3(0.27f, 0.24f, 0.02f) : Vector3(0.0f, 0.0f, 0.02f);
				spider->set_position(spider->get_position().lerp(target, Math::min(1.0f, delta * 4.0f)));
				spider_out = d >= 2.2f;
				spider->set_rotation(Vector3(0.0f, 0.0f,
						std::sin(t * 0.8f) * 0.05f + (d < 2.2f ? 0.3f : 0.0f)));
			}
		} break;
		default:
			break;
	}
}

void DewWebs::build() {
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

	Ref<StandardMaterial3D> web_mat;
	web_mat.instantiate();
	web_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	web_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	web_mat->set_albedo(Color(0.92f, 0.95f, 0.98f, 0.38f));
	web_mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	web_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);

	Ref<CylinderMesh> spoke;
	spoke.instantiate();
	spoke->set_top_radius(0.003f);
	spoke->set_bottom_radius(0.003f);
	spoke->set_height(1.0f);
	spoke->set_radial_segments(4);
	spoke->set_rings(1);
	spoke->set_material(web_mat);

	Ref<StandardMaterial3D> dew_mat;
	dew_mat.instantiate();
	dew_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dew_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dew_mat->set_albedo(Color(0.82f, 0.93f, 1.0f, 0.85f));
	dew_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	Ref<SphereMesh> dew_mesh;
	dew_mesh.instantiate();
	dew_mesh->set_radius(0.008f);
	dew_mesh->set_height(0.016f);
	dew_mesh->set_radial_segments(4);
	dew_mesh->set_rings(2);
	dew_mesh->set_material(dew_mat);

	const Vector3 wpos[WEB_COUNT] = {
		Vector3(0.0f, 1.05f, 0.0f),
		Vector3(0.52f, 0.86f, 0.10f),
		Vector3(-0.48f, 1.16f, 0.06f),
	};
	const float wradius[WEB_COUNT] = { 0.34f, 0.27f, 0.24f };

	for (int w = 0; w < WEB_COUNT; w++) {
		Node3D *web = memnew(Node3D);
		web->set_name(String("Web") + String::num_int64(w));
		web->set_position(wpos[w]);
		web->set_rotation(Vector3(0.0f, (float)w * 0.35f - 0.2f, 0.0f));
		add_child(web);
		webs[w] = web;

		for (int s = 0; s < 8; s++) {
			const float a = (float)s * 0.7854f;
			const float len = wradius[w];
			const Basis b(Vector3(0.0f, 0.0f, 1.0f), a - 1.5707964f);
			const Basis sb = b.scaled(Vector3(1.0f, len, 1.0f));
			MeshInstance3D *sp = memnew(MeshInstance3D);
			sp->set_mesh(spoke);
			sp->set_transform(Transform3D(sb, Vector3(std::cos(a) * len * 0.5f, std::sin(a) * len * 0.5f, 0.0f)));
			sp->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			web->add_child(sp);
		}
		for (int r = 1; r <= 3; r++) {
			Ref<TorusMesh> ring;
			ring.instantiate();
			const float rad = wradius[w] * (float)r / 3.0f;
			ring->set_inner_radius(rad - 0.0025f);
			ring->set_outer_radius(rad + 0.0025f);
			ring->set_rings(20);
			ring->set_ring_segments(3);
			ring->set_material(web_mat);
			MeshInstance3D *rm = memnew(MeshInstance3D);
			rm->set_mesh(ring);
			rm->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
			rm->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			web->add_child(rm);
		}
		// 露珠分布在网线上
		for (int d = 0; d < DEW_PER_WEB; d++) {
			const float a = (float)d * 0.9f + (float)w * 0.7f;
			const float rad = wradius[w] * (0.25f + 0.7f * (float)((d + w) % 3) / 2.0f);
			MeshInstance3D *dm = memnew(MeshInstance3D);
			dm->set_mesh(dew_mesh);
			dm->set_position(Vector3(std::cos(a) * rad, std::sin(a) * rad, 0.006f));
			dm->set_visible(false);
			dm->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			web->add_child(dm);
			dewdrops[w][d] = dm;
		}
	}

	// 小蜘蛛
	spider = memnew(Node3D);
	spider->set_name("Spider");
	spider->set_position(Vector3(0.0f, 0.0f, 0.02f));
	webs[0]->add_child(spider);
	meshutil::add_sphere(spider, toon, 0.030f, Vector3(0.0f, 0.0f, 0.0f), Color(0.16f, 0.12f, 0.10f));
	meshutil::add_sphere(spider, toon, 0.042f, Vector3(0.0f, -0.035f, 0.0f), Color(0.22f, 0.15f, 0.11f));
	for (int l = 0; l < 8; l++) {
		const float a = (float)(l % 4) * 0.55f + 1.5707964f;
		const float sgn = (l < 4) ? -1.0f : 1.0f;
		meshutil::add_box(spider, toon, Vector3(0.006f, 0.075f, 0.006f),
				Vector3(std::cos(a) * 0.045f * sgn, -0.01f, std::sin(a) * 0.045f),
				Color(0.18f, 0.13f, 0.10f), Vector3(0.0f, a, sgn * 0.7f));
	}
}
