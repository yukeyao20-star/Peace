#include "well.h"

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

namespace {

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// 造一个圆柱 MeshInstance3D（axis 欧拉角让它改变朝向）
MeshInstance3D *add_cylinder(Node3D *p_parent, float p_radius, float p_height, const Color &p_color,
		const Ref<Shader> &p_shader, const Vector3 &p_pos, const Vector3 &p_rot = Vector3()) {
	Ref<CylinderMesh> cyl;
	cyl.instantiate();
	cyl->set_top_radius(p_radius);
	cyl->set_bottom_radius(p_radius);
	cyl->set_height(p_height);
	cyl->set_radial_segments(10);
	cyl->set_rings(1);
	cyl->set_material(meshutil::toon_material(p_shader, p_color));

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(cyl);
	Transform3D t;
	Basis b;
	b.set_euler_scale(p_rot, Vector3(1, 1, 1));
	t.set_basis(b);
	t.set_origin(p_pos);
	mi->set_transform(t);
	p_parent->add_child(mi);
	return mi;
}

} // namespace

void Well::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_up"), &Well::is_up);
}

void Well::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成
			}
			build(meshutil::load_toon_shader());
			add_to_group("interactables");
			set_process(true);
		} break;
		default:
			break;
	}
}

void Well::build(Ref<Shader> p_shader) {
	const Color stone(0.60f, 0.59f, 0.55f);   // 井圈：石灰岩
	const Color stone_dark(0.48f, 0.47f, 0.43f);
	const Color wood(0.52f, 0.34f, 0.20f);    // 木架：橡木
	const Color wood_dark(0.34f, 0.22f, 0.13f);
	const Color roof_c(0.52f, 0.30f, 0.21f);  // 顶棚
	const Color water_c(0.16f, 0.34f, 0.46f); // 井水
	const Color rope_c(0.72f, 0.64f, 0.48f);  // 麻绳

	// —— 石砌井圈：8 块石头围一圈（带碰撞） ——
	StaticBody3D *ring = memnew(StaticBody3D);
	ring->set_name("Ring");
	add_child(ring);
	for (int i = 0; i < 8; i++) {
		float a = (float)i / 8.0f * 6.2831853f;
		Vector3 pos(std::cos(a) * 0.78f, 0.275f, std::sin(a) * 0.78f);
		// 石块的宽面贴着切线方向 → yaw 转到切线角
		meshutil::add_box(ring, p_shader, Vector3(0.52f, 0.55f, 0.30f), pos,
				(i % 2 == 0 ? stone : stone_dark), Vector3(0.0f, -a, 0.0f));
	}
	// 井里的水（深色小圆盘，微微低于井口）
	add_cylinder(ring, 0.62f, 0.02f, water_c, p_shader, Vector3(0.0f, 0.32f, 0.0f));

	Ref<CylinderShape3D> ring_shape;
	ring_shape.instantiate();
	ring_shape->set_radius(0.88f);
	ring_shape->set_height(0.6f);
	CollisionShape3D *ring_col = memnew(CollisionShape3D);
	ring_col->set_name("Collision");
	ring_col->set_shape(ring_shape);
	ring_col->set_position(Vector3(0.0f, 0.3f, 0.0f));
	ring->add_child(ring_col);

	// —— 辘轳架：两根木柱 + 横轴 + 人字顶棚 ——
	meshutil::add_box(this, p_shader, Vector3(0.14f, 1.9f, 0.14f), Vector3(-0.95f, 0.95f, 0.0f), wood);
	meshutil::add_box(this, p_shader, Vector3(0.14f, 1.9f, 0.14f), Vector3(0.95f, 0.95f, 0.0f), wood);
	add_cylinder(this, 0.055f, 2.10f, wood_dark, p_shader, Vector3(0.0f, 1.88f, 0.0f),
			Vector3(0.0f, 0.0f, 1.5707964f)); // π/2，圆柱转向沿 X
	meshutil::add_box(this, p_shader, Vector3(2.30f, 0.05f, 0.75f), Vector3(0.0f, 2.35f, 0.28f),
			roof_c, Vector3(0.6f, 0.0f, 0.0f));
	meshutil::add_box(this, p_shader, Vector3(2.30f, 0.05f, 0.75f), Vector3(0.0f, 2.35f, -0.28f),
			roof_c, Vector3(-0.6f, 0.0f, 0.0f));
	meshutil::add_box(this, p_shader, Vector3(2.34f, 0.06f, 0.10f), Vector3(0.0f, 2.58f, 0.0f), wood_dark);

	// —— 摇把：横轴右端，打水时旋转 ——
	crank = memnew(Node3D);
	crank->set_name("Crank");
	crank->set_position(Vector3(1.02f, 1.88f, 0.0f));
	add_child(crank);
	meshutil::add_box(crank, p_shader, Vector3(0.05f, 0.05f, 0.26f), Vector3(0.0f, 0.0f, 0.13f), wood_dark);
	add_cylinder(crank, 0.028f, 0.16f, wood, p_shader, Vector3(0.08f, 0.0f, 0.26f),
			Vector3(0.0f, 0.0f, 1.5707964f));

	// —— 绳索锚点 + 吊绳 + 水桶 ——
	winch = memnew(Node3D);
	winch->set_name("Winch");
	winch->set_position(Vector3(0.0f, 1.82f, 0.0f));
	add_child(winch);

	rope = add_cylinder(winch, 0.012f, 1.0f, rope_c, p_shader, Vector3(0.0f, -0.5f, 0.0f));

	bucket = memnew(Node3D);
	bucket->set_name("Bucket");
	winch->add_child(bucket);
	add_cylinder(bucket, 0.17f, 0.24f, wood, p_shader, Vector3(0.0f, -0.12f, 0.0f));
	meshutil::add_box(bucket, p_shader, Vector3(0.36f, 0.04f, 0.36f), Vector3(0.0f, -0.02f, 0.0f), wood_dark);
	meshutil::add_box(bucket, p_shader, Vector3(0.03f, 0.03f, 0.30f), Vector3(0.0f, 0.02f, 0.0f), wood_dark); // 提梁
	add_cylinder(bucket, 0.02f, 0.05f, rope_c, p_shader, Vector3(0.0f, 0.05f, 0.0f)); // 绳结
}

void Well::_process(double p_delta) {
	if (up_t == up_target || bucket == nullptr) {
		return;
	}
	float dir = up_target > up_t ? 1.0f : -1.0f;
	up_t = Math::clamp(up_t + dir * (float)p_delta * 0.85f, 0.0f, 1.0f);
	// Hermite 缓动
	float e = up_t * up_t * (3.0f - 2.0f * up_t);
	float len = lerpf(1.30f, 0.14f, e); // 绳长：井底 → 横轴下
	rope->set_scale(Vector3(1.0f, len, 1.0f));
	rope->set_position(Vector3(0.0f, -len * 0.5f, 0.0f));
	bucket->set_position(Vector3(0.0f, -len, 0.0f));
	if (crank != nullptr) {
		crank->set_rotation(Vector3(-12.566f * e, 0.0f, 0.0f)); // 升满恰好两整圈
	}
}

String Well::get_prompt() const {
	return is_up() ? String::utf8("按 E 放下水桶") : String::utf8("按 E 打水");
}

Vector3 Well::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.2f, 0.0f);
}

void Well::interact(const Vector3 &p_player_pos) {
	if (up_t != up_target) {
		return; // 动画进行中不接受新指令
	}
	up_target = is_up() ? 0.0f : 1.0f;
}
