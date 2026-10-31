#include "beehive.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"

using namespace godot;

int Beehive::get_visible_bee_count() const {
	int n = 0;
	for (const Bee &b : bees) {
		if (b.root != nullptr && b.root->is_visible()) {
			n++;
		}
	}
	return n;
}

bool Beehive::get_buzz_playing() const {
	return buzz != nullptr && buzz->is_playing();
}

void Beehive::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bee_count"), &Beehive::get_bee_count);
	ClassDB::bind_method(D_METHOD("get_visible_bee_count"), &Beehive::get_visible_bee_count);
	ClassDB::bind_method(D_METHOD("get_buzz_ready"), &Beehive::get_buzz_ready);
	ClassDB::bind_method(D_METHOD("get_buzz_playing"), &Beehive::get_buzz_playing);
}

void Beehive::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
				TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain != nullptr) {
					Vector3 p = get_global_position();
					p.y = terrain->get_terrain_height(p.x, p.z) + 0.01f;
					set_global_position(p);
				}
			}
			hive_center = get_global_position() + Vector3(0.0f, 1.55f, 0.0f);
			build();
			snd_buzz = make_buzz();
			buzz = memnew(AudioStreamPlayer3D);
			buzz->set_name("BeeBuzz");
			buzz->set_stream(snd_buzz);
			buzz->set_volume_db(-40.0f);
			buzz->set_max_distance(18.0f);
			buzz->set_global_position(hive_center);
			add_child(buzz);
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float wet = (sky != nullptr) ? sky->get_rain_amount() + sky->get_snow_amount() : 0.0f;
			const bool active = night < 0.4f && wet < 0.35f;
			for (Bee &b : bees) {
				if (b.root == nullptr) {
					continue;
				}
				b.root->set_visible(active);
				if (!active) {
					continue;
				}
				b.phase += b.speed * delta;
				Vector3 p = hive_center + Vector3(
						std::cos(b.phase) * b.radius,
						b.lift + std::sin(t * 2.4f + b.phase) * 0.08f,
						std::sin(b.phase) * b.radius);
				b.root->set_global_position(p);
				b.root->set_rotation(Vector3(0.0f, -b.phase, 0.0f));
				float flap = std::sin(t * 42.0f + b.phase) * 0.9f;
				if (b.wing_l != nullptr) {
					b.wing_l->set_rotation(Vector3(0.0f, 0.0f, flap));
				}
				if (b.wing_r != nullptr) {
					b.wing_r->set_rotation(Vector3(0.0f, 0.0f, -flap));
				}
			}
			if (buzz != nullptr) {
				bool near = true;
				if (player != nullptr) {
					near = player->get_global_position().distance_to(hive_center) < 15.0f;
				}
				float target = (active && near) ? -26.0f : -60.0f;
				buzz->set_volume_db(Math::lerp((float)buzz->get_volume_db(), target, 1.0f - std::exp(-2.5f * delta)));
				if (active && near && !buzz->is_playing()) {
					buzz->play(0.0f);
				}
			}
		} break;
		default:
			break;
	}
}

void Beehive::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.40f, 0.27f, 0.14f);
	const Color honey(0.86f, 0.62f, 0.24f);
	const Color honey_dark(0.62f, 0.42f, 0.14f);
	const Color dark(0.12f, 0.10f, 0.08f);
	const Color bee_y(0.94f, 0.75f, 0.20f);

	// 木桩
	meshutil::add_box(this, toon, Vector3(0.12f, 1.85f, 0.12f), Vector3(0, 0.92f, 0), wood);
	meshutil::add_box(this, toon, Vector3(0.32f, 0.06f, 0.32f), Vector3(0, 1.45f, 0), wood);

	// 蜂巢
	Node3D *hive = memnew(Node3D);
	hive->set_name("Hive");
	hive->set_position(Vector3(0, 1.55f, 0));
	add_child(hive);
	MeshInstance3D *body = memnew(MeshInstance3D);
	Ref<SphereMesh> bm;
	bm.instantiate();
	bm->set_radius(0.34f);
	bm->set_height(0.68f);
	bm->set_radial_segments(14);
	bm->set_rings(8);
	bm->set_material(meshutil::toon_material(toon, honey, 0.4f));
	body->set_mesh(bm);
	body->set_scale(Vector3(1.0f, 1.08f, 1.0f));
	hive->add_child(body);
	for (int i = 0; i < 3; i++) {
		Ref<TorusMesh> ring;
		ring.instantiate();
		ring->set_inner_radius(0.26f + (float)i * 0.015f);
		ring->set_outer_radius(0.30f + (float)i * 0.015f);
		ring->set_rings(20);
		ring->set_ring_segments(4);
		ring->set_material(meshutil::toon_material(toon, honey_dark, 0.2f));
		MeshInstance3D *rm = memnew(MeshInstance3D);
		rm->set_mesh(ring);
		rm->set_position(Vector3(0, -0.18f + (float)i * 0.18f, 0));
		rm->set_scale(Vector3(1.0f, 1.0f, 1.0f));
		hive->add_child(rm);
	}
	meshutil::add_sphere(hive, toon, 0.055f, Vector3(0, -0.02f, 0.33f), dark);

	// 蜜蜂
	Ref<SphereMesh> bee_body;
	bee_body.instantiate();
	bee_body->set_radius(0.020f);
	bee_body->set_height(0.040f);
	bee_body->set_radial_segments(6);
	bee_body->set_rings(4);
	Ref<BoxMesh> wing;
	wing.instantiate();
	wing->set_size(Vector3(0.030f, 0.002f, 0.016f));
	for (int i = 0; i < 8; i++) {
		Bee b;
		b.phase = (float)(rng() % 628u) * 0.01f;
		b.radius = 0.45f + (float)(rng() % 100u) * 0.007f;
		b.speed = 1.8f + (float)(rng() % 100u) * 0.012f;
		b.lift = (float)(rng() % 100u) * 0.006f - 0.25f;
		b.root = memnew(Node3D);
		b.root->set_name(String("Bee") + String::num_int64(i));
		add_child(b.root);
		MeshInstance3D *bd = memnew(MeshInstance3D);
		bd->set_mesh(bee_body);
		bd->set_material_override(meshutil::toon_material(toon, bee_y, 0.3f));
		bd->set_scale(Vector3(1.0f, 0.9f, 1.5f));
		b.root->add_child(bd);
		MeshInstance3D *stripe = memnew(MeshInstance3D);
		Ref<SphereMesh> sm;
		sm.instantiate();
		sm->set_radius(0.019f);
		sm->set_height(0.038f);
		sm->set_radial_segments(6);
		sm->set_rings(3);
		stripe->set_mesh(sm);
		stripe->set_material_override(meshutil::toon_material(toon, dark));
		stripe->set_position(Vector3(0, 0, -0.008f));
		stripe->set_scale(Vector3(1.0f, 0.9f, 0.45f));
		b.root->add_child(stripe);
		b.wing_l = memnew(MeshInstance3D);
		b.wing_l->set_mesh(wing);
		b.wing_l->set_position(Vector3(-0.026f, 0.012f, 0));
		b.root->add_child(b.wing_l);
		b.wing_r = memnew(MeshInstance3D);
		b.wing_r->set_mesh(wing);
		b.wing_r->set_position(Vector3(0.026f, 0.012f, 0));
		b.root->add_child(b.wing_r);
		b.root->set_visible(false);
		bees.push_back(b);
	}
}

// 3 秒循环的低频蜂鸣，靠近才会明显
Ref<AudioStreamWAV> Beehive::make_buzz() {
	const int sr = 22050;
	const float dur = 3.0f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float am = 0.55f + 0.45f * std::sin(6.2831853f * 9.0f * t);
		float carrier = std::sin(6.2831853f * 185.0f * t) * 0.5f
				+ std::sin(6.2831853f * 370.0f * t) * 0.22f
				+ std::sin(6.2831853f * 555.0f * t) * 0.10f;
		float fade = 1.0f;
		if (i < 1800) {
			fade = (float)i / 1800.0f;
		}
		if (i > n - 1800) {
			fade = (float)(n - i) / 1800.0f;
		}
		float samp = carrier * am * fade * 0.5f;
		int16_t q = (int16_t)Math::clamp(samp * 9000.0f, -32000.0f, 32000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
	wav->set_loop_end(n);
	wav->set_data(bytes);
	return wav;
}
