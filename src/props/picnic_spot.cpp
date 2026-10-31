#include "picnic_spot.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
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

void PicnicSpot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_butterflies_visible"), &PicnicSpot::get_butterflies_visible);
	ClassDB::bind_method(D_METHOD("get_item_count"), &PicnicSpot::get_item_count);
	ClassDB::bind_method(D_METHOD("get_breeze_amp"), &PicnicSpot::get_breeze_amp);
}

void PicnicSpot::_notification(int p_what) {
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
			SkyCycle *sky = (get_tree() != nullptr)
					? Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false))
					: nullptr;
			bool day = true;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				day = tod > 0.28f && tod < 0.72f;
				breeze_amp = 0.05f + 0.10f * Math::clamp(sky->get_rain_amount() * 0.5f + 0.2f, 0.0f, 1.0f);
				butterflies_visible = (day && sky->get_snow_amount() < 0.2f && sky->get_rain_amount() < 0.3f)
						? BUTTERFLY_COUNT
						: 0;
			} else {
				butterflies_visible = BUTTERFLY_COUNT;
			}
			for (int i = 0; i < 2; i++) {
				if (napkins[i] != nullptr) {
					napkins[i]->set_rotation(Vector3(
							std::sin(t * 2.1f + (float)i) * breeze_amp,
							0.0f,
							std::sin(t * 1.7f + (float)i * 2.0f) * breeze_amp * 0.6f));
				}
			}
			for (int i = 0; i < BUTTERFLY_COUNT; i++) {
				if (butterflies[i] == nullptr) {
					continue;
				}
				const bool vis = i < butterflies_visible;
				butterflies[i]->set_visible(vis);
				if (!vis) {
					continue;
				}
				const float a = t * 0.9f + (float)i * 2.6f;
				butterflies[i]->set_position(Vector3(0.55f + std::cos(a) * 0.55f,
						0.55f + 0.12f * std::sin(t * 2.0f + (float)i),
						0.35f + std::sin(a) * 0.40f));
				butterflies[i]->set_rotation(Vector3(0.0f, -a, 0.0f));
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = std::sin(t * 12.0f + (float)i * 1.2f) * 0.9f;
						wings[i][w]->set_rotation(Vector3(0.0f, (w == 0 ? 1.0f : -1.0f) * flap, 0.0f));
					}
				}
			}
		} break;
		default:
			break;
	}
}

void PicnicSpot::build() {
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

	// 红白格子野餐毯：4x3 的小块拼起来
	const Color red(0.74f, 0.26f, 0.22f);
	const Color cream(0.94f, 0.90f, 0.80f);
	for (int ix = 0; ix < 4; ix++) {
		for (int iz = 0; iz < 3; iz++) {
			const Color c = ((ix + iz) % 2 == 0) ? red : cream;
			meshutil::add_box(this, toon, Vector3(0.40f, 0.018f, 0.40f),
					Vector3(-0.60f + 0.40f * (float)ix, 0.009f, -0.40f + 0.40f * (float)iz), c);
		}
	}
	item_count = 12;

	// 两块翘起的餐巾（风一吹轻轻动）
	for (int i = 0; i < 2; i++) {
		Ref<PlaneMesh> nap;
		nap.instantiate();
		nap->set_size(Vector2(0.34f, 0.28f));
		Ref<StandardMaterial3D> nm;
		nm.instantiate();
		nm->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
		nm->set_albedo(Color(0.95f, 0.93f, 0.86f));
		nm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		nap->set_material(nm);
		Node3D *g = memnew(Node3D);
		g->set_name(String("Napkin") + String::num_int64(i));
		g->set_position(Vector3(-0.84f + 1.68f * (float)i, 0.04f, 0.62f));
		add_child(g);
		napkins[i] = g;
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(nap);
		mi->set_rotation(Vector3(-1.5707964f, 0.0f, 0.0f));
		mi->set_position(Vector3(0.0f, 0.0f, 0.14f));
		mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		g->add_child(mi);
	}

	// 藤篮
	Node3D *b = memnew(Node3D);
	b->set_name("Basket");
	b->set_position(Vector3(0.52f, 0.0f, -0.18f));
	add_child(b);
	meshutil::add_box(b, toon, Vector3(0.42f, 0.24f, 0.30f), Vector3(0.0f, 0.13f, 0.0f), Color(0.66f, 0.48f, 0.26f));
	meshutil::add_box(b, toon, Vector3(0.44f, 0.04f, 0.32f), Vector3(0.0f, 0.26f, 0.0f), Color(0.56f, 0.40f, 0.22f));
	Ref<TorusMesh> handle;
	handle.instantiate();
	handle->set_inner_radius(0.12f);
	handle->set_outer_radius(0.14f);
	handle->set_rings(16);
	handle->set_ring_segments(4);
	handle->set_material(meshutil::toon_material(toon, Color(0.58f, 0.40f, 0.22f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(handle);
	hmi->set_position(Vector3(0.0f, 0.27f, 0.0f));
	b->add_child(hmi);
	// 篮口露出的布
	meshutil::add_box(b, toon, Vector3(0.34f, 0.04f, 0.24f), Vector3(0.0f, 0.27f, 0.0f), Color(0.90f, 0.86f, 0.74f));

	// 面包、苹果、两只盘子
	meshutil::add_box(this, toon, Vector3(0.24f, 0.08f, 0.13f), Vector3(0.20f, 0.07f, 0.34f), Color(0.78f, 0.56f, 0.30f));
	meshutil::add_sphere(this, toon, 0.055f, Vector3(-0.30f, 0.065f, 0.30f), Color(0.78f, 0.20f, 0.16f));
	meshutil::add_sphere(this, toon, 0.050f, Vector3(-0.14f, 0.06f, 0.42f), Color(0.82f, 0.24f, 0.18f));
	item_count += 3;
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> plate;
		plate.instantiate();
		plate->set_top_radius(0.13f);
		plate->set_bottom_radius(0.10f);
		plate->set_height(0.018f);
		plate->set_radial_segments(12);
		plate->set_rings(1);
		plate->set_material(meshutil::toon_material(toon, Color(0.94f, 0.93f, 0.88f), 0.2f));
		MeshInstance3D *pmi = memnew(MeshInstance3D);
		pmi->set_mesh(plate);
		pmi->set_position(Vector3(-0.42f + 0.84f * (float)i, 0.02f, 0.30f));
		add_child(pmi);
		item_count++;
	}

	// 两只小蝴蝶
	const Color wing_col(0.95f, 0.62f, 0.24f);
	for (int i = 0; i < BUTTERFLY_COUNT; i++) {
		Node3D *bf = memnew(Node3D);
		bf->set_name(String("Butterfly") + String::num_int64(i));
		add_child(bf);
		butterflies[i] = bf;
		meshutil::add_box(bf, toon, Vector3(0.012f, 0.012f, 0.07f), Vector3(0.0f, 0.0f, 0.0f), Color(0.20f, 0.16f, 0.12f));
		for (int w = 0; w < 2; w++) {
			Ref<PlaneMesh> wm;
			wm.instantiate();
			wm->set_size(Vector2(0.09f, 0.07f));
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			mat->set_albedo(Color(wing_col.r, wing_col.g, wing_col.b, 0.9f));
			mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
			wm->set_material(mat);
			MeshInstance3D *wmi = memnew(MeshInstance3D);
			wmi->set_mesh(wm);
			wmi->set_position(Vector3((w == 0 ? 1.0f : -1.0f) * 0.045f, 0.0f, 0.0f));
			wmi->set_rotation(Vector3(-1.5707964f, 0.0f, 0.0f));
			wmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			bf->add_child(wmi);
			wings[i][w] = wmi;
		}
	}
}
