#include "plant_stand.h"

#include <cmath>
#include <vector>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
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

	// 花架（立柱/隔板各一个 MultiMesh）
	Ref<BoxMesh> unit_box;
	unit_box.instantiate();
	unit_box->set_size(Vector3(1.0f, 1.0f, 1.0f));
	Ref<MultiMesh> post_mm;
	post_mm.instantiate();
	post_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	post_mm->set_mesh(unit_box);
	post_mm->set_instance_count(2);
	post_mm->set_instance_transform(0, Transform3D(Basis().scaled(Vector3(0.08f, 1.20f, 0.08f)), Vector3(-0.48f, 0.60f, 0.0f)));
	post_mm->set_instance_transform(1, Transform3D(Basis().scaled(Vector3(0.08f, 1.20f, 0.08f)), Vector3(0.48f, 0.60f, 0.0f)));
	MultiMeshInstance3D *post_mi = memnew(MultiMeshInstance3D);
	post_mi->set_name("StandPosts");
	post_mi->set_multimesh(post_mm);
	add_child(post_mi);

	Ref<MultiMesh> board_mm;
	board_mm.instantiate();
	board_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	board_mm->set_mesh(unit_box);
	board_mm->set_instance_count(3);
	for (int i = 0; i < 3; i++) {
		board_mm->set_instance_transform(i, Transform3D(Basis().scaled(Vector3(1.08f, 0.04f, 0.34f)),
				Vector3(0.0f, 0.30f + 0.36f * (float)i, 0.0f)));
	}
	MultiMeshInstance3D *board_mi = memnew(MultiMeshInstance3D);
	board_mi->set_name("StandBoards");
	board_mi->set_multimesh(board_mm);
	add_child(board_mi);

	// 陶盆合成一个 MultiMesh（统一陶土色，靠实例缩放区分大小）
	Ref<CylinderMesh> pot_unit;
	pot_unit.instantiate();
	pot_unit->set_top_radius(1.0f);
	pot_unit->set_bottom_radius(0.78f);
	pot_unit->set_height(1.5f);
	pot_unit->set_radial_segments(9);
	pot_unit->set_rings(1);
	pot_unit->set_material(meshutil::toon_material(toon, Color(0.68f, 0.42f, 0.28f), 0.2f));
	std::vector<Transform3D> pot_xf;
	auto add_pot = [&](const Vector3 &p_pos, float p_r) {
		pot_xf.push_back(Transform3D(Basis().scaled(Vector3(p_r, p_r, p_r)), p_pos + Vector3(0.0f, p_r * 0.75f, 0.0f)));
	};
	add_pot(Vector3(-0.32f, 0.32f, 0.0f), 0.085f); // 蕨类
	add_pot(Vector3(0.30f, 0.32f, 0.0f), 0.090f); // 垂藤
	add_pot(Vector3(-0.25f, 0.68f, 0.0f), 0.080f); // 开花
	add_pot(Vector3(0.14f, 0.68f, 0.0f), 0.045f);
	add_pot(Vector3(0.27f, 0.68f, 0.0f), 0.045f);
	add_pot(Vector3(0.40f, 0.68f, 0.0f), 0.045f);
	add_pot(Vector3(0.0f, 1.04f, 0.0f), 0.100f); // 顶层大叶
	Ref<MultiMesh> pot_mm;
	pot_mm.instantiate();
	pot_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	pot_mm->set_mesh(pot_unit);
	pot_mm->set_instance_count((int)pot_xf.size());
	for (size_t i = 0; i < pot_xf.size(); i++) {
		pot_mm->set_instance_transform((int)i, pot_xf[i]);
	}
	MultiMeshInstance3D *pot_mi = memnew(MultiMeshInstance3D);
	pot_mi->set_name("Pots");
	pot_mi->set_multimesh(pot_mm);
	add_child(pot_mi);

	// 绿叶合成一个 MultiMesh：蕨类 6 + 多肉 15 + 顶层 4
	Ref<SphereMesh> leaf_unit;
	leaf_unit.instantiate();
	leaf_unit->set_radius(1.0f);
	leaf_unit->set_height(2.0f);
	leaf_unit->set_radial_segments(6);
	leaf_unit->set_rings(3);
	leaf_unit->set_material(meshutil::toon_material(toon, Color(0.30f, 0.50f, 0.22f), 0.2f));
	std::vector<Transform3D> leaf_xf;
	auto add_leaf = [&](const Vector3 &p_pos, const Vector3 &p_rot, const Vector3 &p_scale) {
		Basis b;
		b.set_euler_scale(p_rot, p_scale);
		leaf_xf.push_back(Transform3D(b, p_pos));
	};
	for (int i = 0; i < 6; i++) {
		const float a = (float)i * 1.047f;
		add_leaf(Vector3(-0.32f + std::cos(a) * 0.08f, 0.48f + 0.03f * (float)(i % 2), std::sin(a) * 0.08f),
				Vector3(0.5f, a, 0.3f), Vector3(0.042f, 0.112f, 0.0245f));
	}
	for (int i = 0; i < 3; i++) {
		const float x = 0.14f + 0.13f * (float)i;
		for (int l = 0; l < 5; l++) {
			const float a = (float)l * 1.2566f;
			add_leaf(Vector3(x + std::cos(a) * 0.028f, 0.77f, std::sin(a) * 0.028f),
					Vector3(0.7f, a, 0.0f), Vector3(0.0192f, 0.0384f, 0.016f));
		}
	}
	for (int i = 0; i < 4; i++) {
		const float a = (float)i * 1.5708f + 0.3f;
		add_leaf(Vector3(std::cos(a) * 0.08f, 1.30f, std::sin(a) * 0.08f),
				Vector3(0.4f, a, 0.4f), Vector3(0.07f, 0.15f, 0.025f));
	}
	Ref<MultiMesh> leaf_mm;
	leaf_mm.instantiate();
	leaf_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	leaf_mm->set_mesh(leaf_unit);
	leaf_mm->set_instance_count((int)leaf_xf.size());
	for (size_t i = 0; i < leaf_xf.size(); i++) {
		leaf_mm->set_instance_transform((int)i, leaf_xf[i]);
	}
	MultiMeshInstance3D *leaf_mi = memnew(MultiMeshInstance3D);
	leaf_mi->set_name("Leaves");
	leaf_mi->set_multimesh(leaf_mm);
	add_child(leaf_mi);

	// 垂藤单独挂在 vine 下（会随风摆）：茎与叶仍单独建
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

	// 开花小盆栽：3 根茎 + 3 朵花各合成一个 MultiMesh
	Ref<MultiMesh> stem_mm;
	stem_mm.instantiate();
	stem_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	stem_mm->set_mesh(unit_box);
	stem_mm->set_instance_count(3);
	Ref<MultiMesh> flower_mm;
	flower_mm.instantiate();
	flower_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	flower_mm->set_mesh(leaf_unit);
	flower_mm->set_instance_count(3);
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.1f;
		Basis sb;
		sb.set_euler_scale(Vector3(0.0f, 0.0f, std::cos(a) * 0.2f), Vector3(0.02f, 0.20f, 0.02f));
		stem_mm->set_instance_transform(i, Transform3D(sb, Vector3(-0.25f + std::cos(a) * 0.03f, 0.88f, std::sin(a) * 0.03f)));
		Basis fb = Basis().scaled(Vector3(0.045f, 0.045f, 0.045f));
		flower_mm->set_instance_transform(i, Transform3D(fb, Vector3(-0.25f + std::cos(a) * 0.05f, 0.99f, std::sin(a) * 0.05f)));
	}
	// 花与茎用同一把材质更省，这里花单独给粉红
	stem_mm->set_mesh(unit_box);
	MultiMeshInstance3D *stem_mi = memnew(MultiMeshInstance3D);
	stem_mi->set_name("FlowerStems");
	stem_mi->set_multimesh(stem_mm);
	add_child(stem_mi);
	Ref<SphereMesh> flower_unit;
	flower_unit.instantiate();
	flower_unit->set_radius(1.0f);
	flower_unit->set_height(2.0f);
	flower_unit->set_radial_segments(6);
	flower_unit->set_rings(3);
	flower_unit->set_material(meshutil::toon_material(toon, Color(0.86f, 0.40f, 0.44f), 0.25f));
	flower_mm->set_mesh(flower_unit);
	MultiMeshInstance3D *flower_mi = memnew(MultiMeshInstance3D);
	flower_mi->set_name("Flowers");
	flower_mi->set_multimesh(flower_mm);
	add_child(flower_mi);
}
