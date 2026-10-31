#include "umbrella_stand.h"

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
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void UmbrellaStand::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_umbrella_count"), &UmbrellaStand::get_umbrella_count);
	ClassDB::bind_method(D_METHOD("get_drips_on"), &UmbrellaStand::get_drips_on);
	ClassDB::bind_method(D_METHOD("get_wet_level"), &UmbrellaStand::get_wet_level);
	ClassDB::bind_method(D_METHOD("get_snow_level"), &UmbrellaStand::get_snow_level);
}

void UmbrellaStand::_notification(int p_what) {
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
			if (sky != nullptr) {
				wet = Math::move_toward(wet, Math::clamp(sky->get_rain_amount(), 0.0f, 1.0f), delta * 0.5f);
				snow_lvl = Math::move_toward(snow_lvl, Math::clamp(sky->get_snow_amount(), 0.0f, 1.0f), delta * 0.3f);
			}
			drips_on = wet > 0.25f;
			if (drips != nullptr) {
				if (drips->is_emitting() != drips_on) {
					drips->set_emitting(drips_on);
				}
				// 雨越大，伞尖滴得越急
				drips->set_amount(drips_on ? (int)(10 + 18 * wet) : 10);
			}
			if (snow_ring != nullptr) {
				snow_ring->set_visible(snow_lvl > 0.45f);
			}
			// 风把靠着的伞吹得轻轻晃
			for (int i = 0; i < UMBRELLA_COUNT; i++) {
				if (umbrellas[i] != nullptr) {
					const float base = (i == 0) ? 0.12f : -0.10f;
					umbrellas[i]->set_rotation(Vector3(
							base + std::sin(t * 1.1f + (float)i) * 0.015f,
							0.0f,
							(i == 0 ? 0.06f : -0.08f) + std::sin(t * 0.9f + (float)i) * 0.02f));
				}
			}
		} break;
		default:
			break;
	}
}

void UmbrellaStand::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();

	// 木桶伞架
	Ref<CylinderMesh> barrel;
	barrel.instantiate();
	barrel->set_top_radius(0.16f);
	barrel->set_bottom_radius(0.13f);
	barrel->set_height(0.42f);
	barrel->set_radial_segments(12);
	barrel->set_rings(1);
	barrel->set_material(meshutil::toon_material(toon, Color(0.48f, 0.32f, 0.18f), 0.2f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(barrel);
	bmi->set_position(Vector3(0.0f, 0.21f, 0.0f));
	add_child(bmi);
	// 桶箍
	for (int i = 0; i < 2; i++) {
		Ref<CylinderMesh> hoop;
		hoop.instantiate();
		hoop->set_top_radius(0.158f);
		hoop->set_bottom_radius(0.158f);
		hoop->set_height(0.025f);
		hoop->set_radial_segments(12);
		hoop->set_rings(1);
		hoop->set_material(meshutil::toon_material(toon, Color(0.36f, 0.32f, 0.28f), 0.15f));
		MeshInstance3D *h = memnew(MeshInstance3D);
		h->set_mesh(hoop);
		h->set_position(Vector3(0.0f, 0.12f + 0.18f * (float)i, 0.0f));
		add_child(h);
	}

	// 两把收拢的伞
	const Color cols[UMBRELLA_COUNT] = { Color(0.72f, 0.24f, 0.20f), Color(0.24f, 0.36f, 0.54f) };
	for (int i = 0; i < UMBRELLA_COUNT; i++) {
		Node3D *u = memnew(Node3D);
		u->set_name(String("Umbrella") + String::num_int64(i));
		u->set_position(Vector3((i == 0 ? -0.05f : 0.06f), 0.22f, (i == 0 ? 0.02f : -0.03f)));
		u->set_rotation(Vector3((i == 0 ? 0.12f : -0.10f), 0.0f, (i == 0 ? 0.06f : -0.08f)));
		add_child(u);
		umbrellas[i] = u;

		Ref<CylinderMesh> shaft;
		shaft.instantiate();
		shaft->set_top_radius(0.010f);
		shaft->set_bottom_radius(0.010f);
		shaft->set_height(0.92f);
		shaft->set_radial_segments(5);
		shaft->set_rings(1);
		shaft->set_material(meshutil::toon_material(toon, Color(0.28f, 0.24f, 0.20f), 0.2f));
		MeshInstance3D *sh = memnew(MeshInstance3D);
		sh->set_mesh(shaft);
		sh->set_position(Vector3(0.0f, 0.46f, 0.0f));
		u->add_child(sh);

		Ref<CylinderMesh> canopy;
		canopy.instantiate();
		canopy->set_top_radius(0.018f);
		canopy->set_bottom_radius(0.075f);
		canopy->set_height(0.55f);
		canopy->set_radial_segments(7);
		canopy->set_rings(1);
		canopy->set_material(meshutil::toon_material(toon, cols[i], 0.2f));
		MeshInstance3D *cm = memnew(MeshInstance3D);
		cm->set_mesh(canopy);
		cm->set_position(Vector3(0.0f, 0.52f, 0.0f));
		u->add_child(cm);

		// 弯手柄
		meshutil::add_box(u, toon, Vector3(0.10f, 0.03f, 0.03f), Vector3(0.05f, 0.95f, 0.0f), Color(0.34f, 0.24f, 0.16f));
		meshutil::add_box(u, toon, Vector3(0.03f, 0.08f, 0.03f), Vector3(0.10f, 0.91f, 0.0f), Color(0.34f, 0.24f, 0.16f));
	}

	// 伞尖滴水
	Ref<SphereMesh> drop;
	drop.instantiate();
	drop->set_radius(0.010f);
	drop->set_height(0.020f);
	drop->set_radial_segments(4);
	drop->set_rings(2);
	Ref<StandardMaterial3D> dm;
	dm.instantiate();
	dm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dm->set_albedo(Color(0.72f, 0.86f, 0.95f, 0.7f));
	dm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	drop->set_material(dm);
	drips = memnew(CPUParticles3D);
	drips->set_name("UmbrellaDrips");
	drips->set_amount(28);
	drips->set_lifetime(1.0f);
	drips->set_mesh(drop);
	drips->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	drips->set_emission_box_extents(Vector3(0.12f, 0.10f, 0.08f));
	drips->set_direction(Vector3(0.0f, -1.0f, 0.0f));
	drips->set_spread(6.0f);
	drips->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.0f);
	drips->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.1f);
	drips->set_gravity(Vector3(0.0f, -3.0f, 0.0f));
	drips->set_position(Vector3(0.0f, 0.35f, 0.0f));
	drips->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	drips->set_emitting(false);
	add_child(drips);

	// 桶沿的雪
	Ref<CylinderMesh> ring;
	ring.instantiate();
	ring->set_top_radius(0.165f);
	ring->set_bottom_radius(0.165f);
	ring->set_height(0.02f);
	ring->set_radial_segments(12);
	ring->set_rings(1);
	ring->set_material(meshutil::toon_material(toon, Color(0.92f, 0.96f, 1.0f), 0.2f));
	snow_ring = memnew(MeshInstance3D);
	snow_ring->set_mesh(ring);
	snow_ring->set_position(Vector3(0.0f, 0.425f, 0.0f));
	snow_ring->set_visible(false);
	add_child(snow_ring);
}
