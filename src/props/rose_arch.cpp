#include "rose_arch.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

void RoseArch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &RoseArch::get_item_types);
	ClassDB::bind_method(D_METHOD("get_sway"), &RoseArch::get_sway);
}
void RoseArch::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			sway = std::sin(t * 1.1f) * 0.05f;
			if (arch != nullptr) arch->set_rotation(Vector3(0.0f, 0.0f, sway));
		} break;
		default: break;
	}
}
void RoseArch::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 拱门（会轻轻摆的上半部分）
	arch = memnew(Node3D);
	arch->set_name("Arch");
	add_child(arch);
	const Color wood(0.52f, 0.38f, 0.24f);
	meshutil::add_box(arch, toon, Vector3(0.09f, 1.70f, 0.09f), Vector3(-0.62f, 0.85f, 0.0f), wood);
	meshutil::add_box(arch, toon, Vector3(0.09f, 1.70f, 0.09f), Vector3(0.62f, 0.85f, 0.0f), wood);
	// 顶部用几块短木板拼成弧
	for (int i = 0; i < 5; i++) {
		const float a = 3.14159f - (float)i * 0.7854f;
		meshutil::add_box(arch, toon, Vector3(0.30f, 0.07f, 0.09f),
				Vector3(std::cos(a) * 0.58f, 1.70f + std::sin(a) * 0.30f, 0.0f), wood, Vector3(0.0f, 0.0f, a - 1.5708f));
	}
	// 藤与花
	for (int i = 0; i < 14; i++) {
		const float a = (float)i * 0.45f;
		const float x = (i < 7) ? -0.62f + 0.03f * std::sin(a) : 0.62f + 0.03f * std::sin(a);
		const float y = 0.15f + 0.11f * (float)(i % 7);
		meshutil::add_sphere(this, toon, 0.045f, Vector3(x, y, 0.05f), Color(0.30f, 0.50f, 0.24f));
		if (i % 3 == 0) {
			meshutil::add_sphere(this, toon, 0.05f, Vector3(x + 0.03f, y + 0.04f, 0.07f),
					(i % 6 == 0) ? Color(0.84f, 0.30f, 0.34f) : Color(0.90f, 0.56f, 0.62f));
		}
	}
	for (int i = 0; i < 3; i++) {
		const float a = 3.14159f - (float)i * 1.0f;
		meshutil::add_sphere(this, toon, 0.05f, Vector3(std::cos(a) * 0.55f, 1.78f + std::sin(a) * 0.25f, 0.05f), Color(0.86f, 0.32f, 0.36f));
	}
	// 飘落花瓣
	Ref<SphereMesh> pmesh;
	pmesh.instantiate();
	pmesh->set_radius(0.025f);
	pmesh->set_height(0.012f);
	pmesh->set_radial_segments(5);
	pmesh->set_rings(3);
	Ref<StandardMaterial3D> pm;
	pm.instantiate();
	pm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	pm->set_albedo(Color(0.88f, 0.42f, 0.46f));
	pm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	pmesh->set_material(pm);
	petals = memnew(CPUParticles3D);
	petals->set_name("Petals");
	petals->set_amount(18);
	petals->set_lifetime(6.0f);
	petals->set_mesh(pmesh);
	petals->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	petals->set_emission_box_extents(Vector3(0.7f, 0.6f, 0.2f));
	petals->set_direction(Vector3(0.0f, -1.0f, 0.0f));
	petals->set_spread(40.0f);
	petals->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.02f);
	petals->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	petals->set_gravity(Vector3(0.15f, -0.10f, 0.05f));
	petals->set_position(Vector3(0.0f, 1.5f, 0.0f));
	petals->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(petals);
}
