#include "herb_bundles.h"
#include <cmath>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
using namespace godot;

void HerbBundles::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bundle_count"), &HerbBundles::get_bundle_count);
	ClassDB::bind_method(D_METHOD("is_snowy"), &HerbBundles::is_snowy);
}
bool HerbBundles::is_snowy() const { return sky != nullptr && sky->get_snow_amount() > 0.35f; }
void HerbBundles::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (Engine::get_singleton()->is_editor_hint()) break;
			build();
			set_process(true);
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			for (int i = 0; i < HANGING; i++) {
				if (hanging[i] != nullptr) {
					hanging[i]->set_rotation(Vector3(std::sin(t * 1.3f + (float)i * 0.9f) * 0.05f, 0.0f, std::sin(t * 1.05f + (float)i * 1.7f) * 0.07f));
				}
				if (snow_caps[i] != nullptr) snow_caps[i]->set_visible(is_snowy());
			}
		} break;
		default: break;
	}
}
void HerbBundles::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 与门廊檐平行的挂绳
	meshutil::add_box(this, toon, Vector3(1.6f, 0.015f, 0.015f), Vector3(0.0f, 0.02f, 0.0f), Color(0.34f, 0.28f, 0.20f));
	const float xs[HANGING] = { -0.58f, -0.18f, 0.24f, 0.64f };
	for (int i = 0; i < HANGING; i++) {
		Node3D *h = memnew(Node3D);
		h->set_name(String("Bundle") + String::num_int64(i));
		h->set_position(Vector3(xs[i], 0.0f, 0.0f));
		add_child(h);
		hanging[i] = h;
		meshutil::add_box(h, toon, Vector3(0.010f, 0.16f, 0.010f), Vector3(0.0f, -0.06f, 0.0f), Color(0.56f, 0.48f, 0.34f));
		if (i == 0) {
			// 一束鼠尾草：灰绿叶 + 细茎
			for (int s = 0; s < 5; s++) {
				const float a = -0.25f + 0.125f * (float)s;
				meshutil::add_box(h, toon, Vector3(0.014f, 0.30f, 0.014f), Vector3(std::sin(a) * 0.05f, -0.26f, 0.0f), Color(0.36f, 0.46f, 0.30f), Vector3(0.0f, 0.0f, a));
				meshutil::add_sphere(h, toon, 0.030f, Vector3(std::sin(a) * 0.075f, -0.34f, 0.0f), Color(0.46f, 0.54f, 0.38f));
			}
			meshutil::add_box(h, toon, Vector3(0.10f, 0.03f, 0.10f), Vector3(0.0f, 0.02f, 0.0f), Color(0.62f, 0.52f, 0.36f));
		} else if (i == 1) {
			// 一束薰衣草：紫色小穗
			for (int s = 0; s < 4; s++) {
				const float a = -0.18f + 0.12f * (float)s;
				meshutil::add_box(h, toon, Vector3(0.012f, 0.28f, 0.012f), Vector3(std::sin(a) * 0.04f, -0.24f, 0.0f), Color(0.44f, 0.42f, 0.32f), Vector3(0.0f, 0.0f, a));
				meshutil::add_sphere(h, toon, 0.026f, Vector3(std::sin(a) * 0.06f, -0.33f, 0.0f), Color(0.56f, 0.44f, 0.72f));
			}
		} else if (i == 2) {
			// 蒜辫：五颗蒜头
			for (int g = 0; g < 5; g++) {
				meshutil::add_sphere(h, toon, 0.045f, Vector3(0.02f * std::sin((float)g * 2.0f), -0.14f - 0.085f * (float)g, 0.02f * std::cos((float)g * 2.0f)), Color(0.90f, 0.87f, 0.78f))->set_scale(Vector3(1.0f, 0.85f, 1.0f));
			}
		} else {
			// 辣椒串
			for (int c = 0; c < 7; c++) {
				const float a = (float)c * 0.9f;
				meshutil::add_sphere(h, toon, 0.030f, Vector3(std::sin(a) * 0.05f, -0.12f - 0.075f * (float)c, std::cos(a) * 0.03f), Color(0.74f, 0.16f, 0.12f))->set_scale(Vector3(0.7f, 1.6f, 0.7f));
				meshutil::add_box(h, toon, Vector3(0.010f, 0.04f, 0.010f), Vector3(std::sin(a) * 0.05f, -0.09f - 0.075f * (float)c, std::cos(a) * 0.03f), Color(0.30f, 0.46f, 0.22f));
			}
		}
		MeshInstance3D *snow = meshutil::add_box(h, toon, Vector3(0.12f, 0.02f, 0.10f), Vector3(0.0f, -0.30f, 0.0f), Color(0.94f, 0.96f, 0.98f));
		snow->set_visible(false);
		snow_caps[i] = snow;
	}
}
