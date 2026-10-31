#include "wind_charms.h"

#include <cmath>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

float WindCharms::get_vane_rotation() const {
	return vane != nullptr ? (float)vane->get_rotation().y : 0.0f;
}

int WindCharms::get_tube_count() const {
	int n = 0;
	for (int i = 0; i < 5; i++) {
		if (tube[i] != nullptr) {
			n++;
		}
	}
	return n;
}

bool WindCharms::get_chime_playing() const {
	return voice != nullptr && voice->is_playing();
}

int WindCharms::get_bulb_count() const {
	int n = 0;
	for (int i = 0; i < 9; i++) {
		if (bulb[i] != nullptr && bulb[i]->is_visible()) {
			n++;
		}
	}
	return n;
}

float WindCharms::get_bulb_energy() const {
	return bulb_mat.is_valid() ? (float)bulb_mat->get("emission_energy_multiplier") : 0.0f;
}

void WindCharms::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_wind"), &WindCharms::get_wind);
	ClassDB::bind_method(D_METHOD("get_vane_rotation"), &WindCharms::get_vane_rotation);
	ClassDB::bind_method(D_METHOD("get_tube_count"), &WindCharms::get_tube_count);
	ClassDB::bind_method(D_METHOD("get_chime_ready"), &WindCharms::get_chime_ready);
	ClassDB::bind_method(D_METHOD("get_chime_playing"), &WindCharms::get_chime_playing);
	ClassDB::bind_method(D_METHOD("get_bulb_count"), &WindCharms::get_bulb_count);
	ClassDB::bind_method(D_METHOD("get_bulb_energy"), &WindCharms::get_bulb_energy);
}

void WindCharms::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			build();
			snd_chime = make_chime();
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("WindChimeVoice");
			voice->set_stream(snd_chime);
			voice->set_volume_db(-22.0f);
			voice->set_max_distance(28.0f);
			add_child(voice);
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			float target_wind = 0.25f + rain * 0.7f + snow * 0.3f + 0.12f * std::sin(t * 0.17f);
			wind = Math::lerp(wind, Math::clamp(target_wind, 0.1f, 1.0f), 1.0f - std::exp(-0.8f * delta));
			if (vane != nullptr) {
				wind_dir = 0.55f * std::sin(t * 0.075f) + 0.30f * std::sin(t * 0.19f + 1.1f)
						+ rain * 0.35f * std::sin(t * 0.45f);
				float cur = (float)vane->get_rotation().y;
				float next = Math::lerp(cur, wind_dir, 1.0f - std::exp(-1.6f * delta));
				vane->set_rotation(Vector3(0.0f, next, 0.0f));
			}
			if (chime != nullptr) {
				float amp = 0.03f + wind * 0.075f;
				chime->set_rotation(Vector3(
						std::sin(t * 1.9f) * amp * 0.7f,
						0.0f,
						std::sin(t * 1.35f + 0.6f) * amp));
			}
			for (int i = 0; i < 5; i++) {
				if (tube[i] == nullptr) {
					continue;
				}
				float a = std::sin(t * (2.4f + (float)i * 0.13f) + (float)i * 1.3f) * (0.02f + wind * 0.06f);
				tube[i]->set_rotation(Vector3(0.0f, 0.0f, a));
			}
			if (bulb_mat.is_valid()) {
				float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
				float on = Math::clamp((night - 0.22f) / 0.4f, 0.0f, 1.0f);
				bulb_mat->set("emission_energy_multiplier", 0.22f + 2.0f * on);
				for (int i = 0; i < 9; i++) {
					if (bulb_pivot[i] == nullptr) {
						continue;
					}
					bulb_pivot[i]->set_rotation(Vector3(
							std::sin(t * 1.3f + (float)i * 0.9f) * wind * 0.10f,
							0.0f,
							std::sin(t * 1.05f + (float)i * 1.4f) * wind * 0.08f));
				}
			}
			chime_t -= delta;
			if (chime_t <= 0.0f) {
				bool near = true;
				if (get_parent() != nullptr) {
					Node3D *p = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
					if (p != nullptr) {
						near = p->get_global_position().distance_to(get_global_position()) < 26.0f;
					}
				}
				if (near && voice != nullptr && wind > 0.28f && !voice->is_playing()) {
					voice->set_global_position(get_global_position() + Vector3(0.0f, 2.6f, 0.0f));
					voice->set_pitch_scale(0.95f + (float)(rng() % 100u) * 0.0016f);
					voice->set_volume_db(-22.0f - 6.0f * (1.0f - wind));
					voice->play(0.0f);
				}
				chime_t = 2.5f + (float)(rng() % 500u) * 0.012f + (1.0f - wind) * 4.0f;
			}
		} break;
		default:
			break;
	}
}

void WindCharms::build() {
	if (get_parent() == nullptr) {
		return;
	}
	Node3D *host = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Homestead"));
	if (host == nullptr) {
		return;
	}
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color iron(0.26f, 0.28f, 0.30f);
	const Color iron_light(0.42f, 0.45f, 0.48f);
	const Color wood(0.36f, 0.24f, 0.13f);

	// —— 风向标 ——
	const Vector3 vane_base(-1.6f, 2.92f, 4.9f);
	meshutil::add_box(host, toon, Vector3(0.045f, 0.55f, 0.045f), vane_base + Vector3(0, 0.27f, 0), iron);
	meshutil::add_box(host, toon, Vector3(0.62f, 0.025f, 0.025f), vane_base + Vector3(0, 0.55f, 0), iron_light);
	meshutil::add_box(host, toon, Vector3(0.025f, 0.025f, 0.62f), vane_base + Vector3(0, 0.55f, 0), iron_light);
	vane = memnew(Node3D);
	vane->set_name("Vane");
	vane->set_position(vane_base + Vector3(0, 0.62f, 0));
	host->add_child(vane);
	meshutil::add_box(vane, toon, Vector3(0.030f, 0.03f, 0.78f), Vector3(0, 0, 0), iron);
	meshutil::add_box(vane, toon, Vector3(0.24f, 0.03f, 0.14f), Vector3(0, 0, -0.33f), iron_light);
	// 箭头（小圆锥）
	Ref<CylinderMesh> tip;
	tip.instantiate();
	tip->set_top_radius(0.0f);
	tip->set_bottom_radius(0.055f);
	tip->set_height(0.18f);
	tip->set_radial_segments(6);
	tip->set_rings(1);
	tip->set_material(meshutil::toon_material(toon, iron_light));
	MeshInstance3D *tip_mi = memnew(MeshInstance3D);
	tip_mi->set_mesh(tip);
	tip_mi->set_position(Vector3(0.0f, 0.0f, 0.43f));
	tip_mi->set_rotation(Vector3(1.5707964f, 0.0f, 0.0f));
	vane->add_child(tip_mi);

	// —— 门廊风铃 ——
	chime = memnew(Node3D);
	chime->set_name("PorchChime");
	chime->set_position(Vector3(1.55f, 2.62f, 4.95f));
	host->add_child(chime);
	meshutil::add_box(chime, toon, Vector3(0.02f, 0.22f, 0.02f), Vector3(0, 0.11f, 0), wood);
	meshutil::add_box(chime, toon, Vector3(0.46f, 0.035f, 0.05f), Vector3(0, 0.0f, 0), wood);
	const Color metal[5] = {
		Color(0.82f, 0.72f, 0.48f),
		Color(0.75f, 0.78f, 0.82f),
		Color(0.88f, 0.78f, 0.58f),
		Color(0.68f, 0.74f, 0.80f),
		Color(0.90f, 0.82f, 0.62f),
	};
	for (int i = 0; i < 5; i++) {
		float x = -0.17f + (float)i * 0.085f;
		float len = 0.24f + (float)(i % 3) * 0.07f;
		meshutil::add_box(chime, toon, Vector3(0.006f, len, 0.006f), Vector3(x, -len * 0.5f, 0), iron);
		Ref<CylinderMesh> bar;
		bar.instantiate();
		bar->set_top_radius(0.018f);
		bar->set_bottom_radius(0.022f);
		bar->set_height(0.20f + (float)(i % 3) * 0.05f);
		bar->set_radial_segments(6);
		bar->set_rings(1);
		bar->set_material(meshutil::toon_material(toon, metal[i], 0.6f));
		tube[i] = memnew(Node3D);
		tube[i]->set_name(String("ChimeTube") + String::num_int64(i));
		tube[i]->set_position(Vector3(x, -len, 0));
		chime->add_child(tube[i]);
		MeshInstance3D *bm = memnew(MeshInstance3D);
		bm->set_mesh(bar);
		bm->set_position(Vector3(0, -0.10f - (float)(i % 3) * 0.025f, 0));
		tube[i]->add_child(bm);
	}

	// —— 门廊小串灯：两根柱子之间一条下垂的灯线 ——
	const Vector3 lp(-1.6f, 2.64f, 4.9f);
	const Vector3 rp(1.6f, 2.64f, 4.9f);
	for (int i = 0; i < 10; i++) {
		float u0 = (float)i / 10.0f;
		float u1 = (float)(i + 1) / 10.0f;
		Vector3 a = lp.lerp(rp, u0);
		Vector3 b = lp.lerp(rp, u1);
		a.y -= std::sin(u0 * 3.1415926f) * 0.22f;
		b.y -= std::sin(u1 * 3.1415926f) * 0.22f;
		Vector3 mid = (a + b) * 0.5f;
		Vector3 dir = b - a;
		float len = dir.length();
		MeshInstance3D *seg = memnew(MeshInstance3D);
		Ref<BoxMesh> segm;
		segm.instantiate();
		segm->set_size(Vector3(len, 0.008f, 0.008f));
		segm->set_material(meshutil::toon_material(toon, Color(0.20f, 0.20f, 0.18f)));
		seg->set_mesh(segm);
		seg->set_position(mid);
		seg->set_rotation(Vector3(0.0f, 0.0f, std::atan2(dir.y, dir.x)));
		host->add_child(seg);
	}
	bulb_mat.instantiate();
	bulb_mat->set_albedo(Color(1.0f, 0.86f, 0.55f));
	bulb_mat->set("emission_enabled", true);
	bulb_mat->set("emission", Color(1.0f, 0.72f, 0.36f));
	bulb_mat->set("emission_energy_multiplier", 0.22f);
	Ref<SphereMesh> bulb_mesh;
	bulb_mesh.instantiate();
	bulb_mesh->set_radius(0.035f);
	bulb_mesh->set_height(0.07f);
	bulb_mesh->set_radial_segments(8);
	bulb_mesh->set_rings(5);
	bulb_mesh->set_material(Ref<Material>(bulb_mat));
	for (int i = 0; i < 9; i++) {
		float u = (float)(i + 1) / 10.0f;
		Vector3 p = lp.lerp(rp, u);
		p.y -= std::sin(u * 3.1415926f) * 0.22f;
		bulb_pivot[i] = memnew(Node3D);
		bulb_pivot[i]->set_name(String("StringBulb") + String::num_int64(i));
		bulb_pivot[i]->set_position(p);
		host->add_child(bulb_pivot[i]);
		bulb[i] = memnew(MeshInstance3D);
		bulb[i]->set_mesh(bulb_mesh);
		bulb[i]->set_position(Vector3(0.0f, -0.07f, 0.0f));
		bulb_pivot[i]->add_child(bulb[i]);
	}
}

// 1.2 秒的五声风铃，音高像随意碰撞
Ref<AudioStreamWAV> WindCharms::make_chime() {
	const int sr = 22050;
	const float dur = 1.3f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	const float notes[5] = {1318.5f, 1568.0f, 1760.0f, 1975.5f, 2349.3f};
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int k = 0; k < 5; k++) {
			float start = (float)k * 0.12f;
			float bt = t - start;
			if (bt < 0.0f) {
				continue;
			}
			float env = std::exp(-4.2f * bt) * Math::clamp(bt / 0.006f, 0.0f, 1.0f);
			samp += std::sin(6.2831853f * notes[k] * bt) * env * 0.20f;
			samp += std::sin(6.2831853f * notes[k] * 2.0f * bt) * env * 0.05f;
		}
		int16_t q = (int16_t)Math::clamp(samp * 14000.0f, -32000.0f, 32000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}
