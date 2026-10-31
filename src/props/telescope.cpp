#include "telescope.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
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
#include "../world/terrain_builder.h"
using namespace godot;

void Telescope::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Telescope::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Telescope::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Telescope::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Telescope::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Telescope::interact);
	ClassDB::bind_method(D_METHOD("get_peeks"), &Telescope::get_peeks);
	ClassDB::bind_method(D_METHOD("get_night"), &Telescope::get_night);
}
String Telescope::get_prompt() const { return String::utf8("按 E 凑近望远镜"); }
Vector3 Telescope::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.85f, 0.0f); }
void Telescope::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (!night) {
		if (get_tree() != nullptr) {
			Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
			if (gd != nullptr) gd->call("flash_hint", String::utf8("天还亮着 · 等星星出来再看"), 2.6f);
		}
		return;
	}
	peeks++;
	static const char *views[4] = {
		"北斗七星挂在北边，勺柄指着北极星。",
		"猎户座的三颗腰带星排成一条斜线。",
		"仙后座像一把歪歪的椅子，在银河边上。",
		"今晚的银河很淡，像一层薄薄的霜。",
	};
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8(views[peeks % 4]), 4.0f);
	}
	if (sparkles != nullptr) sparkles->restart();
}
void Telescope::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (sky != nullptr) {
				const float tod = sky->get_time_of_day();
				night = tod < 0.25f || tod > 0.75f;
			}
			if (sparkles != nullptr && sparkles->is_emitting() != night) sparkles->set_emitting(night);
		} break;
		default: break;
	}
}
void Telescope::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 三脚架
	const Color wood(0.48f, 0.34f, 0.21f);
	for (int i = 0; i < 3; i++) {
		const float a = (float)i * 2.094f;
		meshutil::add_box(this, toon, Vector3(0.03f, 0.90f, 0.03f),
				Vector3(std::cos(a) * 0.18f, 0.44f, std::sin(a) * 0.18f), wood,
				Vector3(std::sin(a) * 0.22f, 0.0f, -std::cos(a) * 0.22f));
	}
	// 镜筒
	Ref<CylinderMesh> tube;
	tube.instantiate();
	tube->set_top_radius(0.055f);
	tube->set_bottom_radius(0.075f);
	tube->set_height(0.62f);
	tube->set_radial_segments(10);
	tube->set_rings(1);
	tube->set_material(meshutil::toon_material(toon, Color(0.66f, 0.52f, 0.26f), 0.2f));
	MeshInstance3D *tmi = memnew(MeshInstance3D);
	tmi->set_mesh(tube);
	tmi->set_position(Vector3(0.0f, 1.02f, 0.02f));
	tmi->set_rotation(Vector3(0.75f, 0.0f, 0.0f));
	add_child(tmi);
	meshutil::add_box(this, toon, Vector3(0.06f, 0.06f, 0.06f), Vector3(0.0f, 0.90f, 0.0f), Color(0.46f, 0.44f, 0.40f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.03f, 0.12f), Vector3(0.0f, 1.22f, 0.22f), Color(0.40f, 0.38f, 0.34f));
	// 星点
	Ref<SphereMesh> m;
	m.instantiate();
	m->set_radius(0.008f);
	m->set_height(0.016f);
	m->set_radial_segments(4);
	m->set_rings(2);
	Ref<StandardMaterial3D> mm;
	mm.instantiate();
	mm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mm->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	mm->set_albedo(Color(1.0f, 0.94f, 0.76f, 0.7f));
	mm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	m->set_material(mm);
	sparkles = memnew(CPUParticles3D);
	sparkles->set_name("ScopeSparkles");
	sparkles->set_amount(10);
	sparkles->set_lifetime(2.5f);
	sparkles->set_mesh(m);
	sparkles->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	sparkles->set_emission_sphere_radius(0.10f);
	sparkles->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	sparkles->set_spread(30.0f);
	sparkles->set_gravity(Vector3(0.0f, 0.02f, 0.0f));
	sparkles->set_position(Vector3(0.0f, 1.25f, 0.25f));
	sparkles->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	sparkles->set_emitting(false);
	add_child(sparkles);
}
