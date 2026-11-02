#include "ship_quest.h"
#include <cmath>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../actors/player.h"
#include "../systems/ui_style.h"
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"
using namespace godot;

namespace {
const char *PART_NAMES[ShipQuest::PART_COUNT] = {
	"龙骨木·前", "龙骨木·后", "肋材·一", "肋材·二", "肋材·三", "肋材·四", "船首柱",
	"船板·一", "船板·二", "船板·三", "船板·四", "船板·五", "船板·六", "船板·七", "船板·八",
	"甲板板·一", "甲板板·二", "甲板板·三", "甲板板·四", "舱门",
	"主桅", "横桁·一", "横桁·二", "缆绳·一", "缆绳·二",
	"帆布·一", "帆布·二", "帆索",
	"舵轮", "尾舵", "罗盘", "船灯"
};
const int PART_STAGE[ShipQuest::PART_COUNT] = {
	1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,
	3,3,3,3,3,
	4,4,4,4,4,
	5,5,5,
	6,6,6,6
};
const int PART_KIND[ShipQuest::PART_COUNT] = {
	0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,
	3,3,3,2,2,
	2,2,2,
	4,1,4,5
};
const float PART_X[ShipQuest::PART_COUNT] = {
	12.9f, 13.6f, 11.6f, 14.6f, 10.8f, 15.2f, 8.0f,
	6.2f, 7.0f, 5.6f, 8.6f, -7.0f, -8.2f, -9.2f, -6.2f,
	13.8f, 16.6f, 15.6f, 17.8f, 14.6f,
	22.6f, 24.2f, 21.0f, -6.6f, -10.8f,
	13.2f, 14.2f, 11.9f,
	16.9f, 6.8f, 10.4f, 9.2f
};
const float PART_Z[ShipQuest::PART_COUNT] = {
	3.2f, 4.1f, 5.0f, 5.2f, 3.0f, 3.6f, 4.0f,
	15.2f, 16.2f, 14.0f, 12.4f, -12.8f, -13.8f, -12.2f, -14.2f,
	21.5f, 21.0f, 24.6f, 20.4f, 20.0f,
	17.4f, 15.0f, 14.0f, -13.5f, -14.4f,
	17.2f, 18.2f, 12.6f,
	21.8f, 16.4f, 20.3f, 11.0f
};
const int STAGE_THRESHOLD[6] = { 7, 15, 20, 25, 28, 32 };
// 离岸航线：从木屋湖畔的干船坞出发，穿过大湖、驶入西侧长河，最后进入西海。
const Vector3 VOYAGE[14] = {
	Vector3(-60.0f, 0.0f, 70.0f), Vector3(-66.0f, 0.0f, 64.0f), Vector3(-80.0f, 0.0f, 58.0f),
	Vector3(-92.0f, 0.0f, 44.0f), Vector3(-102.0f, 0.0f, 30.0f), Vector3(-116.0f, 0.0f, 22.0f),
	Vector3(-124.0f, 0.0f, 8.0f), Vector3(-138.0f, 0.0f, 0.0f), Vector3(-150.0f, 0.0f, -12.0f),
	Vector3(-165.0f, 0.0f, -14.0f), Vector3(-182.0f, 0.0f, -28.0f), Vector3(-200.0f, 0.0f, -45.0f),
	Vector3(-212.0f, 0.0f, -56.0f)
};
const char *STAGE_NAMES[6] = {
	"龙骨与肋材", "船壳木板", "甲板与舱室", "桅杆与横桁", "船帆与绳索", "舵轮与船灯"
};
}

// ============================ ShipPart ============================

void ShipPart::_bind_methods() {}

void ShipPart::init(ShipQuest *p_owner, int p_index, const String &p_name, int p_kind) {
	owner = p_owner;
	index = p_index;
	part_name = p_name;
	kind = p_kind;
	base_pos = get_position();
	build_visual();
}

void ShipPart::build_visual() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.52f, 0.36f, 0.20f);
	const Color plank(0.66f, 0.50f, 0.30f);
	const Color canvas(0.90f, 0.86f, 0.74f);
	const Color brass(0.78f, 0.64f, 0.32f);
	if (kind == 0) {
		meshutil::add_box(this, toon, Vector3(0.55f, 0.20f, 0.22f), Vector3(0.0f, 0.12f, 0.0f), wood);
		meshutil::add_box(this, toon, Vector3(0.08f, 0.26f, 0.26f), Vector3(0.0f, 0.13f, 0.0f), Color(0.42f, 0.28f, 0.16f));
	} else if (kind == 1) {
		meshutil::add_box(this, toon, Vector3(0.95f, 0.07f, 0.28f), Vector3(0.0f, 0.06f, 0.0f), plank, Vector3(0.0f, 0.25f, 0.0f));
		meshutil::add_box(this, toon, Vector3(0.85f, 0.05f, 0.24f), Vector3(0.0f, 0.14f, 0.0f), wood, Vector3(0.0f, -0.2f, 0.0f));
	} else if (kind == 2) {
		meshutil::add_box(this, toon, Vector3(0.52f, 0.10f, 0.42f), Vector3(0.0f, 0.08f, 0.0f), canvas);
		meshutil::add_box(this, toon, Vector3(0.56f, 0.03f, 0.46f), Vector3(0.0f, 0.035f, 0.0f), Color(0.60f, 0.52f, 0.36f));
	} else if (kind == 3) {
		Ref<CylinderMesh> cyl;
		cyl.instantiate();
		cyl->set_top_radius(0.10f);
		cyl->set_bottom_radius(0.12f);
		cyl->set_height(1.20f);
		cyl->set_radial_segments(8);
		cyl->set_rings(1);
		cyl->set_material(meshutil::toon_material(toon, wood));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(cyl);
		mi->set_position(Vector3(0.0f, 0.62f, 0.0f));
		add_child(mi);
	} else if (kind == 4) {
		Ref<TorusMesh> ring;
		ring.instantiate();
		ring->set_inner_radius(0.26f);
		ring->set_outer_radius(0.36f);
		ring->set_rings(12);
		ring->set_ring_segments(4);
		ring->set_material(meshutil::toon_material(toon, brass));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(ring);
		mi->set_position(Vector3(0.0f, 0.32f, 0.0f));
		add_child(mi);
		meshutil::add_sphere(this, toon, 0.06f, Vector3(0.0f, 0.32f, 0.0f), brass);
	} else {
		meshutil::add_sphere(this, toon, 0.16f, Vector3(0.0f, 0.22f, 0.0f), brass);
		meshutil::add_box(this, toon, Vector3(0.18f, 0.04f, 0.18f), Vector3(0.0f, 0.40f, 0.0f), Color(0.40f, 0.36f, 0.30f));
	}
	// 头顶的小金标记：远远就能看见「这里有个零件」
	Ref<StandardMaterial3D> gm;
	gm.instantiate();
	gm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	gm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	gm->set_albedo(Color(1.0f, 0.86f, 0.42f, 0.92f));
	gm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	Ref<SphereMesh> sm;
	sm.instantiate();
	sm->set_radius(0.055f);
	sm->set_height(0.11f);
	sm->set_radial_segments(6);
	sm->set_rings(3);
	sm->set_material(gm);
	marker = memnew(Node3D);
	marker->set_name("Marker");
	marker->set_position(Vector3(0.0f, 0.95f, 0.0f));
	MeshInstance3D *mmi = memnew(MeshInstance3D);
	mmi->set_mesh(sm);
	mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	marker->add_child(mmi);
	add_child(marker);
}

void ShipPart::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (!Engine::get_singleton()->is_editor_hint()) {
			add_to_group("interactables");
			set_process(true);
		}
	} else if (p_what == NOTIFICATION_PROCESS) {
		t += (float)get_process_delta_time();
		if (marker != nullptr && is_visible()) {
			marker->set_rotation(Vector3(0.0f, t * 1.4f, 0.0f));
			marker->set_position(Vector3(0.0f, 0.95f + 0.06f * std::sin(t * 2.2f), 0.0f));
		}
	}
}

String ShipPart::get_prompt() const {
	if (owner == nullptr || !owner->is_started() || owner->is_part_taken(index) || owner->is_complete()) {
		return String();
	}
	return String::utf8("按 E 拾取「") + part_name + String::utf8("」");
}

Vector3 ShipPart::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.55f, 0.0f);
}

void ShipPart::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (owner != nullptr) {
		owner->collect_part(index);
	}
}

// ============================ ShipQuest ============================

void ShipQuest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &ShipQuest::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &ShipQuest::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &ShipQuest::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &ShipQuest::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &ShipQuest::interact);
	ClassDB::bind_method(D_METHOD("is_started"), &ShipQuest::is_started);
	ClassDB::bind_method(D_METHOD("is_complete"), &ShipQuest::is_complete);
	ClassDB::bind_method(D_METHOD("is_departing"), &ShipQuest::is_departing);
	ClassDB::bind_method(D_METHOD("get_part_count"), &ShipQuest::get_part_count);
	ClassDB::bind_method(D_METHOD("get_carried"), &ShipQuest::get_carried);
	ClassDB::bind_method(D_METHOD("get_installed"), &ShipQuest::get_installed);
	ClassDB::bind_method(D_METHOD("get_stage"), &ShipQuest::get_stage);
	ClassDB::bind_method(D_METHOD("is_part_taken", "index"), &ShipQuest::is_part_taken);
	ClassDB::bind_method(D_METHOD("get_parts_visible"), &ShipQuest::get_parts_visible);
}

void ShipQuest::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) {
				add_to_group("interactables");
				build();
			}
			break;
		case NOTIFICATION_PROCESS: {
			if (!departing) break;
			const float delta = (float)get_process_delta_time();
			dep_t += delta;
			// 沿大湖 → 西河 → 西海的航线前进；船先沉入水面，再一路向大海驶去
			const float water_y = (terrain != nullptr ? terrain->get_water_level() : -0.55f);
			dep_s += delta * 10.0f;
			float remaining = dep_s;
			Vector3 pos = VOYAGE[0];
			Vector3 dir(0.0f, 0.0f, 1.0f);
			for (int i = 0; i < 13; ++i) {
				const Vector3 a = VOYAGE[i];
				const Vector3 b = VOYAGE[i + 1];
				const float len = (b - a).length();
				if (remaining <= len || i == 12) {
					const float u = Math::clamp(remaining / Math::max(0.001f, len), 0.0f, 1.0f);
					pos = a.lerp(b, u);
					dir = (b - a).normalized();
					break;
				}
				remaining -= len;
			}
			const float target_y = water_y - 0.10f - 0.69f;
			Vector3 g = ship_root->get_global_position();
			g.x = pos.x;
			g.z = pos.z;
			g.y = Math::lerp(g.y, target_y, Math::min(1.0f, delta * 1.2f));
			ship_root->set_global_position(g);
			ship_root->set_rotation(Vector3(0.0f, std::atan2(dir.x, dir.z), std::sin(dep_t * 0.6f) * 0.012f));
			// 摄影机在岸边缓缓跟随
			if (cine_cam != nullptr) {
				const Vector3 ship_g = ship_root->get_global_position();
				Vector3 cam = cine_cam->get_global_position();
				const Vector3 want = ship_g + Vector3(10.5f, 5.2f, 6.5f);
				cam = cam.lerp(want, Math::min(1.0f, delta * 1.1f));
				cine_cam->set_global_position(cam);
				cine_cam->look_at(ship_g + Vector3(0.0f, 2.8f, 0.0f), Vector3(0, 1, 0));
			}
			if (fade != nullptr) {
				const float a = Math::clamp((dep_t - 18.5f) / 4.0f, 0.0f, 1.0f);
				fade->set_color(Color(0.98f, 0.97f, 0.93f, a));
				if (end_title != nullptr) end_title->set_visible(dep_t > 19.0f);
				if (end_sub != nullptr) end_sub->set_visible(dep_t > 19.8f);
			}
			if (dep_t > 26.0f) set_process(false);
		} break;
		default:
			break;
	}
}

String ShipQuest::get_prompt() const {
	if (!started) return String::utf8("按 E 看看旧船图");
	if (complete) return String::utf8("按 E 登船出发");
	if (carried > 0) return String::utf8("按 E 安装零件（") + String::num_int64(carried) + String::utf8("）");
	return String::utf8("按 E 查看船坞进度");
}

Vector3 ShipQuest::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.6f, 0.0f);
}

void ShipQuest::flash(const String &p_msg) const {
	if (get_tree() == nullptr) return;
	Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
	if (gd != nullptr) gd->call("flash_hint", p_msg, 3.0f);
}

void ShipQuest::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (!started) {
		started = true;
		for (int i = 0; i < PART_COUNT; i++) {
			if (parts[i] != nullptr) parts[i]->set_visible(true);
		}
		update_ui();
		flash(String::utf8("主线开始 ·「离岸」\n在岛上找回 32 个船件，把大船重新拼起来"));
		return;
	}
	if (complete) {
		departing = true;
		ui_layer->set_visible(false);
		if (scaffold != nullptr) scaffold->set_visible(false);
		// 把玩家放到甲板上，并停掉自己的移动/碰撞
		Player *pl = nullptr;
		if (get_parent() != nullptr) {
			pl = Object::cast_to<Player>(get_parent()->get_node_or_null(NodePath("Player")));
			if (pl == nullptr) pl = Object::cast_to<Player>(get_parent()->find_child("Player", true, false));
		}
		if (pl != nullptr) {
			player_node = pl;
			pl->hide_hud_for_cutscene();
			pl->set_process(false);
			pl->set_physics_process(false);
			pl->set_collision_layer(0);
			pl->set_collision_mask(0);
			pl->reparent(ship_root, true);
			pl->set_position(Vector3(0.0f, 2.05f, 1.8f));
			pl->set_rotation(Vector3());
			if (Node3D *m = Object::cast_to<Node3D>(pl->get_node_or_null(NodePath("Model")))) {
				m->set_scale(Vector3(1.0f, 1.0f, 1.0f));
				m->set_position(Vector3(0.0f, 0.9f, 0.0f));
				m->set_rotation(Vector3());
				m->set_visible(false); // 宽景里只留船与海，避免胶囊角色穿帮
			}
		}
		if (cine_cam != nullptr) {
			cine_cam->set_global_position(ship_root->get_global_position() + Vector3(10.5f, 5.2f, 6.5f));
			cine_cam->look_at(ship_root->get_global_position() + Vector3(0.0f, 2.8f, 0.0f), Vector3(0, 1, 0));
			cine_cam->set_current(true);
		}
		if (cine_layer != nullptr) cine_layer->set_visible(true);
		if (title_layer != nullptr) title_layer->set_visible(true);
		dep_t = 0.0f;
		set_process(true);
		return;
	}
	if (carried > 0) {
		const int n = carried;
		installed += carried;
		carried = 0;
		update_stage();
		update_ui();
		flash(String::utf8("装上了 ") + String::num_int64(n) + String::utf8(" 个船件 · 已装 ")
				+ String::num_int64(installed) + String::utf8(" / 32"));
		return;
	}
	if (stage < 6) {
		flash(String::utf8("还缺零件 · 下一段是「") + String::utf8(STAGE_NAMES[stage]) + String::utf8("」"));
	} else {
		flash(String::utf8("船体已经完整 · 再检查一遍零件"));
	}
}

void ShipQuest::collect_part(int p_index) {
	if (!started || complete || departing || p_index < 0 || p_index >= PART_COUNT) return;
	if (collected[p_index]) return;
	collected[p_index] = true;
	if (parts[p_index] != nullptr) parts[p_index]->set_visible(false);
	carried++;
	update_ui();
	flash(String::utf8("找到「") + String::utf8(PART_NAMES[p_index]) + String::utf8("」· 带回旧船坞安装"));
}

int ShipQuest::get_parts_visible() const {
	int n = 0;
	for (int i = 0; i < PART_COUNT; i++) {
		if (parts[i] != nullptr && parts[i]->is_visible()) n++;
	}
	return n;
}

void ShipQuest::update_stage() {
	int s = 0;
	for (int i = 0; i < 6; i++) {
		if (installed >= STAGE_THRESHOLD[i]) s = i + 1;
	}
	stage = s;
	for (int i = 1; i <= 6; i++) {
		if (stage_nodes[i] != nullptr) stage_nodes[i]->set_visible(i <= stage);
	}
	if (installed >= PART_COUNT) {
		complete = true;
		if (scaffold != nullptr) scaffold->set_visible(false);
		flash(String::utf8("大船造好了 ·「Peace」\n按 E 登船，向湖心出发"));
	}
}

void ShipQuest::update_ui() {
	if (tracker == nullptr || ui_layer == nullptr) return;
	ui_layer->set_visible(started && !departing);
	if (!started) return;
	String s;
	if (complete) {
		s = String::utf8("离岸 · 船已造好\n按 E 登船出发");
	} else {
		s = String::utf8("离岸 · 已装 ") + String::num_int64(installed) + String::utf8(" / 32");
		s += String::utf8("\n已找 ") + String::num_int64(installed + carried)
				+ String::utf8(" · 待安装 ") + String::num_int64(carried);
		if (stage < 6) {
			s += String::utf8("\n下一段：") + String::utf8(STAGE_NAMES[stage]);
		}
		if (carried > 0) {
			s += String::utf8("\n回旧船坞按 E 安装");
		}
	}
	tracker->set_text(s);
}

void ShipQuest::build_ui() {
	ui_layer = memnew(CanvasLayer);
	ui_layer->set_name("ShipQuestUI");
	ui_layer->set_layer(6);
	add_child(ui_layer);
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_name("Tracker");
	panel->set_anchors_preset(Control::PRESET_CENTER_TOP);
	panel->set_offset(SIDE_TOP, 92.0f);
	panel->set_offset(SIDE_LEFT, -190.0f);
	panel->set_offset(SIDE_RIGHT, 190.0f);
	panel->add_theme_stylebox_override("panel", uistyle::make_wood_textured(12.0f, 0.96f));
	ui_layer->add_child(panel);
	tracker = memnew(Label);
	tracker->set_name("Text");
	tracker->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	Ref<Font> font = uistyle::load_cjk();
	uistyle::dress_label(tracker, 18, uistyle::text_cream(), font, 3);
	panel->add_child(tracker);
	ui_layer->set_visible(false);
}

void ShipQuest::build_cinematic() {
	cine_layer = memnew(CanvasLayer);
	cine_layer->set_name("DepartureUI");
	cine_layer->set_layer(150);
	add_child(cine_layer);
	Control *root = memnew(Control);
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	cine_layer->add_child(root);
	fade = memnew(ColorRect);
	fade->set_anchors_preset(Control::PRESET_FULL_RECT);
	fade->set_color(Color(0.98f, 0.97f, 0.93f, 0.0f));
	fade->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	fade->set_z_index(-5); // 白场必须在标题下面
	root->add_child(fade);
	// 标题单独一层，保证永远压在白场与 3D 画面之上
	title_layer = memnew(CanvasLayer);
	title_layer->set_name("DepartureTitle");
	title_layer->set_layer(151);
	add_child(title_layer);
	CenterContainer *center = memnew(CenterContainer);
	center->set_anchors_preset(Control::PRESET_FULL_RECT);
	title_layer->add_child(center);
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->add_theme_constant_override("separation", 18);
	center->add_child(vb);
	end_title = memnew(Label);
	end_title->set_text(String::utf8("Peace"));
	end_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	uistyle::dress_label(end_title, 92, Color(0.27f, 0.20f, 0.13f), uistyle::load_cjk(), 0);
	end_title->set_visible(false);
	vb->add_child(end_title);
	end_sub = memnew(Label);
	end_sub->set_text(String::utf8("致所有想出发的人"));
	end_sub->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	uistyle::dress_label(end_sub, 26, Color(0.40f, 0.32f, 0.22f), uistyle::load_cjk(), 0);
	end_sub->set_visible(false);
	vb->add_child(end_sub);
	cine_layer->set_visible(false);
	title_layer->set_visible(false);
	cine_cam = memnew(Camera3D);
	cine_cam->set_name("DepartureCamera");
	cine_cam->set_fov(52.0f);
	cine_cam->set_current(false);
	add_child(cine_cam);
}

void ShipQuest::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}
	// 干船坞脚手架
	scaffold = memnew(Node3D);
	scaffold->set_name("Scaffold");
	add_child(scaffold);
	const Color post(0.42f, 0.30f, 0.18f);
	meshutil::add_box(scaffold, toon, Vector3(0.12f, 3.0f, 0.12f), Vector3(-1.6f, 1.5f, -3.2f), post);
	meshutil::add_box(scaffold, toon, Vector3(0.12f, 3.0f, 0.12f), Vector3(1.6f, 1.5f, -3.2f), post);
	meshutil::add_box(scaffold, toon, Vector3(0.12f, 3.0f, 0.12f), Vector3(-1.6f, 1.5f, 3.2f), post);
	meshutil::add_box(scaffold, toon, Vector3(0.12f, 3.0f, 0.12f), Vector3(1.6f, 1.5f, 3.2f), post);
	meshutil::add_box(scaffold, toon, Vector3(3.4f, 0.10f, 0.10f), Vector3(0.0f, 2.9f, -3.2f), post);
	meshutil::add_box(scaffold, toon, Vector3(3.4f, 0.10f, 0.10f), Vector3(0.0f, 2.9f, 3.2f), post);
	meshutil::add_box(scaffold, toon, Vector3(0.10f, 0.10f, 6.5f), Vector3(-1.6f, 2.9f, 0.0f), post);
	meshutil::add_box(scaffold, toon, Vector3(0.10f, 0.10f, 6.5f), Vector3(1.6f, 2.9f, 0.0f), post);
	meshutil::add_box(scaffold, toon, Vector3(0.10f, 2.0f, 0.10f), Vector3(-1.6f, 1.0f, 0.0f), post, Vector3(0.0f, 0.0f, 0.5f));
	build_ship();
	// 零件
	for (int i = 0; i < PART_COUNT; i++) make_part(i);
	build_ui();
	build_cinematic();
	update_ui();
	set_process(false);
}

void ShipQuest::make_part(int p_index) {
	ShipPart *part = memnew(ShipPart);
	part->set_name(String("Part") + String::num_int64(p_index));
	float y = 0.0f;
	if (terrain != nullptr) y = terrain->get_terrain_height(PART_X[p_index], PART_Z[p_index]);
	part->init(this, p_index, String::utf8(PART_NAMES[p_index]), PART_KIND[p_index]);
	add_child(part);
	part->set_global_position(Vector3(PART_X[p_index], y + 0.18f, PART_Z[p_index]));
	part->set_visible(false);
	parts[p_index] = part;
}

void ShipQuest::build_ship() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood_dark(0.34f, 0.22f, 0.12f);
	const Color wood(0.52f, 0.36f, 0.20f);
	const Color trim(0.64f, 0.48f, 0.28f);
	const Color sail_c(0.92f, 0.88f, 0.76f);
	ship_root = memnew(Node3D);
	ship_root->set_name("Ship");
	add_child(ship_root);
	for (int i = 1; i <= 6; i++) {
		stage_nodes[i] = memnew(Node3D);
		stage_nodes[i]->set_name(String("Stage") + String::num_int64(i));
		ship_root->add_child(stage_nodes[i]);
		stage_nodes[i]->set_visible(false);
	}
	// 1 龙骨与肋材
	Node3D *g1 = stage_nodes[1];
	meshutil::add_box(g1, toon, Vector3(0.34f, 0.40f, 7.2f), Vector3(0.0f, 0.55f, 0.0f), wood_dark);
	meshutil::add_box(g1, toon, Vector3(0.36f, 1.50f, 0.36f), Vector3(0.0f, 1.10f, 3.35f), wood_dark);
	meshutil::add_box(g1, toon, Vector3(0.36f, 1.10f, 0.30f), Vector3(0.0f, 1.00f, -3.40f), wood_dark);
	for (int i = 0; i < 7; i++) {
		meshutil::add_box(g1, toon, Vector3(2.30f, 0.20f, 0.22f), Vector3(0.0f, 1.05f, -2.7f + 0.9f * (float)i), wood);
	}
	// 2 船壳
	Node3D *g2 = stage_nodes[2];
	meshutil::add_box(g2, toon, Vector3(2.30f, 0.18f, 7.1f), Vector3(0.0f, 0.78f, 0.0f), wood);
	meshutil::add_box(g2, toon, Vector3(0.14f, 1.15f, 7.0f), Vector3(-1.18f, 1.35f, 0.0f), trim);
	meshutil::add_box(g2, toon, Vector3(0.14f, 1.15f, 7.0f), Vector3(1.18f, 1.35f, 0.0f), trim);
	meshutil::add_box(g2, toon, Vector3(2.30f, 1.15f, 0.14f), Vector3(0.0f, 1.35f, -3.45f), trim);
	meshutil::add_box(g2, toon, Vector3(0.14f, 1.15f, 2.10f), Vector3(-0.58f, 1.35f, 2.85f), trim, Vector3(0.0f, 0.28f, 0.0f));
	meshutil::add_box(g2, toon, Vector3(0.14f, 1.15f, 2.10f), Vector3(0.58f, 1.35f, 2.85f), trim, Vector3(0.0f, -0.28f, 0.0f));
	// 3 甲板与舱室
	Node3D *g3 = stage_nodes[3];
	meshutil::add_box(g3, toon, Vector3(2.22f, 0.12f, 6.5f), Vector3(0.0f, 1.98f, 0.0f), trim);
	meshutil::add_box(g3, toon, Vector3(1.70f, 0.95f, 1.9f), Vector3(0.0f, 2.50f, -1.45f), trim);
	meshutil::add_box(g3, toon, Vector3(0.56f, 0.68f, 0.05f), Vector3(0.0f, 2.34f, -0.49f), wood_dark);
	for (int s = 0; s < 2; s++) {
		meshutil::add_box(g3, toon, Vector3(0.36f, 0.30f, 0.04f), Vector3(s == 0 ? -0.48f : 0.48f, 2.68f, -2.41f), Color(0.62f, 0.72f, 0.74f));
	}
	// 4 桅杆
	Node3D *g4 = stage_nodes[4];
	Ref<CylinderMesh> mast;
	mast.instantiate();
	mast->set_top_radius(0.07f);
	mast->set_bottom_radius(0.11f);
	mast->set_height(5.2f);
	mast->set_radial_segments(8);
	mast->set_rings(1);
	mast->set_material(meshutil::toon_material(toon, wood));
	MeshInstance3D *mast_mi = memnew(MeshInstance3D);
	mast_mi->set_mesh(mast);
	mast_mi->set_position(Vector3(0.0f, 4.6f, 0.70f));
	g4->add_child(mast_mi);
	meshutil::add_box(g4, toon, Vector3(3.7f, 0.12f, 0.12f), Vector3(0.0f, 6.55f, 0.70f), wood);
	// 5 帆与绳索
	Node3D *g5 = stage_nodes[5];
	meshutil::add_box(g5, toon, Vector3(3.0f, 2.7f, 0.04f), Vector3(0.0f, 4.9f, 0.72f), sail_c);
	meshutil::add_box(g5, toon, Vector3(3.02f, 0.08f, 0.08f), Vector3(0.0f, 6.15f, 0.72f), trim);
	meshutil::add_box(g5, toon, Vector3(0.03f, 5.0f, 0.03f), Vector3(0.0f, 3.7f, 3.15f), Color(0.50f, 0.42f, 0.30f), Vector3(0.55f, 0.0f, 0.0f));
	meshutil::add_box(g5, toon, Vector3(0.03f, 5.2f, 0.03f), Vector3(0.0f, 3.7f, -3.20f), Color(0.50f, 0.42f, 0.30f), Vector3(-0.55f, 0.0f, 0.0f));
	// 6 舵轮、尾舵、船灯、船名
	Node3D *g6 = stage_nodes[6];
	meshutil::add_box(g6, toon, Vector3(0.18f, 0.9f, 0.18f), Vector3(0.0f, 2.40f, -2.85f), wood_dark);
	Ref<TorusMesh> wheel;
	wheel.instantiate();
	wheel->set_inner_radius(0.30f);
	wheel->set_outer_radius(0.40f);
	wheel->set_rings(14);
	wheel->set_ring_segments(4);
	wheel->set_material(meshutil::toon_material(toon, Color(0.78f, 0.62f, 0.30f)));
	MeshInstance3D *wmi = memnew(MeshInstance3D);
	wmi->set_mesh(wheel);
	wmi->set_position(Vector3(0.0f, 2.95f, -2.80f));
	wmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	g6->add_child(wmi);
	meshutil::add_box(g6, toon, Vector3(0.10f, 1.1f, 0.52f), Vector3(0.0f, 1.55f, -3.62f), wood_dark);
	meshutil::add_sphere(g6, toon, 0.16f, Vector3(0.0f, 2.12f, 3.05f), Color(0.94f, 0.78f, 0.42f));
	Label3D *name = memnew(Label3D);
	name->set_text(String::utf8("Peace"));
	name->set_font_size(54);
	name->set_pixel_size(0.0045f);
	name->set_modulate(Color(0.92f, 0.82f, 0.58f));
	name->set_position(Vector3(0.0f, 2.15f, -3.70f));
	name->set_rotation(Vector3(0.0f, 3.14159f, 0.0f));
	Ref<Font> cjk = uistyle::load_cjk();
	if (cjk.is_valid()) name->set_font(cjk);
	g6->add_child(name);
}
