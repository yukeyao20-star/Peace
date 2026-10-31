#include "bean_trellis.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
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
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

namespace {
Basis basis_from_y(const Vector3 &p_dir) {
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

void BeanTrellis::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_teepee_count"), &BeanTrellis::get_teepee_count);
	ClassDB::bind_method(D_METHOD("get_bees_visible"), &BeanTrellis::get_bees_visible);
}
void BeanTrellis::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			for (int i = 0; i < TEEPEE_COUNT; i++) {
				if (frames[i] != nullptr) frames[i]->set_rotation(Vector3(std::sin(t * 0.9f + (float)i) * 0.012f, 0.0f, std::cos(t * 1.1f + (float)i) * 0.016f));
			}
			const bool bees_on = sky != nullptr && sky->get_snow_amount() < 0.1f && sky->get_rain_amount() < 0.4f && sky->get_time_of_day() > 0.3f && sky->get_time_of_day() < 0.7f;
			for (int i = 0; i < 2; i++) {
				if (bees[i] == nullptr) continue;
				bees[i]->set_visible(bees_on);
				if (bees_on) {
					const float a = t * 1.5f + (float)i * 2.6f;
					bees[i]->set_position(Vector3(std::cos(a) * 0.75f, 1.15f + 0.12f * std::sin(a * 2.1f), std::sin(a) * 0.42f));
				}
			}
		} break;
		default: break;
	}
}
void BeanTrellis::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 0.35f, 0.0f));
	// 三座三脚架各自的竿、藤、叶、豆荚、花
	Ref<CylinderMesh> pole_mesh;
	pole_mesh.instantiate();
	pole_mesh->set_top_radius(0.016f);
	pole_mesh->set_bottom_radius(0.020f);
	pole_mesh->set_height(1.0f);
	pole_mesh->set_radial_segments(6);
	pole_mesh->set_rings(1);
	Ref<Material> pole_mat = meshutil::toon_material(toon, Color(0.66f, 0.54f, 0.32f), 0.2f);
	Ref<CylinderMesh> vine_mesh;
	vine_mesh.instantiate();
	vine_mesh->set_top_radius(0.008f);
	vine_mesh->set_bottom_radius(0.011f);
	vine_mesh->set_height(1.0f);
	vine_mesh->set_radial_segments(5);
	vine_mesh->set_rings(1);
	Ref<Material> vine_mat = meshutil::toon_material(toon, Color(0.30f, 0.48f, 0.22f), 0.25f);
	Ref<SphereMesh> leaf_mesh;
	leaf_mesh.instantiate();
	leaf_mesh->set_radius(0.055f);
	leaf_mesh->set_height(0.11f);
	leaf_mesh->set_radial_segments(6);
	leaf_mesh->set_rings(3);
	Ref<Material> leaf_mat = meshutil::toon_material(toon, Color(0.36f, 0.56f, 0.24f), 0.25f);
	Ref<BoxMesh> bean_mesh;
	bean_mesh.instantiate();
	bean_mesh->set_size(Vector3(0.018f, 0.09f, 0.018f));
	Ref<Material> bean_mat = meshutil::toon_material(toon, Color(0.52f, 0.66f, 0.30f), 0.25f);
	Ref<SphereMesh> flower_mesh;
	flower_mesh.instantiate();
	flower_mesh->set_radius(0.026f);
	flower_mesh->set_height(0.045f);
	flower_mesh->set_radial_segments(6);
	flower_mesh->set_rings(3);
	Ref<Material> flower_mat = meshutil::toon_material(toon, Color(0.94f, 0.93f, 0.86f), 0.3f);
	for (int i = 0; i < TEEPEE_COUNT; i++) {
		const float cx = -0.85f + 0.85f * (float)i;
		Node3D *frame = memnew(Node3D);
		frame->set_name(String("Teepee") + String::num_int64(i));
		frame->set_position(Vector3(cx, 0.0f, 0.0f));
		add_child(frame);
		frames[i] = frame;
		const Vector3 apex(0.0f, 1.34f, 0.0f);
		Ref<MultiMesh> pm;
		pm.instantiate();
		pm->set_transform_format(MultiMesh::TRANSFORM_3D);
		pm->set_mesh(pole_mesh);
		pm->set_instance_count(3);
		Ref<MultiMesh> vm;
		vm.instantiate();
		vm->set_transform_format(MultiMesh::TRANSFORM_3D);
		vm->set_mesh(vine_mesh);
		vm->set_instance_count(9);
		Ref<MultiMesh> lm;
		lm.instantiate();
		lm->set_transform_format(MultiMesh::TRANSFORM_3D);
		lm->set_mesh(leaf_mesh);
		lm->set_instance_count(18);
		Ref<MultiMesh> bm;
		bm.instantiate();
		bm->set_transform_format(MultiMesh::TRANSFORM_3D);
		bm->set_mesh(bean_mesh);
		bm->set_instance_count(9);
		Ref<MultiMesh> fm;
		fm.instantiate();
		fm->set_transform_format(MultiMesh::TRANSFORM_3D);
		fm->set_mesh(flower_mesh);
		fm->set_instance_count(6);
		int vi = 0;
		int li = 0;
		int bi = 0;
		int fi = 0;
		for (int p = 0; p < 3; p++) {
			const float a = (float)p * 2.0944f + (float)i * 0.21f;
			const Vector3 base(std::cos(a) * 0.30f, 0.0f, std::sin(a) * 0.30f);
			const Vector3 dir = apex - base;
			const float len = dir.length();
			pm->set_instance_transform(p, Transform3D(basis_from_y(dir).scaled(Vector3(1.0f, len, 1.0f)), (base + apex) * 0.5f));
			for (int s = 0; s < 3; s++) {
				const float t0 = 0.12f + 0.24f * (float)s;
				const float t1 = t0 + 0.34f;
				const float aa = t0 * 0.45f + 0.6f;
				const Vector3 b0(base + dir * t0 + Vector3(std::cos(aa) * 0.05f, 0.0f, std::sin(aa) * 0.05f));
				const Vector3 b1(base + dir * t1 + Vector3(std::cos(aa + 1.1f) * 0.06f, 0.0f, std::sin(aa + 1.1f) * 0.06f));
				const Vector3 d = b1 - b0;
				vm->set_instance_transform(vi++, Transform3D(basis_from_y(d).scaled(Vector3(1.0f, d.length(), 1.0f)), (b0 + b1) * 0.5f));
			}
			for (int L = 0; L < 6; L++) {
				const float tt = 0.12f + 0.075f * (float)L;
				const Vector3 pos = base + dir * tt + Vector3(std::cos(a * 2.0f + (float)L) * 0.07f, 0.0f, std::sin(a * 2.0f + (float)L) * 0.07f);
				const Basis lb(Vector3(0.0f, 1.0f, 0.0f), a + (float)L * 1.4f);
				lm->set_instance_transform(li++, Transform3D(lb.scaled(Vector3(1.0f, 0.55f, 0.45f)), pos));
			}
			for (int B = 0; B < 3; B++) {
				const float tt = 0.34f + 0.16f * (float)B;
				const Vector3 pos = base + dir * tt + Vector3(std::cos(a + 1.2f) * 0.05f, 0.0f, std::sin(a + 1.2f) * 0.05f);
				bm->set_instance_transform(bi++, Transform3D(Basis(Vector3(1.0f, 0.0f, 0.0f), 0.25f).scaled(Vector3(1.4f, 1.3f, 1.4f)), pos));
			}
			for (int F = 0; F < 2; F++) {
				const Vector3 fpos = base + dir * (0.68f + 0.16f * (float)F) + Vector3(std::cos(a + 2.2f + (float)F) * 0.05f, 0.0f, std::sin(a + 2.2f + (float)F) * 0.05f);
				fm->set_instance_transform(fi++, Transform3D(Basis().scaled(Vector3(0.7f, 0.7f, 0.7f)), fpos));
			}
		}
		MultiMeshInstance3D *pmi = memnew(MultiMeshInstance3D);
		pmi->set_name("Poles");
		pmi->set_multimesh(pm);
		pmi->set_material_override(pole_mat);
		frame->add_child(pmi);
		MultiMeshInstance3D *vmi = memnew(MultiMeshInstance3D);
		vmi->set_name("Vines");
		vmi->set_multimesh(vm);
		vmi->set_material_override(vine_mat);
		frame->add_child(vmi);
		MultiMeshInstance3D *lmi = memnew(MultiMeshInstance3D);
		lmi->set_name("Leaves");
		lmi->set_multimesh(lm);
		lmi->set_material_override(leaf_mat);
		frame->add_child(lmi);
		MultiMeshInstance3D *bmi = memnew(MultiMeshInstance3D);
		bmi->set_name("Beans");
		bmi->set_multimesh(bm);
		bmi->set_material_override(bean_mat);
		frame->add_child(bmi);
		MultiMeshInstance3D *fmi = memnew(MultiMeshInstance3D);
		fmi->set_name("Flowers");
		fmi->set_multimesh(fm);
		fmi->set_material_override(flower_mat);
		frame->add_child(fmi);
	}
	// 两只小蜂
	for (int i = 0; i < 2; i++) {
		Node3D *bee = memnew(Node3D);
		bee->set_name(String("Bee") + String::num_int64(i));
		bee->set_visible(false);
		add_child(bee);
		bees[i] = bee;
		meshutil::add_sphere(bee, toon, 0.018f, Vector3(), Color(0.92f, 0.76f, 0.22f))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		meshutil::add_sphere(bee, toon, 0.012f, Vector3(0.0f, -0.012f, 0.0f), Color(0.18f, 0.15f, 0.10f))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	}
}
