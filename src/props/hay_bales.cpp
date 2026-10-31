#include "hay_bales.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

void HayBales::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bale_count"), &HayBales::get_bale_count);
	ClassDB::bind_method(D_METHOD("get_birds_visible"), &HayBales::get_birds_visible);
	ClassDB::bind_method(D_METHOD("get_snow_covered"), &HayBales::get_snow_covered);
}

void HayBales::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			Window *root = (get_tree() != nullptr) ? get_tree()->get_root() : nullptr;
			if (root == nullptr) {
				break;
			}
			if (player == nullptr) {
				player = Object::cast_to<Node3D>(root->find_child("Player", true, false));
			}
			if (sky == nullptr) {
				sky = Object::cast_to<SkyCycle>(root->find_child("SkyCycle", true, false));
			}
			snow_covered = sky != nullptr && sky->get_snow_amount() > 0.45f;
			for (int i = 0; i < BALE_COUNT; i++) {
				if (snow_caps[i] != nullptr) {
					snow_caps[i]->set_visible(snow_covered);
				}
			}

			birds_visible = 0;
			const bool day = sky == nullptr || (sky->get_time_of_day() > 0.27f && sky->get_time_of_day() < 0.73f);
			for (int i = 0; i < BIRD_COUNT; i++) {
				if (birds[i] == nullptr) {
					continue;
				}
				const Vector3 wp = get_global_position() + Vector3(bird_pos[i][0], 0.05f, bird_pos[i][1]);
				float d = 999.0f;
				if (player != nullptr) {
					d = player->get_global_position().distance_to(wp);
				}
				if (d < 2.4f && !bird_flying[i]) {
					bird_flying[i] = true;
					bird_timer[i] = 2.2f;
				}
				if (bird_flying[i]) {
					bird_timer[i] -= delta;
					// 飞离草捆，绕一圈再落回
					const float a = t * 1.6f + (float)i;
					const float h = 1.2f + 0.5f * std::sin(t * 2.0f + (float)i);
					birds[i]->set_position(Vector3(2.5f + std::cos(a) * 2.2f, h, 1.2f + std::sin(a) * 2.0f));
					birds[i]->set_visible(day);
					for (int w = 0; w < 2; w++) {
						if (wings[i][w] != nullptr) {
							wings[i][w]->set_rotation(Vector3(0.0f, 0.0f,
									(w == 0 ? 1.0f : -1.0f) * (0.6f + 0.4f * std::sin(t * 20.0f))));
						}
					}
					if (bird_timer[i] <= 0.0f) {
						bird_flying[i] = false;
						bird_pos[i][0] = -0.2f + 1.2f * (float)i;
						bird_pos[i][1] = 1.1f + 0.3f * (float)i;
					}
					if (day) {
						birds_visible++;
					}
				} else {
					bird_timer[i] += delta;
					// 落地后在小范围里跳着啄食
					const float hop = std::fabs(std::sin(t * 2.2f + (float)i));
					const float peek = 0.035f * std::max(0.0f, std::sin(t * 4.0f + (float)i));
					birds[i]->set_position(Vector3(bird_pos[i][0] + 0.10f * std::sin(t * 0.7f + (float)i),
							0.045f + hop * 0.02f, bird_pos[i][1] + 0.08f * std::cos(t * 0.6f + (float)i)));
					birds[i]->set_rotation(Vector3(peek, std::sin(t * 0.7f + (float)i) * 0.6f, 0.0f));
					birds[i]->set_visible(day);
					for (int w = 0; w < 2; w++) {
						if (wings[i][w] != nullptr) {
							wings[i][w]->set_rotation(Vector3());
						}
					}
					if (day) {
						birds_visible++;
					}
				}
			}
		} break;
		default:
			break;
	}
}

void HayBales::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(
				get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) {
			Vector3 p = get_global_position();
			p.y = terrain->get_terrain_height(p.x, p.z);
			set_global_position(p);
		}
	}

	const Color straw(0.84f, 0.70f, 0.34f);
	Ref<CylinderMesh> bale_mesh;
	bale_mesh.instantiate();
	bale_mesh->set_top_radius(0.55f);
	bale_mesh->set_bottom_radius(0.55f);
	bale_mesh->set_height(0.82f);
	bale_mesh->set_radial_segments(16);
	bale_mesh->set_rings(1);
	bale_mesh->set_material(meshutil::toon_material(toon, straw, 0.2f));
	Ref<TorusMesh> band_mesh;
	band_mesh.instantiate();
	band_mesh->set_inner_radius(0.55f);
	band_mesh->set_outer_radius(0.575f);
	band_mesh->set_rings(16);
	band_mesh->set_ring_segments(3);
	band_mesh->set_material(meshutil::toon_material(toon, Color(0.62f, 0.52f, 0.32f), 0.15f));

	const Vector3 bpos[BALE_COUNT] = {
		Vector3(-0.55f, 0.55f, 0.0f),
		Vector3(0.35f, 0.55f, 0.05f),
		Vector3(-0.10f, 1.55f, 0.02f),
	};
	for (int i = 0; i < BALE_COUNT; i++) {
		MeshInstance3D *bi = memnew(MeshInstance3D);
		bi->set_mesh(bale_mesh);
		bi->set_position(bpos[i]);
		bi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
		add_child(bi);
		for (int b = 0; b < 2; b++) {
			MeshInstance3D *band = memnew(MeshInstance3D);
			band->set_mesh(band_mesh);
			band->set_position(bpos[i] + Vector3(-0.20f + 0.40f * (float)b, 0.0f, 0.0f));
			band->set_rotation(Vector3(0.0f, 1.5707964f, 0.0f));
			band->set_scale(Vector3(1.0f, 1.0f, 1.0f));
			add_child(band);
		}
		// 雪帽
		Ref<BoxMesh> cap;
		cap.instantiate();
		cap->set_size(Vector3(0.84f, 0.06f, 0.72f));
		cap->set_material(meshutil::toon_material(toon, Color(0.90f, 0.95f, 1.0f), 0.2f));
		MeshInstance3D *ci = memnew(MeshInstance3D);
		ci->set_mesh(cap);
		ci->set_position(bpos[i] + Vector3(0.0f, 0.52f, 0.0f));
		ci->set_visible(false);
		ci->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(ci);
		snow_caps[i] = ci;
	}

	// 木叉斜插在草捆边
	Ref<CylinderMesh> handle;
	handle.instantiate();
	handle->set_top_radius(0.020f);
	handle->set_bottom_radius(0.024f);
	handle->set_height(1.45f);
	handle->set_radial_segments(6);
	handle->set_rings(1);
	handle->set_material(meshutil::toon_material(toon, Color(0.46f, 0.31f, 0.18f), 0.2f));
	MeshInstance3D *hm = memnew(MeshInstance3D);
	hm->set_mesh(handle);
	hm->set_position(Vector3(1.35f, 0.68f, 0.12f));
	hm->set_rotation(Vector3(0.0f, 0.0f, -0.32f));
	add_child(hm);
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.018f, 0.30f, 0.018f),
				Vector3(1.10f + 0.10f * (float)i, 1.34f, 0.12f), Color(0.58f, 0.58f, 0.56f),
				Vector3(0.0f, 0.0f, -0.32f));
	}

	// 草屑
	Ref<BoxMesh> bit;
	bit.instantiate();
	bit->set_size(Vector3(0.02f, 0.005f, 0.045f));
	Ref<StandardMaterial3D> bm;
	bm.instantiate();
	bm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bm->set_albedo(Color(0.88f, 0.74f, 0.38f));
	bm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	bit->set_material(bm);
	CPUParticles3D *motes = memnew(CPUParticles3D);
	motes->set_name("StrawMotes");
	motes->set_amount(14);
	motes->set_lifetime(4.0f);
	motes->set_mesh(bit);
	motes->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	motes->set_emission_box_extents(Vector3(1.2f, 0.4f, 0.8f));
	motes->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	motes->set_spread(70.0f);
	motes->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.01f);
	motes->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.06f);
	motes->set_gravity(Vector3(0.1f, -0.03f, 0.05f));
	motes->set_position(Vector3(0.0f, 0.7f, 0.0f));
	motes->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(motes);

	// 两只小雀
	for (int i = 0; i < BIRD_COUNT; i++) {
		Node3D *bird = memnew(Node3D);
		bird->set_name(String("Sparrow") + String::num_int64(i));
		add_child(bird);
		birds[i] = bird;
		bird_pos[i][0] = -0.2f + 1.2f * (float)i;
		bird_pos[i][1] = 1.1f + 0.3f * (float)i;
		bird->set_position(Vector3(bird_pos[i][0], 0.05f, bird_pos[i][1]));
		const Color brown(0.55f, 0.40f, 0.26f);
		MeshInstance3D *body = meshutil::add_sphere(bird, toon, 0.055f, Vector3(0.0f, 0.05f, 0.0f), brown);
		body->set_scale(Vector3(1.0f, 0.85f, 1.35f));
		meshutil::add_sphere(bird, toon, 0.035f, Vector3(0.0f, 0.10f, 0.055f), Color(0.62f, 0.48f, 0.33f));
		meshutil::add_box(bird, toon, Vector3(0.018f, 0.012f, 0.028f), Vector3(0.0f, 0.095f, 0.092f), Color(0.86f, 0.64f, 0.24f));
		meshutil::add_box(bird, toon, Vector3(0.10f, 0.008f, 0.05f), Vector3(0.0f, 0.035f, -0.075f), Color(0.42f, 0.30f, 0.20f), Vector3(-0.3f, 0.0f, 0.0f));
		for (int w = 0; w < 2; w++) {
			MeshInstance3D *wing = meshutil::add_box(bird, toon, Vector3(0.11f, 0.008f, 0.055f),
					Vector3((w == 0 ? -1.0f : 1.0f) * 0.055f, 0.06f, 0.0f), Color(0.48f, 0.34f, 0.22f));
			wings[i][w] = wing;
		}
	}
}
