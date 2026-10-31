#include "camp_tent.h"

#include <cmath>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void CampTent::_bind_methods() {
}

void CampTent::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
				TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain != nullptr) {
					Vector3 p = get_global_position();
					p.y = terrain->get_terrain_height(p.x, p.z) + 0.01f;
					set_global_position(p);
				}
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (flap_l != nullptr) {
				flap_l->set_rotation(Vector3(0.0f, 0.0f, -0.10f + std::sin(t * 1.1f) * 0.05f));
			}
			if (flap_r != nullptr) {
				flap_r->set_rotation(Vector3(0.0f, 0.0f, 0.10f - std::sin(t * 1.3f + 0.6f) * 0.05f));
			}
			float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			float on = Math::clamp((night - 0.25f) / 0.4f, 0.0f, 1.0f);
			if (lamp != nullptr) {
				float flick = 0.9f + 0.08f * std::sin(t * 7.0f);
				lamp->set_param(Light3D::PARAM_ENERGY, 0.62f * on * flick);
				lamp->set_visible(on > 0.03f);
			}
			if (lamp_glass.is_valid()) {
				lamp_glass->set("emission_energy_multiplier", 0.22f + 1.5f * on);
			}
		} break;
		default:
			break;
	}
}

void CampTent::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color canvas(0.74f, 0.68f, 0.52f);
	const Color canvas_dark(0.60f, 0.54f, 0.40f);
	const Color wood(0.40f, 0.28f, 0.15f);
	const Color blanket(0.36f, 0.46f, 0.34f);
	const Color cream(0.90f, 0.84f, 0.70f);

	// 脊梁与前后 A 字架
	meshutil::add_box(this, toon, Vector3(0.07f, 0.07f, 2.45f), Vector3(0.0f, 1.55f, 0.0f), wood);
	for (int end = 0; end < 2; end++) {
		float z = (end == 0) ? -1.05f : 1.05f;
		for (int side = 0; side < 2; side++) {
			float sx = (side == 0) ? -1.0f : 1.0f;
			Vector3 a(0.0f, 1.55f, z);
			Vector3 b(sx * 0.78f, 0.02f, z);
			Vector3 mid = (a + b) * 0.5f;
			Vector3 d = b - a;
			float len = d.length();
			float ang = std::atan2(-d.x, d.y);
			meshutil::add_box(this, toon, Vector3(0.06f, len, 0.06f), mid, wood, Vector3(0.0f, 0.0f, ang));
		}
	}

	// 两片帆布
	meshutil::add_box(this, toon, Vector3(1.72f, 0.03f, 2.22f), Vector3(-0.39f, 0.78f, 0.0f), canvas,
			Vector3(0.0f, 0.0f, -2.04f));
	meshutil::add_box(this, toon, Vector3(1.72f, 0.03f, 2.22f), Vector3(0.39f, 0.78f, 0.0f), canvas,
			Vector3(0.0f, 0.0f, -1.10f));
	// 后壁
	meshutil::add_box(this, toon, Vector3(1.50f, 1.50f, 0.03f), Vector3(0.0f, 0.75f, -1.10f), canvas_dark);

	// 前襟两片，会随风轻掀
	flap_l = memnew(Node3D);
	flap_l->set_name("FlapL");
	flap_l->set_position(Vector3(-0.26f, 1.42f, 1.06f));
	add_child(flap_l);
	meshutil::add_box(flap_l, toon, Vector3(0.46f, 0.78f, 0.02f), Vector3(0.0f, -0.39f, 0.0f), canvas);
	flap_r = memnew(Node3D);
	flap_r->set_name("FlapR");
	flap_r->set_position(Vector3(0.26f, 1.42f, 1.06f));
	add_child(flap_r);
	meshutil::add_box(flap_r, toon, Vector3(0.46f, 0.78f, 0.02f), Vector3(0.0f, -0.39f, 0.0f), canvas);

	// 床卷与枕头
	Node3D *bed = memnew(Node3D);
	bed->set_name("Bedroll");
	bed->set_position(Vector3(0.0f, 0.0f, 0.06f));
	add_child(bed);
	meshutil::add_box(bed, toon, Vector3(0.72f, 0.07f, 1.42f), Vector3(0.0f, 0.05f, 0.0f), blanket);
	meshutil::add_box(bed, toon, Vector3(0.54f, 0.10f, 0.28f), Vector3(0.0f, 0.10f, -0.52f), cream);

	// 门口小灯
	Node3D *lantern = memnew(Node3D);
	lantern->set_name("TentLantern");
	lantern->set_position(Vector3(0.0f, 1.36f, 0.98f));
	add_child(lantern);
	meshutil::add_box(lantern, toon, Vector3(0.02f, 0.18f, 0.02f), Vector3(0.0f, 0.09f, 0.0f), wood);
	meshutil::add_box(lantern, toon, Vector3(0.14f, 0.03f, 0.14f), Vector3(0.0f, 0.0f, 0.0f), wood);
	Ref<BoxMesh> glass;
	glass.instantiate();
	glass->set_size(Vector3(0.11f, 0.14f, 0.11f));
	lamp_glass.instantiate();
	lamp_glass->set_albedo(Color(1.0f, 0.80f, 0.48f));
	lamp_glass->set("emission_enabled", true);
	lamp_glass->set("emission", Color(1.0f, 0.68f, 0.32f));
	lamp_glass->set("emission_energy_multiplier", 0.22f);
	glass->set_material(Ref<Material>(lamp_glass));
	MeshInstance3D *gm = memnew(MeshInstance3D);
	gm->set_mesh(glass);
	gm->set_position(Vector3(0.0f, -0.09f, 0.0f));
	lantern->add_child(gm);
	lamp = memnew(OmniLight3D);
	lamp->set_name("TentLight");
	lamp->set_position(Vector3(0.0f, -0.09f, 0.0f));
	lamp->set_color(Color(1.0f, 0.70f, 0.36f));
	lamp->set_param(Light3D::PARAM_RANGE, 4.0f);
	lamp->set_param(Light3D::PARAM_ENERGY, 0.0f);
	lamp->set_visible(false);
	lantern->add_child(lamp);

	// 地钉与拉绳
	for (int end = 0; end < 2; end++) {
		float z = (end == 0) ? -1.05f : 1.05f;
		for (int side = 0; side < 2; side++) {
			float sx = (side == 0) ? -1.0f : 1.0f;
			Vector3 a(0.0f, 1.55f, z);
			Vector3 b(sx * 1.25f, 0.02f, z + (end == 0 ? -0.5f : 0.5f));
			Vector3 mid = (a + b) * 0.5f;
			Vector3 d = b - a;
			float len = d.length();
			float ang = std::atan2(-d.x, d.y);
			meshutil::add_box(this, toon, Vector3(0.012f, len, 0.012f), mid, Color(0.78f, 0.72f, 0.56f), Vector3(0.0f, 0.0f, ang));
		}
	}
}
