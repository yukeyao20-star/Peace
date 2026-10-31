#include "stew_bowl.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"

using namespace godot;

void StewBowl::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &StewBowl::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &StewBowl::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &StewBowl::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &StewBowl::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &StewBowl::interact);
	ClassDB::bind_method(D_METHOD("get_bowls_left"), &StewBowl::get_bowls_left);
	ClassDB::bind_method(D_METHOD("get_holding"), &StewBowl::get_holding);
	ClassDB::bind_method(D_METHOD("get_held_visible"), &StewBowl::get_held_visible);
	ClassDB::bind_method(D_METHOD("get_sip_ready"), &StewBowl::get_sip_ready);
}

bool StewBowl::get_held_visible() const {
	return held != nullptr && held->is_visible();
}

String StewBowl::get_prompt() const {
	if (holding) {
		return String::utf8("按 E 喝掉热汤");
	}
	if (bowls_left > 0) {
		return String::utf8("按 E 盛一碗热汤");
	}
	return String::utf8("按 E 看看空碗");
}

Vector3 StewBowl::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.55f, 0.0f);
}

void StewBowl::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	if (holding) {
		holding = false;
		held_t = 0.0f;
		if (held != nullptr) {
			held->set_visible(false);
		}
		if (held_steam != nullptr) {
			held_steam->set_emitting(false);
		}
		if (sip_puff != nullptr) {
			sip_puff->restart();
		}
		if (voice != nullptr && snd_sip.is_valid()) {
			voice->set_global_position(get_global_position());
			voice->set_pitch_scale(0.95f + 0.05f * (float)(t * 7.0f - std::floor(t * 7.0f)));
			voice->play(0.0f);
		}
		if (get_parent() != nullptr) {
			Node *gd = get_parent()->get_node_or_null("GameDirector");
			if (gd != nullptr) {
				gd->call("flash_hint", String::utf8("热汤下肚 · 从胃里暖到指尖"), 3.2f);
				gd->call("on_soup_drunk");
			}
		}
		return;
	}
	if (bowls_left <= 0) {
		if (get_parent() != nullptr) {
			Node *gd = get_parent()->get_node_or_null("GameDirector");
			if (gd != nullptr) {
				gd->call("flash_hint", String::utf8("碗空了 · 锅边过一会儿会再补满"), 2.6f);
			}
		}
		return;
	}
	bowls_left--;
	if (bowls[bowls_left] != nullptr) {
		bowls[bowls_left]->set_visible(false);
	}
	holding = true;
	held_t = 0.0f;
	refill_t = 0.0f;
	if (held != nullptr) {
		held->set_visible(true);
	}
	if (held_steam != nullptr) {
		held_steam->restart();
		held_steam->set_emitting(true);
	}
	if (get_parent() != nullptr) {
		Node *gd = get_parent()->get_node_or_null("GameDirector");
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8("端稳一碗热汤 · 再按 E 喝掉"), 3.0f);
		}
	}
}

void StewBowl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			if (player == nullptr && get_parent() != nullptr) {
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			if (holding) {
				held_t += delta;
				if (player != nullptr && held != nullptr) {
					Player *pl = Object::cast_to<Player>(player);
					Vector3 f = (pl != nullptr) ? pl->get_facing() : Vector3(0, 0, 1);
					f.y = 0.0f;
					if (f.length_squared() < 0.01f) {
						f = Vector3(0, 0, 1);
					}
					f = f.normalized();
					const Vector3 hand = player->get_global_position() + f * 0.52f + Vector3(0.0f, 1.06f, 0.0f);
					held->set_global_position(hand + Vector3(0.0f, 0.012f * std::sin(t * 3.4f), 0.0f));
				}
				// 走远：汤洒了，碗放回凳上
				if (player != nullptr && player->get_global_position().distance_to(get_global_position()) > 6.0f) {
					holding = false;
					if (held != nullptr) {
						held->set_visible(false);
					}
					if (held_steam != nullptr) {
						held_steam->set_emitting(false);
					}
					if (bowls_left < 3 && bowls[bowls_left] != nullptr) {
						bowls[bowls_left]->set_visible(true);
						bowls_left++;
					}
					if (get_parent() != nullptr) {
						Node *gd = get_parent()->get_node_or_null("GameDirector");
						if (gd != nullptr) {
							gd->call("flash_hint", String::utf8("走太急，汤洒了……"), 2.4f);
						}
					}
				}
			} else if (bowls_left < 3) {
				refill_t += delta;
				if (refill_t > 90.0f) {
					refill_t = 0.0f;
					bowls_left = 3;
					for (int i = 0; i < 3; i++) {
						if (bowls[i] != nullptr) {
							bowls[i]->set_visible(true);
						}
					}
				}
			}
		} break;
		default:
			break;
	}
}

void StewBowl::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.44f, 0.31f, 0.18f);
	const Color bowl_col(0.62f, 0.46f, 0.27f);
	const Color soup_col(0.78f, 0.42f, 0.12f);

	// 小木凳
	meshutil::add_box(this, toon, Vector3(0.10f, 0.36f, 0.10f), Vector3(0.0f, 0.18f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.46f, 0.05f, 0.46f), Vector3(0.0f, 0.38f, 0.0f), Color(0.54f, 0.39f, 0.23f));

	// 三只叠着的木碗（最上面那只有汤）
	for (int i = 0; i < 3; i++) {
		Node3D *b = memnew(Node3D);
		b->set_name(String("Bowl") + String::num_int64(i));
		b->set_position(Vector3(0.0f, 0.42f + 0.048f * (float)i, 0.0f));
		add_child(b);
		Ref<CylinderMesh> bowl;
		bowl.instantiate();
		bowl->set_top_radius(0.155f);
		bowl->set_bottom_radius(0.095f);
		bowl->set_height(0.06f);
		bowl->set_radial_segments(14);
		bowl->set_rings(1);
		bowl->set_material(meshutil::toon_material(toon, bowl_col, 0.2f));
		MeshInstance3D *bmi = memnew(MeshInstance3D);
		bmi->set_mesh(bowl);
		b->add_child(bmi);
		if (i == 2) {
			Ref<CylinderMesh> soup;
			soup.instantiate();
			soup->set_top_radius(0.13f);
			soup->set_bottom_radius(0.13f);
			soup->set_height(0.012f);
			soup->set_radial_segments(14);
			soup->set_rings(1);
			Ref<StandardMaterial3D> sm;
			sm.instantiate();
			sm->set_albedo(soup_col);
			sm->set_roughness(0.35f);
			soup->set_material(sm);
			MeshInstance3D *smi = memnew(MeshInstance3D);
			smi->set_mesh(soup);
			smi->set_position(Vector3(0.0f, 0.030f, 0.0f));
			b->add_child(smi);
		}
		bowls[i] = b;
	}

	// 端在手里的一碗
	held = memnew(Node3D);
	held->set_name("HeldBowl");
	held->set_visible(false);
	add_child(held);
	Ref<CylinderMesh> hb;
	hb.instantiate();
	hb->set_top_radius(0.155f);
	hb->set_bottom_radius(0.095f);
	hb->set_height(0.06f);
	hb->set_radial_segments(14);
	hb->set_rings(1);
	hb->set_material(meshutil::toon_material(toon, bowl_col, 0.2f));
	MeshInstance3D *hmi = memnew(MeshInstance3D);
	hmi->set_mesh(hb);
	held->add_child(hmi);
	Ref<CylinderMesh> hs;
	hs.instantiate();
	hs->set_top_radius(0.13f);
	hs->set_bottom_radius(0.13f);
	hs->set_height(0.012f);
	hs->set_radial_segments(14);
	Ref<StandardMaterial3D> hsm;
	hsm.instantiate();
	hsm->set_albedo(soup_col);
	hsm->set_roughness(0.35f);
	hs->set_material(hsm);
	MeshInstance3D *hsmi = memnew(MeshInstance3D);
	hsmi->set_mesh(hs);
	hsmi->set_position(Vector3(0.0f, 0.030f, 0.0f));
	held->add_child(hsmi);

	// 端碗时升起的热气
	Ref<SphereMesh> puff;
	puff.instantiate();
	puff->set_radius(0.035f);
	puff->set_height(0.07f);
	puff->set_radial_segments(6);
	puff->set_rings(3);
	Ref<StandardMaterial3D> pm;
	pm.instantiate();
	pm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	pm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	pm->set_albedo(Color(0.92f, 0.94f, 0.93f, 0.16f));
	pm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	puff->set_material(pm);
	held_steam = memnew(CPUParticles3D);
	held_steam->set_name("HeldSteam");
	held_steam->set_amount(10);
	held_steam->set_lifetime(1.6f);
	held_steam->set_mesh(puff);
	held_steam->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	held_steam->set_spread(16.0f);
	held_steam->set_gravity(Vector3(0.02f, 0.10f, 0.0f));
	held_steam->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.08f);
	held_steam->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.18f);
	held_steam->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	held_steam->set_emission_sphere_radius(0.07f);
	held_steam->set_position(Vector3(0.0f, 0.06f, 0.0f));
	held_steam->set_emitting(false);
	held->add_child(held_steam);

	// 喝汤时脸前的一小团热气
	sip_puff = memnew(CPUParticles3D);
	sip_puff->set_name("SipPuff");
	sip_puff->set_amount(8);
	sip_puff->set_lifetime(0.9f);
	sip_puff->set_one_shot(true);
	sip_puff->set_mesh(puff);
	sip_puff->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	sip_puff->set_spread(32.0f);
	sip_puff->set_gravity(Vector3(0.0f, 0.12f, 0.0f));
	sip_puff->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	sip_puff->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.24f);
	sip_puff->set_emitting(false);
	add_child(sip_puff);

	snd_sip = make_sip();
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("SipVoice");
	voice->set_stream(snd_sip);
	voice->set_volume_db(-9.0f);
	voice->set_max_distance(10.0f);
	add_child(voice);
}

// 0.35 秒的啜汤声：一记低沉的"咕咚"加一点气声。
Ref<AudioStreamWAV> StewBowl::make_sip() {
	const int sr = 22050;
	const int n = (int)(0.35f * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::mt19937 rng(24680u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float phase = 0.0f;
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tsec = (float)i / (float)sr;
		const float env = std::exp(-tsec * 9.0f) * Math::clamp(tsec / 0.01f, 0.0f, 1.0f);
		const float freq = 190.0f - 70.0f * (tsec / 0.35f);
		phase += 6.2831853f * freq / (float)sr;
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.10f;
		float samp = (std::sin(phase) * 0.8f + lp * 1.2f) * env;
		samp = Math::clamp(samp, -1.0f, 1.0f);
		const int16_t q = (int16_t)(samp * 9000.0f);
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
