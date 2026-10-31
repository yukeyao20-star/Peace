#include "kelp_field.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "terrain_builder.h"

using namespace godot;

void KelpField::_bind_methods() {
}

void KelpField::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			if (player != nullptr) {
				set_visible(player->get_global_position().distance_to(
						Vector3(lake_center.x, water_level, lake_center.y)) < 75.0f);
			}
		} break;
		default:
			break;
	}
}

void KelpField::build() {
	Ref<ArrayMesh> mesh;
	build_mesh(mesh);
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://assets/kelp.gdshader");
	Ref<ShaderMaterial> mat;
	mat.instantiate();
	if (shader.is_valid()) {
		mat->set_shader(shader);
	}
	mesh->surface_set_material(0, mat);

	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(mesh);
	const int COUNT = 240;
	mm->set_instance_count(COUNT);

	std::mt19937 rng(707070u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	int placed = 0;
	int guard = 0;
	while (placed < COUNT && guard < 9000) {
		guard++;
		// 两片水域：桥下水湾 + 西北主湖
		float cx = -6.0f;
		float cz = 26.0f;
		float rr = 12.0f;
		if (d01(rng) < 0.55f) {
			cx = -14.0f;
			cz = 66.0f;
			rr = 19.0f;
		}
		float a = d01(rng) * 6.2831853f;
		float r = std::sqrt(d01(rng)) * rr;
		float bx = cx + std::cos(a) * r;
		float bz = cz + std::sin(a) * r;
		if (terrain == nullptr) {
			break;
		}
		if (terrain->water_depth_at(bx, bz) < 1.6f) {
			continue;
		}
		// 一丛 3~4 片细叶，围着一小块湖床
		int blades = 3 + (int)(d01(rng) * 2.0f);
		for (int k = 0; k < blades && placed < COUNT; k++) {
			float ox = bx + (d01(rng) - 0.5f) * 0.72f;
			float oz = bz + (d01(rng) - 0.5f) * 0.72f;
			float depth = terrain->water_depth_at(ox, oz);
			if (depth < 1.4f || depth > 5.2f) {
				continue;
			}
			float h = terrain->get_terrain_height(ox, oz);
			float height = 0.9f + d01(rng) * 1.5f;
			float width = 0.28f + d01(rng) * 0.24f;
			Transform3D xf;
			xf.origin = Vector3(ox, h + 0.02f, oz);
			Basis b = Basis(Vector3(0.0f, 1.0f, 0.0f), d01(rng) * 6.2831853f + (float)k * 1.7f);
			b = b.scaled(Vector3(width, height, width));
			xf.basis = b;
			mm->set_instance_transform(placed, xf);
			placed++;
		}
	}
	mm->set_instance_count(placed);

	mmi = memnew(MultiMeshInstance3D);
	mmi->set_name("Kelp");
	mmi->set_multimesh(mm);
	mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(mmi);

	if (terrain == nullptr) {
		return;
	}

	// 湖底气泡泉：三处持续冒泡的小泉眼
	Ref<SphereMesh> bubble;
	bubble.instantiate();
	bubble->set_radius(0.018f);
	bubble->set_height(0.036f);
	bubble->set_radial_segments(6);
	bubble->set_rings(3);
	Ref<StandardMaterial3D> bmat;
	bmat.instantiate();
	bmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	bmat->set_albedo(Color(0.85f, 0.95f, 1.0f, 0.5f));
	bmat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	bubble->set_material(bmat);
	const Vector2 vents[3] = { Vector2(-8.0f, 22.0f), Vector2(-16.0f, 62.0f), Vector2(14.0f, 54.0f) };
	for (int i = 0; i < 3; i++) {
		float h = terrain->get_terrain_height(vents[i].x, vents[i].y);
		CPUParticles3D *vent = memnew(CPUParticles3D);
		vent->set_name("Vent" + String::num_int64(i));
		vent->set_position(Vector3(vents[i].x, h + 0.15f, vents[i].y));
		vent->set_amount(18);
		vent->set_lifetime(2.6f);
		vent->set_mesh(bubble);
		vent->set_use_local_coordinates(false);
		vent->set_direction(Vector3(0.0f, 1.0f, 0.0f));
		vent->set_spread(10.0f);
		vent->set_gravity(Vector3(0.0f, 0.28f, 0.0f));
		vent->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.15f);
		vent->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.38f);
		vent->set_param_min(CPUParticles3D::PARAM_SCALE, 0.5f);
		vent->set_param_max(CPUParticles3D::PARAM_SCALE, 1.3f);
		vent->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
		vent->set_emission_sphere_radius(0.14f);
		vent->set_emitting(true);
		add_child(vent);
	}

	// 湖底贝壳：散落的浅色小贝壳
	Ref<SphereMesh> shell_mesh;
	shell_mesh.instantiate();
	shell_mesh->set_radius(0.07f);
	shell_mesh->set_height(0.05f);
	shell_mesh->set_radial_segments(8);
	shell_mesh->set_rings(4);
	Ref<StandardMaterial3D> shell_mat;
	shell_mat.instantiate();
	shell_mat->set_albedo(Color(0.92f, 0.86f, 0.74f));
	shell_mat->set_roughness(0.8f);
	shell_mesh->set_material(shell_mat);
	Ref<MultiMesh> sm;
	sm.instantiate();
	sm->set_transform_format(MultiMesh::TRANSFORM_3D);
	sm->set_mesh(shell_mesh);
	sm->set_instance_count(14);
	int sp = 0;
	int guard2 = 0;
	while (sp < 14 && guard2 < 800) {
		guard2++;
		float x = -6.0f + (d01(rng) - 0.5f) * 26.0f;
		float z = 26.0f + (d01(rng) - 0.5f) * 34.0f;
		float depth = terrain->water_depth_at(x, z);
		if (depth < 1.5f) {
			continue;
		}
		float h = terrain->get_terrain_height(x, z);
		Transform3D xf;
		xf.origin = Vector3(x, h + 0.03f, z);
		Basis b = Basis(Vector3(0.0f, 1.0f, 0.0f), d01(rng) * 6.2831853f);
		float s = 0.7f + d01(rng) * 0.8f;
		b = b.scaled(Vector3(s, 0.55f * s, s));
		xf.basis = b;
		sm->set_instance_transform(sp, xf);
		sp++;
	}
	sm->set_instance_count(sp);
	MultiMeshInstance3D *smm = memnew(MultiMeshInstance3D);
	smm->set_name("Shells");
	smm->set_multimesh(sm);
	smm->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(smm);
}

void KelpField::build_mesh(Ref<ArrayMesh> &r_mesh) {
	const int SEG = 7;
	std::vector<Vector3> verts;
	std::vector<Vector3> normals;
	std::vector<Vector2> uvs;
	std::vector<int> idx;
	for (int i = 0; i <= SEG; i++) {
		float t = (float)i / (float)SEG;
		float y = t; // 单位高度，实例缩放决定实际高度
		float curve = std::sin(t * 2.3f) * 0.10f;
		float half = 0.5f * (1.0f - t * 0.72f); // 顶部收窄
		verts.push_back(Vector3(-half, y, curve));
		verts.push_back(Vector3(half, y, curve));
		normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
		normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
		uvs.push_back(Vector2(0.0f, t));
		uvs.push_back(Vector2(1.0f, t));
		if (i < SEG) {
			int b = i * 2;
			idx.push_back(b);
			idx.push_back(b + 1);
			idx.push_back(b + 2);
			idx.push_back(b + 1);
			idx.push_back(b + 3);
			idx.push_back(b + 2);
		}
	}
	PackedVector3Array pv;
	pv.resize((int)verts.size());
	for (size_t i = 0; i < verts.size(); i++) {
		pv[(int)i] = verts[i];
	}
	PackedVector3Array pn;
	pn.resize((int)normals.size());
	for (size_t i = 0; i < normals.size(); i++) {
		pn[(int)i] = normals[i];
	}
	PackedVector2Array pu;
	pu.resize((int)uvs.size());
	for (size_t i = 0; i < uvs.size(); i++) {
		pu[(int)i] = uvs[i];
	}
	PackedInt32Array pi;
	pi.resize((int)idx.size());
	for (size_t i = 0; i < idx.size(); i++) {
		pi[(int)i] = idx[i];
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = pv;
	arrays[Mesh::ARRAY_NORMAL] = pn;
	arrays[Mesh::ARRAY_TEX_UV] = pu;
	arrays[Mesh::ARRAY_INDEX] = pi;
	r_mesh.instantiate();
	r_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
}
