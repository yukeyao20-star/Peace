#include "door.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

void CabinDoor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_open"), &CabinDoor::is_open);
}

void CabinDoor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成，保持场景干净
			}
			build(meshutil::load_toon_shader());
			add_to_group("interactables");
			set_process(true);
		} break;
		default:
			break;
	}
}

void CabinDoor::build(Ref<Shader> p_shader) {
	const Color wood(0.55f, 0.36f, 0.20f);  // 门板：橡木
	const Color trim(0.38f, 0.25f, 0.14f);  // 门框：深木
	const Color brass(0.85f, 0.70f, 0.35f); // 把手：黄铜

	// —— 门框：左右门柱 + 上槛（静态，直接挂在节点上） ——
	meshutil::add_box(this, p_shader, Vector3(0.14f, 2.0f, 0.16f), Vector3(-0.61f, 1.0f, 0.0f), trim);
	meshutil::add_box(this, p_shader, Vector3(0.14f, 2.0f, 0.16f), Vector3(0.61f, 1.0f, 0.0f), trim);
	meshutil::add_box(this, p_shader, Vector3(1.36f, 0.16f, 0.16f), Vector3(0.0f, 1.97f, 0.0f), trim);

	// —— 门扇：合页枢轴 + 带碰撞的 StaticBody3D ——
	pivot = memnew(Node3D);
	pivot->set_name("Pivot");
	pivot->set_position(Vector3(-0.54f, 0.0f, 0.0f)); // 铰链在门扇左缘
	add_child(pivot);

	StaticBody3D *panel = memnew(StaticBody3D);
	panel->set_name("Panel");
	panel->set_position(Vector3(0.54f, 0.0f, 0.0f)); // 门扇中心相对铰链右移半宽
	pivot->add_child(panel);

	// 门板主体（1.04 宽 × 1.9 高 × 0.09 厚，底部落在地面上）
	Ref<BoxMesh> leaf;
	leaf.instantiate();
	leaf->set_size(Vector3(1.04f, 1.9f, 0.09f));
	leaf->set_material(meshutil::toon_material(p_shader, wood));
	MeshInstance3D *leaf_mi = memnew(MeshInstance3D);
	leaf_mi->set_name("Leaf");
	leaf_mi->set_mesh(leaf);
	leaf_mi->set_position(Vector3(0.0f, 0.95f, 0.0f));
	panel->add_child(leaf_mi);

	// 上下两块凹槽饰条（MC 木门的凹槽观感，略微凸出门板表面）
	meshutil::add_box(panel, p_shader, Vector3(0.72f, 0.62f, 0.02f), Vector3(0.0f, 1.32f, 0.0f), trim);
	meshutil::add_box(panel, p_shader, Vector3(0.72f, 0.62f, 0.02f), Vector3(0.0f, 0.52f, 0.0f), trim);

	// —— 门把手：穿门短轴 + 两侧圆头（黄铜色） ——
	Ref<CylinderMesh> bar;
	bar.instantiate();
	bar->set_top_radius(0.018f);
	bar->set_bottom_radius(0.018f);
	bar->set_height(0.22f);
	{
		Ref<ShaderMaterial> bm = meshutil::toon_material(p_shader, brass, 0.5f);
		bm->set_shader_parameter("roughness_value", 0.4f);
		bar->set_material(bm);
	}
	MeshInstance3D *bar_mi = memnew(MeshInstance3D);
	bar_mi->set_mesh(bar);
	{ // 圆柱默认沿 Y 轴，转 90° 让它垂直穿过门板（沿 Z）
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(1.5707964f, 0.0f, 0.0f), Vector3(1, 1, 1)); // π/2，圆柱转向垂直穿门
		t.set_basis(b);
		t.set_origin(Vector3(0.42f, 0.95f, 0.0f));
		bar_mi->set_transform(t);
	}
	panel->add_child(bar_mi);

	Ref<SphereMesh> knob;
	knob.instantiate();
	knob->set_radius(0.045f);
	knob->set_height(0.09f);
	knob->set_radial_segments(10);
	knob->set_rings(5);
	{
		Ref<ShaderMaterial> bm = meshutil::toon_material(p_shader, brass, 0.5f);
		bm->set_shader_parameter("roughness_value", 0.4f);
		knob->set_material(bm);
	}
	for (int side = 0; side < 2; side++) {
		MeshInstance3D *k = memnew(MeshInstance3D);
		k->set_mesh(knob);
		k->set_position(Vector3(0.42f, 0.95f, side == 0 ? 0.075f : -0.075f));
		panel->add_child(k);
	}

	// —— 碰撞：与门板同尺寸的方盒，开关时随枢轴一起转 ——
	Ref<BoxShape3D> shape;
	shape.instantiate();
	shape->set_size(Vector3(1.04f, 1.9f, 0.09f));
	CollisionShape3D *col = memnew(CollisionShape3D);
	col->set_name("Collision");
	col->set_shape(shape);
	col->set_position(Vector3(0.0f, 0.95f, 0.0f));
	panel->add_child(col);
}

void CabinDoor::_process(double p_delta) {
	if (open_t == open_target || pivot == nullptr) {
		return;
	}
	float dir = open_target > open_t ? 1.0f : -1.0f;
	open_t = Math::clamp(open_t + dir * (float)p_delta * 3.0f, 0.0f, 1.0f);
	// Hermite 缓动：起步慢、中段快、收尾慢，像真实的门
	float e = open_t * open_t * (3.0f - 2.0f * open_t);
	pivot->set_rotation(Vector3(0.0f, open_sign * 1.92f * e, 0.0f)); // ≈110°
}

String CabinDoor::get_prompt() const {
	return is_open() ? String::utf8("按 E 关门") : String::utf8("按 E 开门");
}

Vector3 CabinDoor::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.0f, 0.0f);
}

void CabinDoor::interact(const Vector3 &p_player_pos) {
	if (open_t != open_target) {
		return; // 动画进行中不接受新指令
	}
	if (!is_open()) {
		// 把主角位置换到门局部空间：z>0 是门外（门廊侧），z<0 是屋内
		Vector3 lp = to_local(p_player_pos);
		open_sign = lp.z > 0.0f ? 1.0f : -1.0f; // 永远朝远离主角的方向开
		open_target = 1.0f;
	} else {
		open_target = 0.0f;
	}
}
