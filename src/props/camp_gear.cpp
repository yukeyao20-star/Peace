#include "camp_gear.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
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

void CampGear::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &CampGear::get_item_types);
	ClassDB::bind_method(D_METHOD("get_lamp_on"), &CampGear::get_lamp_on);
}

void CampGear::_notification(int p_what) {
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
			lamp_on = sky != nullptr && (sky->get_time_of_day() < 0.26f || sky->get_time_of_day() > 0.74f);
			if (flame != nullptr) {
				flame->set_visible(lamp_on);
				const float f = 0.85f + 0.15f * std::sin(t * 6.4f) + 0.07f * std::sin(t * 11.2f);
				flame->set_scale(Vector3(0.8f * f, f, 0.8f * f));
			}
		} break;
		default:
			break;
	}
}

void CampGear::build() {
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

	// 圆木凳
	Ref<CylinderMesh> log;
	log.instantiate();
	log->set_top_radius(0.18f);
	log->set_bottom_radius(0.18f);
	log->set_height(0.92f);
	log->set_radial_segments(10);
	log->set_rings(1);
	log->set_material(meshutil::toon_material(toon, Color(0.48f, 0.34f, 0.20f), 0.2f));
	MeshInstance3D *lmi = memnew(MeshInstance3D);
	lmi->set_mesh(log);
	lmi->set_position(Vector3(0.0f, 0.18f, 0.0f));
	lmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(lmi);
	// 搪瓷杯
	Ref<CylinderMesh> mug;
	mug.instantiate();
	mug->set_top_radius(0.055f);
	mug->set_bottom_radius(0.048f);
	mug->set_height(0.10f);
	mug->set_radial_segments(9);
	mug->set_rings(1);
	mug->set_material(meshutil::toon_material(toon, Color(0.80f, 0.84f, 0.82f), 0.2f));
	MeshInstance3D *mmi = memnew(MeshInstance3D);
	mmi->set_mesh(mug);
	mmi->set_position(Vector3(0.10f, 0.40f, 0.05f));
	add_child(mmi);
	meshutil::add_box(this, toon, Vector3(0.02f, 0.06f, 0.04f), Vector3(0.16f, 0.40f, 0.05f), Color(0.70f, 0.74f, 0.72f));

	// 铁皮提灯
	meshutil::add_box(this, toon, Vector3(0.10f, 0.03f, 0.10f), Vector3(-0.30f, 0.03f, 0.25f), Color(0.30f, 0.28f, 0.26f));
	Ref<CylinderMesh> lamp_glass;
	lamp_glass.instantiate();
	lamp_glass->set_top_radius(0.05f);
	lamp_glass->set_bottom_radius(0.06f);
	lamp_glass->set_height(0.15f);
	lamp_glass->set_radial_segments(8);
	lamp_glass->set_rings(1);
	Ref<StandardMaterial3D> lg;
	lg.instantiate();
	lg->set_albedo(Color(0.92f, 0.82f, 0.58f, 0.45f));
	lg->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	lg->set_roughness(0.15f);
	lamp_glass->set_material(lg);
	MeshInstance3D *lgm = memnew(MeshInstance3D);
	lgm->set_mesh(lamp_glass);
	lgm->set_position(Vector3(-0.30f, 0.13f, 0.25f));
	add_child(lgm);
	meshutil::add_box(this, toon, Vector3(0.11f, 0.02f, 0.11f), Vector3(-0.30f, 0.215f, 0.25f), Color(0.30f, 0.28f, 0.26f));
	Ref<SphereMesh> fl;
	fl.instantiate();
	fl->set_radius(0.028f);
	fl->set_height(0.06f);
	fl->set_radial_segments(5);
	fl->set_rings(3);
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fm->set_albedo(Color(1.0f, 0.74f, 0.30f));
	fm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	fl->set_material(fm);
	flame = memnew(MeshInstance3D);
	flame->set_mesh(fl);
	flame->set_position(Vector3(-0.30f, 0.14f, 0.25f));
	flame->set_visible(false);
	flame->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(flame);

	// 折地图
	meshutil::add_box(this, toon, Vector3(0.44f, 0.006f, 0.30f), Vector3(0.42f, 0.02f, 0.10f), Color(0.94f, 0.90f, 0.76f), Vector3(0.0f, 0.15f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.18f, 0.007f, 0.12f), Vector3(0.36f, 0.03f, 0.06f), Color(0.40f, 0.56f, 0.42f), Vector3(0.0f, 0.15f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.007f, 0.14f), Vector3(0.52f, 0.03f, 0.16f), Color(0.60f, 0.46f, 0.30f), Vector3(0.0f, 0.15f, 0.0f));

	// 指南针
	Ref<CylinderMesh> compass;
	compass.instantiate();
	compass->set_top_radius(0.055f);
	compass->set_bottom_radius(0.055f);
	compass->set_height(0.025f);
	compass->set_radial_segments(10);
	compass->set_rings(1);
	compass->set_material(meshutil::toon_material(toon, Color(0.34f, 0.32f, 0.28f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(compass);
	cmi->set_position(Vector3(0.06f, 0.03f, 0.30f));
	add_child(cmi);
	meshutil::add_box(this, toon, Vector3(0.06f, 0.006f, 0.014f), Vector3(0.06f, 0.045f, 0.30f), Color(0.80f, 0.24f, 0.20f), Vector3(0.0f, 0.5f, 0.0f));
}
