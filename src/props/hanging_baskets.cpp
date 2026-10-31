#include "hanging_baskets.h"
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
using namespace godot;

void HangingBaskets::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_types"), &HangingBaskets::get_item_types);
	ClassDB::bind_method(D_METHOD("get_sway"), &HangingBaskets::get_sway);
}
void HangingBaskets::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			sway = std::sin(t * 1.1f) * 0.07f + std::sin(t * 2.3f) * 0.02f;
			for (int i = 0; i < 3; i++) if (vines[i] != nullptr) vines[i]->set_rotation(Vector3(sway * 0.5f, 0.0f, sway));
		} break;
		default: break;
	}
}
void HangingBaskets::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 横杆与钩
	meshutil::add_box(this, toon, Vector3(1.50f, 0.06f, 0.06f), Vector3(0.0f, 0.0f, 0.0f), Color(0.42f, 0.38f, 0.32f));
	for (int i = 0; i < 3; i++) {
		const float x = -0.50f + 0.50f * (float)i;
		meshutil::add_box(this, toon, Vector3(0.02f, 0.28f, 0.02f), Vector3(x, -0.15f, 0.0f), Color(0.36f, 0.34f, 0.30f));
		// 藤篮
		Ref<CylinderMesh> basket;
		basket.instantiate();
		basket->set_top_radius(0.16f);
		basket->set_bottom_radius(0.10f);
		basket->set_height(0.16f);
		basket->set_radial_segments(10);
		basket->set_rings(1);
		basket->set_material(meshutil::toon_material(toon, Color(0.62f, 0.46f, 0.28f), 0.2f));
		MeshInstance3D *bmi = memnew(MeshInstance3D);
		bmi->set_mesh(basket);
		bmi->set_position(Vector3(x, -0.38f, 0.0f));
		add_child(bmi);
		// 花与叶
		for (int l = 0; l < 6; l++) {
			const float a = (float)l * 1.047f;
			meshutil::add_sphere(this, toon, 0.04f, Vector3(x + std::cos(a) * 0.10f, -0.28f, std::sin(a) * 0.10f), Color(0.30f, 0.50f, 0.22f));
		}
		for (int f = 0; f < 3; f++) {
			const float a = (float)f * 2.094f;
			meshutil::add_sphere(this, toon, 0.035f, Vector3(x + std::cos(a) * 0.07f, -0.25f, std::sin(a) * 0.07f),
					i % 2 == 0 ? Color(0.86f, 0.42f, 0.46f) : Color(0.92f, 0.78f, 0.36f));
		}
		// 垂藤（会摆）
		Node3D *v = memnew(Node3D);
		v->set_name(String("Vine") + String::num_int64(i));
		v->set_position(Vector3(x, -0.45f, 0.0f));
		add_child(v);
		vines[i] = v;
		for (int s = 0; s < 4; s++) {
			meshutil::add_box(v, toon, Vector3(0.012f, 0.16f, 0.012f),
					Vector3(0.02f * (float)(s % 2), -0.08f * (float)s, 0.02f * (float)(s % 3)), Color(0.30f, 0.48f, 0.22f),
					Vector3(0.0f, 0.0f, 0.10f * (float)s));
			meshutil::add_sphere(v, toon, 0.028f,
					Vector3(0.02f * (float)(s % 2) - 0.02f, -0.08f * (float)s, 0.02f * (float)(s % 3)), Color(0.34f, 0.52f, 0.26f));
		}
	}
}
