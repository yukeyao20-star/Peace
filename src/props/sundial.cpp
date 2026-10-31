#include "sundial.h"
#include <cmath>
#include <cstdio>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void Sundial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Sundial::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Sundial::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Sundial::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Sundial::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Sundial::interact);
	ClassDB::bind_method(D_METHOD("get_hour"), &Sundial::get_hour);
	ClassDB::bind_method(D_METHOD("is_daytime"), &Sundial::is_daytime);
	ClassDB::bind_method(D_METHOD("get_reads"), &Sundial::get_reads);
}
String Sundial::get_prompt() const { return String::utf8("按 E 看看日晷"); }
Vector3 Sundial::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.85f, 0.0f); }
void Sundial::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	reads++;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) {
			String msg;
			if (!is_daytime()) {
				msg = String::utf8("天黑了 · 日晷只剩一圈石头的凉意");
			} else {
				const int hh = (int)(tod * 24.0f);
				const int mm = (int)((tod * 24.0f - (float)hh) * 60.0f);
				char buf[96];
				std::snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
				msg = String::utf8("晷影指在 ") + String::utf8(buf) + String::utf8(" · 云影挪得很慢");
			}
			gd->call("flash_hint", msg, 2.8f);
		}
	}
}
void Sundial::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (sky != nullptr) tod = sky->get_time_of_day();
			if (shadow != nullptr) {
				const bool vis = is_daytime() && (sky == nullptr || sky->get_rain_amount() < 0.6f);
				shadow->set_visible(vis);
				if (vis) {
					// 影子绕表盘转：清晨指向一侧，正午最短，傍晚转到另一侧。
					const float ang = -(tod - 0.5f) * 12.56637f;
					shadow->set_rotation(Vector3(0.0f, ang, 0.0f));
					const float len = 0.9f + 0.5f * std::fabs(tod - 0.5f) * 4.0f;
					shadow->set_scale(Vector3(1.0f, 1.0f, Math::clamp(len, 0.8f, 1.9f)));
				}
			}
			if (snow_cap != nullptr) snow_cap->set_visible(sky != nullptr && sky->get_snow_amount() > 0.35f);
		} break;
		default: break;
	}
}
void Sundial::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	// 石座：底盘、立柱、柱头
	Ref<CylinderMesh> base;
	base.instantiate();
	base->set_top_radius(0.32f);
	base->set_bottom_radius(0.38f);
	base->set_height(0.12f);
	base->set_radial_segments(10);
	base->set_rings(1);
	base->set_material(meshutil::toon_material(toon, Color(0.56f, 0.55f, 0.50f), 0.3f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(base);
	bmi->set_position(Vector3(0.0f, 0.06f, 0.0f));
	add_child(bmi);
	Ref<CylinderMesh> column;
	column.instantiate();
	column->set_top_radius(0.16f);
	column->set_bottom_radius(0.20f);
	column->set_height(0.62f);
	column->set_radial_segments(8);
	column->set_rings(1);
	column->set_material(meshutil::toon_material(toon, Color(0.60f, 0.58f, 0.52f), 0.3f));
	MeshInstance3D *cmi = memnew(MeshInstance3D);
	cmi->set_mesh(column);
	cmi->set_position(Vector3(0.0f, 0.43f, 0.0f));
	add_child(cmi);
	meshutil::add_box(this, toon, Vector3(0.46f, 0.07f, 0.46f), Vector3(0.0f, 0.77f, 0.0f), Color(0.62f, 0.60f, 0.54f));
	// 青铜表盘
	Ref<CylinderMesh> dial;
	dial.instantiate();
	dial->set_top_radius(0.42f);
	dial->set_bottom_radius(0.42f);
	dial->set_height(0.035f);
	dial->set_radial_segments(16);
	dial->set_rings(1);
	dial->set_material(meshutil::toon_material(toon, Color(0.42f, 0.52f, 0.46f), 0.2f));
	MeshInstance3D *dmi = memnew(MeshInstance3D);
	dmi->set_mesh(dial);
	dmi->set_position(Vector3(0.0f, 0.82f, 0.0f));
	add_child(dmi);
	// 小时刻度：12 根小铜条
	Ref<BoxMesh> mark;
	mark.instantiate();
	mark->set_size(Vector3(0.012f, 0.006f, 0.06f));
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(mark);
	mm->set_instance_count(12);
	for (int i = 0; i < 12; i++) {
		const float a = (float)i * 0.5236f;
		const Basis b(Vector3(0.0f, 1.0f, 0.0f), a);
		mm->set_instance_transform(i, Transform3D(b, Vector3(std::sin(a) * 0.33f, 0.84f, std::cos(a) * 0.33f)));
	}
	MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
	mmi->set_name("HourMarks");
	mmi->set_multimesh(mm);
	mmi->set_material_override(meshutil::toon_material(toon, Color(0.30f, 0.38f, 0.33f), 0.2f));
	add_child(mmi);
	// 晷针：斜放的三角板
	Ref<BoxMesh> gnomon;
	gnomon.instantiate();
	gnomon->set_size(Vector3(0.02f, 0.26f, 0.10f));
	gnomon->set_material(meshutil::toon_material(toon, Color(0.46f, 0.56f, 0.50f), 0.2f));
	MeshInstance3D *gmi = memnew(MeshInstance3D);
	gmi->set_mesh(gnomon);
	gmi->set_position(Vector3(0.0f, 0.94f, 0.0f));
	gmi->set_rotation(Vector3(0.42f, 0.0f, 0.0f));
	add_child(gmi);
	// 影子：一张贴地深色薄片，随时间转动
	shadow = memnew(Node3D);
	shadow->set_name("Shadow");
	shadow->set_position(Vector3(0.0f, 0.845f, 0.0f));
	add_child(shadow);
	Ref<BoxMesh> sh;
	sh.instantiate();
	sh->set_size(Vector3(0.045f, 0.004f, 0.34f));
	Ref<StandardMaterial3D> shm;
	shm.instantiate();
	shm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	shm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	shm->set_albedo(Color(0.10f, 0.12f, 0.10f, 0.72f));
	shm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	sh->set_material(shm);
	MeshInstance3D *smi = memnew(MeshInstance3D);
	smi->set_mesh(sh);
	smi->set_position(Vector3(0.0f, 0.0f, 0.17f));
	smi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	shadow->add_child(smi);
	// 雪帽
	snow_cap = meshutil::add_box(this, toon, Vector3(0.84f, 0.03f, 0.84f), Vector3(0.0f, 0.855f, 0.0f), Color(0.94f, 0.96f, 0.98f));
	snow_cap->set_visible(false);
}
