#include "pinwheels.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Pinwheels::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_count"), &Pinwheels::get_count);
	ClassDB::bind_method(D_METHOD("get_spin"), &Pinwheels::get_spin);
	ClassDB::bind_method(D_METHOD("get_snow_stopped"), &Pinwheels::get_snow_stopped);
}

void Pinwheels::_notification(int p_what) {
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
			float target = 1.4f;
			snow_stopped = false;
			if (sky != nullptr) {
				target += sky->get_rain_amount() * 5.0f;
				if (sky->get_snow_amount() > 0.45f) {
					target = 0.0f;
					snow_stopped = true;
				}
			}
			spin = Math::lerp(spin, target, Math::min(1.0f, delta * 0.8f));
			for (int i = 0; i < COUNT; i++) {
				if (wheels[i] != nullptr) {
					wheels[i]->set_rotation(Vector3(0.0f, 0.0f, wheels[i]->get_rotation().z + spin * delta * (1.0f + 0.15f * (float)i)));
				}
			}
		} break;
		default:
			break;
	}
}

void Pinwheels::build() {
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

	const Color cols[COUNT] = {
		Color(0.86f, 0.30f, 0.26f),
		Color(0.92f, 0.78f, 0.28f),
		Color(0.32f, 0.56f, 0.82f),
	};
	const Vector3 pos[COUNT] = { Vector3(-0.55f, 0, 0.0f), Vector3(0.0f, 0, 0.15f), Vector3(0.55f, 0, -0.05f) };

	for (int i = 0; i < COUNT; i++) {
		// 木签
		meshutil::add_box(this, toon, Vector3(0.020f, 0.95f, 0.020f), pos[i] + Vector3(0.0f, 0.475f, 0.0f), Color(0.52f, 0.38f, 0.22f));
		// 轮轴
		Ref<SphereMesh> hub;
		hub.instantiate();
		hub->set_radius(0.025f);
		hub->set_height(0.05f);
		hub->set_radial_segments(6);
		hub->set_rings(3);
		hub->set_material(meshutil::toon_material(toon, Color(0.30f, 0.26f, 0.22f), 0.1f));
		MeshInstance3D *hm = memnew(MeshInstance3D);
		hm->set_mesh(hub);
		hm->set_position(pos[i] + Vector3(0.0f, 0.96f, 0.03f));
		add_child(hm);

		Node3D *wheel = memnew(Node3D);
		wheel->set_name(String("Wheel") + String::num_int64(i));
		wheel->set_position(pos[i] + Vector3(0.0f, 0.96f, 0.05f));
		add_child(wheel);
		wheels[i] = wheel;
		for (int b = 0; b < 4; b++) {
			const float a = (float)b * 1.5707964f;
			// 每片纸叶是一个扭过一点角度的薄板
			MeshInstance3D *blade = meshutil::add_box(wheel, toon, Vector3(0.22f, 0.085f, 0.008f),
					Vector3(std::cos(a) * 0.11f, std::sin(a) * 0.11f, 0.0f), cols[i],
					Vector3(0.35f, 0.0f, a));
			blade->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		}
	}
}
