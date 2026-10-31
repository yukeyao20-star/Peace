#include "morning_glory.h"

#include <cmath>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void MorningGlory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_flower_count"), &MorningGlory::get_flower_count);
	ClassDB::bind_method(D_METHOD("get_open_flowers"), &MorningGlory::get_open_flowers);
	ClassDB::bind_method(D_METHOD("get_dew_visible"), &MorningGlory::get_dew_visible);
	ClassDB::bind_method(D_METHOD("get_bees_visible"), &MorningGlory::get_bees_visible);
	ClassDB::bind_method(D_METHOD("get_flower_open"), &MorningGlory::get_flower_open);
	ClassDB::bind_method(D_METHOD("get_sway_amp"), &MorningGlory::get_sway_amp);
}

float MorningGlory::open_target() const {
	if (sky == nullptr) {
		return 0.0f;
	}
	const float tod = sky->get_time_of_day();
	const float rain = sky->get_rain_amount();
	const float snow = sky->get_snow_amount();
	float o = 0.0f;
	if (tod > 0.22f && tod < 0.56f) {
		o = std::sin(3.14159f * (tod - 0.22f) / 0.34f);
	}
	o *= Math::clamp(1.0f - 0.85f * rain, 0.0f, 1.0f);
	o *= Math::clamp(1.0f - snow, 0.0f, 1.0f);
	return Math::clamp(o, 0.0f, 1.0f);
}

void MorningGlory::_notification(int p_what) {
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

			const float target = open_target();
			flower_open = Math::move_toward(flower_open, target, delta * 0.55f);
			open_count = (flower_open > 0.5f) ? FLOWER_COUNT : 0;

			for (int i = 0; i < FLOWER_COUNT; i++) {
				if (flowers[i] == nullptr) {
					continue;
				}
				const float s = 0.18f + 0.82f * flower_open;
				flowers[i]->set_scale(Vector3(s, s, s));
				flowers[i]->set_rotation(Vector3(
						-0.95f * (1.0f - flower_open),
						phase[i] * 0.35f,
						std::sin(t * 1.2f + phase[i]) * sway_amp * (0.35f + 0.65f * flower_open)));
			}

			// 清晨花瓣上的露珠：日出后半小时内最明显。
			float d = 0.0f;
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				if (tod > 0.23f && tod < 0.37f) {
					d = std::sin(3.14159f * (tod - 0.23f) / 0.14f);
				}
				d *= Math::clamp(1.0f - sky->get_rain_amount(), 0.0f, 1.0f);
				d *= Math::clamp(1.0f - sky->get_snow_amount(), 0.0f, 1.0f);
			}
			dew = Math::move_toward(dew, Math::clamp(d, 0.0f, 1.0f), delta * 0.5f);
			dew_visible = 0;
			for (int i = 0; i < FLOWER_COUNT; i++) {
				if (dewdrops[i] == nullptr) {
					continue;
				}
				const bool vis = dew > 0.05f;
				dewdrops[i]->set_visible(vis);
				if (vis) {
					const float s = 0.4f + 0.6f * dew;
					dewdrops[i]->set_scale(Vector3(s, s, s));
					dew_visible++;
				}
			}

			// 白天有两只小蜂绕着花转；雨雪和夜里都不出来。
			const bool bee_on = flower_open > 0.5f && sky != nullptr
					&& sky->get_rain_amount() < 0.25f && sky->get_snow_amount() < 0.15f;
			bees_visible = bee_on ? BEE_COUNT : 0;
			for (int i = 0; i < BEE_COUNT; i++) {
				if (bees[i] != nullptr) {
					bees[i]->set_visible(bee_on);
				}
			}
			if (bee_on) {
				update_bees(delta, bee_on);
			}
		} break;
		default:
			break;
	}
}

void MorningGlory::update_bees(float delta, bool visible) {
	(void)delta;
	(void)visible;
	const float cx[BEE_COUNT] = { -0.55f, 0.55f };
	for (int i = 0; i < BEE_COUNT; i++) {
		const float a = t * (1.15f + 0.15f * (float)i) + (float)i * 2.4f;
		const Vector3 p(cx[i] + std::cos(a) * 0.38f,
				1.02f + 0.10f * std::sin(t * 2.3f + (float)i),
				0.18f + std::sin(a) * 0.18f);
		bees[i]->set_position(p);
		bees[i]->set_rotation(Vector3(0.0f, -a, std::sin(t * 18.0f + (float)i) * 0.18f));
	}
}

void MorningGlory::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	set_rotation(Vector3(0.0f, -0.7854f, 0.0f)); // 顺着前院篱笆的方向

	// 藤蔓骨架：5 根主藤、每根 3 段，拼成 MultiMesh
	Ref<CylinderMesh> stem_mesh;
	stem_mesh.instantiate();
	stem_mesh->set_top_radius(0.009f);
	stem_mesh->set_bottom_radius(0.009f);
	stem_mesh->set_height(1.0f);
	stem_mesh->set_radial_segments(5);
	stem_mesh->set_rings(1);
	stem_mesh->set_material(meshutil::toon_material(toon, Color(0.24f, 0.44f, 0.20f), 0.2f));
	stem_multimesh.instantiate();
	stem_multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	stem_multimesh->set_mesh(stem_mesh);
	stem_multimesh->set_instance_count(VINE_COUNT * 3);

	Ref<SphereMesh> leaf_mesh_base;
	leaf_mesh_base.instantiate();
	leaf_mesh_base->set_radius(1.0f);
	leaf_mesh_base->set_height(2.0f);
	leaf_mesh_base->set_radial_segments(7);
	leaf_mesh_base->set_rings(4);
	leaf_mat = meshutil::toon_material(toon, Color(0.30f, 0.52f, 0.22f), 0.25f);
	leaf_mesh_base->set_material(leaf_mat);
	leaf_multimesh.instantiate();
	leaf_multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	leaf_multimesh->set_mesh(leaf_mesh_base);
	leaf_multimesh->set_instance_count(VINE_COUNT * 3 * 2);

	int stem_i = 0;
	int leaf_i = 0;
	for (int v = 0; v < VINE_COUNT; v++) {
		const float base_x = -1.2f + 0.6f * (float)v;
		float x0 = base_x;
		float y0 = 0.0f;
		for (int s = 0; s < 3; s++) {
			const float y1 = y0 + 0.40f;
			const float x1 = base_x + std::sin((float)v * 1.7f + (float)s * 0.9f) * 0.07f;
			const Vector3 a(x0, y0, 0.07f);
			const Vector3 b(x1, y1, 0.07f);
			const Vector3 d = b - a;
			const float len = d.length();
			const float ang = std::atan2(-d.x, d.y);
			const Basis basis = Basis(Vector3(0.0f, 0.0f, 1.0f), ang).scaled(Vector3(1.0f, len, 1.0f));
			stem_multimesh->set_instance_transform(stem_i++, Transform3D(basis, (a + b) * 0.5f));

			// 每段两片叶：扁椭球，朝向随机
			for (int k = 0; k < 2; k++) {
				const float lx = x1 + (k == 0 ? -1.0f : 1.0f) * 0.05f;
				const float ly = y1 - 0.06f + 0.04f * (float)k;
				const Basis lb(Vector3(0.0f, 1.0f, 0.0f), (float)(v * 7 + s * 3 + k) * 0.9f);
				const Basis sb = lb.scaled(Vector3(0.052f, 0.010f, 0.032f));
				leaf_multimesh->set_instance_transform(leaf_i++, Transform3D(sb, Vector3(lx, ly, 0.09f)));
			}
			x0 = x1;
			y0 = y1;
		}
	}

	stem_mm = memnew(MultiMeshInstance3D);
	stem_mm->set_name("Stems");
	stem_mm->set_multimesh(stem_multimesh);
	add_child(stem_mm);
	leaf_mm = memnew(MultiMeshInstance3D);
	leaf_mm->set_name("Leaves");
	leaf_mm->set_multimesh(leaf_multimesh);
	add_child(leaf_mm);

	// 六朵牵牛花：喇叭 + 五片花瓣 + 小黄心
	const float fx[FLOWER_COUNT] = { -1.25f, -0.85f, -0.35f, 0.15f, 0.65f, 1.15f };
	const Color petal_cols[FLOWER_COUNT] = {
		Color(0.40f, 0.52f, 0.88f),
		Color(0.62f, 0.42f, 0.86f),
		Color(0.84f, 0.48f, 0.72f),
		Color(0.92f, 0.88f, 0.96f),
		Color(0.44f, 0.66f, 0.90f),
		Color(0.72f, 0.50f, 0.88f),
	};
	dew_mat.instantiate();
	dew_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dew_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dew_mat->set_albedo(Color(0.80f, 0.92f, 1.0f, 0.85f));
	dew_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	Ref<SphereMesh> dew_sphere;
	dew_sphere.instantiate();
	dew_sphere->set_radius(0.010f);
	dew_sphere->set_height(0.020f);
	dew_sphere->set_radial_segments(5);
	dew_sphere->set_rings(3);
	dew_sphere->set_material(dew_mat);

	for (int i = 0; i < FLOWER_COUNT; i++) {
		phase[i] = (float)i * 1.31f;
		flower_mats[i] = meshutil::toon_material(toon, petal_cols[i], 0.35f);

		Node3D *f = memnew(Node3D);
		f->set_name(String("Flower") + String::num_int64(i));
		f->set_position(Vector3(fx[i], 1.06f + 0.03f * (float)(i % 3), 0.12f));
		f->set_scale(Vector3(0.2f, 0.2f, 0.2f));
		add_child(f);
		flowers[i] = f;

		// 优化（第 180 轮）：喇叭 + 五片花瓣合并成一张烘焙网格，每朵花少 6 个节点。
		Ref<SurfaceTool> bake;
		bake.instantiate();
		bake->begin(Mesh::PRIMITIVE_TRIANGLES);
		Ref<CylinderMesh> trumpet;
		trumpet.instantiate();
		trumpet->set_top_radius(0.050f);
		trumpet->set_bottom_radius(0.012f);
		trumpet->set_height(0.075f);
		trumpet->set_radial_segments(9);
		trumpet->set_rings(1);
		bake->append_from(trumpet, 0, Transform3D(Basis(Vector3(1.0f, 0.0f, 0.0f), 1.5707964f), Vector3(0.0f, 0.0f, 0.035f)));
		Ref<BoxMesh> petal;
		petal.instantiate();
		petal->set_size(Vector3(0.055f, 0.022f, 0.008f));
		for (int p = 0; p < 5; p++) {
			const float a = (float)p * 1.2566f;
			const Basis b(Vector3(0.0f, 0.0f, 1.0f), a);
			bake->append_from(petal, 0, Transform3D(b, Vector3(std::cos(a) * 0.048f, std::sin(a) * 0.048f, 0.070f)));
		}
		Ref<ArrayMesh> bloom = bake->commit();
		bloom->surface_set_material(0, flower_mats[i]);
		MeshInstance3D *bm = memnew(MeshInstance3D);
		bm->set_mesh(bloom);
		f->add_child(bm);
		Ref<SphereMesh> core;
		core.instantiate();
		core->set_radius(0.012f);
		core->set_height(0.024f);
		core->set_radial_segments(6);
		core->set_rings(3);
		core->set_material(meshutil::toon_material(toon, Color(0.98f, 0.92f, 0.52f), 0.3f));
		MeshInstance3D *cm = memnew(MeshInstance3D);
		cm->set_mesh(core);
		cm->set_position(Vector3(0.0f, 0.0f, 0.058f));
		f->add_child(cm);

		MeshInstance3D *dw = memnew(MeshInstance3D);
		dw->set_mesh(dew_sphere);
		dw->set_position(Vector3(0.028f, 0.022f, 0.078f));
		dw->set_visible(false);
		f->add_child(dw);
		dewdrops[i] = dw;
	}

	// 两只小蜂：黄黑相间的小身子 + 半透明翅膀
	for (int i = 0; i < BEE_COUNT; i++) {
		Node3D *bee = memnew(Node3D);
		bee->set_name(String("Bee") + String::num_int64(i));
		bee->set_visible(false);
		add_child(bee);
		bees[i] = bee;

		Ref<BoxMesh> body;
		body.instantiate();
		body->set_size(Vector3(0.030f, 0.022f, 0.055f));
		body->set_material(meshutil::toon_material(toon, Color(0.92f, 0.76f, 0.22f), 0.2f));
		MeshInstance3D *bm = memnew(MeshInstance3D);
		bm->set_mesh(body);
		bee->add_child(bm);

		Ref<BoxMesh> stripe;
		stripe.instantiate();
		stripe->set_size(Vector3(0.032f, 0.024f, 0.010f));
		stripe->set_material(meshutil::toon_material(toon, Color(0.16f, 0.13f, 0.10f), 0.1f));
		for (int s = 0; s < 2; s++) {
			MeshInstance3D *sm = memnew(MeshInstance3D);
			sm->set_mesh(stripe);
			sm->set_position(Vector3(0.0f, 0.0f, -0.010f + 0.024f * (float)s));
			bee->add_child(sm);
		}

		Ref<PlaneMesh> wing;
		wing.instantiate();
		wing->set_size(Vector2(0.034f, 0.020f));
		Ref<StandardMaterial3D> wm;
		wm.instantiate();
		wm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		wm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		wm->set_albedo(Color(0.92f, 0.96f, 1.0f, 0.50f));
		wm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		wm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		wing->set_material(wm);
		for (int s = 0; s < 2; s++) {
			MeshInstance3D *wmi = memnew(MeshInstance3D);
			wmi->set_mesh(wing);
			wmi->set_position(Vector3(s == 0 ? -0.020f : 0.020f, 0.014f, 0.0f));
			wmi->set_rotation(Vector3(0.0f, 0.0f, s == 0 ? 0.4f : -0.4f));
			bee->add_child(wmi);
		}
	}
}
