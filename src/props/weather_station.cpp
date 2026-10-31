#include "weather_station.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void WeatherStation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &WeatherStation::get_item_types);
	ClassDB::bind_method(D_METHOD("get_wind"), &WeatherStation::get_wind);
	ClassDB::bind_method(D_METHOD("get_rain_level"), &WeatherStation::get_rain_level);
	ClassDB::bind_method(D_METHOD("get_temp_level"), &WeatherStation::get_temp_level);
}
void WeatherStation::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (sky == nullptr) break;
			wind = 0.6f + sky->get_rain_amount() * 3.4f;
			rain_level = Math::move_toward(rain_level, Math::clamp(sky->get_rain_amount(), 0.0f, 1.0f), delta * 0.08f);
			const float snow = sky->get_snow_amount();
			temp_level = Math::move_toward(temp_level, Math::clamp(0.72f - 0.55f * snow, 0.12f, 0.9f), delta * 0.05f);
			if (cups != nullptr) cups->set_rotation(Vector3(0.0f, cups->get_rotation().y + wind * delta * 2.2f, 0.0f));
			if (vane != nullptr) vane->set_rotation(Vector3(0.0f, std::sin(t * 0.35f) * 0.9f + 0.4f, 0.0f));
			if (sock != nullptr) {
				const float lift = Math::clamp(wind / 4.0f, 0.0f, 1.0f);
				sock->set_rotation(Vector3(0.0f, 0.4f, -1.2f + lift * 0.9f + std::sin(t * 2.2f) * 0.05f));
			}
			if (rain_water != nullptr) rain_water->set_position(Vector3(0.55f, 0.16f + 0.30f * rain_level, 0.0f));
			if (mercury != nullptr) mercury->set_scale(Vector3(1.0f, 0.2f + 0.8f * temp_level, 1.0f));
		} break;
		default: break;
	}
}
void WeatherStation::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	const Color wood(0.52f, 0.38f, 0.24f);
	meshutil::add_box(this, toon, Vector3(0.09f, 1.80f, 0.09f), Vector3(0.0f, 0.90f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.50f, 0.03f, 0.30f), Vector3(0.30f, 1.05f, 0.0f), Color(0.92f, 0.90f, 0.84f));
	// 风速计
	cups = memnew(Node3D);
	cups->set_name("Anemometer");
	cups->set_position(Vector3(0.0f, 1.92f, 0.0f));
	add_child(cups);
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.094f;
		meshutil::add_box(cups, toon, Vector3(0.16f, 0.015f, 0.015f), Vector3(std::cos(a) * 0.08f, 0.0f, std::sin(a) * 0.08f), Color(0.60f, 0.62f, 0.64f), Vector3(0.0f, -a, 0.0f));
		meshutil::add_sphere(cups, toon, 0.035f, Vector3(std::cos(a) * 0.18f, 0.0f, std::sin(a) * 0.18f), Color(0.70f, 0.72f, 0.74f));
	}
	// 风向标
	vane = memnew(Node3D);
	vane->set_name("Vane");
	vane->set_position(Vector3(0.0f, 1.72f, 0.0f));
	add_child(vane);
	meshutil::add_box(vane, toon, Vector3(0.30f, 0.02f, 0.02f), Vector3(0.0f, 0.0f, 0.0f), Color(0.40f, 0.38f, 0.36f));
	meshutil::add_box(vane, toon, Vector3(0.08f, 0.10f, 0.01f), Vector3(-0.14f, 0.04f, 0.0f), Color(0.40f, 0.38f, 0.36f), Vector3(0.0f, 0.0f, 0.6f));
	// 雨量筒
	Ref<CylinderMesh> gauge;
	gauge.instantiate();
	gauge->set_top_radius(0.10f);
	gauge->set_bottom_radius(0.09f);
	gauge->set_height(0.34f);
	gauge->set_radial_segments(9);
	gauge->set_rings(1);
	gauge->set_material(meshutil::toon_material(toon, Color(0.72f, 0.78f, 0.80f, 0.9f), 0.2f));
	MeshInstance3D *gmi = memnew(MeshInstance3D);
	gmi->set_mesh(gauge);
	gmi->set_position(Vector3(0.55f, 0.17f, 0.0f));
	add_child(gmi);
	Ref<CylinderMesh> wm;
	wm.instantiate();
	wm->set_top_radius(0.085f);
	wm->set_bottom_radius(0.085f);
	wm->set_height(0.01f);
	wm->set_radial_segments(9);
	wm->set_rings(1);
	wm->set_material(meshutil::toon_material(toon, Color(0.30f, 0.50f, 0.60f), 0.1f));
	rain_water = memnew(MeshInstance3D);
	rain_water->set_mesh(wm);
	rain_water->set_position(Vector3(0.55f, 0.16f, 0.0f));
	add_child(rain_water);
	// 温度计
	meshutil::add_box(this, toon, Vector3(0.10f, 0.50f, 0.03f), Vector3(0.30f, 1.30f, 0.10f), Color(0.94f, 0.92f, 0.86f));
	Ref<CylinderMesh> merc;
	merc.instantiate();
	merc->set_top_radius(0.012f);
	merc->set_bottom_radius(0.012f);
	merc->set_height(0.40f);
	merc->set_radial_segments(6);
	merc->set_rings(1);
	merc->set_material(meshutil::toon_material(toon, Color(0.82f, 0.22f, 0.18f), 0.2f));
	mercury = memnew(MeshInstance3D);
	mercury->set_mesh(merc);
	mercury->set_position(Vector3(0.30f, 1.18f, 0.12f));
	mercury->set_scale(Vector3(1.0f, 0.7f, 1.0f));
	add_child(mercury);
	// 风向袋
	meshutil::add_box(this, toon, Vector3(0.05f, 0.05f, 0.05f), Vector3(-0.55f, 1.70f, 0.0f), Color(0.44f, 0.32f, 0.20f));
	sock = memnew(Node3D);
	sock->set_name("Windsock");
	sock->set_position(Vector3(-0.55f, 1.70f, 0.0f));
	sock->set_rotation(Vector3(0.0f, 0.4f, -0.4f));
	add_child(sock);
	for (int i = 0; i < 3; i++) {
		Ref<CylinderMesh> seg;
		seg.instantiate();
		seg->set_top_radius(0.055f - 0.012f * (float)i);
		seg->set_bottom_radius(0.045f - 0.012f * (float)i);
		seg->set_height(0.14f);
		seg->set_radial_segments(8);
		seg->set_rings(1);
		seg->set_material(meshutil::toon_material(toon, i % 2 == 0 ? Color(0.86f, 0.46f, 0.22f) : Color(0.92f, 0.90f, 0.84f), 0.2f));
		MeshInstance3D *smi = memnew(MeshInstance3D);
		smi->set_mesh(seg);
		smi->set_position(Vector3(0.0f, 0.0f, 0.20f + 0.13f * (float)i));
		smi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
		sock->add_child(smi);
	}
}
