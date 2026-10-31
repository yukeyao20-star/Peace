#include "icicle_field.h"

#include <cmath>

#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

int IcicleField::get_visible_count() const {
	int n = 0;
	for (const Icicle &i : icicles) {
		if (i.root != nullptr && i.root->is_visible()) {
			n++;
		}
	}
	return n;
}

bool IcicleField::get_drip_emitting() const {
	return drip != nullptr && drip->is_emitting();
}

void IcicleField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_icicle_count"), &IcicleField::get_icicle_count);
	ClassDB::bind_method(D_METHOD("get_visible_count"), &IcicleField::get_visible_count);
	ClassDB::bind_method(D_METHOD("get_drip_emitting"), &IcicleField::get_drip_emitting);
	ClassDB::bind_method(D_METHOD("get_drop_ready"), &IcicleField::get_drop_ready);
	ClassDB::bind_method(D_METHOD("get_grow"), &IcicleField::get_grow);
}

void IcicleField::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			build();
			make_drip();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			float target = Math::clamp((snow - 0.04f) / 0.55f, 0.0f, 1.0f);
			grow = Math::move_toward(grow, target, delta * (target > grow ? 0.10f : 0.22f));
			const bool melt = snow > 0.03f && snow < 0.40f && rain < 0.45f;
			for (Icicle &i : icicles) {
				if (i.root == nullptr) {
					continue;
				}
				float s = grow * i.max_len * (1.0f + 0.05f * std::sin(t * 0.7f + i.phase));
				i.root->set_visible(grow > 0.02f);
				i.root->set_scale(Vector3(1.0f, s, 1.0f));
			}
			if (ice_mat.is_valid()) {
				ice_mat->set_shader_parameter("albedo", Color(0.80f, 0.90f, 0.98f));
			}
			if (drip != nullptr) {
				drip->set_emitting(melt && grow > 0.08f);
			}
			if (melt) {
				drip_t -= delta;
				if (drip_t <= 0.0f) {
					if (voice != nullptr) {
						voice->set_global_position(get_global_position() + Vector3(0.0f, 2.6f, 0.0f));
						voice->set_pitch_scale(0.9f + (float)(rng() % 100u) * 0.003f);
						voice->set_volume_db(-20.0f);
						voice->play(0.0f);
					}
					drip_t = 3.5f + (float)(rng() % 500u) * 0.01f;
				}
			}
		} break;
		default:
			break;
	}
}

void IcicleField::build() {
	if (get_parent() == nullptr) {
		return;
	}
	Node3D *host = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Homestead"));
	if (host == nullptr) {
		return;
	}
	Ref<Shader> toon = meshutil::load_toon_shader();
	ice_mat = meshutil::toon_material(toon, Color(0.80f, 0.90f, 0.98f), 0.7f);

	Ref<CylinderMesh> cone;
	cone.instantiate();
	cone->set_top_radius(0.0f);
	cone->set_bottom_radius(0.035f);
	cone->set_height(1.0f);
	cone->set_radial_segments(6);
	cone->set_rings(1);
	cone->set_material(ice_mat);

	auto add_icicle = [&](const Vector3 &p, float len, float phase) {
		Icicle ic;
		ic.max_len = len;
		ic.phase = phase;
		ic.root = memnew(Node3D);
		ic.root->set_name(String("Icicle") + String::num_int64((int)icicles.size()));
		host->add_child(ic.root);
		// 冰凌从檐口往下挂：根在 p，向下长
		ic.root->set_position(p);
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(cone);
		mi->set_position(Vector3(0.0f, -0.5f, 0.0f)); // 圆锥尖端朝下
		mi->set_rotation(Vector3(3.1415926f, 0.0f, 0.0f));
		ic.root->add_child(mi);
		ic.root->set_visible(false);
		icicles.push_back(ic);
	};

	// 门廊屋檐前沿
	for (int i = 0; i < 11; i++) {
		float x = -1.85f + (float)i * 0.37f;
		float len = 0.22f + (float)(rng() % 100u) * 0.0035f;
		add_icicle(Vector3(x, 2.84f, 5.30f + (float)(rng() % 100u) * 0.0008f), len, (float)i * 0.7f);
	}
	// 左侧屋檐
	for (int i = 0; i < 4; i++) {
		float z = -2.4f + (float)i * 1.4f;
		float len = 0.25f + (float)(rng() % 100u) * 0.0030f;
		add_icicle(Vector3(-3.98f, 3.62f, z), len, 3.0f + (float)i);
	}

	// 融雪滴水
	Ref<SphereMesh> drop;
	drop.instantiate();
	drop->set_radius(0.022f);
	drop->set_height(0.055f);
	drop->set_radial_segments(6);
	drop->set_rings(4);
	Ref<StandardMaterial3D> drop_mat;
	drop_mat.instantiate();
	drop_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	drop_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	drop_mat->set_albedo(Color(0.78f, 0.90f, 1.0f, 0.75f));
	drop_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	drop->set_material(drop_mat);
	drip = memnew(CPUParticles3D);
	drip->set_name("MeltDrip");
	drip->set_amount(16);
	drip->set_lifetime(1.1f);
	drip->set_mesh(drop);
	drip->set_direction(Vector3(0.0f, -1.0f, 0.0f));
	drip->set_spread(8.0f);
	drip->set_gravity(Vector3(0.0f, -3.2f, 0.0f));
	drip->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.0f);
	drip->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.2f);
	drip->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	drip->set_emission_box_extents(Vector3(1.8f, 0.05f, 0.05f));
	drip->set_position(Vector3(0.0f, 2.8f, 5.3f));
	drip->set_emitting(false);
	host->add_child(drip);
}

void IcicleField::make_drip() {
	snd_drop = synth_drop();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("MeltDrop");
	voice->set_stream(snd_drop);
	voice->set_volume_db(-20.0f);
	voice->set_max_distance(20.0f);
	add_child(voice);
}

// 0.18 秒的水滴："嗒"一声，轻而圆
Ref<AudioStreamWAV> IcicleField::synth_drop() {
	const int sr = 22050;
	const float dur = 0.18f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float f = 1450.0f * std::exp(-18.0f * t) + 500.0f;
		float env = std::exp(-22.0f * t) * Math::clamp(t / 0.002f, 0.0f, 1.0f);
		float samp = std::sin(6.2831853f * f * t) * env * 0.6f;
		int16_t q = (int16_t)Math::clamp(samp * 12000.0f, -32000.0f, 32000.0f);
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
