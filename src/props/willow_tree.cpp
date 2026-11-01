#include "willow_tree.h"
#include <cmath>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

namespace {
Basis willow_basis_y(const Vector3 &p_dir) {
	const Vector3 y = p_dir.normalized();
	Vector3 ref(0.0f, 0.0f, 1.0f);
	if (std::fabs(y.z) > 0.9f) ref = Vector3(1.0f, 0.0f, 0.0f);
	const Vector3 x = ref.cross(y).normalized();
	const Vector3 z = x.cross(y).normalized();
	Basis b;
	b.set_column(0, x);
	b.set_column(1, y);
	b.set_column(2, z);
	return b;
}
}

void WillowTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_strand_count"), &WillowTree::get_strand_count);
}
void WillowTree::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (crown != nullptr) {
				crown->set_rotation(Vector3(std::sin(t * 0.55f) * 0.014f, 0.0f, std::sin(t * 0.72f + 1.2f) * 0.02f));
			}
		} break;
		default: break;
	}
}
void WillowTree::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 0.4f, 0.0f));
	// 歪向水面的树干
	Ref<CylinderMesh> trunk;
	trunk.instantiate();
	trunk->set_top_radius(0.13f);
	trunk->set_bottom_radius(0.21f);
	trunk->set_height(2.4f);
	trunk->set_radial_segments(8);
	trunk->set_rings(1);
	trunk->set_material(meshutil::toon_material(toon, Color(0.42f, 0.32f, 0.22f), 0.35f));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(trunk);
	tmi->set_position(Vector3(0.0f, 1.15f, 0.0f));
	tmi->set_rotation(Vector3(0.0f, 0.0f, 0.14f));
	add_child(tmi);
	meshutil::add_box(this, toon, Vector3(0.34f, 0.10f, 0.34f), Vector3(0.02f, 0.05f, 0.0f), Color(0.38f, 0.29f, 0.20f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.10f, 0.36f), Vector3(0.16f, 0.04f, 0.12f), Color(0.40f, 0.30f, 0.20f), Vector3(0.0f, 0.5f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.16f, 0.10f, 0.36f), Vector3(-0.12f, 0.04f, -0.10f), Color(0.40f, 0.30f, 0.20f), Vector3(0.0f, -0.6f, 0.0f));
	// 三根主枝
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.0944f + 0.4f;
		const Vector3 start(0.10f * std::cos(a), 1.9f, 0.10f * std::sin(a));
		const Vector3 end(std::cos(a) * 0.9f, 2.55f, std::sin(a) * 0.9f);
		const Vector3 d = end - start;
		Ref<CylinderMesh> br;
		br.instantiate();
		br->set_top_radius(0.035f);
		br->set_bottom_radius(0.065f);
		br->set_height(d.length());
		br->set_radial_segments(6);
		br->set_rings(1);
		br->set_material(meshutil::toon_material(toon, Color(0.44f, 0.33f, 0.22f), 0.35f));
		MeshInstance3D *bmi = memnew(MeshInstance3D);
		bmi->set_mesh(br);
		bmi->set_transform(Transform3D(willow_basis_y(d), (start + end) * 0.5f));
		add_child(bmi);
	}
	crown = memnew(Node3D);
	crown->set_name("Crown");
	add_child(crown);
	// 柳条：40 根，每根两段，合成一个 MultiMesh
	Ref<BoxMesh> strand_unit;
	strand_unit.instantiate();
	strand_unit->set_size(Vector3(1.0f, 1.0f, 1.0f));
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(strand_unit);
	mm->set_instance_count(80);
	Ref<SphereMesh> leaf_unit;
	leaf_unit.instantiate();
	leaf_unit->set_radius(0.10f);
	leaf_unit->set_height(0.20f);
	leaf_unit->set_radial_segments(5);
	leaf_unit->set_rings(3);
	Ref<MultiMesh> lm;
	lm.instantiate();
	lm->set_transform_format(MultiMesh::TRANSFORM_3D);
	lm->set_mesh(leaf_unit);
	lm->set_instance_count(132);
	int idx = 0;
	for (int i = 0; i < 40; i++) {
		const float a = (float)i * 0.1571f + 0.3f * std::sin((float)i * 2.7f);
		const float rad = 0.85f + 0.35f * std::sin((float)i * 1.3f);
		const Vector3 start(std::cos(a) * rad, 2.35f + 0.18f * std::sin((float)i * 3.1f), std::sin(a) * rad);
		const float drop1 = 0.55f + 0.20f * std::sin((float)i * 0.9f);
		const float drop2 = 0.55f + 0.35f * std::sin((float)i * 1.7f);
		const Vector3 mid = start + Vector3(std::cos(a) * 0.22f, -drop1, std::sin(a) * 0.22f);
		const Vector3 end = mid + Vector3(std::cos(a) * 0.06f, -drop2, std::sin(a) * 0.06f);
		const Vector3 d1 = mid - start;
		const Vector3 d2 = end - mid;
		mm->set_instance_transform(idx++, Transform3D(willow_basis_y(d1).scaled(Vector3(0.014f, d1.length(), 0.014f)), (start + mid) * 0.5f));
		mm->set_instance_transform(idx++, Transform3D(willow_basis_y(d2).scaled(Vector3(0.012f, d2.length(), 0.012f)), (mid + end) * 0.5f));
		const Basis lb(Vector3(0.0f, 1.0f, 0.0f), a * 1.7f);
		const Vector3 p1 = (start + mid) * 0.5f;
		lm->set_instance_transform(i * 3 + 0, Transform3D(lb.scaled(Vector3(1.2f, 0.5f, 0.85f)), p1));
		lm->set_instance_transform(i * 3 + 1, Transform3D(lb.scaled(Vector3(1.0f, 0.45f, 0.8f)), mid));
		lm->set_instance_transform(i * 3 + 2, Transform3D(lb.scaled(Vector3(0.9f, 0.4f, 0.7f)), end + Vector3(0.0f, 0.04f, 0.0f)));
	}
	MultiMeshInstance3D *smi = memnew(MultiMeshInstance3D);
	smi->set_name("Strands");
	smi->set_multimesh(mm);
	smi->set_material_override(meshutil::toon_material(toon, Color(0.36f, 0.50f, 0.24f), 0.2f));
	smi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	crown->add_child(smi);
	// 树冠上的大团叶簇，补足柳树的伞形轮廓
	for (int c = 0; c < 12; c++) {
		const float ca = (float)c * 0.5236f;
		const float cr = 0.55f + 0.18f * std::sin((float)c * 2.2f);
		const Basis cb(Vector3(0.0f, 1.0f, 0.0f), ca);
		const Vector3 cp(std::cos(ca) * cr, 2.36f + 0.16f * std::sin((float)c * 1.9f), std::sin(ca) * cr);
		lm->set_instance_transform(120 + c, Transform3D(cb.scaled(Vector3(2.0f, 0.8f, 1.6f)), cp));
	}
	MultiMeshInstance3D *lmi = memnew(MultiMeshInstance3D);
	lmi->set_name("Leaves");
	lmi->set_multimesh(lm);
	lmi->set_material_override(meshutil::toon_material(toon, Color(0.42f, 0.58f, 0.28f), 0.2f));
	crown->add_child(lmi);
}
