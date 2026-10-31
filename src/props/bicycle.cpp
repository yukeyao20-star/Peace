#include "bicycle.h"
#include <cmath>
#include <random>
#include <vector>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"
#include "../world/terrain_builder.h"
using namespace godot;

namespace {
Basis bike_basis_y(const Vector3 &p_dir) {
	const Vector3 y = p_dir.normalized();
	Vector3 ref(0.0f, 0.0f, 1.0f);
	if (std::fabs(y.z) > 0.9f) ref = Vector3(1.0f, 0.0f, 0.0f);
	const Vector3 x = ref.cross(y).normalized();
	const Vector3 z = x.cross(y).normalized();
	Basis b;
	b.set_column(0, x);
	b.set_column(1, y);
	b.set_column(2, z);
	return b;
}
}

void Bicycle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Bicycle::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Bicycle::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Bicycle::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Bicycle::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Bicycle::interact);
	ClassDB::bind_method(D_METHOD("get_rings"), &Bicycle::get_rings);
	ClassDB::bind_method(D_METHOD("is_ringing"), &Bicycle::is_ringing);
}
String Bicycle::get_prompt() const { return String::utf8("按 E 拨一下车铃"); }
Vector3 Bicycle::get_interact_pos() const { return get_global_position() + Vector3(0.0f, 0.75f, 0.0f); }
void Bicycle::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	rings++;
	ring_t = 1.1f;
	if (voice != nullptr && snd.is_valid()) voice->play(0.0f);
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) gd->call("flash_hint", String::utf8("车铃叮的一声 · 车筐里的野花跟着晃"), 2.6f);
	}
}
void Bicycle::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (!Engine::get_singleton()->is_editor_hint()) { add_to_group("interactables"); build(); set_process(true); }
			break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (sky == nullptr && get_tree() != nullptr) sky = Object::cast_to<SkyCycle>(get_tree()->get_root()->find_child("SkyCycle", true, false));
			if (ring_t > 0.0f) ring_t = Math::max(0.0f, ring_t - delta);
			if (bell_pivot != nullptr) bell_pivot->set_rotation(Vector3(std::sin(t * 2.0f) * 0.03f, 0.0f, ring_t > 0.0f ? std::sin(ring_t * 42.0f) * 0.25f : 0.0f));
			const bool snow = sky != nullptr && sky->get_snow_amount() > 0.35f;
			if (snow_seat != nullptr) snow_seat->set_visible(snow);
			if (snow_basket != nullptr) snow_basket->set_visible(snow);
		} break;
		default: break;
	}
}
void Bicycle::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	if (get_tree() != nullptr) {
		TerrainBuilder *terrain = Object::cast_to<TerrainBuilder>(get_tree()->get_root()->find_child("Terrain", true, false));
		if (terrain != nullptr) { Vector3 p = get_global_position(); p.y = terrain->get_terrain_height(p.x, p.z) - 0.05f; set_global_position(p); }
	}
	set_rotation(Vector3(0.0f, -0.9f, 0.16f));
	const Color frame_col(0.56f, 0.30f, 0.24f);
	const Color tire_col(0.20f, 0.19f, 0.18f);
	const Color metal_col(0.62f, 0.62f, 0.60f);
	// 两只车轮：外胎 + 辐条
	Ref<TorusMesh> tire;
	tire.instantiate();
	tire->set_inner_radius(0.30f);
	tire->set_outer_radius(0.355f);
	tire->set_rings(16);
	tire->set_ring_segments(5);
	tire->set_material(meshutil::toon_material(toon, tire_col, 0.2f));
	for (int w = 0; w < 2; w++) {
		MeshInstance3D *wmi = memnew(MeshInstance3D);
		wmi->set_mesh(tire);
		wmi->set_position(Vector3(w == 0 ? -0.55f : 0.55f, 0.355f, 0.0f));
		wmi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
		add_child(wmi);
		meshutil::add_sphere(this, toon, 0.035f, Vector3(w == 0 ? -0.55f : 0.55f, 0.355f, 0.0f), metal_col);
	}
	Ref<BoxMesh> spoke_unit;
	spoke_unit.instantiate();
	spoke_unit->set_size(Vector3(0.008f, 0.66f, 0.008f));
	Ref<MultiMesh> spokes;
	spokes.instantiate();
	spokes->set_transform_format(MultiMesh::TRANSFORM_3D);
	spokes->set_mesh(spoke_unit);
	spokes->set_instance_count(12);
	int si = 0;
	for (int w = 0; w < 2; w++) {
		const float wx = w == 0 ? -0.55f : 0.55f;
		for (int s = 0; s < 6; s++) {
			const float a = (float)s * 1.0472f;
			spokes->set_instance_transform(si++, Transform3D(Basis(Vector3(0.0f, 0.0f, 1.0f), a), Vector3(wx, 0.355f, 0.0f)));
		}
	}
	MultiMeshInstance3D *smi = memnew(MultiMeshInstance3D);
	smi->set_name("Spokes");
	smi->set_multimesh(spokes);
	smi->set_material_override(meshutil::toon_material(toon, Color(0.55f, 0.55f, 0.53f), 0.2f));
	add_child(smi);
	// 车架管件
	struct Tube { Vector3 a; Vector3 b; float r; };
	const Tube tubes[] = {
		{ Vector3(-0.55f, 0.355f, 0.0f), Vector3(-0.06f, 0.30f, 0.0f), 0.020f }, // 后下叉
		{ Vector3(-0.55f, 0.355f, 0.0f), Vector3(-0.02f, 0.78f, 0.0f), 0.018f }, // 后上叉
		{ Vector3(-0.06f, 0.30f, 0.0f), Vector3(-0.02f, 0.80f, 0.0f), 0.026f },  // 立管
		{ Vector3(-0.02f, 0.80f, 0.0f), Vector3(0.40f, 0.76f, 0.0f), 0.023f },   // 上管
		{ Vector3(-0.06f, 0.30f, 0.0f), Vector3(0.40f, 0.76f, 0.0f), 0.023f },   // 下管
		{ Vector3(0.40f, 0.80f, 0.0f), Vector3(0.55f, 0.355f, 0.0f), 0.020f },   // 前叉
	};
	for (const Tube &tb : tubes) {
		const Vector3 d = tb.b - tb.a;
		Ref<CylinderMesh> cm;
		cm.instantiate();
		cm->set_top_radius(tb.r);
		cm->set_bottom_radius(tb.r);
		cm->set_height(d.length());
		cm->set_radial_segments(6);
		cm->set_rings(1);
		cm->set_material(meshutil::toon_material(toon, frame_col, 0.2f));
		MeshInstance3D *cmi = memnew(MeshInstance3D);
		cmi->set_mesh(cm);
		cmi->set_transform(Transform3D(bike_basis_y(d), (tb.a + tb.b) * 0.5f));
		add_child(cmi);
	}
	// 车座、车把、脚踏
	meshutil::add_box(this, toon, Vector3(0.20f, 0.05f, 0.12f), Vector3(-0.03f, 0.84f, 0.0f), Color(0.34f, 0.24f, 0.18f));
	meshutil::add_box(this, toon, Vector3(0.04f, 0.04f, 0.34f), Vector3(0.42f, 0.84f, 0.0f), Color(0.42f, 0.42f, 0.40f));
	meshutil::add_box(this, toon, Vector3(0.04f, 0.04f, 0.04f), Vector3(0.42f, 0.80f, 0.0f), Color(0.42f, 0.42f, 0.40f));
	meshutil::add_box(this, toon, Vector3(0.05f, 0.02f, 0.18f), Vector3(-0.02f, 0.30f, 0.0f), Color(0.35f, 0.35f, 0.34f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.02f, 0.03f), Vector3(-0.02f, 0.30f, 0.12f), Color(0.22f, 0.22f, 0.21f));
	meshutil::add_box(this, toon, Vector3(0.10f, 0.02f, 0.03f), Vector3(-0.02f, 0.30f, -0.12f), Color(0.22f, 0.22f, 0.21f));
	// 车铃
	bell_pivot = memnew(Node3D);
	bell_pivot->set_name("Bell");
	bell_pivot->set_position(Vector3(0.38f, 0.88f, 0.10f));
	add_child(bell_pivot);
	Ref<SphereMesh> bell;
	bell.instantiate();
	bell->set_radius(0.035f);
	bell->set_height(0.05f);
	bell->set_radial_segments(8);
	bell->set_rings(4);
	bell->set_material(meshutil::toon_material(toon, Color(0.80f, 0.68f, 0.34f), 0.15f));
	MeshInstance3D *bmi = memnew(MeshInstance3D);
	bmi->set_mesh(bell);
	bell_pivot->add_child(bmi);
	// 车筐 + 野花
	meshutil::add_box(this, toon, Vector3(0.28f, 0.06f, 0.24f), Vector3(0.50f, 0.66f, 0.0f), Color(0.62f, 0.48f, 0.28f));
	meshutil::add_box(this, toon, Vector3(0.28f, 0.14f, 0.02f), Vector3(0.50f, 0.72f, 0.11f), Color(0.58f, 0.44f, 0.26f));
	meshutil::add_box(this, toon, Vector3(0.28f, 0.14f, 0.02f), Vector3(0.50f, 0.72f, -0.11f), Color(0.58f, 0.44f, 0.26f));
	const Color flower_cols[3] = { Color(0.90f, 0.58f, 0.62f), Color(0.94f, 0.86f, 0.50f), Color(0.62f, 0.54f, 0.84f) };
	for (int i = 0; i < 3; i++) {
		meshutil::add_box(this, toon, Vector3(0.012f, 0.14f, 0.012f), Vector3(0.44f + 0.05f * (float)i, 0.78f, -0.05f + 0.05f * (float)i), Color(0.34f, 0.48f, 0.24f));
		meshutil::add_sphere(this, toon, 0.035f, Vector3(0.44f + 0.05f * (float)i, 0.86f, -0.05f + 0.05f * (float)i), flower_cols[i]);
	}
	// 撑脚
	meshutil::add_box(this, toon, Vector3(0.02f, 0.26f, 0.02f), Vector3(-0.30f, 0.14f, 0.12f), Color(0.42f, 0.42f, 0.40f), Vector3(0.25f, 0.0f, 0.12f));
	// 雪帽
	snow_seat = meshutil::add_box(this, toon, Vector3(0.21f, 0.02f, 0.13f), Vector3(-0.03f, 0.875f, 0.0f), Color(0.94f, 0.96f, 0.98f));
	snow_seat->set_visible(false);
	snow_basket = meshutil::add_box(this, toon, Vector3(0.29f, 0.025f, 0.25f), Vector3(0.50f, 0.80f, 0.0f), Color(0.94f, 0.96f, 0.98f));
	snow_basket->set_visible(false);
	build_sound();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_stream(snd);
	voice->set_unit_size(4.0f);
	voice->set_max_distance(14.0f);
	voice->set_position(Vector3(0.38f, 0.88f, 0.10f));
	add_child(voice);
}
void Bicycle::build_sound() {
	const int sr = 22050;
	const int n = sr * 7 / 10;
	std::vector<float> buf((size_t)n, 0.0f);
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float strike = tt < 0.05f ? tt / 0.05f : std::exp(-(tt - 0.05f) * 9.0f);
		const float env = std::exp(-tt * 5.5f);
		const float tone = std::sin(6.28318f * 2350.0f * tt) * 0.6f
				+ std::sin(6.28318f * 3170.0f * tt) * 0.3f
				+ std::sin(6.28318f * 4720.0f * tt) * 0.15f;
		buf[i] = tone * env * (0.6f + 0.4f * strike) * 0.5f;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 15000.0f, -15000.0f, 15000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	snd = wav;
}
