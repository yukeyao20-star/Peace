#include "flower_bed.h"

#include <cmath>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void FlowerBed::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_flower_count"), &FlowerBed::get_flower_count);
	ClassDB::bind_method(D_METHOD("get_dew_visible"), &FlowerBed::get_dew_visible);
	ClassDB::bind_method(D_METHOD("get_bees_visible"), &FlowerBed::get_bees_visible);
	ClassDB::bind_method(D_METHOD("get_snow_covered"), &FlowerBed::get_snow_covered);
	ClassDB::bind_method(D_METHOD("get_open"), &FlowerBed::get_open);
}

void FlowerBed::_notification(int p_what) {
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
			if (sky == nullptr && get_tree() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			}
			float tod = 0.45f;
			float rain = 0.0f;
			if (sky != nullptr) {
				tod = sky->get_time_of_day();
				rain = sky->get_rain_amount();
				snow_covered = sky->get_snow_amount() > 0.5f;
			}
			// 白天开、夜里合，雨天花也收一点
			float want = 1.0f;
			if (tod < 0.26f || tod > 0.74f) {
				want = 0.15f;
			}
			want *= Math::clamp(1.0f - rain * 0.7f, 0.0f, 1.0f);
			open = Math::move_toward(open, want, delta * 0.6f);
			for (int i = 0; i < FLOWER_COUNT; i++) {
				if (heads[i] == nullptr) {
					continue;
				}
				const float s = 0.35f + 0.65f * open;
				heads[i]->set_scale(Vector3(s, s, s));
				heads[i]->set_rotation(Vector3(std::sin(t * 1.4f + (float)i) * 0.05f, 0.0f,
						std::sin(t * 1.1f + (float)i * 1.7f) * 0.05f));
			}
			float want_dew = 0.0f;
			if (tod > 0.23f && tod < 0.37f) {
				want_dew = std::sin(3.14159f * (tod - 0.23f) / 0.14f);
			}
			want_dew *= Math::clamp(1.0f - rain, 0.0f, 1.0f);
			dew = Math::move_toward(dew, Math::clamp(want_dew, 0.0f, 1.0f), delta * 0.5f);
			dew_visible = 0;
			for (int i = 0; i < DEW_COUNT; i++) {
				if (dewdrops[i] == nullptr) {
					continue;
				}
				const bool vis = dew > 0.08f;
				dewdrops[i]->set_visible(vis);
				if (vis) {
					const float s = 0.4f + 0.6f * dew;
					dewdrops[i]->set_scale(Vector3(s, s, s));
					dew_visible++;
				}
			}
			const bool bee_on = sky == nullptr || (tod > 0.29f && tod < 0.70f && rain < 0.3f && !snow_covered);
			bees_visible = bee_on ? BEE_COUNT : 0;
			for (int i = 0; i < BEE_COUNT; i++) {
				if (bees[i] == nullptr) {
					continue;
				}
				bees[i]->set_visible(bee_on);
				if (!bee_on) {
					continue;
				}
				const float a = t * 1.4f + (float)i * 2.1f;
				bees[i]->set_position(Vector3(std::cos(a) * 0.55f, 0.55f + 0.08f * std::sin(t * 2.4f + (float)i), std::sin(a) * 0.35f));
				for (int w = 0; w < 2; w++) {
					if (wings[i][w] != nullptr) {
						const float flap = std::sin(t * 26.0f + (float)i) * 0.8f;
						wings[i][w]->set_rotation(Vector3(0.0f, 0.0f, (w == 0 ? 1.0f : -1.0f) * flap));
					}
				}
			}
			if (snow_cover != nullptr) {
				snow_cover->set_visible(snow_covered);
			}
		} break;
		default:
			break;
	}
}

void FlowerBed::build() {
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

	// 土床与木板边框
	meshutil::add_box(this, toon, Vector3(1.66f, 0.10f, 0.96f), Vector3(0.0f, 0.05f, 0.0f), Color(0.30f, 0.22f, 0.14f));
	meshutil::add_box(this, toon, Vector3(1.76f, 0.10f, 0.08f), Vector3(0.0f, 0.10f, -0.50f), Color(0.50f, 0.34f, 0.20f));
	meshutil::add_box(this, toon, Vector3(1.76f, 0.10f, 0.08f), Vector3(0.0f, 0.10f, 0.50f), Color(0.50f, 0.34f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.08f, 0.10f, 0.96f), Vector3(-0.84f, 0.10f, 0.0f), Color(0.50f, 0.34f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.08f, 0.10f, 0.96f), Vector3(0.84f, 0.10f, 0.0f), Color(0.50f, 0.34f, 0.20f));

	const Color petals[3] = {
		Color(0.80f, 0.24f, 0.22f),
		Color(0.92f, 0.76f, 0.24f),
		Color(0.60f, 0.34f, 0.74f),
	};
	Ref<CylinderMesh> stem_mesh;
	stem_mesh.instantiate();
	stem_mesh->set_top_radius(0.012f);
	stem_mesh->set_bottom_radius(0.016f);
	stem_mesh->set_height(0.34f);
	stem_mesh->set_radial_segments(5);
	stem_mesh->set_rings(1);
	stem_mesh->set_material(meshutil::toon_material(toon, Color(0.28f, 0.46f, 0.20f), 0.2f));

	// 优化（第 130 轮）：12 根茎、24 片叶各合成一个 MultiMesh；
	// 每朵花头（杯+3片花瓣）烘成一张网格，花圃 draw call 从约 90 降到 20。
	Ref<MultiMesh> stem_mm;
	stem_mm.instantiate();
	stem_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	stem_mm->set_mesh(stem_mesh);
	stem_mm->set_instance_count(FLOWER_COUNT);

	Ref<SphereMesh> leaf_sphere;
	leaf_sphere.instantiate();
	leaf_sphere->set_radius(1.0f);
	leaf_sphere->set_height(2.0f);
	leaf_sphere->set_radial_segments(6);
	leaf_sphere->set_rings(3);
	leaf_sphere->set_material(meshutil::toon_material(toon, Color(0.30f, 0.48f, 0.20f), 0.2f));
	Ref<MultiMesh> leaf_mm;
	leaf_mm.instantiate();
	leaf_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	leaf_mm->set_mesh(leaf_sphere);
	leaf_mm->set_instance_count(FLOWER_COUNT * 2);

	int leaf_i = 0;
	for (int i = 0; i < FLOWER_COUNT; i++) {
		const int col = i % 4;
		const int row = i / 4;
		const float x = -0.60f + 0.40f * (float)col;
		const float z = -0.26f + 0.26f * (float)row;
		stem_mm->set_instance_transform(i, Transform3D(Basis(), Vector3(x, 0.27f, z)));
		for (int l = 0; l < 2; l++) {
			const Basis sb = Basis().scaled(Vector3(0.077f, 0.0126f, 0.035f));
			leaf_mm->set_instance_transform(leaf_i++, Transform3D(sb,
					Vector3(x + (l == 0 ? -0.05f : 0.05f), 0.20f + 0.05f * (float)l, z)));
		}

		Node3D *h = memnew(Node3D);
		h->set_name(String("FlowerHead") + String::num_int64(i));
		h->set_position(Vector3(x, 0.44f, z));
		add_child(h);
		heads[i] = h;

		Ref<CylinderMesh> cup;
		cup.instantiate();
		cup->set_top_radius(0.075f);
		cup->set_bottom_radius(0.025f);
		cup->set_height(0.09f);
		cup->set_radial_segments(8);
		cup->set_rings(1);
		Ref<BoxMesh> petal_mesh;
		petal_mesh.instantiate();
		petal_mesh->set_size(Vector3(0.06f, 0.02f, 0.035f));

		Ref<SurfaceTool> st;
		st.instantiate();
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		st->append_from(cup, 0, Transform3D());
		for (int p = 0; p < 3; p++) {
			const float a = (float)p * 2.094f;
			Basis pb;
			pb.set_euler_scale(Vector3(0.4f, a, 0.0f), Vector3(1, 1, 1));
			st->append_from(petal_mesh, 0, Transform3D(pb, Vector3(std::cos(a) * 0.06f, 0.03f, std::sin(a) * 0.06f)));
		}
		Ref<ArrayMesh> head_mesh = st->commit();
		head_mesh->surface_set_material(0, meshutil::toon_material(toon, petals[row % 3], 0.3f));
		MeshInstance3D *hm = memnew(MeshInstance3D);
		hm->set_mesh(head_mesh);
		h->add_child(hm);
	}

	MultiMeshInstance3D *stem_mi = memnew(MultiMeshInstance3D);
	stem_mi->set_name("Stems");
	stem_mi->set_multimesh(stem_mm);
	add_child(stem_mi);
	MultiMeshInstance3D *leaf_mi = memnew(MultiMeshInstance3D);
	leaf_mi->set_name("Leaves");
	leaf_mi->set_multimesh(leaf_mm);
	add_child(leaf_mi);

	// 露珠
	Ref<SphereMesh> dew_mesh;
	dew_mesh.instantiate();
	dew_mesh->set_radius(0.011f);
	dew_mesh->set_height(0.022f);
	dew_mesh->set_radial_segments(4);
	dew_mesh->set_rings(2);
	Ref<StandardMaterial3D> dm;
	dm.instantiate();
	dm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dm->set_albedo(Color(0.82f, 0.93f, 1.0f, 0.85f));
	dm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	dew_mesh->set_material(dm);
	for (int i = 0; i < DEW_COUNT; i++) {
		const int fi = (i * 2) % FLOWER_COUNT;
		MeshInstance3D *d = memnew(MeshInstance3D);
		d->set_mesh(dew_mesh);
		d->set_position(Vector3(-0.60f + 0.40f * (float)(fi % 4) + 0.05f, 0.48f, -0.26f + 0.26f * (float)(fi / 4) + 0.04f));
		d->set_visible(false);
		d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(d);
		dewdrops[i] = d;
	}

	// 两只蜂
	for (int i = 0; i < BEE_COUNT; i++) {
		Node3D *bee = memnew(Node3D);
		bee->set_name(String("Bee") + String::num_int64(i));
		bee->set_visible(false);
		add_child(bee);
		bees[i] = bee;
		meshutil::add_box(bee, toon, Vector3(0.030f, 0.022f, 0.055f), Vector3(), Color(0.92f, 0.76f, 0.22f));
		for (int w = 0; w < 2; w++) {
			Ref<BoxMesh> wing;
			wing.instantiate();
			wing->set_size(Vector3(0.045f, 0.010f, 0.028f));
			wing->set_material(meshutil::toon_material(toon, Color(0.92f, 0.95f, 1.0f), 0.1f));
			MeshInstance3D *wm = memnew(MeshInstance3D);
			wm->set_mesh(wing);
			wm->set_position(Vector3((w == 0 ? -1.0f : 1.0f) * 0.030f, 0.016f, 0.0f));
			bee->add_child(wm);
			wings[i][w] = wm;
		}
	}

	// 雪盖
	Ref<BoxMesh> snow;
	snow.instantiate();
	snow->set_size(Vector3(1.58f, 0.03f, 0.88f));
	snow->set_material(meshutil::toon_material(toon, Color(0.92f, 0.96f, 1.0f), 0.2f));
	snow_cover = memnew(MeshInstance3D);
	snow_cover->set_mesh(snow);
	snow_cover->set_position(Vector3(0.0f, 0.15f, 0.0f));
	snow_cover->set_visible(false);
	snow_cover->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(snow_cover);
}
