#include "honey_stand.h"
#include <cmath>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void HoneyStand::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &HoneyStand::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &HoneyStand::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &HoneyStand::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &HoneyStand::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &HoneyStand::interact);
	ClassDB::bind_method(D_METHOD("get_tastes"), &HoneyStand::get_tastes);
	ClassDB::bind_method(D_METHOD("get_bees_visible"), &HoneyStand::get_bees_visible);
}
String HoneyStand::get_prompt() const { return String::utf8("按 E 尝一口蜂蜜"); }
Vector3 HoneyStand::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.85f, 0.0f); }
void HoneyStand::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	tastes++;
	tilt = 0.5f;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("一勺蜂蜜 · 有野花、阳光和一点木头烟味"), 2.8f);
	}
}
void HoneyStand::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (jar != nullptr) {
				tilt = Math::move_toward(tilt, 0.0f, delta * 1.4f);
				jar->set_rotation(Vector3(0.0f, 0.0f, -tilt));
			}
			const bool bees_on = sky != nullptr && sky->get_snow_amount() < 0.1f && sky->get_rain_amount() < 0.4f && sky->get_time_of_day() > 0.3f && sky->get_time_of_day() < 0.7f;
			for (int i = 0; i < 2; i++) {
				if (bees[i] == nullptr) continue;
				bees[i]->set_visible(bees_on);
				if (bees_on) {
					const float a = t * (1.8f + 0.5f * (float)i) + (float)i * 2.2f;
					bees[i]->set_position(Vector3(0.18f + std::cos(a) * 0.14f, 0.93f + 0.06f * std::sin(a * 2.4f), 0.02f + std::sin(a) * 0.12f));
				}
			}
		} break;
		default: break;
	}
}
void HoneyStand::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 0.9f, 0.0f));
	// 木板桌面与四腿
	meshutil::add_box(this, toon, Vector3(0.96f, 0.05f, 0.52f), Vector3(0.0f, 0.70f, 0.0f), Color(0.56f, 0.41f, 0.25f));
	for (int i = 0; i < 4; i++) {
		const float x = (i % 2 == 0) ? -0.40f : 0.40f;
		const float z = (i < 2) ? -0.20f : 0.20f;
		meshutil::add_box(this, toon, Vector3(0.05f, 0.68f, 0.05f), Vector3(x, 0.35f, z), Color(0.46f, 0.33f, 0.20f));
	}
	meshutil::add_box(this, toon, Vector3(0.86f, 0.03f, 0.04f), Vector3(0.0f, 0.30f, 0.0f), Color(0.46f, 0.33f, 0.20f));
	// 三罐蜂蜜
	Ref<CylinderMesh> jar_mesh;
	jar_mesh.instantiate();
	jar_mesh->set_top_radius(0.075f);
	jar_mesh->set_bottom_radius(0.068f);
	jar_mesh->set_height(0.16f);
	jar_mesh->set_radial_segments(10);
	jar_mesh->set_rings(1);
	Ref<StandardMaterial3D> jm;
	jm.instantiate();
	jm->set_albedo(Color(0.86f, 0.55f, 0.14f));
	jm->set_roughness(0.15f);
	jm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	jm->set_albedo(Color(0.86f, 0.55f, 0.14f, 0.88f));
	jar_mesh->set_material(jm);
	for (int i = 0; i < 3; i++) {
		Node3D *holder = memnew(Node3D);
		holder->set_position(Vector3(-0.28f + 0.28f * (float)i, 0.735f, 0.0f));
		add_child(holder);
		godot::MeshInstance3D *jmi = memnew(godot::MeshInstance3D);
		jmi->set_mesh(jar_mesh);
		jmi->set_position(Vector3(0.0f, 0.08f, 0.0f));
		holder->add_child(jmi);
		meshutil::add_box(holder, toon, Vector3(0.13f, 0.02f, 0.13f), Vector3(0.0f, 0.165f, 0.0f), Color(0.62f, 0.45f, 0.26f));
		if (i == 1) jar = holder;
	}
	// 木勺斜插在中间罐里
	meshutil::add_box(this, toon, Vector3(0.018f, 0.24f, 0.018f), Vector3(0.0f, 0.90f, 0.0f), Color(0.66f, 0.50f, 0.30f), Vector3(0.0f, 0.0f, 0.35f));
	meshutil::add_box(this, toon, Vector3(0.045f, 0.015f, 0.05f), Vector3(0.075f, 0.795f, 0.0f), Color(0.70f, 0.54f, 0.32f), Vector3(0.0f, 0.0f, 0.35f));
	// 蜂巢块
	meshutil::add_box(this, toon, Vector3(0.24f, 0.10f, 0.18f), Vector3(0.30f, 0.775f, 0.14f), Color(0.88f, 0.66f, 0.20f));
	for (int hx = 0; hx < 3; hx++) {
		for (int hz = 0; hz < 2; hz++) {
			meshutil::add_box(this, toon, Vector3(0.055f, 0.02f, 0.055f), Vector3(0.23f + 0.065f * (float)hx, 0.832f, 0.09f + 0.07f * (float)hz), Color(0.74f, 0.52f, 0.12f));
		}
	}
	// 小蜂
	for (int i = 0; i < 2; i++) {
		Node3D *bee = memnew(Node3D);
		bee->set_name(String("Bee") + String::num_int64(i));
		bee->set_visible(false);
		add_child(bee);
		bees[i] = bee;
		meshutil::add_sphere(bee, toon, 0.018f, Vector3(), Color(0.92f, 0.76f, 0.22f))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		meshutil::add_sphere(bee, toon, 0.011f, Vector3(0.0f, -0.012f, 0.0f), Color(0.16f, 0.14f, 0.10f))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	}
}
