#include "pan_rack.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
using namespace godot;

void PanRack::_bind_methods() { ClassDB::bind_method(D_METHOD("get_item_types"), &PanRack::get_item_types); }
void PanRack::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) build();
}
void PanRack::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	set_rotation(Vector3(0.0f, -1.5708f, 0.0f));
	// 挂杆与托架
	meshutil::add_box(this, toon, Vector3(0.90f, 0.05f, 0.05f), Vector3(0.0f, 0.40f, 0.0f), Color(0.46f, 0.40f, 0.32f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.10f, 0.16f), Vector3(-0.40f, 0.36f, -0.06f), Color(0.46f, 0.40f, 0.32f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.10f, 0.16f), Vector3(0.40f, 0.36f, -0.06f), Color(0.46f, 0.40f, 0.32f));
	// 三口锅（挂着的圆锅）
	const float xs[3] = { -0.28f, 0.02f, 0.30f };
	const Color metal[3] = { Color(0.34f, 0.32f, 0.30f), Color(0.52f, 0.40f, 0.28f), Color(0.40f, 0.42f, 0.44f) };
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.02f, 0.10f, 0.02f), Vector3(xs[i], 0.35f, 0.0f), Color(0.30f, 0.28f, 0.26f));
		Ref<CylinderMesh> pan;
		pan.instantiate();
		pan->set_top_radius(0.11f + 0.02f * (float)i);
		pan->set_bottom_radius(0.09f + 0.02f * (float)i);
		pan->set_height(0.16f);
		pan->set_radial_segments(12);
		pan->set_rings(1);
		pan->set_material(meshutil::toon_material(toon, metal[i], 0.2f));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(pan);
		mi->set_position(Vector3(xs[i], 0.18f, 0.02f));
		add_child(mi);
		meshutil::add_box(this, toon, Vector3(0.04f, 0.02f, 0.20f), Vector3(xs[i], 0.26f, 0.16f), metal[i]);
	}
	// 汤勺
	meshutil::add_box(this, toon, Vector3(0.02f, 0.28f, 0.02f), Vector3(0.16f, 0.15f, 0.10f), Color(0.50f, 0.38f, 0.24f));
	meshutil::add_sphere(this, toon, 0.05f, Vector3(0.16f, 0.01f, 0.10f), Color(0.50f, 0.38f, 0.24f));
	// 隔热垫
	meshutil::add_box(this, toon, Vector3(0.16f, 0.02f, 0.16f), Vector3(-0.25f, 0.01f, 0.15f), Color(0.62f, 0.42f, 0.34f), Vector3(0.0f, 0.4f, 0.0f));
}
