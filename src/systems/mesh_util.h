#pragma once

// mesh_util —— 结构搭建共用小工具。
//
// 世界里所有"人造物"（石桥、小木屋、门窗）统一用卡通着色器（project/assets/toon.gdshader，
// 与场景里旧的桥/主角材质一致），这里封装了"造一个方块 MeshInstance3D"的常用操作。

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {
namespace meshutil {

// 加载项目里的卡通着色器（加载失败返回 null，调用方回退即可）
inline Ref<Shader> load_toon_shader() {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/toon.gdshader");
	return Ref<Shader>(Object::cast_to<Shader>(res.ptr()));
}

// 卡通材质：albedo 颜色 + 边缘光强度
inline Ref<ShaderMaterial> toon_material(const Ref<Shader> &p_shader, const Color &p_albedo, float p_rim = 0.35f) {
	Ref<ShaderMaterial> m;
	m.instantiate();
	if (p_shader.is_valid()) {
		m->set_shader(p_shader);
	}
	m->set_shader_parameter("albedo", p_albedo);
	m->set_shader_parameter("rim", p_rim);
	m->set_shader_parameter("roughness_value", 0.9f);
	return m;
}

// 造一个方块 MeshInstance3D 挂到 parent 下。
// size 是方块尺寸，pos 是中心点位置（父节点局部坐标），rot 为欧拉角（弧度）。
inline MeshInstance3D *add_box(Node3D *p_parent, const Ref<Shader> &p_shader, const Vector3 &p_size,
		const Vector3 &p_pos, const Color &p_albedo, const Vector3 &p_rot = Vector3()) {
	Ref<BoxMesh> box;
	box.instantiate();
	box->set_size(p_size);
	box->set_material(toon_material(p_shader, p_albedo));

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(box);
	Transform3D t;
	Basis b;
	b.set_euler_scale(p_rot, Vector3(1, 1, 1));
	t.set_basis(b);
	t.set_origin(p_pos);
	mi->set_transform(t);
	p_parent->add_child(mi);
	return mi;
}

// 造一个球体 MeshInstance3D 挂到 parent 下。
// radius 为球半径（height 自动取 2×radius），pos 是中心点位置（父节点局部坐标）。
inline MeshInstance3D *add_sphere(Node3D *p_parent, const Ref<Shader> &p_shader, float p_radius,
		const Vector3 &p_pos, const Color &p_albedo) {
	Ref<SphereMesh> sphere;
	sphere.instantiate();
	sphere->set_radius(p_radius);
	sphere->set_height(p_radius * 2.0f);
	sphere->set_radial_segments(16);
	sphere->set_rings(8);
	sphere->set_material(toon_material(p_shader, p_albedo));

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(sphere);
	mi->set_position(p_pos);
	p_parent->add_child(mi);
	return mi;
}

} // namespace meshutil
} // namespace godot
