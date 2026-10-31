#include "bookshelf.h"

#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../systems/mesh_util.h"

using namespace godot;

void Bookshelf::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Bookshelf::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Bookshelf::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Bookshelf::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Bookshelf::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Bookshelf::interact);
	ClassDB::bind_method(D_METHOD("get_book_count"), &Bookshelf::get_book_count);
	ClassDB::bind_method(D_METHOD("get_read_count"), &Bookshelf::get_read_count);
	ClassDB::bind_method(D_METHOD("get_last_index"), &Bookshelf::get_last_index);
}

String Bookshelf::get_prompt() const {
	return String::utf8("按 E 抽一本书读读");
}

Vector3 Bookshelf::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 1.15f, 0.25f);
}

void Bookshelf::interact(const Vector3 &p_player_pos) {
	(void)p_player_pos;
	static const char *passages[6] = {
		"「雨落在屋檐上，像有人在很远的地方翻书。」",
		"「湖面结冰的那几天，连时间也走得慢一些。」",
		"「火快熄的时候，添一根柴，屋子就又亮起来。」",
		"「猫总在同一个位置打盹，好像那里藏着太阳。」",
		"「远行的人寄来一张明信片，只写了：这里也很好。」",
		"「把今天过完，明天还会有新的风从湖上吹来。」",
	};
	last_index = read_count % 6; // 顺序轮换，读久了也不重复
	read_count++;
	if (get_tree() != nullptr) {
		Node *gd = get_tree()->get_root()->find_child("GameDirector", true, false);
		if (gd != nullptr) {
			gd->call("flash_hint", String::utf8(passages[last_index]), 4.2f);
		}
	}
	if (page_voice != nullptr && snd_page.is_valid()) {
		page_voice->set_pitch_scale(0.96f + 0.03f * (float)(read_count % 4));
		page_voice->play();
	}
}

void Bookshelf::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
			build();
		} break;
		default:
			break;
	}
}

void Bookshelf::build() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.42f, 0.28f, 0.16f);
	const Color wood_light(0.55f, 0.38f, 0.22f);

	// 框体与三层隔板
	meshutil::add_box(this, toon, Vector3(0.06f, 1.72f, 0.34f), Vector3(-0.80f, 0.86f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(0.06f, 1.72f, 0.34f), Vector3(0.80f, 0.86f, 0.0f), wood);
	meshutil::add_box(this, toon, Vector3(1.66f, 0.06f, 0.34f), Vector3(0.0f, 1.72f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(1.66f, 0.06f, 0.34f), Vector3(0.0f, 0.03f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(1.60f, 0.035f, 0.32f), Vector3(0.0f, 1.16f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(1.60f, 0.035f, 0.32f), Vector3(0.0f, 0.60f, 0.0f), wood_light);
	meshutil::add_box(this, toon, Vector3(1.60f, 0.02f, 0.30f), Vector3(0.0f, 0.88f, -0.10f), Color(0.30f, 0.20f, 0.12f));

	// 三层书
	const Color palette[8] = {
		Color(0.55f, 0.24f, 0.20f),
		Color(0.24f, 0.36f, 0.48f),
		Color(0.40f, 0.46f, 0.28f),
		Color(0.68f, 0.54f, 0.30f),
		Color(0.36f, 0.28f, 0.42f),
		Color(0.60f, 0.40f, 0.26f),
		Color(0.30f, 0.42f, 0.40f),
		Color(0.70f, 0.62f, 0.46f),
	};
	std::mt19937 rng(81231u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	const float shelf_y[3] = { 0.063f, 0.623f, 1.183f };
	for (int row = 0; row < 3; row++) {
		float x = -0.70f;
		for (int b = 0; b < 9; b++) {
			const float w = 0.035f + 0.030f * d01(rng);
			const float h = 0.26f + 0.12f * d01(rng);
			const float tilt = (d01(rng) < 0.22f) ? (d01(rng) - 0.5f) * 0.20f : 0.0f;
			const Color col = palette[(int)(d01(rng) * 8.0f) % 8];
			meshutil::add_box(this, toon, Vector3(w, h, 0.20f),
					Vector3(x + w * 0.5f, shelf_y[row] + h * 0.5f, 0.01f), col,
					Vector3(0.0f, 0.0f, tilt));
			// 书脊上的一道浅色横线
			if (d01(rng) < 0.45f) {
				meshutil::add_box(this, toon, Vector3(w * 0.8f, 0.012f, 0.005f),
						Vector3(x + w * 0.5f, shelf_y[row] + h * 0.72f, 0.112f),
						Color(0.90f, 0.84f, 0.66f));
			}
			x += w + 0.012f + 0.01f * d01(rng);
		}
	}

	// 书架边的细小尘光
	Ref<SphereMesh> mote;
	mote.instantiate();
	mote->set_radius(0.008f);
	mote->set_height(0.016f);
	mote->set_radial_segments(4);
	mote->set_rings(2);
	Ref<StandardMaterial3D> mm;
	mm.instantiate();
	mm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mm->set_albedo(Color(1.0f, 0.92f, 0.70f, 0.30f));
	mm->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	mote->set_material(mm);
	motes = memnew(CPUParticles3D);
	motes->set_name("DustMotes");
	motes->set_amount(12);
	motes->set_lifetime(6.0f);
	motes->set_mesh(mote);
	motes->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	motes->set_emission_box_extents(Vector3(0.75f, 0.75f, 0.20f));
	motes->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	motes->set_spread(60.0f);
	motes->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.005f);
	motes->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.025f);
	motes->set_gravity(Vector3(0.0f, 0.004f, 0.0f));
	motes->set_position(Vector3(0.0f, 0.9f, 0.25f));
	motes->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(motes);

	// 纸页翻动声
	build_page_sound();
	page_voice = memnew(AudioStreamPlayer);
	page_voice->set_name("PageVoice");
	page_voice->set_stream(snd_page);
	page_voice->set_volume_db(-13.0f);
	add_child(page_voice);
}

void Bookshelf::build_page_sound() {
	const int sr = 22050;
	const int n = (int)(0.34f * (float)sr);
	std::vector<float> buf((size_t)n, 0.0f);
	std::mt19937 rng(5150u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float lp = 0.0f;
	for (int i = 0; i < n; i++) {
		const float tt = (float)i / (float)sr;
		const float white = d01(rng) - 0.5f;
		lp += (white - lp) * 0.22f;
		const float hp = white - lp;
		// 两下翻页的沙沙
		const float env = std::exp(-std::fabs(tt - 0.10f) * 38.0f) + 0.65f * std::exp(-std::fabs(tt - 0.22f) * 46.0f);
		buf[i] = hp * env * 0.55f;
	}
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		const int16_t q = (int16_t)Math::clamp(buf[i] * 26000.0f, -26000.0f, 26000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	snd_page = wav;
}
