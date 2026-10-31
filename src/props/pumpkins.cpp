#include "pumpkins.h"

#include <cmath>

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

void Pumpkins::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pumpkin_count"), &Pumpkins::get_pumpkin_count);
	ClassDB::bind_method(D_METHOD("get_dew_visible"), &Pumpkins::get_dew_visible);
	ClassDB::bind_method(D_METHOD("get_glow_on"), &Pumpkins::get_glow_on);
	ClassDB::bind_method(D_METHOD("get_snow_covered"), &Pumpkins::get_snow_covered);
}

void Pumpkins::_notification(int p_what) {
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
			float target_dew = 0.0f;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				if (tod > 0.23f && tod < 0.37f) {
					target_dew = std::sin(3.14159f * (tod - 0.23f) / 0.14f);
				}
				target_dew *= Math::clamp(1.0f - sky->get_rain_amount(), 0.0f, 1.0f);
				glow_on = tod < 0.24f || tod > 0.77f;
				snow_covered = sky->get_snow_amount() > 0.45f;
			}
			dew = Math::move_toward(dew, Math::clamp(target_dew, 0.0f, 1.0f), delta * 0.5f);
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
			if (glow != nullptr) {
				glow->set_visible(false); // 内部光球不直接露出来，靠发光的刻痕表现
			}
			for (int i = 0; i < 3; i++) {
				if (face_glow[i] != nullptr) {
					face_glow[i]->set_visible(glow_on);
				}
			}
			for (int i = 0; i < COUNT; i++) {
				if (snow_caps[i] != nullptr) {
					snow_caps[i]->set_visible(snow_covered);
				}
			}
		} break;
		default:
			break;
	}
}

void Pumpkins::build() {
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

	const Color orange[COUNT] = {
		Color(0.86f, 0.38f, 0.12f),
		Color(0.92f, 0.48f, 0.14f),
		Color(0.80f, 0.34f, 0.11f),
		Color(0.90f, 0.44f, 0.13f),
	};
	const Vector3 pos[COUNT] = {
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.62f, 0.0f, 0.18f),
		Vector3(-0.55f, 0.0f, 0.32f),
		Vector3(0.18f, 0.0f, -0.62f),
	};
	const float radii[COUNT] = { 0.34f, 0.24f, 0.20f, 0.16f };

	for (int i = 0; i < COUNT; i++) {
		Node3D *p = memnew(Node3D);
		p->set_name(String("Pumpkin") + String::num_int64(i));
		p->set_position(pos[i]);
		add_child(p);
		pumpkins[i] = p;
		const float r = radii[i];
		// 三颗重叠的扁球做出南瓜瓣（烘成一张网格，一只南瓜一次 draw）
		Ref<SphereMesh> unit_sphere;
		unit_sphere.instantiate();
		unit_sphere->set_radius(1.0f);
		unit_sphere->set_height(2.0f);
		unit_sphere->set_radial_segments(10);
		unit_sphere->set_rings(6);
		Ref<SurfaceTool> surf;
		surf.instantiate();
		surf->begin(Mesh::PRIMITIVE_TRIANGLES);
		for (int s = -1; s <= 1; s++) {
			const Basis sb = Basis().scaled(Vector3(r * 0.82f, r * 0.62f, r * 0.95f));
			surf->append_from(unit_sphere, 0, Transform3D(sb,
					Vector3((float)s * r * 0.28f, r * 0.62f, 0.0f)));
		}
		Ref<ArrayMesh> lobes = surf->commit();
		lobes->surface_set_material(0, meshutil::toon_material(toon, orange[i], 0.25f));
		MeshInstance3D *lmi = memnew(MeshInstance3D);
		lmi->set_mesh(lobes);
		p->add_child(lmi);
		Ref<CylinderMesh> stem_mesh;
		stem_mesh.instantiate();
		stem_mesh->set_top_radius(r * 0.10f);
		stem_mesh->set_bottom_radius(r * 0.14f);
		stem_mesh->set_height(r * 0.35f);
		stem_mesh->set_radial_segments(6);
		stem_mesh->set_rings(1);
		stem_mesh->set_material(meshutil::toon_material(toon, Color(0.34f, 0.46f, 0.20f), 0.2f));
		MeshInstance3D *st = memnew(MeshInstance3D);
		st->set_mesh(stem_mesh);
		st->set_position(Vector3(0.0f, r * 1.15f, 0.0f));
		st->set_rotation(Vector3(0.0f, 0.0f, 0.18f));
		p->add_child(st);

		if (i == 0) {
			// 刻南瓜灯：两只三角眼 + 锯齿嘴（用暗色小方块近似）
			meshutil::add_box(p, toon, Vector3(0.09f, 0.06f, 0.02f), Vector3(-0.10f, r * 0.72f, r * 0.96f), Color(0.18f, 0.10f, 0.06f), Vector3(0.0f, 0.0f, 0.5f));
			meshutil::add_box(p, toon, Vector3(0.09f, 0.06f, 0.02f), Vector3(0.10f, r * 0.72f, r * 0.96f), Color(0.18f, 0.10f, 0.06f), Vector3(0.0f, 0.0f, -0.5f));
			meshutil::add_box(p, toon, Vector3(0.18f, 0.035f, 0.02f), Vector3(0.0f, r * 0.48f, r * 0.97f), Color(0.18f, 0.10f, 0.06f));
			// 夜里刻痕透出的暖光
			Ref<StandardMaterial3D> fg;
			fg.instantiate();
			fg->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			fg->set_albedo(Color(1.0f, 0.72f, 0.28f));
			fg->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
			const Vector3 glow_pos[3] = {
				Vector3(-0.10f, r * 0.72f, r * 0.985f),
				Vector3(0.10f, r * 0.72f, r * 0.985f),
				Vector3(0.0f, r * 0.48f, r * 0.995f),
			};
			const Vector3 glow_size[3] = {
				Vector3(0.075f, 0.05f, 0.01f),
				Vector3(0.075f, 0.05f, 0.01f),
				Vector3(0.16f, 0.03f, 0.01f),
			};
			for (int f = 0; f < 3; f++) {
				Ref<BoxMesh> fm;
				fm.instantiate();
				fm->set_size(glow_size[f]);
				fm->set_material(fg);
				MeshInstance3D *fmi = memnew(MeshInstance3D);
				fmi->set_mesh(fm);
				fmi->set_position(glow_pos[f]);
				fmi->set_visible(false);
				fmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				p->add_child(fmi);
				face_glow[f] = fmi;
			}
		}

		Ref<BoxMesh> cap;
		cap.instantiate();
		cap->set_size(Vector3(r * 1.5f, 0.035f, r * 1.5f));
		cap->set_material(meshutil::toon_material(toon, Color(0.92f, 0.96f, 1.0f), 0.2f));
		MeshInstance3D *ci = memnew(MeshInstance3D);
		ci->set_mesh(cap);
		ci->set_position(Vector3(0.0f, r * 0.98f, 0.0f));
		ci->set_visible(false);
		ci->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		p->add_child(ci);
		snow_caps[i] = ci;
	}

	// 南瓜灯里的暖光
	Ref<SphereMesh> glow_mesh;
	glow_mesh.instantiate();
	glow_mesh->set_radius(0.18f);
	glow_mesh->set_height(0.36f);
	glow_mesh->set_radial_segments(8);
	glow_mesh->set_rings(4);
	Ref<StandardMaterial3D> gm;
	gm.instantiate();
	gm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	gm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	gm->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	gm->set_albedo(Color(1.0f, 0.62f, 0.20f, 0.30f));
	gm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	glow_mesh->set_material(gm);
	glow = memnew(MeshInstance3D);
	glow->set_mesh(glow_mesh);
	glow->set_position(Vector3(0.0f, 0.26f, 0.0f));
	glow->set_visible(false);
	glow->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	pumpkins[0]->add_child(glow);

	// 露珠
	Ref<SphereMesh> dew_mesh;
	dew_mesh.instantiate();
	dew_mesh->set_radius(0.012f);
	dew_mesh->set_height(0.024f);
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
		const float a = (float)i * 1.3f;
		MeshInstance3D *d = memnew(MeshInstance3D);
		d->set_mesh(dew_mesh);
		d->set_position(Vector3(std::cos(a) * 0.18f, 0.30f + 0.04f * (float)(i % 3), std::sin(a) * 0.18f));
		d->set_visible(false);
		d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		pumpkins[i % COUNT]->add_child(d);
		dewdrops[i] = d;
	}

	// 藤叶（7 片合成一个 MultiMesh）
	Ref<SphereMesh> leaf_unit;
	leaf_unit.instantiate();
	leaf_unit->set_radius(1.0f);
	leaf_unit->set_height(2.0f);
	leaf_unit->set_radial_segments(6);
	leaf_unit->set_rings(3);
	leaf_unit->set_material(meshutil::toon_material(toon, Color(0.30f, 0.46f, 0.18f), 0.2f));
	Ref<MultiMesh> leaf_mm;
	leaf_mm.instantiate();
	leaf_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	leaf_mm->set_mesh(leaf_unit);
	leaf_mm->set_instance_count(7);
	for (int i = 0; i < 7; i++) {
		const float a = (float)i * 0.9f;
		const Basis sb = Basis(Vector3(0.0f, 1.0f, 0.0f), a * 0.5f).scaled(Vector3(0.132f, 0.0154f, 0.088f));
		leaf_mm->set_instance_transform(i, Transform3D(sb,
				Vector3(std::cos(a) * 0.85f, 0.04f, std::sin(a) * 0.85f)));
	}
	MultiMeshInstance3D *leaf_mi = memnew(MultiMeshInstance3D);
	leaf_mi->set_name("VineLeaves");
	leaf_mi->set_multimesh(leaf_mm);
	add_child(leaf_mi);
}
