#include "kitchen_shelf.h"

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

using namespace godot;

void KitchenShelf::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &KitchenShelf::get_item_types);
	ClassDB::bind_method(D_METHOD("get_flame_on"), &KitchenShelf::get_flame_on);
	ClassDB::bind_method(D_METHOD("get_steam_on"), &KitchenShelf::get_steam_on);
	ClassDB::bind_method(D_METHOD("get_herbs_sway"), &KitchenShelf::get_herbs_sway);
}

void KitchenShelf::_notification(int p_what) {
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
			// 烛火白天也点着，夜里更稳一点；每天清晨壶嘴冒热气
			flame_on = true;
			if (flame != nullptr) {
				const float f = 0.85f + 0.15f * std::sin(t * 7.0f) + 0.1f * std::sin(t * 13.0f + 1.2f);
				flame->set_scale(Vector3(0.8f * f, 1.0f * f, 0.8f * f));
			}
			steam_on = tod > 0.20f && tod < 0.42f;
			if (steam != nullptr && steam->is_emitting() != steam_on) {
				steam->set_emitting(steam_on);
			}
			herbs_sway = std::sin(t * 1.1f) * 0.06f + std::sin(t * 2.3f) * 0.02f;
			if (herbs != nullptr) {
				herbs->set_rotation(Vector3(0.0f, 0.0f, herbs_sway));
			}
		} break;
		default:
			break;
	}
}

void KitchenShelf::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	set_rotation(Vector3(0.0f, -1.5708f, 0.0f)); // 贴右墙朝屋里
	const Color wood(0.48f, 0.33f, 0.19f);
	const Color wood_light(0.60f, 0.44f, 0.26f);

	// 搁板与托架
	meshutil::add_box(this, toon, Vector3(1.00f, 0.05f, 0.28f), Vector3(0.0f, 0.0f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(0.05f, 0.18f, 0.20f), Vector3(-0.38f, -0.10f, -0.06f), wood);
	meshutil::add_box(this, toon, Vector3(0.05f, 0.18f, 0.20f), Vector3(0.38f, -0.10f, -0.06f), wood);
	meshutil::add_box(this, toon, Vector3(1.00f, 0.03f, 0.30f), Vector3(0.0f, 0.04f, 0.0f), Color(0.70f, 0.62f, 0.46f)); // 垫布

	// 陶茶壶
	MeshInstance3D *body = meshutil::add_sphere(this, toon, 0.085f, Vector3(-0.30f, 0.13f, 0.0f), Color(0.56f, 0.66f, 0.62f));
	body->set_scale(Vector3(1.1f, 0.9f, 1.0f));
	Ref<CylinderMesh> spout;
	spout.instantiate();
	spout->set_top_radius(0.015f);
	spout->set_bottom_radius(0.030f);
	spout->set_height(0.12f);
	spout->set_radial_segments(6);
	spout->set_rings(1);
	spout->set_material(meshutil::toon_material(toon, Color(0.50f, 0.60f, 0.56f), 0.2f));
	MeshInstance3D *sp = memnew(MeshInstance3D);
	sp->set_mesh(spout);
	sp->set_position(Vector3(-0.20f, 0.15f, 0.0f));
	sp->set_rotation(Vector3(0.0f, 0.0f, -0.9f));
	add_child(sp);
	Ref<TorusMesh> handle;
	handle.instantiate();
	handle->set_inner_radius(0.05f);
	handle->set_outer_radius(0.062f);
	handle->set_rings(14);
	handle->set_ring_segments(3);
	handle->set_material(meshutil::toon_material(toon, Color(0.50f, 0.60f, 0.56f), 0.2f));
	MeshInstance3D *hm = memnew(MeshInstance3D);
	hm->set_mesh(handle);
	hm->set_position(Vector3(-0.38f, 0.15f, 0.0f));
	hm->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	add_child(hm);
	meshutil::add_sphere(this, toon, 0.030f, Vector3(-0.30f, 0.215f, 0.0f), Color(0.62f, 0.70f, 0.66f));

	// 两只茶杯与碟
	for (int i = 0; i < 2; i++) {
		const float x = -0.08f + 0.14f * (float)i;
		Ref<CylinderMesh> cup;
		cup.instantiate();
		cup->set_top_radius(0.036f);
		cup->set_bottom_radius(0.028f);
		cup->set_height(0.05f);
		cup->set_radial_segments(9);
		cup->set_rings(1);
		cup->set_material(meshutil::toon_material(toon, Color(0.92f, 0.90f, 0.84f), 0.2f));
		MeshInstance3D *c = memnew(MeshInstance3D);
		c->set_mesh(cup);
		c->set_position(Vector3(x, 0.055f, 0.03f));
		add_child(c);
		Ref<CylinderMesh> saucer;
		saucer.instantiate();
		saucer->set_top_radius(0.052f);
		saucer->set_bottom_radius(0.045f);
		saucer->set_height(0.010f);
		saucer->set_radial_segments(10);
		saucer->set_rings(1);
		saucer->set_material(meshutil::toon_material(toon, Color(0.94f, 0.92f, 0.86f), 0.2f));
		MeshInstance3D *sa = memnew(MeshInstance3D);
		sa->set_mesh(saucer);
		sa->set_position(Vector3(x, 0.025f, 0.03f));
		add_child(sa);
	}

	// 两罐果酱
	for (int i = 0; i < 2; i++) {
		const float x = 0.22f + 0.10f * (float)i;
		Ref<CylinderMesh> jar;
		jar.instantiate();
		jar->set_top_radius(0.040f);
		jar->set_bottom_radius(0.036f);
		jar->set_height(0.105f);
		jar->set_radial_segments(9);
		jar->set_rings(1);
		Ref<StandardMaterial3D> jm;
		jm.instantiate();
		jm->set_albedo(Color(0.72f, 0.20f, 0.16f, 0.82f));
		jm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		jm->set_roughness(0.18f);
		jar->set_material(jm);
		MeshInstance3D *j = memnew(MeshInstance3D);
		j->set_mesh(jar);
		j->set_position(Vector3(x, 0.085f, -0.02f));
		add_child(j);
		meshutil::add_box(this, toon, Vector3(0.078f, 0.020f, 0.078f), Vector3(x, 0.145f, -0.02f), Color(0.86f, 0.78f, 0.58f));
	}

	// 面包篮
	meshutil::add_box(this, toon, Vector3(0.18f, 0.055f, 0.12f), Vector3(0.33f, 0.06f, 0.14f), Color(0.66f, 0.48f, 0.26f));
	MeshInstance3D *bread = meshutil::add_sphere(this, toon, 0.065f, Vector3(0.33f, 0.11f, 0.14f), Color(0.80f, 0.58f, 0.30f));
	bread->set_scale(Vector3(1.15f, 0.65f, 0.8f));

	// 蜡烛与火苗
	Ref<CylinderMesh> candle;
	candle.instantiate();
	candle->set_top_radius(0.022f);
	candle->set_bottom_radius(0.025f);
	candle->set_height(0.13f);
	candle->set_radial_segments(8);
	candle->set_rings(1);
	candle->set_material(meshutil::toon_material(toon, Color(0.94f, 0.90f, 0.78f), 0.2f));
	MeshInstance3D *cm = memnew(MeshInstance3D);
	cm->set_mesh(candle);
	cm->set_position(Vector3(0.06f, 0.085f, -0.06f));
	add_child(cm);
	Ref<CylinderMesh> holder;
	holder.instantiate();
	holder->set_top_radius(0.040f);
	holder->set_bottom_radius(0.045f);
	holder->set_height(0.018f);
	holder->set_radial_segments(8);
	holder->set_rings(1);
	holder->set_material(meshutil::toon_material(toon, Color(0.62f, 0.52f, 0.28f), 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(holder);
	hmi->set_position(Vector3(0.06f, 0.017f, -0.06f));
	add_child(hmi);
	Ref<CylinderMesh> flame_mesh;
	flame_mesh.instantiate();
	flame_mesh->set_top_radius(0.001f);
	flame_mesh->set_bottom_radius(0.014f);
	flame_mesh->set_height(0.045f);
	flame_mesh->set_radial_segments(6);
	flame_mesh->set_rings(1);
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fm->set_albedo(Color(1.0f, 0.76f, 0.32f));
	fm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	flame_mesh->set_material(fm);
	flame = memnew(MeshInstance3D);
	flame->set_mesh(flame_mesh);
	flame->set_position(Vector3(0.06f, 0.172f, -0.06f));
	flame->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(flame);

	// 板下挂着的干香草
	herbs = memnew(Node3D);
	herbs->set_name("DriedHerbs");
	herbs->set_position(Vector3(-0.44f, -0.02f, 0.06f));
	add_child(herbs);
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(herbs, toon, Vector3(0.030f, 0.22f, 0.030f),
				Vector3((float)(i - 1) * 0.035f, -0.11f, 0.0f), Color(0.34f, 0.42f, 0.24f),
				Vector3(0.0f, 0.0f, (float)(i - 1) * 0.12f));
	}
	meshutil::add_box(herbs, toon, Vector3(0.12f, 0.015f, 0.015f), Vector3(0.0f, 0.0f, 0.0f), Color(0.60f, 0.50f, 0.32f));

	// 壶嘴的热气
	Ref<SphereMesh> puff;
	puff.instantiate();
	puff->set_radius(0.022f);
	puff->set_height(0.044f);
	puff->set_radial_segments(5);
	puff->set_rings(3);
	Ref<StandardMaterial3D> pm;
	pm.instantiate();
	pm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	pm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	pm->set_albedo(Color(0.94f, 0.95f, 0.93f, 0.20f));
	pm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	puff->set_material(pm);
	steam = memnew(CPUParticles3D);
	steam->set_name("TeaSteam");
	steam->set_amount(10);
	steam->set_lifetime(1.6f);
	steam->set_mesh(puff);
	steam->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	steam->set_spread(18.0f);
	steam->set_gravity(Vector3(0.02f, 0.12f, 0.0f));
	steam->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.04f);
	steam->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.12f);
	steam->set_position(Vector3(-0.16f, 0.21f, 0.0f));
	steam->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	steam->set_emitting(false);
	add_child(steam);
}
