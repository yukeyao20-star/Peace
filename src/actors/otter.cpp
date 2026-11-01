#include "otter.h"
#include <cmath>
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

void Otter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_diving"), &Otter::is_diving);
	ClassDB::bind_method(D_METHOD("is_on_back"), &Otter::is_on_back);
	ClassDB::bind_method(D_METHOD("is_visible_creature"), &Otter::is_visible_creature);
}
void Otter::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			const bool icy = sky != nullptr && sky->get_snow_amount() > 0.6f;
			set_visible(!icy);
			if (icy) return;
			// 沿椭圆慢慢游
			const float a = t * 0.22f;
			const float rx = 1.8f;
			const float rz = 1.2f;
			const Vector3 center(-16.0f, 0.0f, -10.6f);
			const Vector3 pos(center.x + std::cos(a) * rx, 0.0f, center.z + std::sin(a) * rz);
			const Vector3 d(-std::sin(a) * rx, 0.0f, std::cos(a) * rz);
			const float yaw = std::atan2(-d.z, d.x);
			// 潜水周期
			next_dive -= delta;
			if (!diving && next_dive <= 0.0f) {
				diving = true;
				dive_t = 2.4f;
				next_dive = 16.0f + 6.0f * std::sin(t * 0.7f);
			}
			float y = water_level + 0.10f + 0.02f * std::sin(t * 2.2f);
			float pitch = std::sin(t * 1.3f) * 0.05f;
			float roll = 0.0f;
			if (diving) {
				dive_t -= delta;
				const float k = Math::clamp(1.0f - dive_t / 2.4f, 0.0f, 1.0f);
				const float depth = std::sin(k * 3.14159f);
				y = water_level + 0.10f - 0.95f * depth;
				pitch = -0.5f * depth;
				if (dive_t <= 0.0f) diving = false;
			}
			// 偶尔仰泳
			on_back = !diving && std::sin(t * 0.17f) > 0.55f;
			if (on_back) {
				roll = 3.14159f;
				y += 0.06f;
			}
			set_position(Vector3(pos.x, y, pos.z));
			set_rotation(Vector3(roll * 0.0f, yaw, 0.0f));
			if (body != nullptr) body->set_rotation(Vector3(roll, pitch, 0.0f));
			// 水面涟漪
			if (ripple != nullptr) {
				const bool vis = diving || on_back;
				ripple->set_visible(vis);
				if (vis) {
					ripple->set_position(Vector3(pos.x, water_level + 0.015f, pos.z));
					const float ph = std::fmod(t * 0.9f, 1.0f);
					const float s = 0.6f + 1.8f * ph;
					ripple->set_scale(Vector3(s, 1.0f, s));
				}
			}
		} break;
		default: break;
	}
}
void Otter::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) water_level = terrain->get_water_level();
	}
	set_position(Vector3(-16.0f, water_level + 0.10f, -10.6f));
	body = memnew(Node3D);
	body->set_name("Body");
	add_child(body);
	// 身体与浅色肚皮
	MeshInstance3D *torso = meshutil::add_sphere(body, toon, 0.22f, Vector3(0.0f, 0.0f, 0.0f), Color(0.46f, 0.32f, 0.20f));
	torso->set_scale(Vector3(1.75f, 0.62f, 0.78f));
	MeshInstance3D *belly = meshutil::add_sphere(body, toon, 0.20f, Vector3(0.02f, -0.055f, 0.0f), Color(0.78f, 0.70f, 0.58f));
	belly->set_scale(Vector3(1.45f, 0.35f, 0.65f));
	// 头、吻、耳朵、眼睛
	meshutil::add_sphere(body, toon, 0.14f, Vector3(0.34f, 0.10f, 0.0f), Color(0.48f, 0.34f, 0.22f));
	meshutil::add_sphere(body, toon, 0.075f, Vector3(0.46f, 0.06f, 0.0f), Color(0.80f, 0.72f, 0.60f))->set_scale(Vector3(1.2f, 0.8f, 1.0f));
	for (int s = 0; s < 2; s++) {
		const float z = s == 0 ? -0.085f : 0.085f;
		meshutil::add_sphere(body, toon, 0.035f, Vector3(0.27f, 0.20f, z), Color(0.42f, 0.30f, 0.20f));
		meshutil::add_sphere(body, toon, 0.018f, Vector3(0.44f, 0.14f, z * 0.75f), Color(0.10f, 0.09f, 0.08f))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		meshutil::add_box(body, toon, Vector3(0.10f, 0.006f, 0.006f), Vector3(0.50f, 0.055f, z), Color(0.88f, 0.86f, 0.80f), Vector3(0.0f, 0.15f * (float)(s == 0 ? -1 : 1), 0.0f));
	}
	// 尾巴
	meshutil::add_box(body, toon, Vector3(0.42f, 0.06f, 0.10f), Vector3(-0.34f, -0.02f, 0.0f), Color(0.44f, 0.30f, 0.19f), Vector3(0.0f, 0.0f, 0.10f));
	// 前爪
	for (int s = 0; s < 2; s++) {
		meshutil::add_sphere(body, toon, 0.045f, Vector3(0.20f, -0.10f, s == 0 ? -0.13f : 0.13f), Color(0.40f, 0.28f, 0.18f));
	}
	// 涟漪环
	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.22f);
	ring->set_outer_radius(0.27f);
	ring->set_rings(16);
	ring->set_ring_segments(3);
	Ref<StandardMaterial3D> rm;
	rm.instantiate();
	rm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	rm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	rm->set_albedo(Color(0.82f, 0.92f, 0.96f, 0.5f));
	rm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ring->set_material(rm);
	ripple = memnew(MeshInstance3D);
	ripple->set_mesh(ring);
	ripple->set_material_override(rm);
	ripple->set_visible(false);
	ripple->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(ripple);
}
