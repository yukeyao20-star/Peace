#include "bread_oven.h"
#include <cmath>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void BreadOven::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &BreadOven::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &BreadOven::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &BreadOven::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &BreadOven::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &BreadOven::interact);
	ClassDB::bind_method(D_METHOD("is_baking"), &BreadOven::is_baking);
	ClassDB::bind_method(D_METHOD("get_bakes"), &BreadOven::get_bakes);
	ClassDB::bind_method(D_METHOD("has_bread"), &BreadOven::has_bread);
}
String BreadOven::get_prompt() const {
	if (baking) return String::utf8("炉火正旺 · 再等等");
	return String::utf8("按 E 生火烤面包");
}
Vector3 BreadOven::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.75f, 0.0f); }
void BreadOven::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (baking) {
		if (get_tree() != nullptr) {
			Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
			if (gd != nullptr) gd->call("flash_hint", String::utf8("炉膛里的柴噼啪响 · 面包还在里头"), 2.4f);
		}
		return;
	}
	baking = true;
	bake_t = 6.0f;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("添了把柴 · 炉膛慢慢亮起来"), 2.6f);
	}
}
void BreadOven::finish_bake() {
	baking = false;
	bakes++;
	if (bread != nullptr) bread->set_visible(true);
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("面包出炉 · 外壳脆，掰开还冒热气"), 3.0f);
	}
}
void BreadOven::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (baking) {
				bake_t -= delta;
				if (bake_t <= 0.0f) finish_bake();
			}
			if (fire != nullptr) {
				fire->set_visible(baking);
				fire->set_param(Light3D::PARAM_ENERGY, 0.6f + 0.18f * std::sin(t * 9.0f) + 0.08f * std::sin(t * 23.0f));
			}
			if (smoke != nullptr) smoke->set_emitting(baking);
			if (bread != nullptr && bread->is_visible()) {
				bread->set_scale(Vector3(1.0f, 1.0f + 0.01f * std::sin(t * 2.0f), 1.0f));
			}
			if (snow_cap != nullptr) snow_cap->set_visible(sky != nullptr && sky->get_snow_amount() > 0.35f);
		} break;
		default: break;
	}
}
void BreadOven::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 2.20f, 0.0f));
	// 石基与圆顶
	meshutil::add_box(this, toon, Vector3(1.00f, 0.16f, 0.90f), Vector3(0.0f, 0.08f, 0.0f), Color(0.52f, 0.50f, 0.46f));
	Ref<CylinderMesh> drum;
	drum.instantiate();
	drum->set_top_radius(0.40f);
	drum->set_bottom_radius(0.46f);
	drum->set_height(0.55f);
	drum->set_radial_segments(12);
	drum->set_rings(1);
	drum->set_material(meshutil::toon_material(toon, Color(0.58f, 0.55f, 0.50f), 0.35f));
	MeshInstance3D *dmi = memnew(MeshInstance3D);
	dmi->set_mesh(drum);
	dmi->set_position(Vector3(0.0f, 0.43f, 0.0f));
	add_child(dmi);
	Ref<SphereMesh> dome;
	dome.instantiate();
	dome->set_radius(0.44f);
	dome->set_height(0.44f);
	dome->set_radial_segments(12);
	dome->set_rings(6);
	dome->set_is_hemisphere(true);
	dome->set_material(meshutil::toon_material(toon, Color(0.62f, 0.59f, 0.53f), 0.35f));
	MeshInstance3D *domi = memnew(MeshInstance3D);
	domi->set_mesh(dome);
	domi->set_position(Vector3(0.0f, 0.70f, 0.0f));
	add_child(domi);
	// 炉口：深色拱洞 + 石拱
	meshutil::add_box(this, toon, Vector3(0.34f, 0.28f, 0.06f), Vector3(0.0f, 0.30f, 0.42f), Color(0.16f, 0.13f, 0.11f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.34f, 0.12f), Vector3(-0.22f, 0.32f, 0.45f), Color(0.56f, 0.53f, 0.48f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.34f, 0.12f), Vector3(0.22f, 0.32f, 0.45f), Color(0.56f, 0.53f, 0.48f));
	meshutil::add_box(this, toon, Vector3(0.54f, 0.10f, 0.12f), Vector3(0.0f, 0.48f, 0.45f), Color(0.56f, 0.53f, 0.48f));
	meshutil::add_box(this, toon, Vector3(0.46f, 0.04f, 0.18f), Vector3(0.0f, 0.18f, 0.52f), Color(0.50f, 0.47f, 0.42f));
	// 烟囱
	Ref<CylinderMesh> pipe;
	pipe.instantiate();
	pipe->set_top_radius(0.09f);
	pipe->set_bottom_radius(0.11f);
	pipe->set_height(0.55f);
	pipe->set_radial_segments(8);
	pipe->set_rings(1);
	pipe->set_material(meshutil::toon_material(toon, Color(0.46f, 0.43f, 0.40f), 0.4f));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_mesh(pipe);
	pmi->set_position(Vector3(0.22f, 1.10f, -0.10f));
	add_child(pmi);
	// 炉口的橘光
	fire = memnew(OmniLight3D);
	fire->set_position(Vector3(0.0f, 0.30f, 0.62f));
	fire->set_color(Color(1.0f, 0.55f, 0.22f));
	fire->set_param(Light3D::PARAM_ENERGY, 0.7f);
	fire->set_param(Light3D::PARAM_RANGE, 2.6f);
	fire->set_shadow(false);
	fire->set_visible(false);
	add_child(fire);
	// 面包：出炉后出现在炉口前的木板上
	meshutil::add_box(this, toon, Vector3(0.46f, 0.04f, 0.28f), Vector3(0.0f, 0.205f, 0.66f), Color(0.60f, 0.44f, 0.27f));
	bread = memnew(Node3D);
	bread->set_name("Bread");
	bread->set_position(Vector3(0.0f, 0.255f, 0.66f));
	bread->set_visible(false);
	add_child(bread);
	Ref<SphereMesh> loaf;
	loaf.instantiate();
	loaf->set_radius(0.13f);
	loaf->set_height(0.16f);
	loaf->set_radial_segments(10);
	loaf->set_rings(5);
	loaf->set_material(meshutil::toon_material(toon, Color(0.76f, 0.52f, 0.26f), 0.3f));
	MeshInstance3D *lmi = memnew(MeshInstance3D);
	lmi->set_mesh(loaf);
	lmi->set_scale(Vector3(1.2f, 0.55f, 0.8f));
	bread->add_child(lmi);
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(bread, toon, Vector3(0.012f, 0.008f, 0.14f), Vector3(-0.05f + 0.05f * (float)i, 0.05f, 0.0f), Color(0.55f, 0.36f, 0.18f), Vector3(0.0f, 0.0f, 0.5f));
	}
	// 木铲靠在炉边
	meshutil::add_box(this, toon, Vector3(0.035f, 1.10f, 0.035f), Vector3(0.52f, 0.62f, 0.28f), Color(0.62f, 0.46f, 0.28f), Vector3(0.0f, 0.0f, 0.22f));
	meshutil::add_box(this, toon, Vector3(0.24f, 0.03f, 0.30f), Vector3(0.40f, 0.10f, 0.34f), Color(0.64f, 0.48f, 0.30f), Vector3(0.0f, 0.0f, 0.22f));
	// 雪帽
	snow_cap = memnew(MeshInstance3D);
	Ref<SphereMesh> cap;
	cap.instantiate();
	cap->set_radius(0.45f);
	cap->set_height(0.20f);
	cap->set_radial_segments(12);
	cap->set_rings(4);
	cap->set_is_hemisphere(true);
	cap->set_material(meshutil::toon_material(toon, Color(0.94f, 0.96f, 0.98f), 0.4f));
	snow_cap->set_mesh(cap);
	snow_cap->set_position(Vector3(0.0f, 0.74f, 0.0f));
	snow_cap->set_visible(false);
	add_child(snow_cap);
	build_smoke();
}
void BreadOven::build_smoke() {
	smoke = memnew(CPUParticles3D);
	smoke->set_name("Smoke");
	smoke->set_position(Vector3(0.22f, 1.42f, -0.10f));
	smoke->set_emitting(false);
	smoke->set_amount(12);
	smoke->set_lifetime(3.2f);
	smoke->set_use_local_coordinates(false);
	smoke->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	smoke->set_spread(18.0f);
	smoke->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.12f);
	smoke->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.26f);
	smoke->set_param_min(CPUParticles3D::PARAM_SCALE, 0.10f);
	smoke->set_param_max(CPUParticles3D::PARAM_SCALE, 0.22f);
	smoke->set_color(Color(0.78f, 0.76f, 0.72f, 0.55f));
	smoke->set_gravity(Vector3(0.05f, 0.02f, 0.0f));
	smoke->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(smoke);
}
