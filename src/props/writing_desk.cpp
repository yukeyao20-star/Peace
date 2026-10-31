#include "writing_desk.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"

using namespace godot;

void WritingDesk::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &WritingDesk::get_item_types);
	ClassDB::bind_method(D_METHOD("get_flame_on"), &WritingDesk::get_flame_on);
}

void WritingDesk::_notification(int p_what) {
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
			flame_on = true;
			if (flame != nullptr) {
				const float f = 0.85f + 0.15f * std::sin(t * 6.3f) + 0.08f * std::sin(t * 11.7f);
				flame->set_scale(Vector3(0.8f * f, f, 0.8f * f));
			}
		} break;
		default:
			break;
	}
}

void WritingDesk::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	set_rotation(Vector3(0.0f, 1.5708f, 0.0f)); // 贴左墙朝屋里

	const Color wood(0.46f, 0.31f, 0.18f);
	const Color wood_light(0.60f, 0.44f, 0.26f);

	// 写字台
	meshutil::add_box(this, toon, Vector3(1.05f, 0.06f, 0.55f), Vector3(0.0f, 0.75f, 0.0f), wood_light);
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.46f : 0.46f;
		const float sz = (i < 2) ? -0.22f : 0.22f;
		meshutil::add_box(this, toon, Vector3(0.055f, 0.72f, 0.055f), Vector3(sx, 0.36f, sz), wood);
	}
	meshutil::add_box(this, toon, Vector3(0.40f, 0.15f, 0.03f), Vector3(0.0f, 0.62f, 0.26f), wood);
	meshutil::add_box(this, toon, Vector3(0.08f, 0.02f, 0.02f), Vector3(0.0f, 0.62f, 0.28f), Color(0.72f, 0.60f, 0.32f));

	// 椅子
	meshutil::add_box(this, toon, Vector3(0.40f, 0.05f, 0.38f), Vector3(0.10f, 0.44f, 0.62f), wood_light);
	meshutil::add_box(this, toon, Vector3(0.38f, 0.42f, 0.05f), Vector3(0.10f, 0.66f, 0.80f), wood);
	for (int i = 0; i < 4; i++) {
		const float sx = (i % 2 == 0) ? -0.08f : 0.28f;
		const float sz = (i < 2) ? 0.48f : 0.76f;
		meshutil::add_box(this, toon, Vector3(0.045f, 0.44f, 0.045f), Vector3(sx, 0.22f, sz), wood);
	}

	// 墨水瓶与羽毛笔
	Ref<CylinderMesh> ink;
	ink.instantiate();
	ink->set_top_radius(0.035f);
	ink->set_bottom_radius(0.040f);
	ink->set_height(0.065f);
	ink->set_radial_segments(8);
	ink->set_rings(1);
	ink->set_material(meshutil::toon_material(toon, Color(0.16f, 0.16f, 0.22f), 0.15f));
	MeshInstance3D *imi = memnew(MeshInstance3D);
	imi->set_mesh(ink);
	imi->set_position(Vector3(0.30f, 0.81f, -0.06f));
	add_child(imi);
	// 羽毛笔：细杆 + 扁羽片
	meshutil::add_box(this, toon, Vector3(0.008f, 0.30f, 0.008f), Vector3(0.33f, 0.95f, -0.06f), Color(0.92f, 0.88f, 0.76f), Vector3(0.0f, 0.0f, 0.35f));
	Ref<PlaneMesh> feather;
	feather.instantiate();
	feather->set_size(Vector2(0.09f, 0.26f));
	Ref<StandardMaterial3D> fm;
	fm.instantiate();
	fm->set_albedo(Color(0.94f, 0.92f, 0.86f));
	fm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	feather->set_material(fm);
	MeshInstance3D *fmi = memnew(MeshInstance3D);
	fmi->set_mesh(feather);
	fmi->set_position(Vector3(0.35f, 1.02f, -0.06f));
	fmi->set_rotation(Vector3(0.2f, 0.0f, 0.35f));
	add_child(fmi);

	// 信纸与蜡封
	meshutil::add_box(this, toon, Vector3(0.24f, 0.006f, 0.17f), Vector3(-0.22f, 0.785f, 0.02f), Color(0.95f, 0.93f, 0.87f), Vector3(0.0f, 0.2f, 0.0f));
	meshutil::add_box(this, toon, Vector3(0.22f, 0.006f, 0.16f), Vector3(-0.20f, 0.795f, 0.06f), Color(0.92f, 0.90f, 0.84f), Vector3(0.0f, -0.1f, 0.0f));
	Ref<CylinderMesh> seal;
	seal.instantiate();
	seal->set_top_radius(0.025f);
	seal->set_bottom_radius(0.025f);
	seal->set_height(0.012f);
	seal->set_radial_segments(8);
	seal->set_rings(1);
	seal->set_material(meshutil::toon_material(toon, Color(0.70f, 0.18f, 0.14f), 0.2f));
	MeshInstance3D *smi = memnew(MeshInstance3D);
	smi->set_mesh(seal);
	smi->set_position(Vector3(-0.27f, 0.80f, 0.07f));
	add_child(smi);

	// 打开的书
	meshutil::add_box(this, toon, Vector3(0.18f, 0.012f, 0.13f), Vector3(-0.05f, 0.785f, 0.13f), Color(0.92f, 0.90f, 0.84f), Vector3(0.0f, 0.0f, 0.12f));
	meshutil::add_box(this, toon, Vector3(0.18f, 0.012f, 0.13f), Vector3(0.10f, 0.785f, 0.13f), Color(0.92f, 0.90f, 0.84f), Vector3(0.0f, 0.0f, -0.12f));

	// 蜡烛
	Ref<CylinderMesh> candle;
	candle.instantiate();
	candle->set_top_radius(0.020f);
	candle->set_bottom_radius(0.024f);
	candle->set_height(0.12f);
	candle->set_radial_segments(8);
	candle->set_rings(1);
	candle->set_material(meshutil::toon_material(toon, Color(0.94f, 0.90f, 0.78f), 0.2f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(candle);
	cmi->set_position(Vector3(0.44f, 0.84f, 0.12f));
	add_child(cmi);
	Ref<CylinderMesh> flame_mesh;
	flame_mesh.instantiate();
	flame_mesh->set_top_radius(0.001f);
	flame_mesh->set_bottom_radius(0.013f);
	flame_mesh->set_height(0.042f);
	flame_mesh->set_radial_segments(6);
	flame_mesh->set_rings(1);
	Ref<StandardMaterial3D> flm;
	flm.instantiate();
	flm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	flm->set_albedo(Color(1.0f, 0.76f, 0.32f));
	flm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	flame_mesh->set_material(flm);
	flame = memnew(MeshInstance3D);
	flame->set_mesh(flame_mesh);
	flame->set_position(Vector3(0.44f, 0.925f, 0.12f));
	flame->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(flame);
}
