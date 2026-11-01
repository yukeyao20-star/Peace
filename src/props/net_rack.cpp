#include "net_rack.h"
#include <cmath>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

void NetRack::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &NetRack::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &NetRack::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &NetRack::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &NetRack::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &NetRack::interact);
	ClassDB::bind_method(D_METHOD("get_floats"), &NetRack::get_floats);
	ClassDB::bind_method(D_METHOD("get_shakes"), &NetRack::get_shakes);
	ClassDB::bind_method(D_METHOD("is_swinging"), &NetRack::is_swinging);
}
String NetRack::get_prompt() const { return String::utf8("按 E 抖一抖渔网"); }
Vector3 NetRack::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 1.0f, 0.0f); }
void NetRack::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	shakes++;
	shake = 1.1f;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("抖了抖渔网 · 水珠顺着网眼往下掉"), 2.6f);
	}
}
void NetRack::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (shake > 0.0f) shake = Math::max(0.0f, shake - delta);
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (net != nullptr) {
				const float s = shake > 0.0f ? std::sin(shake * 26.0f) * shake * 0.10f : 0.0f;
				net->set_rotation(Vector3(s, std::sin(t * 0.8f) * 0.015f, std::sin(t * 1.1f) * 0.02f));
			}
			if (floats != nullptr) floats->set_rotation(Vector3(0.0f, 0.0f, std::sin(t * 1.3f) * 0.02f));
		} break;
		default: break;
	}
}
void NetRack::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z); set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, 0.4f, 0.0f));
	// ㄇ 形木架
	meshutil::add_box(this, toon, Vector3(0.08f, 1.34f, 0.08f), Vector3(-0.82f, 0.67f, 0.0f), Color(0.52f, 0.38f, 0.23f));
	meshutil::add_box(this, toon, Vector3(0.08f, 1.34f, 0.08f), Vector3(0.82f, 0.67f, 0.0f), Color(0.52f, 0.38f, 0.23f));
	meshutil::add_box(this, toon, Vector3(1.80f, 0.075f, 0.075f), Vector3(0.0f, 1.31f, 0.0f), Color(0.48f, 0.35f, 0.21f));
	meshutil::add_box(this, toon, Vector3(0.20f, 0.05f, 0.20f), Vector3(-0.82f, 0.025f, 0.0f), Color(0.44f, 0.42f, 0.38f));
	meshutil::add_box(this, toon, Vector3(0.20f, 0.05f, 0.20f), Vector3(0.82f, 0.025f, 0.0f), Color(0.44f, 0.42f, 0.38f));
	// 渔网：横竖网线合成一个 MultiMesh
	Ref<BoxMesh> unit;
	unit.instantiate();
	unit->set_size(Vector3(1.0f, 1.0f, 1.0f));
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(unit);
	mm->set_instance_count(20);
	int idx = 0;
	for (int h = 0; h < 8; h++) {
		const float y = -0.06f - 0.125f * (float)h;
		mm->set_instance_transform(idx++, Transform3D(Basis().scaled(Vector3(1.56f, 0.008f, 0.008f)), Vector3(0.0f, y, 0.0f)));
	}
	for (int v = 0; v < 12; v++) {
		const float x = -0.72f + 0.131f * (float)v;
		mm->set_instance_transform(idx++, Transform3D(Basis().scaled(Vector3(0.008f, 1.06f, 0.008f)), Vector3(x, -0.55f, 0.0f)));
	}
	net = memnew(Node3D);
	net->set_name("Net");
	net->set_position(Vector3(0.0f, 1.28f, 0.0f));
	add_child(net);
	MultiMeshInstance3D *nmi = memnew(MultiMeshInstance3D);
	nmi->set_name("Mesh");
	nmi->set_multimesh(mm);
	nmi->set_material_override(meshutil::toon_material(toon, Color(0.80f, 0.78f, 0.68f), 0.2f));
	net->add_child(nmi);
	// 浮子：软木色小圆柱，挂在横杆前
	floats = memnew(Node3D);
	floats->set_name("Floats");
	floats->set_position(Vector3(0.0f, 1.18f, 0.12f));
	add_child(floats);
	Ref<CylinderMesh> cork;
	cork.instantiate();
	cork->set_top_radius(0.052f);
	cork->set_bottom_radius(0.045f);
	cork->set_height(0.12f);
	cork->set_radial_segments(8);
	cork->set_rings(1);
	cork->set_material(meshutil::toon_material(toon, Color(0.78f, 0.52f, 0.26f), 0.25f));
	for (int i = 0; i < 6; i++) {
		MeshInstance3D *cmi = memnew(MeshInstance3D);
		cmi->set_mesh(cork);
		cmi->set_position(Vector3(-0.60f + 0.24f * (float)i, -0.02f * std::sin((float)i * 1.7f), 0.0f));
		cmi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
		floats->add_child(cmi);
	}
}
