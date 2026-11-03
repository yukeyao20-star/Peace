#include "ship_quest.h"
#include <cmath>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
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
// 零件散布：从木屋院子一路向西——瀑布、西岸河滩、长河、直到入海口。
// 越靠后阶段的零件越远，主线会自然把玩家带向"离岸"的方向。
const float PART_X[ShipQuest::PART_COUNT] = {
	12.9f, 13.6f, 16.9f, 7.0f, 10.8f, 15.2f, 8.0f,
	6.2f, 7.0f, 5.6f, 5.2f, -4.2f, -11.6f, -12.8f,
	-1.2f, 76.0f, 84.0f, 68.0f, 90.0f, 80.0f,
	-52.0f, -58.0f, -88.0f, -98.0f, -110.0f,
	-132.0f, -150.0f, -101.0f,
	-140.0f, -146.0f, -152.0f, -64.0f
};
const float PART_Z[ShipQuest::PART_COUNT] = {
	3.2f, 4.1f, 6.8f, 6.4f, 3.0f, 3.6f, 4.0f,
	15.2f, 16.2f, 14.0f, 13.0f, -12.2f, -16.2f, -12.8f,
	-16.4f, 84.0f, 74.0f, 88.0f, 70.0f, 64.0f,
	64.0f, 52.0f, 62.0f, 52.0f, 46.0f,
	23.0f, 12.0f, 14.0f,
	-26.0f, -32.0f, -40.0f, 76.0f
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
// 每阶段零件的方位线索（散布范围变远后，靠这行字把玩家往西引）
const char *STAGE_HINTS[6] = {
	"木屋院子", "湖边与木屋南面", "瀑布一带", "西岸河滩", "长河两岸", "入海口的滩上"
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
			const float delta = (float)get_process_delta_time();
			t += delta;
			// 主线引导：开局几秒提示一次旧船坞的方位（只在未开工时出现）
			if (!started && !intro_hint_shown && t > 6.0f) {
				intro_hint_shown = true;
				flash(String::utf8("听说西边河口的旧船坞里，停着一条没造完的船"));
			}
			if (!departing) break;
			dep_t += delta;
			// 路径跟随：船头朝前找航线上的“前视点”，先慢转、再沿船头方向前进。
			// 关键：位置由船头方向积分，而不是直接贴曲线采样——大船转弯时不会侧滑。
			const float water_y = (terrain != nullptr ? terrain->get_water_level() : -0.55f);
			const float total_len = voyage_curve.is_valid() ? voyage_curve->get_baked_length() : 1.0f;
			const float speed = 6.0f;
			dep_s = Math::min(dep_s + delta * speed, total_len);
			Vector3 pos = ship_root->get_global_position();
			const float look = 12.0f;
			Vector3 target = voyage_curve.is_valid()
					? voyage_curve->sample_baked(Math::min(total_len, dep_s + look))
					: VOYAGE[0];
			Vector3 to = target - pos;
			to.y = 0.0f;
			float target_yaw = std::atan2(to.x, to.z);
			if (!yaw_init) {
				ship_yaw = target_yaw;
				yaw_init = true;
			} else {
				float dy = target_yaw - ship_yaw;
				while (dy > 3.1415926f) dy -= 6.2831853f;
				while (dy < -3.1415926f) dy += 6.2831853f;
				const float max_step = 0.22f * delta; // 大船约 13°/秒，稳而慢
				ship_yaw += Math::clamp(dy, -max_step, max_step);
			}
			const Vector3 forward(std::sin(ship_yaw), 0.0f, std::cos(ship_yaw));
			pos += forward * speed * delta;
			pos.y = Math::lerp(pos.y, water_y - 0.10f - 0.69f, Math::min(1.0f, delta * 1.2f));
			ship_root->set_global_position(pos);
			ship_root->set_global_rotation(Vector3(0.0f, ship_yaw, std::sin(dep_t * 0.6f) * 0.008f));
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
				const float a = Math::clamp((dep_t - 25.0f) / 4.0f, 0.0f, 1.0f);
				fade->set_color(Color(0.98f, 0.97f, 0.93f, a));
				if (end_title != nullptr) end_title->set_visible(dep_t > 27.5f);
				if (end_sub != nullptr) end_sub->set_visible(dep_t > 28.3f);
			}
			if (dep_t > 34.0f) set_process(false);
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
		// 曲线航线 + 缓慢转向：大船不可能在折点瞬间甩头
		voyage_curve.instantiate();
		for (int i = 0; i < 13; i++) voyage_curve->add_point(VOYAGE[i]);
		voyage_curve->set_bake_interval(0.8f);
		ship_yaw = 0.0f;
		yaw_init = false;
		dep_s = 0.0f;
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

String ShipQuest::get_save_serial() const {
	String mask;
	for (int i = 0; i < PART_COUNT; i++) {
		mask += collected[i] ? String("1") : String("0");
	}
	return String(started ? "1" : "0") + String("|") + mask + String("|")
			+ String::num_int64(installed);
}

void ShipQuest::apply_save_serial(const String &p_serial) {
	PackedStringArray parts_s = p_serial.split("|");
	if (parts_s.size() != 3) {
		return; // 旧档/坏档：保持初始状态（主线未开始）
	}
	const String mask = parts_s[1];
	if (mask.length() != PART_COUNT) {
		return;
	}
	int count = 0;
	for (int i = 0; i < PART_COUNT; i++) {
		collected[i] = mask[i] == '1';
		if (collected[i]) count++;
	}
	// 已安装数只可能 ≤ 已拾取数；越界的坏档夹回安全范围
	installed = Math::clamp((int)parts_s[2].to_int(), 0, count);
	carried = count - installed;
	complete = installed >= PART_COUNT;
	started = (parts_s[0] == "1") || count > 0 || installed > 0;
	departing = false;
	for (int i = 0; i < PART_COUNT; i++) {
		if (parts[i] != nullptr) {
			parts[i]->set_visible(started && !collected[i] && !complete);
		}
	}
	update_stage(false); // 读档不弹"船造好了"的提示
	update_ui();
}

void ShipQuest::update_stage(bool p_announce) {
	int s = 0;
	for (int i = 0; i < 6; i++) {
		if (installed >= STAGE_THRESHOLD[i]) s = i + 1;
	}
	stage = s;
	for (int i = 1; i <= 6; i++) {
		// 第 1 阶段的龙骨永远露出：旧船坞里本来就该躺着一条没造完的船
		if (stage_nodes[i] != nullptr) stage_nodes[i]->set_visible(i == 1 || i <= stage);
	}
	if (installed >= PART_COUNT) {
		complete = true;
		if (scaffold != nullptr) scaffold->set_visible(false);
		if (p_announce) {
			flash(String::utf8("大船造好了 ·「Peace」\n按 E 登船，向湖心出发"));
		}
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
			s += String::utf8("\n下一段：") + String::utf8(STAGE_NAMES[stage])
					+ String::utf8(" · 去") + String::utf8(STAGE_HINTS[stage]) + String::utf8("找");
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
	panel->set_offset(SIDE_LEFT, -230.0f);
	panel->set_offset(SIDE_RIGHT, 230.0f);
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
	// 船坞旗杆：一根高杆 + 奶白招幌，从湖对岸也能认出这里
	const float pole_h = 7.4f;
	meshutil::add_box(scaffold, toon, Vector3(0.10f, pole_h, 0.10f), Vector3(2.05f, pole_h * 0.5f, 3.55f), post);
	meshutil::add_box(scaffold, toon, Vector3(2.20f, 1.15f, 0.03f), Vector3(3.15f, pole_h - 0.82f, 3.55f),
			Color(0.95f, 0.87f, 0.68f), Vector3(0.0f, 0.0f, -0.06f));
	meshutil::add_box(scaffold, toon, Vector3(0.09f, 0.09f, 0.09f), Vector3(2.05f, pole_h + 0.05f, 3.55f),
			Color(0.78f, 0.64f, 0.32f));
	build_ship();
	// 零件
	for (int i = 0; i < PART_COUNT; i++) make_part(i);
	update_stage(false); // 初始：龙骨可见，其余阶段收着
	build_ui();
	build_cinematic();
	update_ui();
	set_process(true);
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
	// 5 帆与绳索：更大的弧面主帆 + 前三角帆，帆面鼓起，整面写着 Peace
	Node3D *g5 = stage_nodes[5];
	const float sail_w = 5.0f;
	const float sail_h = 3.75f;
	const float sail_top = 6.45f;
	const float sail_base_z = 0.92f;
	const float sail_bulge = 1.05f;
	Ref<SurfaceTool> sail_st;
	sail_st.instantiate();
	sail_st->begin(Mesh::PRIMITIVE_TRIANGLES);
	const int NU = 12;
	const int NV = 9;
	auto sail_p = [&](int i, int j) {
		const float u = (float)i / (float)NU;
		const float v = (float)j / (float)NV;
		const float ww = sail_w * (1.0f - 0.16f * v); // 下缘略收，像被风撑开的帆
		const float x = (u - 0.5f) * ww;
		const float y = sail_top - v * sail_h + 0.22f * std::sin(3.1415926f * u) * v * v;
		const float z = sail_base_z + std::sin(3.1415926f * u) * std::sin(3.1415926f * v) * sail_bulge;
		return Vector3(x, y, z);
	};
	for (int j = 0; j < NV; j++) {
		for (int i = 0; i < NU; i++) {
			const Vector3 a = sail_p(i, j);
			const Vector3 b = sail_p(i + 1, j);
			const Vector3 c = sail_p(i + 1, j + 1);
			const Vector3 d = sail_p(i, j + 1);
			sail_st->set_uv(Vector2((float)i / NU, (float)j / NV));
			sail_st->add_vertex(a);
			sail_st->set_uv(Vector2((float)(i + 1) / NU, (float)j / NV));
			sail_st->add_vertex(c);
			sail_st->set_uv(Vector2((float)(i + 1) / NU, (float)(j + 1) / NV));
			sail_st->add_vertex(b);
			sail_st->set_uv(Vector2((float)i / NU, (float)j / NV));
			sail_st->add_vertex(a);
			sail_st->set_uv(Vector2((float)i / NU, (float)(j + 1) / NV));
			sail_st->add_vertex(d);
			sail_st->set_uv(Vector2((float)(i + 1) / NU, (float)(j + 1) / NV));
			sail_st->add_vertex(c);
		}
	}
	sail_st->generate_normals();
	Ref<ArrayMesh> sail_mesh = sail_st->commit();
	// 帆布材质：Peace 字样先渲染进 SubViewport，再贴到会飘动的帆面 UV 上
	SubViewport *mark_vp = memnew(SubViewport);
	mark_vp->set_name("SailMarkViewport");
	mark_vp->set_size(Vector2i(1024, 768));
	mark_vp->set_transparent_background(false);
	mark_vp->set_disable_3d(true);
	add_child(mark_vp);
	Control *mark_root = memnew(Control);
	mark_root->set_size(Vector2(1024, 768));
	mark_vp->add_child(mark_root);
	ColorRect *mark_bg = memnew(ColorRect);
	mark_bg->set_size(Vector2(1024, 768));
	mark_bg->set_color(sail_c);
	mark_root->add_child(mark_bg);
	Label *mark_label = memnew(Label);
	mark_label->set_text(String::utf8("Peace"));
	mark_label->set_size(Vector2(1024, 768));
	mark_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	mark_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	mark_label->add_theme_font_size_override("font_size", 318);
	Ref<Font> sail_font = uistyle::heavy_cjk(900, 0.85f);
	if (sail_font.is_null()) sail_font = uistyle::load_cjk();
	if (sail_font.is_valid()) mark_label->add_theme_font_override("font", sail_font);
	// 同色描边当"加粗"用：NotoSansSC 可变字体的 wght/embolden 上限不够壮
	const Color ink(0.19f, 0.12f, 0.055f);
	mark_label->add_theme_color_override("font_color", ink);
	mark_label->add_theme_color_override("font_outline_color", ink);
	mark_label->add_theme_constant_override("outline_size", 18);
	mark_label->set_pivot_offset(Vector2(512, 384));
	mark_label->set_rotation(-0.20f);
	mark_root->add_child(mark_label);
	mark_vp->set_update_mode(SubViewport::UPDATE_ALWAYS);
	Ref<Resource> sail_shader_res = ResourceLoader::get_singleton()->load("res://assets/sail.gdshader");
	Ref<Shader> sail_shader = Ref<Shader>(Object::cast_to<Shader>(sail_shader_res.ptr()));
	Ref<Material> sail_mat;
	if (sail_shader.is_valid()) {
		Ref<ShaderMaterial> smat;
		smat.instantiate();
		smat->set_shader(sail_shader);
		sail_tex = mark_vp->get_texture();
		smat->set_shader_parameter("sail_tex", sail_tex.ptr());
		smat->set_shader_parameter("base_color", sail_c);
		smat->set_shader_parameter("wind", 1.0f);
		smat->set_shader_parameter("sail_w", sail_w);
		smat->set_shader_parameter("sail_h", sail_h);
		smat->set_shader_parameter("sail_top", sail_top);
		sail_mat = smat;
		sail_vp = mark_vp;
	} else {
		Ref<StandardMaterial3D> fallback;
		fallback.instantiate();
		fallback->set_albedo(sail_c);
		fallback->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		fallback->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		sail_mat = fallback;
	}
	sail_mesh->surface_set_material(0, sail_mat);
	MeshInstance3D *sail_mi = memnew(MeshInstance3D);
	sail_mi->set_name("MainSail");
	sail_mi->set_mesh(sail_mesh);
	g5->add_child(sail_mi);
	// 帆的横桁（不再加竖缝，避免线条穿过 Peace 字样）
	meshutil::add_box(g5, toon, Vector3(sail_w + 0.5f, 0.10f, 0.10f), Vector3(0.0f, sail_top + 0.02f, sail_base_z), trim);
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
