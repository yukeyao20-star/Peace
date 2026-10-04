#include "soccer_field.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/memory.hpp>

#include "soccer_ball.h"
#include "mesh_util.h"

using namespace godot;

void SoccerField::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			Ref<Shader> toon = meshutil::load_toon_shader();
			build_pitch_markings(toon);
			build_corner_flags(toon);

			// 西球门（在 x = 36.0, 朝向 +X 即面对球场内）
			build_goal(this, Vector3(pitch_center.x - pitch_half_w, pitch_center.y, pitch_center.z), 1.0f, toon);
			// 东球门（在 x = 84.0, 朝向 -X 即面对球场内）
			build_goal(this, Vector3(pitch_center.x + pitch_half_w, pitch_center.y, pitch_center.z), -1.0f, toon);

			spawn_ball();
		} break;
		default:
			break;
	}
}

// 建造一座逼真白木质标准球门（宽 5.2m, 高 2.2m, 深 1.8m）
void SoccerField::build_goal(Node3D *p_parent, const Vector3 &p_pos, float p_facing_x, Ref<Shader> p_shader) {
	StaticBody3D *goal = memnew(StaticBody3D);
	goal->set_name(p_facing_x > 0 ? "GoalWest" : "GoalEast");
	goal->set_position(p_pos);
	p_parent->add_child(goal);

	const Color white_bar(0.96f, 0.96f, 0.95f);
	const Color net_col(0.85f, 0.88f, 0.92f);
	const Color net_mesh(0.75f, 0.80f, 0.85f);

	const float gw = 5.2f;  // 球门净宽
	const float gh = 2.2f;  // 球门高度
	const float gd = 1.8f;  // 球门深度
	const float post_r = 0.07f;
	const float post_w = post_r * 2.0f;

	float back_dir = -p_facing_x; // 球网向球场外后延

	// 1. 球门正框（两根立柱 + 一根横梁）
	// 左立柱
	meshutil::add_box(goal, p_shader, Vector3(post_w, gh, post_w),
			Vector3(0.0f, gh * 0.5f, -gw * 0.5f), white_bar);
	// 右立柱
	meshutil::add_box(goal, p_shader, Vector3(post_w, gh, post_w),
			Vector3(0.0f, gh * 0.5f, gw * 0.5f), white_bar);
	// 顶部横梁
	meshutil::add_box(goal, p_shader, Vector3(post_w, post_w, gw + post_w),
			Vector3(0.0f, gh, 0.0f), white_bar);

	// 2. 底部支承撑杆（地面两侧与后方）
	meshutil::add_box(goal, p_shader, Vector3(gd, post_w, post_w),
			Vector3(back_dir * gd * 0.5f, 0.04f, -gw * 0.5f), white_bar);
	meshutil::add_box(goal, p_shader, Vector3(gd, post_w, post_w),
			Vector3(back_dir * gd * 0.5f, 0.04f, gw * 0.5f), white_bar);
	meshutil::add_box(goal, p_shader, Vector3(post_w, post_w, gw),
			Vector3(back_dir * gd, 0.04f, 0.0f), white_bar);

	// 3. 斜后撑支架（从横梁后倾到底框）
	meshutil::add_box(goal, p_shader, Vector3(0.06f, 0.06f, 0.06f),
			Vector3(back_dir * gd * 0.5f, gh * 0.5f, -gw * 0.5f), white_bar);
	meshutil::add_box(goal, p_shader, Vector3(0.06f, 0.06f, 0.06f),
			Vector3(back_dir * gd * 0.5f, gh * 0.5f, gw * 0.5f), white_bar);

	// 4. 球网（后壁、顶壁、左右侧壁）
	// 后网
	meshutil::add_box(goal, p_shader, Vector3(0.02f, gh - 0.1f, gw - 0.1f),
			Vector3(back_dir * gd, gh * 0.5f, 0.0f), net_col);
	// 顶网
	meshutil::add_box(goal, p_shader, Vector3(gd - 0.1f, 0.02f, gw - 0.1f),
			Vector3(back_dir * gd * 0.5f, gh - 0.02f, 0.0f), net_col);
	// 两侧侧网
	meshutil::add_box(goal, p_shader, Vector3(gd - 0.1f, gh - 0.1f, 0.02f),
			Vector3(back_dir * gd * 0.5f, gh * 0.5f, -gw * 0.5f + 0.02f), net_mesh);
	meshutil::add_box(goal, p_shader, Vector3(gd - 0.1f, gh - 0.1f, 0.02f),
			Vector3(back_dir * gd * 0.5f, gh * 0.5f, gw * 0.5f - 0.02f), net_mesh);

	// 5. 物理碰撞体
	// 左右立柱碰撞
	Ref<BoxShape3D> post_shape;
	post_shape.instantiate();
	post_shape->set_size(Vector3(post_w * 1.2f, gh, post_w * 1.2f));

	CollisionShape3D *col_l = memnew(CollisionShape3D);
	col_l->set_name("ColPostL");
	col_l->set_shape(post_shape);
	col_l->set_position(Vector3(0.0f, gh * 0.5f, -gw * 0.5f));
	goal->add_child(col_l);

	CollisionShape3D *col_r = memnew(CollisionShape3D);
	col_r->set_name("ColPostR");
	col_r->set_shape(post_shape);
	col_r->set_position(Vector3(0.0f, gh * 0.5f, gw * 0.5f));
	goal->add_child(col_r);

	// 横梁碰撞
	Ref<BoxShape3D> bar_shape;
	bar_shape.instantiate();
	bar_shape->set_size(Vector3(post_w * 1.2f, post_w * 1.2f, gw + post_w));

	CollisionShape3D *col_bar = memnew(CollisionShape3D);
	col_bar->set_name("ColBar");
	col_bar->set_shape(bar_shape);
	col_bar->set_position(Vector3(0.0f, gh, 0.0f));
	goal->add_child(col_bar);

	// 后网物理阻挡（进球后把球拦住，产生真实的网窝感）
	Ref<BoxShape3D> back_net_shape;
	back_net_shape.instantiate();
	back_net_shape->set_size(Vector3(0.12f, gh, gw));

	CollisionShape3D *col_net = memnew(CollisionShape3D);
	col_net->set_name("ColNet");
	col_net->set_shape(back_net_shape);
	col_net->set_position(Vector3(back_dir * gd, gh * 0.5f, 0.0f));
	goal->add_child(col_net);
}

// 足球场白线标记（边线、中线、中圈、禁区白线）
void SoccerField::build_pitch_markings(Ref<Shader> p_shader) {
	const Color chalk(0.96f, 0.98f, 0.98f);
	const float y = pitch_center.y + 0.035f; // 抬高至草根上方 3.5cm，清晰可见
	const float lw = 0.14f; // 线宽 14cm

	// 1. 四周边线
	// 北边线 (z = -55.0)
	meshutil::add_box(this, p_shader, Vector3(pitch_half_w * 2.0f, 0.01f, lw),
			Vector3(pitch_center.x, y, pitch_center.z - pitch_half_d), chalk);
	// 南边线 (z = -25.0)
	meshutil::add_box(this, p_shader, Vector3(pitch_half_w * 2.0f, 0.01f, lw),
			Vector3(pitch_center.x, y, pitch_center.z + pitch_half_d), chalk);
	// 西底线 (x = 36.0)
	meshutil::add_box(this, p_shader, Vector3(lw, 0.01f, pitch_half_d * 2.0f),
			Vector3(pitch_center.x - pitch_half_w, y, pitch_center.z), chalk);
	// 东底线 (x = 84.0)
	meshutil::add_box(this, p_shader, Vector3(lw, 0.01f, pitch_half_d * 2.0f),
			Vector3(pitch_center.x + pitch_half_w, y, pitch_center.z), chalk);

	// 2. 中线
	meshutil::add_box(this, p_shader, Vector3(lw, 0.01f, pitch_half_d * 2.0f),
			Vector3(pitch_center.x, y, pitch_center.z), chalk);

	// 3. 中圈（由 16 段短线拼接而成的圆环，半径 4.5m）
	for (int i = 0; i < 20; i++) {
		float a = (float)i / 20.0f * 6.2831853f;
		float seg_w = 4.5f * 0.32f;
		Vector3 p = pitch_center + Vector3(std::cos(a) * 4.5f, 0.015f, std::sin(a) * 4.5f);
		meshutil::add_box(this, p_shader, Vector3(lw, 0.01f, seg_w), p, chalk, Vector3(0.0f, -a, 0.0f));
	}
	// 中圈开球点
	meshutil::add_box(this, p_shader, Vector3(0.36f, 0.015f, 0.36f),
			Vector3(pitch_center.x, y, pitch_center.z), chalk);

	// 4. 两侧禁区（宽 10m, 深 5.5m）
	// 西禁区
	meshutil::add_box(this, p_shader, Vector3(5.5f, 0.01f, lw),
			Vector3(pitch_center.x - pitch_half_w + 2.75f, y, pitch_center.z - 5.0f), chalk);
	meshutil::add_box(this, p_shader, Vector3(5.5f, 0.01f, lw),
			Vector3(pitch_center.x - pitch_half_w + 2.75f, y, pitch_center.z + 5.0f), chalk);
	meshutil::add_box(this, p_shader, Vector3(lw, 0.01f, 10.0f),
			Vector3(pitch_center.x - pitch_half_w + 5.5f, y, pitch_center.z), chalk);

	// 东禁区
	meshutil::add_box(this, p_shader, Vector3(5.5f, 0.01f, lw),
			Vector3(pitch_center.x + pitch_half_w - 2.75f, y, pitch_center.z - 5.0f), chalk);
	meshutil::add_box(this, p_shader, Vector3(5.5f, 0.01f, lw),
			Vector3(pitch_center.x + pitch_half_w - 2.75f, y, pitch_center.z + 5.0f), chalk);
	meshutil::add_box(this, p_shader, Vector3(lw, 0.01f, 10.0f),
			Vector3(pitch_center.x + pitch_half_w - 5.5f, y, pitch_center.z), chalk);
}

void SoccerField::build_corner_flags(Ref<Shader> p_shader) {
	const Color pole_c(0.92f, 0.90f, 0.85f);
	const Color flag_c(0.95f, 0.25f, 0.18f); // 鲜红角旗

	const Vector2 corners[] = {
		Vector2(pitch_center.x - pitch_half_w, pitch_center.z - pitch_half_d),
		Vector2(pitch_center.x + pitch_half_w, pitch_center.z - pitch_half_d),
		Vector2(pitch_center.x - pitch_half_w, pitch_center.z + pitch_half_d),
		Vector2(pitch_center.x + pitch_half_w, pitch_center.z + pitch_half_d)
	};

	for (int i = 0; i < 4; i++) {
		Vector3 p(corners[i].x, pitch_center.y, corners[i].y);
		// 旗杆
		meshutil::add_box(this, p_shader, Vector3(0.04f, 1.4f, 0.04f), p + Vector3(0.0f, 0.7f, 0.0f), pole_c);
		// 三角旗帜
		meshutil::add_box(this, p_shader, Vector3(0.32f, 0.22f, 0.02f), p + Vector3(0.16f, 1.25f, 0.0f), flag_c);
	}
}

void SoccerField::spawn_ball() {
	SoccerBall *ball = memnew(SoccerBall);
	ball->set_name("SoccerBall");
	ball->set_position(pitch_center + Vector3(0.0f, 0.40f, 0.0f));
	add_child(ball);
}
