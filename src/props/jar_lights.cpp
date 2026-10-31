#include "jar_lights.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void JarLights::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_count"), &JarLights::get_count);
	ClassDB::bind_method(D_METHOD("get_lit"), &JarLights::get_lit);
}
void JarLights::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			lit = sky != nullptr && (sky->get_time_of_day() < 0.26f || sky->get_time_of_day() > 0.74f);
			for (int i = 0; i < COUNT; i++) {
				if (jars[i] != nullptr) jars[i]->set_rotation(Vector3(0.0f, 0.0f, std::sin(t * 1.2f + (float)i) * 0.05f));
				for (int b = 0; b < 2; b++) {
					if (bulbs[i][b] != nullptr) {
						bulbs[i][b]->set_visible(lit);
						const float pulse = 0.7f + 0.3f * std::sin(t * 2.3f + (float)i + (float)b);
						bulbs[i][b]->set_scale(Vector3(pulse, pulse, pulse));
					}
				}
				if (lamps[i] != nullptr) {
					lamps[i]->set_visible(lit);
					lamps[i]->set_param(Light3D::PARAM_ENERGY, 0.38f + 0.08f * std::sin(t * 2.3f + (float)i));
				}
			}
		} break;
		default: break;
	}
}
void JarLights::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	Ref<CylinderMesh> jar_mesh;
	jar_mesh.instantiate();
	jar_mesh->set_top_radius(0.075f);
	jar_mesh->set_bottom_radius(0.068f);
	jar_mesh->set_height(0.17f);
	jar_mesh->set_radial_segments(10);
	jar_mesh->set_rings(1);
	Ref<StandardMaterial3D> jm;
	jm.instantiate();
	jm->set_albedo(Color(0.74f, 0.86f, 0.88f, 0.35f));
	jm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	jm->set_roughness(0.1f);
	jar_mesh->set_material(jm);
	Ref<SphereMesh> bulb_mesh;
	bulb_mesh.instantiate();
	bulb_mesh->set_radius(0.012f);
	bulb_mesh->set_height(0.024f);
	bulb_mesh->set_radial_segments(4);
	bulb_mesh->set_rings(2);
	Ref<StandardMaterial3D> bm;
	bm.instantiate();
	bm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	bm->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	bm->set_albedo(Color(1.0f, 0.82f, 0.42f, 0.9f));
	bm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	bulb_mesh->set_material(bm);
	for (int i = 0; i < COUNT; i++) {
		const float x = -0.55f + 0.55f * (float)i;
		meshutil::add_box(this, toon, Vector3(0.05f, 0.70f, 0.05f), Vector3(x, 0.35f, 0.0f), Color(0.46f, 0.32f, 0.20f));
		meshutil::add_box(this, toon, Vector3(0.18f, 0.03f, 0.18f), Vector3(x, 0.70f, 0.0f), Color(0.52f, 0.36f, 0.22f));
		Node3D *j = memnew(Node3D);
		j->set_name(String("Jar") + String::num_int64(i));
		j->set_position(Vector3(x, 0.80f, 0.0f));
		add_child(j);
		jars[i] = j;
		MeshInstance3D *jmi = memnew(MeshInstance3D);
		jmi->set_mesh(jar_mesh);
		j->add_child(jmi);
		meshutil::add_box(j, toon, Vector3(0.16f, 0.02f, 0.16f), Vector3(0.0f, 0.09f, 0.0f), Color(0.50f, 0.44f, 0.32f));
		OmniLight3D *lamp = memnew(OmniLight3D);
		lamp->set_position(Vector3(0.0f, 0.0f, 0.02f));
		lamp->set_color(Color(1.0f, 0.78f, 0.46f));
		lamp->set_param(Light3D::PARAM_ENERGY, 0.42f);
		lamp->set_param(Light3D::PARAM_RANGE, 1.6f);
		lamp->set_shadow(false);
		lamp->set_visible(false);
		j->add_child(lamp);
		lamps[i] = lamp;
		for (int b = 0; b < 2; b++) {
			MeshInstance3D *bmi = memnew(MeshInstance3D);
			bmi->set_mesh(bulb_mesh);
			bmi->set_position(Vector3(-0.025f + 0.05f * (float)b, -0.02f + 0.03f * (float)b, 0.01f * (float)b));
			bmi->set_visible(false);
			bmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			j->add_child(bmi);
			bulbs[i][b] = bmi;
		}
	}
}
