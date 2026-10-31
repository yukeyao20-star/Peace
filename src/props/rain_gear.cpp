#include "rain_gear.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
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

void RainGear::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &RainGear::get_item_types);
	ClassDB::bind_method(D_METHOD("get_water_level"), &RainGear::get_water_level);
	ClassDB::bind_method(D_METHOD("get_splashing"), &RainGear::get_splashing);
}

void RainGear::_notification(int p_what) {
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
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			if (rain > 0.15f) {
				water_level = Math::min(1.0f, water_level + rain * delta * 0.10f);
			} else {
				water_level = Math::max(0.05f, water_level - delta * 0.004f);
			}
			if (water != nullptr) {
				water->set_position(Vector3(0.0f, 0.20f + 0.55f * water_level, 0.0f));
			}
			splashing = rain > 0.2f;
			if (splash != nullptr && splash->is_emitting() != splashing) {
				splash->set_emitting(splashing);
			}
		} break;
		default:
			break;
	}
}

void RainGear::build() {
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

	// 落水管：从屋檐高度的竖直管 + 弯头
	Ref<CylinderMesh> pipe;
	pipe.instantiate();
	pipe->set_top_radius(0.045f);
	pipe->set_bottom_radius(0.045f);
	pipe->set_height(1.55f);
	pipe->set_radial_segments(8);
	pipe->set_rings(1);
	pipe->set_material(meshutil::toon_material(toon, Color(0.52f, 0.54f, 0.56f), 0.25f));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_mesh(pipe);
	pmi->set_position(Vector3(-0.05f, 1.65f, 0.0f));
	add_child(pmi);
	Ref<CylinderMesh> elbow;
	elbow.instantiate();
	elbow->set_top_radius(0.045f);
	elbow->set_bottom_radius(0.045f);
	elbow->set_height(0.35f);
	elbow->set_radial_segments(8);
	elbow->set_rings(1);
	elbow->set_material(meshutil::toon_material(toon, Color(0.52f, 0.54f, 0.56f), 0.25f));
	MeshInstance3D *emi = memnew(MeshInstance3D);
	emi->set_mesh(elbow);
	emi->set_position(Vector3(0.10f, 0.90f, 0.0f));
	emi->set_rotation(Vector3(0.0f, 0.0f, 0.9f));
	add_child(emi);

	// 木雨桶
	Ref<CylinderMesh> barrel;
	barrel.instantiate();
	barrel->set_top_radius(0.30f);
	barrel->set_bottom_radius(0.26f);
	barrel->set_height(0.85f);
	barrel->set_radial_segments(14);
	barrel->set_rings(1);
	barrel->set_material(meshutil::toon_material(toon, Color(0.50f, 0.35f, 0.21f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(barrel);
	bmi->set_position(Vector3(0.42f, 0.425f, 0.0f));
	add_child(bmi);
	for (int i = 0; i < 3; i++) {
		Ref<CylinderMesh> hoop;
		hoop.instantiate();
		hoop->set_top_radius(0.302f);
		hoop->set_bottom_radius(0.302f);
		hoop->set_height(0.025f);
		hoop->set_radial_segments(14);
		hoop->set_rings(1);
		hoop->set_material(meshutil::toon_material(toon, Color(0.36f, 0.34f, 0.30f), 0.2f));
		MeshInstance3D *h = memnew(MeshInstance3D);
		h->set_mesh(hoop);
		h->set_position(Vector3(0.42f, 0.15f + 0.28f * (float)i, 0.0f));
		add_child(h);
	}
	// 水面
	Ref<CylinderMesh> wm;
	wm.instantiate();
	wm->set_top_radius(0.27f);
	wm->set_bottom_radius(0.27f);
	wm->set_height(0.01f);
	wm->set_radial_segments(14);
	wm->set_rings(1);
	Ref<StandardMaterial3D> wmat;
	wmat.instantiate();
	wmat->set_albedo(Color(0.20f, 0.38f, 0.46f, 0.85f));
	wmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wmat->set_roughness(0.12f);
	wm->set_material(wmat);
	water = memnew(MeshInstance3D);
	water->set_mesh(wm);
	water->set_position(Vector3(0.42f, 0.42f, 0.0f));
	add_child(water);
	// 桶底出水口
	Ref<CylinderMesh> tap;
	tap.instantiate();
	tap->set_top_radius(0.025f);
	tap->set_bottom_radius(0.025f);
	tap->set_height(0.12f);
	tap->set_radial_segments(6);
	tap->set_rings(1);
	tap->set_material(meshutil::toon_material(toon, Color(0.58f, 0.50f, 0.28f), 0.2f));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(tap);
	tmi->set_position(Vector3(0.70f, 0.18f, 0.0f));
	tmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
	add_child(tmi);

	// 洒水壶
	Ref<CylinderMesh> can;
	can.instantiate();
	can->set_top_radius(0.11f);
	can->set_bottom_radius(0.13f);
	can->set_height(0.22f);
	can->set_radial_segments(10);
	can->set_rings(1);
	can->set_material(meshutil::toon_material(toon, Color(0.48f, 0.56f, 0.52f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(can);
	cmi->set_position(Vector3(-0.38f, 0.11f, 0.38f));
	add_child(cmi);
	Ref<CylinderMesh> spout;
	spout.instantiate();
	spout->set_top_radius(0.018f);
	spout->set_bottom_radius(0.035f);
	spout->set_height(0.30f);
	spout->set_radial_segments(6);
	spout->set_rings(1);
	spout->set_material(meshutil::toon_material(toon, Color(0.48f, 0.56f, 0.52f), 0.2f));
	MeshInstance3D *smi = memnew(MeshInstance3D);
	smi->set_mesh(spout);
	smi->set_position(Vector3(-0.30f, 0.22f, 0.52f));
	smi->set_rotation(Vector3(0.9f, 0.0f, 0.0f));
	add_child(smi);
	Ref<TorusMesh> handle;
	handle.instantiate();
	handle->set_inner_radius(0.07f);
	handle->set_outer_radius(0.085f);
	handle->set_rings(12);
	handle->set_ring_segments(3);
	handle->set_material(meshutil::toon_material(toon, Color(0.42f, 0.50f, 0.46f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(handle);
	hmi->set_position(Vector3(-0.38f, 0.23f, 0.32f));
	hmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	add_child(hmi);

	// 一双胶靴
	for (int i = 0; i < 2; i++) {
		const float x = -0.02f + 0.18f * (float)i;
		Ref<CylinderMesh> boot;
		boot.instantiate();
		boot->set_top_radius(0.065f);
		boot->set_bottom_radius(0.060f);
		boot->set_height(0.34f);
		boot->set_radial_segments(8);
		boot->set_rings(1);
		boot->set_material(meshutil::toon_material(toon, i == 0 ? Color(0.84f, 0.68f, 0.20f) : Color(0.30f, 0.48f, 0.40f), 0.2f));
		MeshInstance3D *b = memnew(MeshInstance3D);
		b->set_mesh(boot);
		b->set_position(Vector3(x, 0.17f, 0.62f));
		add_child(b);
		meshutil::add_box(this, toon, Vector3(0.14f, 0.10f, 0.24f), Vector3(x, 0.05f, 0.70f),
				i == 0 ? Color(0.84f, 0.68f, 0.20f) : Color(0.30f, 0.48f, 0.40f));
	}

	// 落水口的水花
	Ref<SphereMesh> drop;
	drop.instantiate();
	drop->set_radius(0.012f);
	drop->set_height(0.024f);
	drop->set_radial_segments(4);
	drop->set_rings(2);
	Ref<StandardMaterial3D> dm;
	dm.instantiate();
	dm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dm->set_albedo(Color(0.74f, 0.88f, 0.96f, 0.7f));
	dm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	drop->set_material(dm);
	splash = memnew(CPUParticles3D);
	splash->set_name("BarrelSplash");
	splash->set_amount(24);
	splash->set_lifetime(0.7f);
	splash->set_mesh(drop);
	splash->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	splash->set_emission_sphere_radius(0.08f);
	splash->set_direction(Vector3(0.0f, 0.6f, 0.0f));
	splash->set_spread(55.0f);
	splash->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	splash->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.35f);
	splash->set_gravity(Vector3(0.0f, -2.6f, 0.0f));
	splash->set_position(Vector3(0.42f, 0.86f, 0.0f));
	splash->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	splash->set_emitting(false);
	add_child(splash);
}
