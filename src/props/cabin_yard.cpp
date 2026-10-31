#include "cabin_yard.h"

#include <cmath>

#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>

#include "../systems/mesh_util.h"

using namespace godot;

void CabinYard::_bind_methods() {
}

void CabinYard::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
		build();
	}
}

void CabinYard::build() {
	if (get_parent() == nullptr) {
		return;
	}
	Node3D *host = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Homestead"));
	if (host == nullptr) {
		return;
	}
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.42f, 0.29f, 0.15f);
	const Color wood_light(0.58f, 0.42f, 0.24f);
	const Color bark(0.30f, 0.20f, 0.11f);
	const Color iron(0.30f, 0.31f, 0.32f);
	const Color terracotta(0.72f, 0.40f, 0.24f);
	const Color leaf(0.26f, 0.50f, 0.24f);
	const Color pink(0.94f, 0.52f, 0.60f);

	// —— 劈柴墩与斧头 ——
	Node3D *block = memnew(Node3D);
	block->set_name("ChoppingBlock");
	block->set_position(Vector3(3.0f, 0.0f, 4.8f));
	host->add_child(block);
	Ref<CylinderMesh> stump;
	stump.instantiate();
	stump->set_top_radius(0.28f);
	stump->set_bottom_radius(0.30f);
	stump->set_height(0.45f);
	stump->set_radial_segments(12);
	stump->set_rings(1);
	stump->set_material(meshutil::toon_material(toon, bark, 0.25f));
	MeshInstance3D *stump_mi = memnew(MeshInstance3D);
	stump_mi->set_mesh(stump);
	stump_mi->set_position(Vector3(0, 0.225f, 0));
	block->add_child(stump_mi);
	meshutil::add_box(block, toon, Vector3(0.045f, 0.55f, 0.045f), Vector3(0.10f, 0.58f, 0.02f), wood_light, Vector3(0.0f, 0.0f, -0.35f));
	meshutil::add_box(block, toon, Vector3(0.18f, 0.12f, 0.035f), Vector3(-0.11f, 0.77f, 0.02f), iron, Vector3(0.0f, 0.0f, -0.35f));

	// —— 柴堆：两排圆木 ——
	Node3D *pile = memnew(Node3D);
	pile->set_name("Woodpile");
	pile->set_position(Vector3(4.15f, 0.0f, 2.1f));
	host->add_child(pile);
	Ref<CylinderMesh> log;
	log.instantiate();
	log->set_top_radius(0.085f);
	log->set_bottom_radius(0.085f);
	log->set_height(0.92f);
	log->set_radial_segments(8);
	log->set_rings(1);
	log->set_material(meshutil::toon_material(toon, wood, 0.25f));
	for (int row = 0; row < 3; row++) {
		int count = 3 - row;
		for (int i = 0; i < count; i++) {
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(log);
			mi->set_position(Vector3(-0.18f + (float)i * 0.20f + (float)row * 0.10f, 0.09f + (float)row * 0.16f, 0.0f));
			mi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
			pile->add_child(mi);
		}
	}

	// —— 门口花盆 ——
	for (int i = 0; i < 2; i++) {
		Node3D *pot = memnew(Node3D);
		pot->set_name(String("FlowerPot") + String::num_int64(i));
		pot->set_position(Vector3(i == 0 ? -0.95f : 0.95f, 0.0f, 3.95f));
		host->add_child(pot);
		Ref<CylinderMesh> pm;
		pm.instantiate();
		pm->set_top_radius(0.14f);
		pm->set_bottom_radius(0.10f);
		pm->set_height(0.22f);
		pm->set_radial_segments(9);
		pm->set_rings(1);
		pm->set_material(meshutil::toon_material(toon, terracotta, 0.3f));
		MeshInstance3D *pmi = memnew(MeshInstance3D);
		pmi->set_mesh(pm);
		pmi->set_position(Vector3(0, 0.11f, 0));
		pot->add_child(pmi);
		meshutil::add_sphere(pot, toon, 0.15f, Vector3(0, 0.25f, 0), leaf);
		for (int k = 0; k < 3; k++) {
			float a = 6.2831853f * (float)k / 3.0f;
			meshutil::add_sphere(pot, toon, 0.035f, Vector3(std::cos(a) * 0.08f, 0.35f + (float)k * 0.015f, std::sin(a) * 0.08f), pink);
		}
	}

	// —— 门垫 ——
	meshutil::add_box(host, toon, Vector3(0.82f, 0.025f, 0.52f), Vector3(0.0f, 0.015f, 4.25f), Color(0.62f, 0.52f, 0.36f));

	// —— 信箱 ——
	Node3D *mail = memnew(Node3D);
	mail->set_name("Mailbox");
	mail->set_position(Vector3(-2.6f, 0.0f, 6.2f));
	host->add_child(mail);
	meshutil::add_box(mail, toon, Vector3(0.09f, 0.95f, 0.09f), Vector3(0, 0.47f, 0), wood);
	meshutil::add_box(mail, toon, Vector3(0.30f, 0.22f, 0.38f), Vector3(0, 1.02f, 0), Color(0.55f, 0.42f, 0.26f));
	meshutil::add_box(mail, toon, Vector3(0.03f, 0.18f, 0.03f), Vector3(0.17f, 1.10f, 0.05f), Color(0.80f, 0.20f, 0.16f));
	meshutil::add_box(mail, toon, Vector3(0.16f, 0.02f, 0.03f), Vector3(0.09f, 1.19f, 0.05f), Color(0.80f, 0.20f, 0.16f));
}
