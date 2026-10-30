#include "farm_plot.h"

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "well.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

namespace {

const Color SOIL_DRY(0.28f, 0.20f, 0.13f);
const Color SOIL_WET(0.17f, 0.12f, 0.085f); // 浇灌后：更深更冷的湿土色
const Color SNOW_WHITE(0.92f, 0.94f, 0.97f);

} // namespace

float FarmPlot::grow_seconds(int p_crop) {
	switch (p_crop) {
		case CROP_RADISH:
			return 60.0f;  // 萝卜快熟
		case CROP_PUMPKIN:
			return 150.0f; // 南瓜慢熟高产
		case CROP_BERRY:
		default:
			return 90.0f;
	}
}

int FarmPlot::crop_yield(int p_crop) {
	switch (p_crop) {
		case CROP_RADISH:
			return 3;
		case CROP_PUMPKIN:
			return 5;
		case CROP_BERRY:
		default:
			return 2;
	}
}

const char *FarmPlot::crop_name(int p_crop) {
	switch (p_crop) {
		case CROP_RADISH:
			return "萝卜";
		case CROP_PUMPKIN:
			return "南瓜";
		case CROP_BERRY:
		default:
			return "浆果";
	}
}

void FarmPlot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_state"), &FarmPlot::get_state);
	ClassDB::bind_method(D_METHOD("get_crop"), &FarmPlot::get_crop);
	ClassDB::bind_method(D_METHOD("cycle_crop"), &FarmPlot::cycle_crop);
	ClassDB::bind_method(D_METHOD("set_state", "p_state", "p_progress"), &FarmPlot::set_state);
	ClassDB::bind_method(D_METHOD("set_watered", "p_watered"), &FarmPlot::set_watered);
	ClassDB::bind_method(D_METHOD("get_plot_serial"), &FarmPlot::get_plot_serial);
	ClassDB::bind_method(D_METHOD("apply_plot_serial", "p_serial"), &FarmPlot::apply_plot_serial);
	ClassDB::bind_method(D_METHOD("_on_water_drawn"), &FarmPlot::_on_water_drawn);
	ClassDB::bind_method(D_METHOD("is_snow_covered"), &FarmPlot::is_snow_covered);

	ADD_SIGNAL(MethodInfo("berry_picked")); // 收获时每颗果发一次（GameDirector 每日待办计分）
	ADD_SIGNAL(MethodInfo("crop_harvested")); // 收获一茬发一次（"照看菜畦"待办计分用）
}

void FarmPlot::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成，保持场景干净
			}
			build();
			add_to_group("interactables");
			add_to_group("farm_plots"); // GameDirector 收获计分/存档聚合用
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			// —— 打水后的浇灌窗口倒计时 ——
			if (water_carry_t > 0.0f) {
				water_carry_t = Math::max(water_carry_t - delta, 0.0f);
			}
			// —— 懒连井的 water_drawn（井 READY 后才找得到，逐帧补连）——
			if (well == nullptr) {
				try_connect_well();
			}
			if (state == STATE_GROWING) {
				grow_t = Math::min(grow_t + delta / grow_seconds(crop), 1.0f);
				if (grow_t >= 1.0f) {
					state = STATE_RIPE;
				}
				refresh_visual();
			}
			if (state == STATE_EMPTY) {
				handle_seed_cycle(); // 空畦 + 玩家在旁：R 换种
			}
			// —— 雪天盖雪（雪强懒查 SkyCycle；停雪自动消）——
			if (snow_cap != nullptr) {
				if (sky == nullptr && get_parent() != nullptr) {
					sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
				}
				snow_cap->set_visible(sky != nullptr && sky->get_snow_amount() > SNOW_SHOW);
			}
		} break;
		default:
			break;
	}
}

void FarmPlot::build() {
	Ref<Shader> shader = meshutil::load_toon_shader();

	// —— 菜畦：木框 + 深褐土垄（贴地矮台） ——
	const Color wood(0.45f, 0.33f, 0.20f);
	const Vector3 frame(1.30f, 0.10f, 0.90f);
	meshutil::add_box(this, shader, frame, Vector3(0.0f, 0.05f, 0.0f), wood);

	// 土垄要单独拿材质（浇灌时把 albedo 改成湿土色；meshutil 每块独立材质，可安全改）
	soil = meshutil::add_box(this, shader, Vector3(frame.x - 0.16f, 0.14f, frame.z - 0.16f),
			Vector3(0.0f, 0.07f, 0.0f), SOIL_DRY);
	Ref<PrimitiveMesh> soil_mesh = Object::cast_to<PrimitiveMesh>(soil->get_mesh().ptr());
	if (soil_mesh.is_valid()) {
		soil_mat = soil_mesh->get_material();
	}

	// —— 苗组：三株嫩苗（小绿球），随 grow_t 从 0.25 长到 1 ——
	sprouts = memnew(Node3D);
	sprouts->set_name("Sprouts");
	add_child(sprouts);
	const Color leaf(0.35f, 0.62f, 0.25f);
	const Vector3 spots[3] = {
		Vector3(-0.30f, 0.14f, 0.0f),
		Vector3(0.02f, 0.14f, -0.18f),
		Vector3(0.30f, 0.14f, 0.16f),
	};
	for (int i = 0; i < 3; i++) {
		MeshInstance3D *s = meshutil::add_sphere(sprouts, shader, 0.13f, spots[i], leaf);
		s->set_scale(Vector3(1.0f, 0.85f, 1.0f));
	}

	// —— 各作物的果组（成熟挂果 / 空畦时当缩小的选种预览）——
	for (int c = 0; c < CROP_COUNT; c++) {
		Node3D *g = memnew(Node3D);
		g->set_name(String("Fruits") + String::num_int64(c));
		add_child(g);
		fruit_groups[c] = g;
	}
	// 浆果：三颗小红果挂在苗顶
	const Color berry_col(0.85f, 0.20f, 0.14f);
	for (int i = 0; i < 3; i++) {
		meshutil::add_sphere(fruit_groups[CROP_BERRY], shader, 0.07f,
				spots[i] + Vector3(0.0f, 0.24f, 0.0f), berry_col);
	}
	// 萝卜：三颗白胖根顶出土，带一点绿缨
	const Color radish_body(0.93f, 0.90f, 0.84f);
	const Color radish_leaf(0.30f, 0.60f, 0.28f);
	for (int i = 0; i < 3; i++) {
		Vector3 at = spots[i] + Vector3(0.0f, 0.10f, 0.0f);
		meshutil::add_sphere(fruit_groups[CROP_RADISH], shader, 0.11f, at, radish_body);
		meshutil::add_sphere(fruit_groups[CROP_RADISH], shader, 0.05f,
				at + Vector3(0.0f, 0.14f, 0.0f), radish_leaf);
	}
	// 南瓜：居中一颗大橙瓜 + 小绿蒂
	const Color pumpkin_col(0.86f, 0.46f, 0.14f);
	MeshInstance3D *pk = meshutil::add_sphere(fruit_groups[CROP_PUMPKIN], shader, 0.26f,
			Vector3(0.0f, 0.30f, 0.0f), pumpkin_col);
	pk->set_scale(Vector3(1.0f, 0.78f, 1.0f)); // 压扁的矮瓜
	meshutil::add_sphere(fruit_groups[CROP_PUMPKIN], shader, 0.045f,
			Vector3(0.0f, 0.52f, 0.0f), radish_leaf);

	// —— 雪盖：盖住整条土垄的白色薄板（雪天才显示，嫩苗从雪里探出来）——
	snow_mat = meshutil::toon_material(shader, SNOW_WHITE, 0.55f);
	Ref<BoxMesh> snow_mesh;
	snow_mesh.instantiate();
	snow_mesh->set_size(Vector3(frame.x - 0.16f, 0.05f, frame.z - 0.16f));
	snow_mesh->set_material(snow_mat);
	snow_cap = memnew(MeshInstance3D);
	snow_cap->set_name("SnowCap");
	snow_cap->set_mesh(snow_mesh);
	snow_cap->set_position(Vector3(0.0f, 0.165f, 0.0f)); // 底面贴土垄顶，薄板盖雪
	snow_cap->set_visible(false);
	add_child(snow_cap);

	refresh_visual();
}

void FarmPlot::refresh_visual() {
	if (sprouts != nullptr) {
		// 空畦苗隐形；生长中从 0.25 → 1.0 长大；成熟保持最大
		float k = (state == STATE_EMPTY) ? 0.0f : Math::lerp(0.25f, 1.0f, grow_t);
		sprouts->set_visible(state != STATE_EMPTY);
		sprouts->set_scale(Vector3(k, k, k));
	}
	for (int c = 0; c < CROP_COUNT; c++) {
		Node3D *g = fruit_groups[c];
		if (g == nullptr) {
			continue;
		}
		if (state == STATE_RIPE && c == crop) {
			g->set_visible(true);
			g->set_scale(Vector3(1.0f, 1.0f, 1.0f));
		} else if (state == STATE_EMPTY && c == crop) {
			// 空畦：当前预选作物以 0.45 倍缩小预览（选种提示）
			g->set_visible(true);
			g->set_scale(Vector3(0.45f, 0.45f, 0.45f));
		} else {
			g->set_visible(false);
		}
	}
}

void FarmPlot::set_soil_wet(bool p_wet) {
	if (soil_mat.is_valid()) {
		soil_mat->set_shader_parameter("albedo", p_wet ? Variant(SOIL_WET) : Variant(SOIL_DRY));
	}
}

void FarmPlot::handle_seed_cycle() {
	// 空畦 + 玩家在旁（2.6m，比交互距离略宽）时按 R 换种。
	// R 在玩家侧是"受身翻滚"键——站在菜畦旁翻滚一下就把种子换了，顺手不冲突。
	if (player_cache == nullptr && get_tree() != nullptr) {
		Node *root = get_tree()->get_current_scene();
		if (root == nullptr && get_tree()->get_root() != nullptr) {
			root = get_tree()->get_root(); // Window 继承自 Node
		}
		if (root != nullptr) {
			player_cache = Object::cast_to<Node3D>(root->find_child("Player", true, false));
		}
	}
	Input *input = Input::get_singleton();
	bool r_down = input != nullptr && input->is_key_pressed(KEY_R);
	bool edge = r_down && !r_prev;
	r_prev = r_down;
	if (!edge || player_cache == nullptr) {
		return;
	}
	if (player_cache->get_global_position().distance_to(get_global_position()) > 2.6f) {
		return; // 离得远：R 还是纯粹的翻滚
	}
	cycle_crop();
}

void FarmPlot::cycle_crop() {
	crop = (crop + 1) % CROP_COUNT;
	refresh_visual(); // 换预览
}

void FarmPlot::try_connect_well() {
	if (well != nullptr || get_tree() == nullptr) {
		return;
	}
	Array nodes = get_tree()->get_nodes_in_group(StringName("interactables"));
	for (int i = 0; i < nodes.size(); i++) {
		Well *w = Object::cast_to<Well>(nodes[i]);
		if (w != nullptr) {
			well = w;
			if (!w->is_connected("water_drawn", Callable(this, "_on_water_drawn"))) {
				w->connect("water_drawn", Callable(this, "_on_water_drawn"));
			}
			break;
		}
	}
}

void FarmPlot::_on_water_drawn() {
	water_carry_t = WATER_CARRY_SECONDS; // 手里有一桶水：20 秒内可浇灌
}

String FarmPlot::get_prompt() const {
	switch (state) {
		case STATE_EMPTY:
			return String::utf8("按 E 种") + String::utf8(crop_name(crop))
					+ String::utf8(" · 按 R 换种");
		case STATE_GROWING: {
			if (!watered && water_carry_t > 0.0f) {
				return String::utf8("按 E 浇灌（生长 +30%）");
			}
			int remain = Math::max((int)((1.0f - grow_t) * grow_seconds(crop)), 1);
			String text = String::utf8(crop_name(crop)) + String::utf8("生长中… 约 ")
					+ String::num_int64(remain) + String::utf8(" 秒");
			if (watered) {
				text += String::utf8("（已浇灌）");
			}
			return text;
		}
		case STATE_RIPE:
		default:
			return String::utf8("按 E 收获") + String::utf8(crop_name(crop))
					+ String("（+") + String::num_int64(crop_yield(crop)) + String("）");
	}
}

Vector3 FarmPlot::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.2f, 0.0f);
}

void FarmPlot::interact(const Vector3 &p_player_pos) {
	if (state == STATE_EMPTY) {
		state = STATE_GROWING;
		grow_t = 0.0f;
		watered = false;
		set_soil_wet(false); // 新翻的干土
	} else if (state == STATE_RIPE) {
		// 收获：按作物产量入账——发 berry_picked，每日待办/音效全走既有链路
		int yld = crop_yield(crop);
		for (int i = 0; i < yld; i++) {
			emit_signal("berry_picked");
		}
		emit_signal("crop_harvested"); // "照看菜畦"按茬计（与按颗计的浆果待办互不干扰）
		state = STATE_EMPTY;
		grow_t = 0.0f;
		watered = false;
		set_soil_wet(false);
	} else if (!watered && water_carry_t > 0.0f) {
		// 浇灌：本茬一次，生长 +30%；加满即当场成熟
		grow_t = Math::min(grow_t + WATER_BOOST, 1.0f);
		watered = true;
		set_soil_wet(true);
		if (grow_t >= 1.0f) {
			state = STATE_RIPE;
		}
	} else {
		return; // 生长中且没水/已浇过：不响应
	}
	refresh_visual();
}

void FarmPlot::set_state(int p_state, float p_progress) {
	if (p_state < STATE_EMPTY || p_state > STATE_RIPE) {
		return;
	}
	state = p_state;
	grow_t = Math::clamp(p_progress, 0.0f, 1.0f);
	refresh_visual();
}

void FarmPlot::set_watered(bool p_watered) {
	watered = p_watered;
	set_soil_wet(p_watered && state != STATE_EMPTY);
}

String FarmPlot::get_plot_serial() const {
	int pct = (int)(grow_t * 100.0f + 0.5f);
	return String::num_int64(state) + String(",") + String::num_int64(pct)
			+ String(",") + (watered ? String("1") : String("0"))
			+ String(",") + String::num_int64(crop);
}

void FarmPlot::apply_plot_serial(const String &p_serial) {
	PackedStringArray parts = p_serial.split(",", true);
	if (parts.size() < 3) {
		return; // 残缺条目整条丢弃，保持现状
	}
	int s = (int)parts[0].to_int();
	if (s < STATE_EMPTY || s > STATE_RIPE) {
		return;
	}
	state = s;
	grow_t = Math::clamp((float)parts[1].to_int() / 100.0f, 0.0f, 1.0f);
	watered = ((int)parts[2].to_int() != 0);
	if (parts.size() >= 4) {
		int c = (int)parts[3].to_int();
		if (c >= 0 && c < CROP_COUNT) {
			crop = c;
		}
	} // 旧档（v2 三段式）缺作物段 → 保持默认浆果
	if (state == STATE_RIPE) {
		grow_t = 1.0f; // 保险：成熟态进度必须满
	}
	set_soil_wet(watered && state != STATE_EMPTY);
	refresh_visual();
}
