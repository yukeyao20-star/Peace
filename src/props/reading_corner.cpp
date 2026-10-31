#include "reading_corner.h"

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

void ReadingCorner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &ReadingCorner::get_item_types);
	ClassDB::bind_method(D_METHOD("get_lamp_on"), &ReadingCorner::get_lamp_on);
	ClassDB::bind_method(D_METHOD("get_steam_on"), &ReadingCorner::get_steam_on);
}

void ReadingCorner::_notification(int p_what) {
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
			if (sky != nullptr) {
				tod = sky->get_time_of_day();
			}
			lamp_on = tod < 0.26f || tod > 0.74f;
			if (bulb != nullptr) {
				bulb->set_visible(lamp_on);
				const float pulse = 0.92f + 0.08f * std::sin(t * 2.1f);
				bulb->set_scale(Vector3(pulse, pulse, pulse));
			}
			steam_on = tod > 0.20f && tod < 0.46f;
			if (steam != nullptr && steam->is_emitting() != steam_on) {
				steam->set_emitting(steam_on);
			}
		} break;
		default:
			break;
	}
}

void ReadingCorner::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	set_rotation(Vector3(0.0f, -1.5708f, 0.0f)); // 贴右墙朝屋里
	const Color fabric(0.52f, 0.42f, 0.36f);
	const Color wood(0.48f, 0.33f, 0.19f);

	// 扶手椅
	meshutil::add_box(this, toon, Vector3(0.58f, 0.18f, 0.58f), Vector3(0.0f, 0.32f, 0.0f), fabric);
	meshutil::add_box(this, toon, Vector3(0.58f, 0.62f, 0.14f), Vector3(0.0f, 0.64f, -0.24f), Color(0.48f, 0.38f, 0.32f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.28f, 0.55f), Vector3(-0.30f, 0.51f, 0.0f), fabric);
	meshutil::add_box(this, toon, Vector3(0.10f, 0.28f, 0.55f), Vector3(0.30f, 0.51f, 0.0f), fabric);
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.24f : 0.24f;
		const float sz = (i < 2) ? -0.22f : 0.22f;
		meshutil::add_box(this, toon, Vector3(0.05f, 0.24f, 0.05f), Vector3(sx, 0.12f, sz), wood);
	}
	// 坐垫
	meshutil::add_box(this, toon, Vector3(0.46f, 0.08f, 0.46f), Vector3(0.0f, 0.44f, 0.02f), Color(0.72f, 0.46f, 0.34f));
	// 毛毯搭在右扶手
	meshutil::add_box(this, toon, Vector3(0.30f, 0.02f, 0.52f), Vector3(0.34f, 0.55f, 0.02f), Color(0.55f, 0.30f, 0.28f), Vector3(0.0f, 0.0f, -0.12f));
	meshutil::add_box(this, toon, Vector3(0.30f, 0.24f, 0.02f), Vector3(0.34f, 0.44f, 0.26f), Color(0.55f, 0.30f, 0.28f));

	// 边桌
	meshutil::add_box(this, toon, Vector3(0.42f, 0.045f, 0.42f), Vector3(-0.62f, 0.46f, 0.10f), wood);
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.094f;
		meshutil::add_box(this, toon, Vector3(0.035f, 0.44f, 0.035f),
				Vector3(-0.62f + std::cos(a) * 0.15f, 0.22f, 0.10f + std::sin(a) * 0.15f), wood);
	}

	// 茶杯
	Ref<CylinderMesh> cup;
	cup.instantiate();
	cup->set_top_radius(0.040f);
	cup->set_bottom_radius(0.032f);
	cup->set_height(0.055f);
	cup->set_radial_segments(9);
	cup->set_rings(1);
	cup->set_material(meshutil::toon_material(toon, Color(0.92f, 0.90f, 0.84f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(cup);
	cmi->set_position(Vector3(-0.62f, 0.51f, 0.10f));
	add_child(cmi);
	// 茶的热气
	Ref<SphereMesh> puff;
	puff.instantiate();
	puff->set_radius(0.020f);
	puff->set_height(0.040f);
	puff->set_radial_segments(5);
	puff->set_rings(3);
	Ref<StandardMaterial3D> pm;
	pm.instantiate();
	pm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	pm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	pm->set_albedo(Color(0.95f, 0.95f, 0.93f, 0.18f));
	pm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	puff->set_material(pm);
	steam = memnew(CPUParticles3D);
	steam->set_name("TeaSteam");
	steam->set_amount(8);
	steam->set_lifetime(1.5f);
	steam->set_mesh(puff);
	steam->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	steam->set_spread(16.0f);
	steam->set_gravity(Vector3(0.01f, 0.10f, 0.0f));
	steam->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.03f);
	steam->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	steam->set_position(Vector3(-0.62f, 0.55f, 0.10f));
	steam->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	steam->set_emitting(false);
	add_child(steam);

	// 落地灯
	Ref<CylinderMesh> pole;
	pole.instantiate();
	pole->set_top_radius(0.015f);
	pole->set_bottom_radius(0.022f);
	pole->set_height(1.30f);
	pole->set_radial_segments(6);
	pole->set_rings(1);
	pole->set_material(meshutil::toon_material(toon, Color(0.26f, 0.24f, 0.22f), 0.2f));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_mesh(pole);
	pmi->set_position(Vector3(-0.95f, 0.66f, -0.10f));
	add_child(pmi);
	Ref<CylinderMesh> shade;
	shade.instantiate();
	shade->set_top_radius(0.12f);
	shade->set_bottom_radius(0.23f);
	shade->set_height(0.22f);
	shade->set_radial_segments(10);
	shade->set_rings(1);
	shade->set_material(meshutil::toon_material(toon, Color(0.90f, 0.82f, 0.62f), 0.2f));
	MeshInstance3D *smi = memnew(MeshInstance3D);
	smi->set_mesh(shade);
	smi->set_position(Vector3(-0.95f, 1.45f, -0.10f));
	add_child(smi);
	// 灯泡
	Ref<SphereMesh> bulb_mesh;
	bulb_mesh.instantiate();
	bulb_mesh->set_radius(0.075f);
	bulb_mesh->set_height(0.15f);
	bulb_mesh->set_radial_segments(8);
	bulb_mesh->set_rings(4);
	Ref<StandardMaterial3D> bm;
	bm.instantiate();
	bm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	bm->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	bm->set_albedo(Color(1.0f, 0.78f, 0.38f, 0.45f));
	bm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	bulb_mesh->set_material(bm);
	bulb = memnew(MeshInstance3D);
	bulb->set_mesh(bulb_mesh);
	bulb->set_position(Vector3(-0.95f, 1.38f, -0.10f));
	bulb->set_visible(false);
	bulb->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(bulb);
}
