#include "parkour_poles.h"

#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/memory.hpp>

#include "mesh_util.h"

using namespace godot;

void ParkourPoles::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build_poles(meshutil::load_toon_shader());
		} break;
		default:
			break;
	}
}

void ParkourPoles::build_poles(Ref<Shader> p_shader) {
	const Color wood_bark(0.42f, 0.28f, 0.17f);
	const Color wood_top(0.65f, 0.48f, 0.30f);
	const Color moss_green(0.36f, 0.52f, 0.30f);
	const Color gold(0.92f, 0.78f, 0.32f);

	// 16 根梅花桩的点位序列（从南侧湖岸踏入湖水，蜿蜒穿向湖心大礁石）：
	// 水面高度为 -0.55m，桩顶标高从 -0.15m 到 0.85m 错落起伏
	struct PoleData {
		Vector3 pos; // x, top_y, z
		float radius;
	};

	const PoleData poles[] = {
		{ Vector3(-3.5f, 0.15f, 16.5f), 0.52f }, // 第1桩（近岸起点）
		{ Vector3(-5.0f, 0.28f, 18.2f), 0.46f },
		{ Vector3(-6.8f, 0.42f, 19.8f), 0.44f },
		{ Vector3(-8.6f, 0.30f, 21.6f), 0.42f },
		{ Vector3(-10.2f, 0.55f, 23.4f), 0.45f },
		{ Vector3(-8.8f, 0.72f, 25.2f), 0.40f }, // 弧度向右拐
		{ Vector3(-7.5f, 0.85f, 27.0f), 0.42f }, // 高峰中继桩
		{ Vector3(-8.5f, 0.65f, 28.8f), 0.40f },
		{ Vector3(-10.4f, 0.45f, 30.2f), 0.44f },
		{ Vector3(-12.2f, 0.32f, 31.8f), 0.42f },
		{ Vector3(-14.0f, 0.50f, 33.2f), 0.40f },
		{ Vector3(-15.8f, 0.68f, 34.6f), 0.42f },
		{ Vector3(-17.5f, 0.82f, 33.2f), 0.44f }, // 绕湖心礁石
		{ Vector3(-18.6f, 0.65f, 31.2f), 0.42f },
		{ Vector3(-17.2f, 0.48f, 29.5f), 0.46f },
		{ Vector3(-15.5f, 0.35f, 28.0f), 0.60f }  // 终点大莲花圆桩
	};

	const int num_poles = sizeof(poles) / sizeof(poles[0]);

	for (int i = 0; i < num_poles; i++) {
		const PoleData &pd = poles[i];
		StaticBody3D *body = memnew(StaticBody3D);
		body->set_name(vformat("Pole_%d", i + 1));
		add_child(body);

		float top_y = pd.pos.y;
		float bottom_y = -3.2f; // 深埋入水底
		float height = top_y - bottom_y;
		float center_y = (top_y + bottom_y) * 0.5f;

		// 碰撞体
		Ref<CylinderShape3D> shape;
		shape.instantiate();
		shape->set_radius(pd.radius);
		shape->set_height(height);

		CollisionShape3D *col = memnew(CollisionShape3D);
		col->set_name("Col");
		col->set_shape(shape);
		col->set_position(Vector3(pd.pos.x, center_y, pd.pos.z));
		body->add_child(col);

		// 原木柱身
		Ref<CylinderMesh> cyl;
		cyl.instantiate();
		cyl->set_top_radius(pd.radius);
		cyl->set_bottom_radius(pd.radius * 1.06f);
		cyl->set_height(height);
		cyl->set_radial_segments(14);
		cyl->set_rings(1);
		cyl->set_material(meshutil::toon_material(p_shader, wood_bark));

		MeshInstance3D *cyl_mi = memnew(MeshInstance3D);
		cyl_mi->set_mesh(cyl);
		cyl_mi->set_position(Vector3(pd.pos.x, center_y, pd.pos.z));
		body->add_child(cyl_mi);

		// 桩顶苔藓青葱踏板（薄圆盘覆盖顶部）
		Ref<CylinderMesh> top_cyl;
		top_cyl.instantiate();
		top_cyl->set_top_radius(pd.radius * 0.96f);
		top_cyl->set_bottom_radius(pd.radius * 0.98f);
		top_cyl->set_height(0.04f);
		top_cyl->set_radial_segments(14);
		top_cyl->set_material(meshutil::toon_material(p_shader, (i % 2 == 0) ? moss_green : wood_top));

		MeshInstance3D *top_mi = memnew(MeshInstance3D);
		top_mi->set_mesh(top_cyl);
		top_mi->set_position(Vector3(pd.pos.x, top_y + 0.01f, pd.pos.z));
		body->add_child(top_mi);

		// 终点第16桩：挂立金色小风铃作为达成标记
		if (i == num_poles - 1) {
			meshutil::add_box(body, p_shader, Vector3(0.08f, 1.2f, 0.08f),
					Vector3(pd.pos.x + 0.45f, top_y + 0.6f, pd.pos.z), wood_bark);
			meshutil::add_box(body, p_shader, Vector3(0.18f, 0.22f, 0.18f),
					Vector3(pd.pos.x + 0.45f, top_y + 1.15f, pd.pos.z), gold);
		}
	}

	// 起点木牌（立在第1桩岸边）
	meshutil::add_box(this, p_shader, Vector3(0.12f, 1.4f, 0.12f), Vector3(-3.2f, 0.7f, 15.0f), wood_bark);
	meshutil::add_box(this, p_shader, Vector3(0.95f, 0.45f, 0.08f), Vector3(-3.2f, 1.25f, 15.0f), wood_top);

	Label3D *sign_label = memnew(Label3D);
	sign_label->set_text(String::utf8("🌸 湖心梅花桩\n轻功水上漂"));
	sign_label->set_position(Vector3(-3.2f, 1.25f, 15.05f));
	sign_label->set_font_size(36);
	sign_label->set_outline_size(8);
	sign_label->set_modulate(Color(1.0f, 0.95f, 0.8f));
	Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	if (cjk.is_valid()) {
		sign_label->set_font(cjk);
	}
	add_child(sign_label);
}
