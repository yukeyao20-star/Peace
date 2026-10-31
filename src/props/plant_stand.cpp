#include "plant_stand.h"

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
#include "../world/terrain_builder.h"

using namespace godot;

void PlantStand::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &PlantStand::get_item_types);
	ClassDB::bind_method(D_METHOD("get_sway"), &PlantStand::get_sway);
}

void PlantStand::_notification(int p_what) {
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
			sway = std::sin(t * 1.2f) * 0.10f + std::sin(t * 2.7f) * 0.03f;
			if (vine != nullptr) {
				vine->set_rotation(Vector3(sway * 0.5f, 0.0f, sway));
			}
		} break;
		default:
			break;
	}
}

void PlantStand::build() {
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

	// 花架
	const Color wood(0.50f, 0.35f, 0.21f);
	meshutil::add_box(this, toon, Vector3(0.08f, 1.20f, 0.08f), Vector3(-0.48f, 0.60f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.08f, 1.20f, 0.08f), Vector3(0.48f, 0.60f, 0.0f), wood);
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(1.08f, 0.04f, 0.34f), Vector3(0.0f, 0.30f + 0.36f * (float)i, 0.0f), Color(0.62f, 0.46f, 0.28f));
	}

	auto pot = [&](const Vector3 &p_pos, const Color &p_col, float p_r) {
		Ref<CylinderMesh> m;
		m.instantiate();
		m->set_top_radius(p_r);
		m->set_bottom_radius(p_r * 0.78f);
		m->set_height(p_r * 1.5f);
		m->set_radial_segments(9);
		m->set_rings(1);
		m->set_material(meshutil::toon_material(toon, p_col, 0.2f));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(m);
		mi->set_position(p_pos + Vector3(0.0f, p_r * 0.75f, 0.0f));
		add_child(mi);
	};

	// 1. 蕨类
	pot(Vector3(-0.32f, 0.32f, 0.0f), Color(0.70f, 0.42f, 0.28f), 0.085f);
	for (int i = 0; i < 6; i++) {
		const float a = (float)i * 1.047f;
		MeshInstance3D *leaf = meshutil::add_sphere(this, toon, 0.07f,
				Vector3(-0.32f + std::cos(a) * 0.08f, 0.48f + 0.03f * (float)(i % 2), std::sin(a) * 0.08f),
				Color(0.28f, 0.50f, 0.22f));
		leaf->set_scale(Vector3(0.6f, 1.6f, 0.35f));
		leaf->set_rotation(Vector3(0.5f, a, 0.3f));
	}

	// 2. 垂藤（会摆）
	pot(Vector3(0.30f, 0.32f, 0.0f), Color(0.66f, 0.38f, 0.26f), 0.090f);
	vine = memnew(Node3D);
	vine->set_name("TrailingVine");
	vine->set_position(Vector3(0.30f, 0.52f, 0.0f));
	add_child(vine);
	for (int i = 0; i < 5; i++) {
		meshutil::add_box(vine, toon, Vector3(0.012f, 0.16f, 0.012f),
				Vector3(0.02f * (float)i, -0.08f * (float)i, 0.04f * (float)i), Color(0.30f, 0.52f, 0.24f),
				Vector3(0.0f, 0.0f, 0.12f * (float)i));
		meshutil::add_sphere(vine, toon, 0.030f,
				Vector3(0.02f * (float)i - 0.03f, -0.08f * (float)i, 0.04f * (float)i), Color(0.32f, 0.54f, 0.26f));
	}

	// 3. 中层开花盆栽
	pot(Vector3(-0.25f, 0.68f, 0.0f), Color(0.72f, 0.44f, 0.30f), 0.080f);
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.1f;
		meshutil::add_box(this, toon, Vector3(0.02f, 0.20f, 0.02f),
				Vector3(-0.25f + std::cos(a) * 0.03f, 0.88f, std::sin(a) * 0.03f), Color(0.32f, 0.50f, 0.22f),
				Vector3(0.0f, 0.0f, std::cos(a) * 0.2f));
		meshutil::add_sphere(this, toon, 0.045f,
				Vector3(-0.25f + std::cos(a) * 0.05f, 0.99f, std::sin(a) * 0.05f), Color(0.86f, 0.40f, 0.44f));
	}

	// 4. 中层小多肉排
	for (int i = 0; i < 3; i++) {
		const float x = 0.14f + 0.13f * (float)i;
		pot(Vector3(x, 0.68f, 0.0f), Color(0.64f, 0.48f, 0.32f), 0.045f);
		for (int l = 0; l < 5; l++) {
			const float a = (float)l * 1.2566f;
			MeshInstance3D *leaf = meshutil::add_sphere(this, toon, 0.032f,
					Vector3(x + std::cos(a) * 0.028f, 0.77f, std::sin(a) * 0.028f), Color(0.38f, 0.56f, 0.36f));
			leaf->set_scale(Vector3(0.6f, 1.2f, 0.5f));
			leaf->set_rotation(Vector3(0.7f, a, 0.0f));
		}
	}

	// 5. 顶层大叶植物
	pot(Vector3(0.0f, 1.04f, 0.0f), Color(0.70f, 0.42f, 0.28f), 0.100f);
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 1.5708f + 0.3f;
		MeshInstance3D *leaf = meshutil::add_sphere(this, toon, 0.10f,
				Vector3(std::cos(a) * 0.08f, 1.30f, std::sin(a) * 0.08f), Color(0.26f, 0.46f, 0.20f));
		leaf->set_scale(Vector3(0.7f, 1.5f, 0.25f));
		leaf->set_rotation(Vector3(0.4f, a, 0.4f));
	}
}
