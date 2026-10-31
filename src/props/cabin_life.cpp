#include "cabin_life.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void CabinLife::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_window_glow_energy", "index"), &CabinLife::get_window_glow_energy);
	ClassDB::bind_method(D_METHOD("get_porch_energy"), &CabinLife::get_porch_energy);
	ClassDB::bind_method(D_METHOD("get_smoke_enabled"), &CabinLife::get_smoke_enabled);
	ClassDB::bind_method(D_METHOD("get_smoke_amount"), &CabinLife::get_smoke_amount);
	ClassDB::bind_method(D_METHOD("get_roof_snow_level"), &CabinLife::get_roof_snow_level);
	ClassDB::bind_method(D_METHOD("get_frost_level"), &CabinLife::get_frost_level);
	ClassDB::bind_method(D_METHOD("get_icicle_level"), &CabinLife::get_icicle_level);
	ClassDB::bind_method(D_METHOD("get_icicle_count"), &CabinLife::get_icicle_count);
	ClassDB::bind_method(D_METHOD("get_wreath_sway"), &CabinLife::get_wreath_sway);
}

float CabinLife::get_window_glow_energy(int p_index) const {
	if (p_index < 0 || p_index >= 4 || !window_glow[p_index].is_valid()) {
		return 0.0f;
	}
	return (float)window_glow[p_index]->get("emission_energy_multiplier");
}

float CabinLife::get_porch_energy() const {
	if (porch_light == nullptr) {
		return 0.0f;
	}
	return porch_light->get_param(Light3D::PARAM_ENERGY);
}

bool CabinLife::get_smoke_enabled() const {
	return smoke != nullptr && smoke->is_emitting();
}

void CabinLife::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			wet = Math::lerp(wet, Math::clamp(rain + snow * 0.7f, 0.0f, 1.0f), 1.0f - std::exp(-1.5f * delta));
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float on = Math::clamp((night - 0.22f) / 0.40f, 0.0f, 1.0f);
			const float flicker = 0.94f + 0.04f * std::sin(t * 6.1f) + 0.02f * std::sin(t * 13.7f + 1.2f);
			for (int i = 0; i < 4; i++) {
				if (window_glow[i].is_valid()) {
					window_glow[i]->set("emission_energy_multiplier", (0.20f + 2.6f * on) * flicker * (1.0f - 0.25f * wet));
				}
			}
			if (window_light != nullptr) {
				window_light->set_param(Light3D::PARAM_ENERGY, 0.55f * on * flicker);
				window_light->set_visible(on > 0.02f);
			}
			if (porch_light != nullptr) {
				porch_light->set_param(Light3D::PARAM_ENERGY, 0.95f * on * flicker);
				porch_light->set_visible(on > 0.02f);
			}
			if (porch_glass.is_valid()) {
				porch_glass->set("emission_energy_multiplier", 0.25f + 1.8f * on);
			}

			// —— 冬日细节：冷度缓慢积累/消退，霜、积雪与垂冰跟着长出来 ——
			const float cold_target = Math::clamp(Math::max(snow * 1.30f, (night - 0.30f) * 1.25f), 0.0f, 1.0f);
			cold = Math::lerp(cold, cold_target, 1.0f - std::exp(-1.1f * delta));
			// 雪停后屋顶的白要留一阵（化雪比积雪慢）
			const float cover_rate = (snow > snow_cover) ? 0.30f : 0.055f;
			snow_cover = Math::lerp(snow_cover, snow, 1.0f - std::exp(-cover_rate * delta));
			frost_level = Math::clamp((cold - 0.18f) / 0.52f, 0.0f, 1.0f);
			icicle_level = Math::clamp((cold - 0.30f) / 0.50f, 0.0f, 1.0f);
			if (frost_mat.is_valid()) {
				frost_mat->set_albedo(Color(0.92f, 0.97f, 1.0f, 0.82f * frost_level));
			}
			for (int i = 0; i < 4; i++) {
				if (frost[i] != nullptr) {
					frost[i]->set_visible(frost_level > 0.02f);
				}
			}
			const float rain_now = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			rain_level = Math::lerp(rain_level, rain_now, 1.0f - std::exp(-1.6f * delta));
			if (rain_glass_mat.is_valid()) {
				rain_glass_mat->set_shader_parameter("wet", rain_level);
			}
			const bool show_rain = rain_level > 0.06f && frost_level < 0.45f;
			for (int i = 0; i < 4; i++) {
				if (rain_glass[i] != nullptr) {
					rain_glass[i]->set_visible(show_rain);
				}
			}
			const float roof_s = 0.15f + 0.85f * Math::clamp(snow_cover * 1.15f, 0.0f, 1.0f);
			for (int i = 0; i < 5; i++) {
				if (roof_snow[i] != nullptr) {
					roof_snow[i]->set_visible(snow_cover > 0.04f);
					roof_snow[i]->set_scale(Vector3(1.0f, roof_s, 1.0f));
				}
			}
			if (drifts != nullptr) {
				const float snow_now = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
				drift_level = Math::lerp(drift_level, snow_now, 1.0f - std::exp(-1.4f * delta));
				drifts->set_visible(drift_level > 0.12f);
				drifts->set_scale(Vector3(1.0f, 0.35f + 0.65f * Math::clamp(drift_level, 0.0f, 1.0f), 1.0f));
			}
			for (size_t i = 0; i < icicles.size(); i++) {
				const float h = icicle_scale[i].y * icicle_level;
				icicles[i]->set_visible(icicle_level > 0.03f);
				icicles[i]->set_scale(Vector3(icicle_scale[i].x, Math::max(0.001f, h), icicle_scale[i].z));
				icicles[i]->set_position(icicle_base[i] - Vector3(0.0f, h * 0.5f, 0.0f));
			}
			if (wreath != nullptr) {
				wreath_sway = std::sin(t * 0.9f) * 0.030f + std::sin(t * 1.7f + 0.6f) * 0.011f;
				wreath->set_rotation(Vector3(1.5708f + std::sin(t * 0.7f + 0.4f) * 0.018f, 0.0f, wreath_sway));
				if (wreath_mat.is_valid()) {
					const float w = Math::clamp(snow_cover * 0.55f, 0.0f, 0.55f);
					wreath_mat->set_shader_parameter("albedo",
							Color(0.13f, 0.27f, 0.15f).lerp(Color(0.80f, 0.86f, 0.92f), w));
				}
			}

			if (smoke != nullptr) {
				// 风把炊烟慢慢往东南带；雪天烟更浓、走得更低
				smoke->set_gravity(Vector3(0.10f + 0.05f * std::sin(t * 0.21f), 0.10f - 0.05f * snow, 0.04f + 0.03f * std::sin(t * 0.13f + 1.0f)));
				smoke->set_emitting(rain < 0.55f);
				const int wanted = 22 + (int)(16.0f * snow);
				if (wanted != smoke_amount) {
					smoke_amount = wanted;
					smoke->set_amount(wanted);
				}
			}
		} break;
		default:
			break;
	}
}

// 程序生成一张"蕨叶状"窗霜纹理：从四边种下几根主枝，逐像素蜿蜒生长、
// 偶尔分叉，越靠近枝梢越细越淡。无需外部贴图，每次开局同一张（固定种子）。
Ref<ImageTexture> CabinLife::make_frost_texture(unsigned p_seed) {
	const int S = 128;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	img->fill(Color(0, 0, 0, 0));
	std::mt19937 rng(p_seed);
	auto rnd = [&](float a, float b) {
		return a + (b - a) * (float)(rng() % 10000u) / 10000.0f;
	};
	struct Branch {
		float x = 0.0f;
		float y = 0.0f;
		float ang = 0.0f;
		float life = 0.0f;
		int depth = 0;
	};
	std::vector<Branch> stack;
	for (int i = 0; i < 12; i++) {
		Branch b;
		switch (rng() % 4u) {
			case 0: b.x = rnd(2.0f, S - 2.0f); b.y = 0.0f; b.ang = 1.5708f + rnd(-0.55f, 0.55f); break;
			case 1: b.x = rnd(2.0f, S - 2.0f); b.y = S - 1.0f; b.ang = -1.5708f + rnd(-0.55f, 0.55f); break;
			case 2: b.x = 0.0f; b.y = rnd(2.0f, S - 2.0f); b.ang = rnd(-0.55f, 0.55f); break;
			default: b.x = S - 1.0f; b.y = rnd(2.0f, S - 2.0f); b.ang = 3.1416f + rnd(-0.55f, 0.55f); break;
		}
		b.life = rnd(36.0f, 92.0f);
		stack.push_back(b);
	}
	int guard = 0;
	while (!stack.empty() && guard++ < 9000) {
		Branch b = stack.back();
		stack.pop_back();
		const int life = (int)b.life;
		float x = b.x;
		float y = b.y;
		float ang = b.ang;
		for (int s = 0; s < life; s++) {
			ang += rnd(-0.24f, 0.24f);
			x += std::cos(ang);
			y += std::sin(ang);
			if (x < 0.0f || x >= (float)S || y < 0.0f || y >= (float)S) {
				break;
			}
			const int ix = (int)x;
			const int iy = (int)y;
			const float taper = 1.0f - (float)s / (float)life;
			const float a = (0.26f + 0.55f * taper) * (b.depth > 0 ? 0.72f : 1.0f);
			const Color prev = img->get_pixel(ix, iy);
			img->set_pixel(ix, iy, Color(0.87f, 0.94f, 1.0f, Math::min(1.0f, prev.a + a)));
			if (ix + 1 < S) {
				const Color p2 = img->get_pixel(ix + 1, iy);
				img->set_pixel(ix + 1, iy, Color(0.87f, 0.94f, 1.0f, Math::min(1.0f, p2.a + a * 0.40f)));
			}
			// 细小的霜针横生
			if (rnd(0.0f, 1.0f) < 0.09f) {
				const int nx = ix + (int)std::lround(std::cos(ang + 1.45f));
				const int ny = iy + (int)std::lround(std::sin(ang + 1.45f));
				if (nx >= 0 && nx < S && ny >= 0 && ny < S) {
					img->set_pixel(nx, ny, Color(0.87f, 0.94f, 1.0f, a * 0.65f));
				}
			}
			if (b.depth < 3 && s > 5 && rnd(0.0f, 1.0f) < 0.038f) {
				Branch c;
				c.x = x;
				c.y = y;
				c.ang = ang + (rnd(0.0f, 1.0f) < 0.5f ? -0.95f : 0.95f);
				c.life = rnd(9.0f, 24.0f);
				c.depth = b.depth + 1;
				stack.push_back(c);
				break; // 主枝到分叉处收住，留下自然的渐细
			}
		}
	}
	// 一层极淡的薄雾，让整块玻璃有"上霜"的感觉，而不只是几条蕨叶
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			const Color c = img->get_pixel(x, y);
			if (c.a < 0.09f) {
				img->set_pixel(x, y, Color(0.90f, 0.96f, 1.0f, 0.09f));
			}
		}
	}
	return ImageTexture::create_from_image(img);
}

void CabinLife::build() {
	if (get_parent() == nullptr) {
		return;
	}
	Node3D *host = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Homestead"));
	if (host == nullptr) {
		return;
	}
	Ref<Shader> toon = meshutil::load_toon_shader();

	// —— 屋顶石烟囱 ——
	const Vector3 chimney_base(1.7f, 3.55f, -1.2f);
	meshutil::add_box(host, toon, Vector3(0.72f, 1.55f, 0.72f), chimney_base, Color(0.40f, 0.39f, 0.37f));
	meshutil::add_box(host, toon, Vector3(0.84f, 0.16f, 0.84f), chimney_base + Vector3(0, 0.80f, 0), Color(0.30f, 0.29f, 0.28f));

	// 青烟：一团团慢慢升上去、散开
	Ref<SphereMesh> puff;
	puff.instantiate();
	puff->set_radius(0.16f);
	puff->set_height(0.32f);
	puff->set_radial_segments(8);
	puff->set_rings(5);
	Ref<StandardMaterial3D> smoke_mat;
	smoke_mat.instantiate();
	smoke_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	smoke_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	smoke_mat->set_albedo(Color(0.62f, 0.65f, 0.64f, 0.10f));
	smoke_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	puff->set_material(smoke_mat);
	smoke = memnew(CPUParticles3D);
	smoke->set_name("ChimneySmoke");
	smoke->set_amount(22);
	smoke->set_lifetime(5.5f);
	smoke->set_mesh(puff);
	smoke->set_direction(Vector3(0, 1, 0));
	smoke->set_spread(10.0f);
	smoke->set_gravity(Vector3(0.10f, 0.10f, 0.04f));
	smoke->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.28f);
	smoke->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.55f);
	smoke->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	smoke->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
	smoke->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	smoke->set_emission_sphere_radius(0.12f);
	smoke->set_position(chimney_base + Vector3(0, 0.95f, 0));
	smoke->set_emitting(true);
	host->add_child(smoke);

	// —— 四扇窗的暖光 ——
	const Vector3 panes[4] = {
		Vector3(-2.4f, 2.0f, 3.60f),   // 前左
		Vector3(2.6f, 2.0f, 3.60f),    // 前右
		Vector3(-4.20f, 2.0f, 0.3f),   // 左侧（贴侧面墙）
		Vector3(1.6f, 2.0f, -3.62f),   // 后墙
	};
	const Vector3 pane_size[4] = {
		Vector3(1.05f, 0.92f, 0.03f),
		Vector3(1.05f, 0.92f, 0.03f),
		Vector3(0.03f, 0.92f, 1.05f),
		Vector3(1.05f, 0.92f, 0.03f),
	};
	for (int i = 0; i < 4; i++) {
		Ref<BoxMesh> pane;
		pane.instantiate();
		pane->set_size(pane_size[i]);
		window_glow[i].instantiate();
		window_glow[i]->set_albedo(Color(1.0f, 0.80f, 0.48f));
		window_glow[i]->set("emission_enabled", true);
		window_glow[i]->set("emission", Color(1.0f, 0.67f, 0.30f));
		window_glow[i]->set("emission_energy_multiplier", 0.20f);
		pane->set_material(Ref<Material>(window_glow[i]));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name(String("WindowGlow") + String::num_int64(i));
		mi->set_mesh(pane);
		mi->set_position(panes[i]);
		host->add_child(mi);
	}

	// 前窗十字窗棂：把整块暖光分成四格，更像一扇真的窗
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(host, toon, Vector3(1.10f, 0.045f, 0.025f), panes[i] + Vector3(0.0f, 0.0f, 0.02f), Color(0.30f, 0.19f, 0.10f));
		meshutil::add_box(host, toon, Vector3(0.045f, 0.96f, 0.025f), panes[i] + Vector3(0.0f, 0.0f, 0.02f), Color(0.30f, 0.19f, 0.10f));
	}

	window_light = memnew(OmniLight3D);
	window_light->set_name("WindowLight");
	window_light->set_position(Vector3(0.0f, 2.1f, 4.3f));
	window_light->set_color(Color(1.0f, 0.68f, 0.34f));
	window_light->set_param(Light3D::PARAM_RANGE, 5.0f);
	window_light->set_param(Light3D::PARAM_ENERGY, 0.0f);
	window_light->set_visible(false);
	host->add_child(window_light);

	// —— 门口小灯 ——
	const Color bark(0.34f, 0.23f, 0.13f);
	const Vector3 post(1.55f, 0.0f, 3.95f);
	meshutil::add_box(host, toon, Vector3(0.09f, 1.55f, 0.09f), post + Vector3(0, 0.77f, 0), bark);
	meshutil::add_box(host, toon, Vector3(0.36f, 0.05f, 0.05f), post + Vector3(-0.13f, 1.50f, 0), bark);
	meshutil::add_box(host, toon, Vector3(0.20f, 0.04f, 0.20f), post + Vector3(-0.28f, 1.53f, 0), bark);
	meshutil::add_box(host, toon, Vector3(0.20f, 0.04f, 0.20f), post + Vector3(-0.28f, 1.26f, 0), bark);

	Ref<BoxMesh> pg;
	pg.instantiate();
	pg->set_size(Vector3(0.16f, 0.22f, 0.16f));
	porch_glass.instantiate();
	porch_glass->set_albedo(Color(1.0f, 0.80f, 0.48f));
	porch_glass->set("emission_enabled", true);
	porch_glass->set("emission", Color(1.0f, 0.68f, 0.32f));
	porch_glass->set("emission_energy_multiplier", 0.25f);
	pg->set_material(Ref<Material>(porch_glass));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_name("PorchGlass");
	pmi->set_mesh(pg);
	pmi->set_position(post + Vector3(-0.28f, 1.40f, 0));
	host->add_child(pmi);

	porch_light = memnew(OmniLight3D);
	porch_light->set_name("PorchLight");
	porch_light->set_position(post + Vector3(-0.30f, 1.37f, 0.16f));
	porch_light->set_color(Color(1.0f, 0.70f, 0.36f));
	porch_light->set_param(Light3D::PARAM_RANGE, 6.0f);
	porch_light->set_param(Light3D::PARAM_ENERGY, 0.0f);
	porch_light->set_visible(false);
	host->add_child(porch_light);

	std::mt19937 rng(20261102u);
	auto rr = [&](float a, float b) {
		return a + (b - a) * (float)(rng() % 10000u) / 10000.0f;
	};

	// —— 冬日 1：窗玻璃上的蕨状霜花（冷天/雪夜慢慢长出来） ——
	frost_mat.instantiate();
	frost_mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, make_frost_texture(20261101u));
	frost_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	frost_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	frost_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	frost_mat->set_albedo(Color(0.92f, 0.97f, 1.0f, 0.0f));
	for (int i = 0; i < 4; i++) {
		const bool side = pane_size[i].x < pane_size[i].z;
		Ref<BoxMesh> fm;
		fm.instantiate();
		fm->set_size(side ? Vector3(0.02f, 0.90f, 1.03f) : Vector3(1.03f, 0.90f, 0.02f));
		fm->set_material(Ref<Material>(frost_mat));
		Vector3 off;
		if (i < 2) {
			off = Vector3(0.0f, 0.0f, 0.035f);
		} else if (i == 2) {
			off = Vector3(-0.035f, 0.0f, 0.0f);
		} else {
			off = Vector3(0.0f, 0.0f, -0.035f);
		}
		frost[i] = memnew(MeshInstance3D);
		frost[i]->set_name(String("WindowFrost") + String::num_int64(i));
		frost[i]->set_mesh(fm);
		frost[i]->set_position(panes[i] + off);
		frost[i]->set_visible(false);
		host->add_child(frost[i]);
	}

	// —— 雨痕：四扇窗共享一个屏幕折射材质（第 103 轮） ——
	rain_glass_mat.instantiate();
	{
		Ref<Shader> rsh = ResourceLoader::get_singleton()->load("res://assets/rain_glass.gdshader");
		if (rsh.is_valid()) {
			rain_glass_mat->set_shader(rsh);
			rain_glass_mat->set_shader_parameter("wet", 0.0f);
		}
	}
	for (int i = 0; i < 4; i++) {
		const bool side = pane_size[i].x < pane_size[i].z;
		Ref<BoxMesh> rm;
		rm.instantiate();
		rm->set_size(side ? Vector3(0.02f, 0.90f, 1.03f) : Vector3(1.03f, 0.90f, 0.02f));
		rm->set_material(Ref<Material>(rain_glass_mat));
		Vector3 off;
		if (i < 2) {
			off = Vector3(0.0f, 0.0f, 0.045f);
		} else if (i == 2) {
			off = Vector3(-0.045f, 0.0f, 0.0f);
		} else {
			off = Vector3(0.0f, 0.0f, -0.045f);
		}
		rain_glass[i] = memnew(MeshInstance3D);
		rain_glass[i]->set_name(String("WindowRain") + String::num_int64(i));
		rain_glass[i]->set_mesh(rm);
		rain_glass[i]->set_position(panes[i] + off);
		rain_glass[i]->set_visible(false);
		host->add_child(rain_glass[i]);
	}

	// —— 冬日 2：屋顶积雪（主屋两面坡 + 屋脊 + 侧棚 + 门廊棚） ——
	const Color snow_col(0.90f, 0.93f, 0.98f);
	const float slope = std::atan2(2.0f, 4.4f);
	roof_snow[0] = meshutil::add_box(host, toon, Vector3(4.95f, 0.12f, 7.7f), Vector3(-2.23f, 4.87f, 0.0f), snow_col, Vector3(0.0f, 0.0f, slope));
	roof_snow[1] = meshutil::add_box(host, toon, Vector3(4.95f, 0.12f, 7.7f), Vector3(2.23f, 4.87f, 0.0f), snow_col, Vector3(0.0f, 0.0f, -slope));
	roof_snow[2] = meshutil::add_box(host, toon, Vector3(0.30f, 0.14f, 7.7f), Vector3(0.0f, 5.84f, 0.0f), snow_col);
	roof_snow[3] = meshutil::add_box(host, toon, Vector3(3.8f, 0.10f, 4.3f), Vector3(5.62f, 3.25f, -1.45f), snow_col, Vector3(0.0f, 0.0f, -0.208f));
	roof_snow[4] = meshutil::add_box(host, toon, Vector3(4.0f, 0.10f, 2.1f), Vector3(0.0f, 3.10f, 4.35f), snow_col, Vector3(-0.139f, 0.0f, 0.0f));
	for (int i = 0; i < 5; i++) {
		roof_snow[i]->set_visible(false);
	}

	// —— 冬日 3：屋檐垂冰 ——
	Ref<CylinderMesh> ice;
	ice.instantiate();
	ice->set_top_radius(0.032f);
	ice->set_bottom_radius(0.0f);
	ice->set_height(1.0f);
	ice->set_radial_segments(5);
	ice->set_rings(1);
	Ref<StandardMaterial3D> ice_mat;
	ice_mat.instantiate();
	ice_mat->set_albedo(Color(0.80f, 0.92f, 1.0f, 0.78f));
	ice_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	ice_mat->set_roughness(0.12f);
	ice_mat->set_metallic(0.25f);
	ice->set_material(Ref<Material>(ice_mat));
	auto add_icicle = [&](const Vector3 &p_eave, float p_len) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(ice);
		mi->set_position(p_eave);
		mi->set_visible(false);
		host->add_child(mi);
		icicles.push_back(mi);
		icicle_base.push_back(p_eave);
		icicle_scale.push_back(Vector3(rr(0.75f, 1.15f), p_len, rr(0.75f, 1.15f)));
	};
	for (int i = 0; i < 8; i++) {
		const float z = -3.3f + 0.94f * (float)i;
		add_icicle(Vector3(-4.36f, 3.78f, z), rr(0.14f, 0.34f));
		add_icicle(Vector3(4.36f, 3.78f, z), rr(0.14f, 0.34f));
	}
	for (int i = 0; i < 6; i++) {
		const float x = -1.7f + 0.68f * (float)i;
		add_icicle(Vector3(x, 3.08f, 5.30f), rr(0.10f, 0.28f));
	}

	// —— 冬日 4：门上的松枝花环（红果 + 随门开合） ——
	wreath_mat = meshutil::toon_material(toon, Color(0.13f, 0.27f, 0.15f), 0.25f);
	Ref<TorusMesh> torus;
	torus.instantiate();
	torus->set_inner_radius(0.16f);
	torus->set_outer_radius(0.30f);
	torus->set_rings(28);
	torus->set_ring_segments(8);
	torus->set_material(wreath_mat);
	wreath = memnew(Node3D);
	wreath->set_name("WinterWreath");
	wreath->set_rotation(Vector3(1.5708f, 0.0f, 0.0f));
	MeshInstance3D *ring_mi = memnew(MeshInstance3D);
	ring_mi->set_mesh(torus);
	wreath->add_child(ring_mi);
	for (int i = 0; i < 7; i++) {
		const float a = (float)i * 0.8976f;
		meshutil::add_sphere(wreath, toon, 0.036f, Vector3(std::cos(a) * 0.23f, 0.058f, std::sin(a) * 0.23f), Color(0.62f, 0.12f, 0.13f));
	}
	// 挂在门扇上：开门时花环跟着门一起转
	Node3D *wreath_mount = host;
	Node *door_panel = host->get_node_or_null(NodePath("CabinDoor/Pivot/Panel"));
	if (door_panel != nullptr && Object::cast_to<Node3D>(door_panel) != nullptr) {
		wreath_mount = Object::cast_to<Node3D>(door_panel);
		wreath->set_position(Vector3(0.0f, 1.32f, 0.125f));
	} else {
		wreath->set_position(Vector3(0.0f, 1.52f, 3.55f));
	}
	wreath_mount->add_child(wreath);

	// —— 室内陈设（第 97 轮）：窗帘 / 挂钩斗篷 / 木靴 / 窗台陶灯 / 编织地毯 ——
	const Color linen(0.86f, 0.81f, 0.70f);
	const Color linen_shadow(0.72f, 0.66f, 0.55f);
	const Color cloak_col(0.30f, 0.38f, 0.30f);
	// 前窗两侧的亚麻窗帘
	for (int wi = 0; wi < 2; wi++) {
		const float wx = (wi == 0) ? -2.4f : 2.6f;
		for (int side = 0; side < 2; side++) {
			const float dx = (side == 0) ? -0.62f : 0.62f;
			MeshInstance3D *curtain = meshutil::add_box(host, toon, Vector3(0.20f, 0.98f, 0.025f),
					Vector3(wx + dx, 2.0f, 3.54f), (side == 0) ? linen : linen_shadow,
					Vector3(0.0f, 0.0f, (side == 0 ? -0.05f : 0.05f)));
			curtain->set_name(String("Curtain") + String::num_int64(wi * 2 + side));
			meshutil::add_box(host, toon, Vector3(0.24f, 0.05f, 0.06f),
					Vector3(wx + dx, 2.50f, 3.54f), Color(0.44f, 0.31f, 0.18f));
		}
	}
	// 门边的挂钩与斗篷
	meshutil::add_box(host, toon, Vector3(0.06f, 0.06f, 0.10f), Vector3(0.78f, 2.22f, 3.42f), Color(0.36f, 0.24f, 0.13f));
	MeshInstance3D *cloak = meshutil::add_box(host, toon, Vector3(0.42f, 0.72f, 0.07f), Vector3(0.78f, 1.82f, 3.46f), cloak_col, Vector3(-0.06f, 0.0f, 0.0f));
	cloak->set_name("DoorCloak");
	meshutil::add_box(host, toon, Vector3(0.12f, 0.10f, 0.06f), Vector3(0.78f, 2.16f, 3.46f), Color(0.24f, 0.30f, 0.24f));
	// 门口的一双木靴
	MeshInstance3D *boot_l = meshutil::add_box(host, toon, Vector3(0.14f, 0.20f, 0.26f), Vector3(-0.82f, 0.11f, 3.62f), Color(0.32f, 0.22f, 0.13f));
	boot_l->set_name("BootL");
	MeshInstance3D *boot_r = meshutil::add_box(host, toon, Vector3(0.14f, 0.20f, 0.26f), Vector3(-0.56f, 0.11f, 3.62f), Color(0.32f, 0.22f, 0.13f));
	boot_r->set_name("BootR");
	// 前窗窗台上的小陶灯
	{
		Ref<CylinderMesh> pot;
		pot.instantiate();
		pot->set_top_radius(0.10f);
		pot->set_bottom_radius(0.07f);
		pot->set_height(0.14f);
		pot->set_radial_segments(10);
		pot->set_material(meshutil::toon_material(toon, Color(0.56f, 0.38f, 0.22f), 0.2f));
		MeshInstance3D *pmi = memnew(MeshInstance3D);
		pmi->set_name("SillLampPot");
		pmi->set_mesh(pot);
		pmi->set_position(Vector3(2.10f, 1.62f, 3.52f));
		host->add_child(pmi);
		Ref<SphereMesh> flame;
		flame.instantiate();
		flame->set_radius(0.045f);
		flame->set_height(0.09f);
		flame->set_radial_segments(8);
		flame->set_rings(4);
		Ref<StandardMaterial3D> flame_mat;
		flame_mat.instantiate();
		flame_mat->set_albedo(Color(1.0f, 0.80f, 0.45f));
		flame_mat->set("emission_enabled", true);
		flame_mat->set("emission", Color(1.0f, 0.62f, 0.28f));
		flame_mat->set("emission_energy_multiplier", 1.6f);
		flame->set_material(Ref<Material>(flame_mat));
		MeshInstance3D *fmi = memnew(MeshInstance3D);
		fmi->set_name("SillLampFlame");
		fmi->set_mesh(flame);
		fmi->set_position(Vector3(2.10f, 1.80f, 3.52f));
		host->add_child(fmi);
	}
	// 火炉前的编织地毯
	{
		Ref<CylinderMesh> rug;
		rug.instantiate();
		rug->set_top_radius(0.95f);
		rug->set_bottom_radius(0.95f);
		rug->set_height(0.02f);
		rug->set_radial_segments(24);
		rug->set_rings(1);
		rug->set_material(meshutil::toon_material(toon, Color(0.66f, 0.46f, 0.30f), 0.15f));
		MeshInstance3D *rmi = memnew(MeshInstance3D);
		rmi->set_name("HearthRug");
		rmi->set_mesh(rug);
		rmi->set_position(Vector3(-2.55f, 0.21f, -0.55f));
		rmi->set_scale(Vector3(1.25f, 1.0f, 0.9f));
		host->add_child(rmi);
	}

	// —— 冬日 5：墙根被风吹拢的雪堆 ——
	drifts = memnew(Node3D);
	drifts->set_name("SnowDrifts");
	host->add_child(drifts);
	const struct { Vector3 pos; Vector3 scale; } drift_defs[3] = {
		{ Vector3(2.2f, 0.05f, 3.55f), Vector3(2.2f, 0.42f, 0.55f) },
		{ Vector3(-4.15f, 0.05f, -0.6f), Vector3(0.55f, 0.40f, 2.0f) },
		{ Vector3(1.2f, 0.05f, -3.55f), Vector3(1.6f, 0.38f, 0.50f) },
	};
	for (int i = 0; i < 3; i++) {
		MeshInstance3D *m = meshutil::add_sphere(drifts, toon, 0.5f, drift_defs[i].pos, snow_col);
		m->set_scale(drift_defs[i].scale);
		m->set_name(String("Drift") + String::num_int64(i));
	}
	drifts->set_visible(false);
}
