#include "window_sill.h"

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
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"

using namespace godot;

void WindowSill::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &WindowSill::get_item_types);
	ClassDB::bind_method(D_METHOD("get_flame_on"), &WindowSill::get_flame_on);
}

void WindowSill::_notification(int p_what) {
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
			flame_on = true;
			if (flame != nullptr) {
				const float f = 0.85f + 0.15f * std::sin(t * 6.7f) + 0.08f * std::sin(t * 12.3f);
				flame->set_scale(Vector3(0.8f * f, f, 0.8f * f));
			}
		} break;
		default:
			break;
	}
}

void WindowSill::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	set_rotation(Vector3(0.0f, 3.14159f, 0.0f)); // 陈设朝向屋内
	// 窗台板
	meshutil::add_box(this, toon, Vector3(1.15f, 0.05f, 0.28f), Vector3(0.0f, 0.0f, 0.0f), Color(0.60f, 0.44f, 0.26f));
	meshutil::add_box(this, toon, Vector3(1.15f, 0.04f, 0.03f), Vector3(0.0f, -0.04f, 0.12f), Color(0.48f, 0.33f, 0.19f));

	// 三只陶盆香草
	for (int i = 0; i < 3; i++) {
		const float x = -0.32f + 0.24f * (float)i;
		Ref<CylinderMesh> pot;
		pot.instantiate();
		pot->set_top_radius(0.060f);
		pot->set_bottom_radius(0.045f);
		pot->set_height(0.085f);
		pot->set_radial_segments(9);
		pot->set_rings(1);
		pot->set_material(meshutil::toon_material(toon, Color(0.72f, 0.40f, 0.24f), 0.2f));
		MeshInstance3D *p = memnew(MeshInstance3D);
		p->set_mesh(pot);
		p->set_position(Vector3(x, 0.07f, 0.0f));
		add_child(p);
		// 土
		Ref<CylinderMesh> soil;
		soil.instantiate();
		soil->set_top_radius(0.050f);
		soil->set_bottom_radius(0.050f);
		soil->set_height(0.012f);
		soil->set_radial_segments(9);
		soil->set_rings(1);
		soil->set_material(meshutil::toon_material(toon, Color(0.30f, 0.22f, 0.14f), 0.2f));
		MeshInstance3D *sm = memnew(MeshInstance3D);
		sm->set_mesh(soil);
		sm->set_position(Vector3(x, 0.112f, 0.0f));
		add_child(sm);
		// 叶簇
		for (int l = 0; l < 4; l++) {
			const float a = (float)l * 1.57f + (float)i;
			MeshInstance3D *leaf = meshutil::add_sphere(this, toon, 0.035f,
					Vector3(x + std::cos(a) * 0.030f, 0.15f + 0.02f * (float)(l % 2), std::sin(a) * 0.030f),
					Color(0.32f + 0.04f * (float)i, 0.50f, 0.24f));
			leaf->set_scale(Vector3(1.0f, 1.6f, 0.45f));
			leaf->set_rotation(Vector3(0.0f, a, 0.35f));
		}
		// 中间那盆开一朵小白花
		if (i == 1) {
			meshutil::add_sphere(this, toon, 0.018f, Vector3(x, 0.20f, 0.0f), Color(0.96f, 0.94f, 0.88f));
		}
	}

	// 玻璃罐与碎玻璃
	Ref<CylinderMesh> jar;
	jar.instantiate();
	jar->set_top_radius(0.050f);
	jar->set_bottom_radius(0.045f);
	jar->set_height(0.13f);
	jar->set_radial_segments(10);
	jar->set_rings(1);
	Ref<StandardMaterial3D> jm;
	jm.instantiate();
	jm->set_albedo(Color(0.70f, 0.86f, 0.90f, 0.45f));
	jm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	jm->set_roughness(0.15f);
	jar->set_material(jm);
	MeshInstance3D *jm2 = memnew(MeshInstance3D);
	jm2->set_mesh(jar);
	jm2->set_position(Vector3(0.38f, 0.09f, 0.0f));
	add_child(jm2);
	const Color glass_cols[3] = { Color(0.35f, 0.62f, 0.60f), Color(0.50f, 0.44f, 0.70f), Color(0.76f, 0.80f, 0.52f) };
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.1f;
		meshutil::add_sphere(this, toon, 0.020f,
				Vector3(0.38f + std::cos(a) * 0.018f, 0.045f, std::sin(a) * 0.018f), glass_cols[i]);
	}

	// 蜡烛
	Ref<CylinderMesh> candle;
	candle.instantiate();
	candle->set_top_radius(0.020f);
	candle->set_bottom_radius(0.023f);
	candle->set_height(0.11f);
	candle->set_radial_segments(8);
	candle->set_rings(1);
	candle->set_material(meshutil::toon_material(toon, Color(0.94f, 0.90f, 0.78f), 0.2f));
	MeshInstance3D *cm = memnew(MeshInstance3D);
	cm->set_mesh(candle);
	cm->set_position(Vector3(-0.48f, 0.08f, 0.0f));
	add_child(cm);
	Ref<CylinderMesh> flame_mesh;
	flame_mesh.instantiate();
	flame_mesh->set_top_radius(0.001f);
	flame_mesh->set_bottom_radius(0.013f);
	flame_mesh->set_height(0.04f);
	flame_mesh->set_radial_segments(6);
	flame_mesh->set_rings(1);
	Ref<StandardMaterial3D> fmat;
	fmat.instantiate();
	fmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fmat->set_albedo(Color(1.0f, 0.76f, 0.32f));
	fmat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	flame_mesh->set_material(fmat);
	flame = memnew(MeshInstance3D);
	flame->set_mesh(flame_mesh);
	flame->set_position(Vector3(-0.48f, 0.16f, 0.0f));
	flame->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(flame);
}
