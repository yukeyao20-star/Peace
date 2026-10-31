#include "compost_bin.h"
#include <cmath>
#include <godot_cpp/classes/cpu_particles3d.hpp>
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

void CompostBin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &CompostBin::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &CompostBin::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &CompostBin::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &CompostBin::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &CompostBin::interact);
	ClassDB::bind_method(D_METHOD("get_turns"), &CompostBin::get_turns);
	ClassDB::bind_method(D_METHOD("is_steaming"), &CompostBin::is_steaming);
	ClassDB::bind_method(D_METHOD("is_snow_covered"), &CompostBin::is_snow_covered);
}
String CompostBin::get_prompt() const { return String::utf8("按 E 翻一翻堆肥"); }
Vector3 CompostBin::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.55f, 0.0f); }
bool CompostBin::is_snow_covered() const { return sky != nullptr && sky->get_snow_amount() > 0.35f; }
void CompostBin::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	turns++;
	turn_t = 3.2f;
	if (pile != nullptr) pile->set_scale(Vector3(1.06f, 0.9f, 1.06f));
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("翻了一遍堆肥 · 热气扑上来，土腥味很暖"), 2.8f);
	}
}
void CompostBin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (turn_t > 0.0f) turn_t = Math::max(0.0f, turn_t - delta);
			const bool warming = turn_t > 0.0f;
			if (steam != nullptr) steam->set_emitting(warming);
			if (pile != nullptr) {
				const float s = warming ? 1.0f + 0.05f * std::sin(t * 9.0f) : 1.0f;
				const float sink = warming ? 0.94f : 1.0f;
				pile->set_scale(Vector3(s, sink, s));
			}
			if (flip != nullptr) {
				const float target = warming ? 0.35f + std::sin(t * 8.0f) * 0.22f : -0.55f;
				flip_angle = Math::move_toward(flip_angle, target, delta * 2.6f);
				flip->set_rotation(Vector3(0.0f, 0.0f, flip_angle));
			}
			if (snow_cap != nullptr) snow_cap->set_visible(is_snow_covered());
			// 夏天有两只小飞虫绕着堆肥打转
			const bool flies_on = sky != nullptr && sky->get_snow_amount() < 0.1f && sky->get_rain_amount() < 0.5f && sky->get_time_of_day() > 0.3f && sky->get_time_of_day() < 0.7f;
			for (int i = 0; i < 2; i++) {
				if (flies[i] == nullptr) continue;
				flies[i]->set_visible(flies_on);
				if (flies_on) {
					const float a = t * (1.3f + 0.4f * (float)i) + (float)i * 2.0f;
					flies[i]->set_position(Vector3(std::cos(a) * 0.20f, 0.74f + 0.10f * std::sin(a * 2.3f), std::sin(a) * 0.18f));
				}
			}
		} break;
		default: break;
	}
}
void CompostBin::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 1.2f, 0.0f));
	// 四角立柱
	for (int i = 0; i < 4; i++) {
		const float x = (i % 2 == 0) ? -0.38f : 0.38f;
		const float z = (i < 2) ? -0.38f : 0.38f;
		meshutil::add_box(this, toon, Vector3(0.07f, 0.78f, 0.07f), Vector3(x, 0.39f, z), Color(0.42f, 0.30f, 0.19f));
	}
	// 板条（三面 + 前面留缝）合成一个 MultiMesh
	Ref<BoxMesh> unit;
	unit.instantiate();
	unit->set_size(Vector3(1.0f, 1.0f, 1.0f));
	Ref<Material> wood = meshutil::toon_material(toon, Color(0.54f, 0.40f, 0.25f), 0.3f);
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(unit);
	mm->set_instance_count(18);
	int idx = 0;
	for (int side = 0; side < 3; side++) {
		for (int s = 0; s < 5; s++) {
			Basis b;
			Vector3 pos;
			const float y = 0.12f + 0.135f * (float)s;
			if (side == 0) { b = Basis().scaled(Vector3(0.74f, 0.06f, 0.02f)); pos = Vector3(0.0f, y, -0.38f); }
			else if (side == 1) { b = Basis().scaled(Vector3(0.02f, 0.06f, 0.74f)); pos = Vector3(-0.38f, y, 0.0f); }
			else { b = Basis().scaled(Vector3(0.02f, 0.06f, 0.74f)); pos = Vector3(0.38f, y, 0.0f); }
			mm->set_instance_transform(idx++, Transform3D(b, pos));
		}
	}
	// 前面三根横板
	for (int s = 0; s < 3; s++) {
		const float y = 0.12f + 0.135f * (float)s;
		mm->set_instance_transform(idx++, Transform3D(Basis().scaled(Vector3(0.74f, 0.06f, 0.02f)), Vector3(0.0f, y, 0.38f)));
	}
	MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
	mmi->set_name("Slats");
	mmi->set_multimesh(mm);
	mmi->set_material_override(wood);
	add_child(mmi);
	// 沤着的堆肥
	Ref<SphereMesh> mound;
	mound.instantiate();
	mound->set_radius(0.34f);
	mound->set_height(0.40f);
	mound->set_radial_segments(10);
	mound->set_rings(5);
	mound->set_material(meshutil::toon_material(toon, Color(0.26f, 0.20f, 0.14f), 0.25f));
	pile = memnew(Node3D);
	pile->set_name("Pile");
	pile->set_position(Vector3(0.0f, 0.52f, 0.0f));
	add_child(pile);
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_mesh(mound);
	pile->add_child(pmi);
	// 露出来的厨余
	meshutil::add_box(pile, toon, Vector3(0.12f, 0.02f, 0.08f), Vector3(-0.10f, 0.13f, 0.06f), Color(0.88f, 0.55f, 0.20f), Vector3(0.0f, 0.4f, 0.15f));
	meshutil::add_sphere(pile, toon, 0.045f, Vector3(0.10f, 0.15f, -0.04f), Color(0.42f, 0.55f, 0.24f));
	meshutil::add_box(pile, toon, Vector3(0.05f, 0.01f, 0.05f), Vector3(-0.02f, 0.19f, -0.10f), Color(0.92f, 0.90f, 0.82f), Vector3(0.2f, 0.0f, -0.2f));
	// 翻堆用的小木叉（斜插在箱边）
	flip = memnew(Node3D);
	flip->set_name("Fork");
	flip->set_position(Vector3(0.46f, 0.42f, 0.30f));
	flip->set_rotation(Vector3(0.0f, 0.0f, -0.55f));
	add_child(flip);
	meshutil::add_box(flip, toon, Vector3(0.025f, 0.62f, 0.025f), Vector3(0.0f, 0.10f, 0.0f), Color(0.60f, 0.44f, 0.27f));
	meshutil::add_box(flip, toon, Vector3(0.12f, 0.02f, 0.02f), Vector3(0.0f, -0.20f, 0.0f), Color(0.42f, 0.43f, 0.44f));
	// 盖子（半掀）
	meshutil::add_box(this, toon, Vector3(0.82f, 0.05f, 0.82f), Vector3(0.0f, 0.81f, 0.0f), Color(0.48f, 0.35f, 0.22f), Vector3(0.06f, 0.0f, 0.0f));
	for (int i = 0; i < 2; i++) {
		Node3D *f = memnew(Node3D);
		f->set_name(String("Fly") + String::num_int64(i));
		f->set_visible(false);
		add_child(f);
		flies[i] = f;
		meshutil::add_sphere(f, toon, 0.012f, Vector3(), Color(0.16f, 0.15f, 0.13f))->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	}
	// 雪帽
	snow_cap = meshutil::add_box(this, toon, Vector3(0.84f, 0.035f, 0.84f), Vector3(0.0f, 0.84f, 0.0f), Color(0.94f, 0.96f, 0.98f), Vector3(0.06f, 0.0f, 0.0f));
	snow_cap->set_visible(false);
	build_steam();
}
void CompostBin::build_steam() {
	steam = memnew(CPUParticles3D);
	steam->set_name("Steam");
	steam->set_position(Vector3(0.0f, 0.88f, 0.0f));
	steam->set_emitting(false);
	steam->set_amount(18);
	steam->set_lifetime(2.2f);
	steam->set_use_local_coordinates(false);
	steam->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.16f);
	steam->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.32f);
	steam->set_param_min(CPUParticles3D::PARAM_ANGLE, 0.0f);
	steam->set_param_max(CPUParticles3D::PARAM_ANGLE, 360.0f);
	steam->set_param_min(CPUParticles3D::PARAM_SCALE, 0.10f);
	steam->set_param_max(CPUParticles3D::PARAM_SCALE, 0.22f);
	steam->set_gravity(Vector3(0.0f, -0.02f, 0.0f));
	steam->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	steam->set_spread(28.0f);
	steam->set_color(Color(0.95f, 0.96f, 0.94f, 0.55f));
	add_child(steam);
}
