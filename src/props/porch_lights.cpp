#include "porch_lights.h"
#include <cmath>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
using namespace godot;

void PorchLights::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &PorchLights::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &PorchLights::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &PorchLights::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &PorchLights::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &PorchLights::interact);
	ClassDB::bind_method(D_METHOD("is_lit"), &PorchLights::is_lit);
	ClassDB::bind_method(D_METHOD("is_enabled"), &PorchLights::is_enabled);
	ClassDB::bind_method(D_METHOD("get_bulb_count"), &PorchLights::get_bulb_count);
}
String PorchLights::get_prompt() const { return enabled ? String::utf8("按 E 关掉小灯串") : String::utf8("按 E 点亮小灯串"); }
Vector3 PorchLights::get_interact_pos() const { return get_global_position() + Vector3(0.0f, -1.0f, 0.0f); }
void PorchLights::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	enabled = !enabled;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", enabled ? String::utf8("门廊的小灯串亮了 · 一串暖黄") : String::utf8("小灯串歇下了 · 只剩瓦檐的风"), 2.6f);
	}
}
void PorchLights::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			const float tod = sky != nullptr ? sky->get_time_of_day() : 0.5f;
			const bool night = tod < 0.26f || tod > 0.74f;
			lit = enabled && night;
			if (bulb_mat.is_valid()) bulb_mat->set_emission_energy_multiplier(lit ? 2.4f : 0.06f);
			for (int i = 0; i < 2; i++) {
				if (lamps[i] == nullptr) continue;
				lamps[i]->set_visible(lit);
				lamps[i]->set_param(Light3D::PARAM_ENERGY, 0.20f + 0.03f * std::sin(t * 2.1f + (float)i));
			}
			if (bulb_mmi != nullptr) bulb_mmi->set_visible(true);
			if (bulbs.is_valid()) {
				const Basis base = Basis().scaled(Vector3(1.0f, 1.0f, 1.0f));
				for (int i = 0; i < BULB_COUNT; i++) {
					const float tw = lit ? (0.92f + 0.10f * std::sin(t * 3.1f + (float)i * 1.7f)) : 1.0f;
					const Basis b = base.scaled(Vector3(tw, tw, tw));
					const Vector3 sway(std::sin(t * 1.1f + (float)i * 0.4f) * 0.012f, 0.0f, 0.0f);
					bulbs->set_instance_transform(i, Transform3D(b, bulb_pos[i] + sway));
				}
			}
		} break;
		default: break;
	}
}
void PorchLights::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 电线：四段悬链，用细圆柱拼成
	Ref<CylinderMesh> wire_mesh;
	wire_mesh.instantiate();
	wire_mesh->set_top_radius(0.006f);
	wire_mesh->set_bottom_radius(0.006f);
	wire_mesh->set_height(1.0f);
	wire_mesh->set_radial_segments(4);
	wire_mesh->set_rings(1);
	Ref<Material> wire_mat = meshutil::toon_material(toon, Color(0.18f, 0.17f, 0.15f), 0.1f);
	const float anchors[4] = { -2.35f, -0.78f, 0.78f, 2.35f };
	const float sag = 0.14f;
	Vector3 pts[25];
	int pi = 0;
	for (int seg = 0; seg < 3; seg++) {
		for (int s = 0; s < 8; s++) {
			const float u = (float)s / 8.0f;
			const float x = anchors[seg] + (anchors[seg + 1] - anchors[seg]) * u;
			const float y = -sag * 4.0f * u * (1.0f - u);
			pts[pi++] = Vector3(x, y, 0.0f);
		}
	}
	pts[pi++] = Vector3(anchors[3], 0.0f, 0.0f);
	Ref<MultiMesh> wm;
	wm.instantiate();
	wm->set_transform_format(MultiMesh::TRANSFORM_3D);
	wm->set_mesh(wire_mesh);
	wm->set_instance_count(pi - 1);
	for (int i = 0; i < pi - 1; i++) {
		const Vector3 d = pts[i + 1] - pts[i];
		const Vector3 y = d.normalized();
		Vector3 ref(0.0f, 0.0f, 1.0f);
		if (std::fabs(y.z) > 0.9f) ref = Vector3(1.0f, 0.0f, 0.0f);
		const Vector3 x = ref.cross(y).normalized();
		const Vector3 z = x.cross(y).normalized();
		Basis b;
		b.set_column(0, x);
		b.set_column(1, y);
		b.set_column(2, z);
		b = b.scaled(Vector3(1.0f, d.length(), 1.0f));
		wm->set_instance_transform(i, Transform3D(b, (pts[i] + pts[i + 1]) * 0.5f));
	}
	MultiMeshInstance3D *wmi = memnew(MultiMeshInstance3D);
	wmi->set_name("Wire");
	wmi->set_multimesh(wm);
	wmi->set_material_override(wire_mat);
	wmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(wmi);
	// 灯泡：16 颗暖色小玻璃珠
	Ref<SphereMesh> bulb_mesh;
	bulb_mesh.instantiate();
	bulb_mesh->set_radius(0.032f);
	bulb_mesh->set_height(0.052f);
	bulb_mesh->set_radial_segments(8);
	bulb_mesh->set_rings(4);
	bulb_mat.instantiate();
	bulb_mat->set_albedo(Color(1.0f, 0.80f, 0.46f));
	bulb_mat->set_roughness(0.2f);
	bulb_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	bulb_mat->set_emission(Color(1.0f, 0.72f, 0.36f));
	bulb_mat->set_emission_energy_multiplier(0.06f);
	bulb_mesh->set_material(bulb_mat);
	bulbs.instantiate();
	bulbs->set_transform_format(MultiMesh::TRANSFORM_3D);
	bulbs->set_mesh(bulb_mesh);
	bulbs->set_instance_count(BULB_COUNT);
	for (int i = 0; i < BULB_COUNT; i++) {
		const float x = -2.25f + 0.30f * (float)i;
		float y = -0.10f;
		for (int seg = 0; seg < 3; seg++) {
			if (x >= anchors[seg] && x <= anchors[seg + 1]) {
				const float u = (x - anchors[seg]) / (anchors[seg + 1] - anchors[seg]);
				y = -sag * 4.0f * u * (1.0f - u) - 0.085f;
				break;
			}
		}
		bulb_pos[i] = Vector3(x, y, 0.0f);
		bulbs->set_instance_transform(i, Transform3D(Basis(), bulb_pos[i]));
	}
	bulb_mmi = memnew(MultiMeshInstance3D);
	bulb_mmi->set_name("Bulbs");
	bulb_mmi->set_multimesh(bulbs);
	bulb_mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(bulb_mmi);
	// 两盏很弱的暖光，把门廊染暖
	for (int i = 0; i < 2; i++) {
		OmniLight3D *l = memnew(OmniLight3D);
		l->set_position(Vector3(i == 0 ? -1.4f : 1.4f, -0.45f, 0.22f));
		l->set_color(Color(1.0f, 0.76f, 0.44f));
		l->set_param(Light3D::PARAM_ENERGY, 0.3f);
		l->set_param(Light3D::PARAM_RANGE, 2.4f);
		l->set_shadow(false);
		l->set_visible(false);
		add_child(l);
		lamps[i] = l;
	}
}
