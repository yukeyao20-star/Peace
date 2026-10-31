#include "firewood_stack.h"
#include <cmath>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void FirewoodStack::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_log_count"), &FirewoodStack::get_log_count);
	ClassDB::bind_method(D_METHOD("is_axe_embedded"), &FirewoodStack::is_axe_embedded);
	ClassDB::bind_method(D_METHOD("is_snow_covered"), &FirewoodStack::is_snow_covered);
}
void FirewoodStack::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			snow_covered = sky != nullptr && sky->get_snow_amount() > 0.35f;
			if (snow_cap != nullptr) snow_cap->set_visible(snow_covered);
		} break;
		default: break;
	}
}
void FirewoodStack::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 两根挡木桩
	meshutil::add_box(this, toon, Vector3(0.07f, 0.52f, 0.07f), Vector3(-0.55f, 0.26f, 0.0f), Color(0.42f, 0.30f, 0.19f));
	meshutil::add_box(this, toon, Vector3(0.07f, 0.52f, 0.07f), Vector3(0.55f, 0.26f, 0.0f), Color(0.42f, 0.30f, 0.19f));
	// 三层原木合成一个 MultiMesh
	Ref<CylinderMesh> log_mesh;
	log_mesh.instantiate();
	log_mesh->set_top_radius(0.055f);
	log_mesh->set_bottom_radius(0.055f);
	log_mesh->set_height(0.46f);
	log_mesh->set_radial_segments(8);
	log_mesh->set_rings(1);
	Ref<ShaderMaterial> lm = meshutil::toon_material(toon, Color(0.55f, 0.40f, 0.26f), 0.35f);
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(log_mesh);
	mm->set_instance_count(LOG_COUNT);
	const Basis lay = Basis(Vector3(1.0f, 0.0f, 0.0f), 1.5707964f);
	int idx = 0;
	for (int row = 0; row < 3; row++) {
		const float y = 0.065f + 0.115f * (float)row;
		const float offset = (row % 2 == 0) ? 0.0f : 0.055f;
		for (int i = 0; i < 8; i++) {
			const float x = -0.42f + 0.12f * (float)i + offset;
			const float jitter = 0.02f * std::sin((float)(row * 8 + i) * 1.7f);
			const Basis b = lay;
			mm->set_instance_transform(idx, Transform3D(b, Vector3(x, y, 0.02f * (float)((i + row) % 3) + jitter)));
			idx++;
		}
	}
	MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
	mmi->set_name("Logs");
	mmi->set_multimesh(mm);
	mmi->set_material_override(lm);
	add_child(mmi);
	// 劈柴墩
	Ref<CylinderMesh> stump;
	stump.instantiate();
	stump->set_top_radius(0.20f);
	stump->set_bottom_radius(0.23f);
	stump->set_height(0.36f);
	stump->set_radial_segments(10);
	stump->set_rings(1);
	stump->set_material(meshutil::toon_material(toon, Color(0.48f, 0.34f, 0.22f), 0.4f));
	MeshInstance3D *smi = memnew(MeshInstance3D);
	smi->set_mesh(stump);
	smi->set_position(Vector3(-0.92f, 0.18f, 0.30f));
	add_child(smi);
	// 斧头：木柄 + 铁刃，斜插在墩上
	meshutil::add_box(this, toon, Vector3(0.030f, 0.52f, 0.030f), Vector3(-0.86f, 0.48f, 0.30f), Color(0.62f, 0.46f, 0.28f), Vector3(0.0f, 0.0f, -0.28f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.07f, 0.035f), Vector3(-0.99f, 0.28f, 0.30f), Color(0.34f, 0.35f, 0.36f), Vector3(0.0f, 0.0f, -0.28f));
	// 引火细枝篮
	meshutil::add_box(this, toon, Vector3(0.30f, 0.05f, 0.24f), Vector3(0.86f, 0.025f, 0.32f), Color(0.46f, 0.32f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.30f, 0.16f, 0.03f), Vector3(0.86f, 0.10f, 0.44f), Color(0.50f, 0.34f, 0.21f));
	meshutil::add_box(this, toon, Vector3(0.30f, 0.16f, 0.03f), Vector3(0.86f, 0.10f, 0.20f), Color(0.50f, 0.34f, 0.21f));
	for (int i = 0; i < 4; i++) {
		meshutil::add_box(this, toon, Vector3(0.22f, 0.025f, 0.025f), Vector3(0.86f + 0.02f * (float)(i % 2), 0.09f + 0.035f * (float)i, 0.30f + 0.03f * (float)(i % 3)), Color(0.66f, 0.50f, 0.32f), Vector3(0.0f, 0.15f * (float)(i - 1), 0.0f));
	}
	// 雪帽
	snow_cap = meshutil::add_box(this, toon, Vector3(1.06f, 0.045f, 0.30f), Vector3(0.0f, 0.385f, 0.0f), Color(0.94f, 0.96f, 0.98f));
	snow_cap->set_visible(false);
}
