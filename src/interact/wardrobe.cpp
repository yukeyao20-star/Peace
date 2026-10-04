#include "wardrobe.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/memory.hpp>

#include "mesh_util.h"
#include "../actors/player.h"

using namespace godot;

void Wardrobe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_costume"), &Wardrobe::get_costume);
}

void Wardrobe::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build(meshutil::load_toon_shader());
			add_to_group("interactables");
		} break;
		default:
			break;
	}
}

void Wardrobe::build(Ref<Shader> p_shader) {
	const Color wood(0.48f, 0.32f, 0.20f);       // 橡木主柜
	const Color wood_dark(0.32f, 0.20f, 0.12f);  // 深木底座与饰边
	const Color mirror(0.85f, 0.92f, 0.98f);     // 穿衣镜玻璃
	const Color brass(0.85f, 0.70f, 0.35f);      // 黄铜镜框与把手
	const Color sausage_red(0.86f, 0.22f, 0.22f); // 挂着的双汇外衣预览
	const Color corn_yellow(0.96f, 0.82f, 0.25f); // 玉米肠外衣预览

	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("Body");
	add_child(body);

	// 衣柜主体（宽 1.1m, 高 2.0m, 深 0.55m）
	meshutil::add_box(body, p_shader, Vector3(1.10f, 2.0f, 0.55f), Vector3(0.0f, 1.0f, 0.0f), wood);
	// 顶部与底部加厚边框
	meshutil::add_box(body, p_shader, Vector3(1.18f, 0.12f, 0.60f), Vector3(0.0f, 2.02f, 0.0f), wood_dark);
	meshutil::add_box(body, p_shader, Vector3(1.16f, 0.10f, 0.58f), Vector3(0.0f, 0.05f, 0.0f), wood_dark);

	// 正面穿衣镜（右扇门上的大镜子）
	meshutil::add_box(body, p_shader, Vector3(0.46f, 1.60f, 0.02f), Vector3(0.26f, 1.05f, 0.285f), mirror);
	meshutil::add_box(body, p_shader, Vector3(0.49f, 1.63f, 0.015f), Vector3(0.26f, 1.05f, 0.280f), brass);

	// 左扇门上的陈列展示窗（挂着火腿肠和玉米肠外衣微缩挂饰）
	meshutil::add_box(body, p_shader, Vector3(0.46f, 1.60f, 0.02f), Vector3(-0.26f, 1.05f, 0.28f), wood_dark);
	meshutil::add_box(body, p_shader, Vector3(0.12f, 0.45f, 0.06f), Vector3(-0.32f, 1.25f, 0.31f), sausage_red);
	meshutil::add_box(body, p_shader, Vector3(0.12f, 0.45f, 0.06f), Vector3(-0.18f, 1.25f, 0.31f), corn_yellow);

	// 黄铜把手
	meshutil::add_box(body, p_shader, Vector3(0.04f, 0.14f, 0.04f), Vector3(-0.06f, 1.05f, 0.30f), brass);
	meshutil::add_box(body, p_shader, Vector3(0.04f, 0.14f, 0.04f), Vector3(0.06f, 1.05f, 0.30f), brass);

	// 碰撞盒
	Ref<BoxShape3D> shape;
	shape.instantiate();
	shape->set_size(Vector3(1.10f, 2.0f, 0.55f));
	CollisionShape3D *col = memnew(CollisionShape3D);
	col->set_name("Collision");
	col->set_shape(shape);
	col->set_position(Vector3(0.0f, 1.0f, 0.0f));
	body->add_child(col);
}

static const char *COSTUME_NAMES[] = {
	"经典探险便服",
	"双汇王中王火腿肠",
	"双汇甜玉米火腿肠",
	"劲脆黑椒牛肉肠"
};

String Wardrobe::get_prompt() const {
	int idx = current_costume % 4;
	return String::utf8("按 E 更衣 [当前: ") + String::utf8(COSTUME_NAMES[idx]) + String::utf8("] (按 E 切换)");
}

Vector3 Wardrobe::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.0f, 0.28f);
}

void Wardrobe::interact(const Vector3 &p_player_pos) {
	current_costume = (current_costume + 1) % 4;

	Node *root = get_tree()->get_current_scene();
	if (root == nullptr) {
		root = get_tree()->get_root();
	}
	if (root == nullptr) {
		return;
	}
	Player *pl = Object::cast_to<Player>(root->find_child("Player", true, false));
	if (pl != nullptr) {
		pl->set_costume(current_costume);
		pl->play_ui(1); // 换装音效
	}
}
