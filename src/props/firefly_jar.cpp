#include "firefly_jar.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
using namespace godot;

void FireflyJar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &FireflyJar::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &FireflyJar::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &FireflyJar::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &FireflyJar::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &FireflyJar::interact);
	ClassDB::bind_method(D_METHOD("get_sealed"), &FireflyJar::get_sealed);
	ClassDB::bind_method(D_METHOD("get_night"), &FireflyJar::get_night);
	ClassDB::bind_method(D_METHOD("get_toggles"), &FireflyJar::get_toggles);
}
String FireflyJar::get_prompt() const { return sealed ? String::utf8("按 E 打开萤火虫罐") : String::utf8("按 E 把萤火虫捉回来"); }
Vector3 FireflyJar::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.18f, 0.0f); }
void FireflyJar::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	sealed = !sealed;
	toggles++;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", sealed ? String::utf8("把萤火虫收回罐里 · 光又聚在一起") : String::utf8("打开盖子 · 几点萤火飘出来"), 2.6f);
	}
}
void FireflyJar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			t += (float)get_process_delta_time();
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			night = sky != nullptr && (sky->get_time_of_day() < 0.25f || sky->get_time_of_day() > 0.75f);
			for (int i = 0; i < COUNT; i++) {
				if (motes[i] == nullptr) continue;
				motes[i]->set_visible(sealed && night);
				const float a = t * 0.7f + (float)i * 1.3f;
				motes[i]->set_position(Vector3(0.05f * std::cos(a), 0.08f + 0.03f * std::sin(t * 1.7f + (float)i), 0.05f * std::sin(a)));
				const float pulse = 0.7f + 0.3f * std::sin(t * 2.1f + (float)i * 1.7f);
				motes[i]->set_scale(Vector3(pulse, pulse, pulse));
			}
		} break;
		default: break;
	}
}
void FireflyJar::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	// 小木凳
	meshutil::add_box(this, toon, Vector3(0.22f, 0.20f, 0.22f), Vector3(0.0f, 0.10f, 0.0f), Color(0.52f, 0.38f, 0.24f));
	// 玻璃罐
	Ref<CylinderMesh> jar;
	jar.instantiate();
	jar->set_top_radius(0.085f);
	jar->set_bottom_radius(0.075f);
	jar->set_height(0.20f);
	jar->set_radial_segments(10);
	jar->set_rings(1);
	Ref<StandardMaterial3D> jm;
	jm.instantiate();
	jm->set_albedo(Color(0.76f, 0.86f, 0.88f, 0.35f));
	jm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	jm->set_roughness(0.1f);
	jar->set_material(jm);
	MeshInstance3D *jmi = memnew(MeshInstance3D);
	jmi->set_mesh(jar);
	jmi->set_position(Vector3(0.0f, 0.30f, 0.0f));
	add_child(jmi);
	// 盖子
	meshutil::add_box(this, toon, Vector3(0.19f, 0.02f, 0.19f), Vector3(0.0f, 0.41f, 0.0f), Color(0.50f, 0.44f, 0.32f));
	// 萤火
	Ref<SphereMesh> m;
	m.instantiate();
	m->set_radius(0.014f);
	m->set_height(0.028f);
	m->set_radial_segments(5);
	m->set_rings(3);
	Ref<StandardMaterial3D> mm;
	mm.instantiate();
	mm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mm->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	mm->set_albedo(Color(1.0f, 0.90f, 0.45f, 0.85f));
	mm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	m->set_material(mm);
	for (int i = 0; i < COUNT; i++) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(m);
		mi->set_visible(false);
		mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(mi);
		motes[i] = mi;
	}
}
