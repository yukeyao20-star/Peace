#include "stone_path.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "sky_cycle.h"
#include "terrain_builder.h"

using namespace godot;

namespace {
// 沿东岸绕湖一圈的控制点：木屋门口 → 东侧草地 → 东南岸 → 码头/长椅
const Vector3 PATH_PTS[5] = {
	Vector3(10.4f, 0.0f, 10.3f),
	Vector3(8.8f, 0.0f, 4.6f),
	Vector3(8.4f, 0.0f, -3.6f),
	Vector3(5.2f, 0.0f, -9.6f),
	Vector3(-6.6f, 0.0f, -11.2f),
};

Vector3 catmull(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) {
	float t2 = t * t;
	float t3 = t2 * t;
	return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
			+ (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

Vector3 path_point(float u) {
	u = Math::clamp(u, 0.0f, 0.999f);
	float f = u * 4.0f;
	int seg = (int)f;
	float lt = f - (float)seg;
	int i0 = Math::max(0, seg - 1);
	int i1 = seg;
	int i2 = Math::min(4, seg + 1);
	int i3 = Math::min(4, seg + 2);
	return catmull(PATH_PTS[i0], PATH_PTS[i1], PATH_PTS[i2], PATH_PTS[i3], lt);
}
} // namespace

// 中间实、边缘渐隐的圆形 alpha 贴图：让水洼和灯光地面没有硬边
static Ref<GradientTexture2D> make_soft_disc_texture() {
	Ref<Gradient> g;
	g.instantiate();
	PackedFloat32Array offsets;
	offsets.push_back(0.0f);
	offsets.push_back(0.62f);
	offsets.push_back(1.0f);
	PackedColorArray colors;
	colors.push_back(Color(1, 1, 1, 1));
	colors.push_back(Color(1, 1, 1, 0.85f));
	colors.push_back(Color(1, 1, 1, 0));
	g->set_offsets(offsets);
	g->set_colors(colors);
	Ref<GradientTexture2D> tex;
	tex.instantiate();
	tex->set_gradient(g);
	tex->set_width(64);
	tex->set_height(64);
	tex->set_fill(GradientTexture2D::FILL_RADIAL);
	tex->set_fill_from(Vector2(0.5f, 0.5f));
	tex->set_fill_to(Vector2(1.0f, 0.5f));
	return tex;
}

void StonePath::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stone_count"), &StonePath::get_stone_count);
	ClassDB::bind_method(D_METHOD("get_lamp_count"), &StonePath::get_lamp_count);
	ClassDB::bind_method(D_METHOD("get_lamp_energy", "index"), &StonePath::get_lamp_energy);
	ClassDB::bind_method(D_METHOD("get_puddle_count"), &StonePath::get_puddle_count);
	ClassDB::bind_method(D_METHOD("get_moth_count"), &StonePath::get_moth_count);
	ClassDB::bind_method(D_METHOD("get_moth_visible_count"), &StonePath::get_moth_visible_count);
	ClassDB::bind_method(D_METHOD("get_splash_ready"), &StonePath::get_splash_ready);
	ClassDB::bind_method(D_METHOD("get_puddle_alpha", "index"), &StonePath::get_puddle_alpha);
	ClassDB::bind_method(D_METHOD("get_pool_alpha", "index"), &StonePath::get_pool_alpha);
}

int StonePath::get_moth_visible_count() const {
	int n = 0;
	for (const Moth &m : moths) {
		if (m.root != nullptr && m.root->is_visible()) {
			n++;
		}
	}
	return n;
}

float StonePath::get_puddle_alpha(int p_index) const {
	if (p_index < 0 || p_index >= (int)puddles.size() || !puddles[p_index].mat.is_valid()) {
		return 0.0f;
	}
	return puddles[p_index].mat->get_albedo().a;
}

float StonePath::get_pool_alpha(int p_index) const {
	if (p_index < 0 || p_index >= 3 || !light_pool_mat[p_index].is_valid()) {
		return 0.0f;
	}
	return light_pool_mat[p_index]->get_albedo().a;
}

float StonePath::get_lamp_energy(int p_index) const {
	if (p_index < 0 || p_index >= 3 || lamp_light[p_index] == nullptr) {
		return 0.0f;
	}
	return lamp_light[p_index]->get_param(Light3D::PARAM_ENERGY);
}

void StonePath::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			winter_snow = memnew(Node3D);
			winter_snow->set_name("WinterSnow");
			add_child(winter_snow);
			build_path();
			build_lamps();
			build_gate();
			build_puddles();
			build_moths();
			build_light_pools();
			make_splash();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float target = Math::clamp(rain * 0.9f + ((sky != nullptr) ? sky->get_snow_amount() * 0.25f : 0.0f), 0.0f, 1.0f);
			wet = Math::lerp(wet, target, 1.0f - std::exp(-1.6f * delta));
			if (stone_mat.is_valid()) {
				Color dry(0.58f, 0.56f, 0.52f);
				Color soaked(0.33f, 0.34f, 0.34f);
				stone_mat->set_shader_parameter("albedo", dry.lerp(soaked, wet));
				stone_mat->set_shader_parameter("roughness_value", 0.9f - 0.55f * wet);
			}
			const float snow_now = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			if (winter_snow != nullptr) {
				winter_snow->set_visible(snow_now > 0.15f);
			}
			for (MeshInstance3D *cap : lamp_snow_caps) {
				if (cap != nullptr) {
					cap->set_visible(snow_now > 0.35f);
				}
			}
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float on = Math::clamp((night - 0.22f) / 0.45f, 0.0f, 1.0f);
			for (int i = 0; i < 3; i++) {
				if (lamp_light[i] != nullptr) {
					float flick = 0.92f + 0.05f * std::sin(t * 7.7f + (float)i * 2.1f) + 0.03f * std::sin(t * 17.3f + (float)i);
					lamp_light[i]->set_param(Light3D::PARAM_ENERGY, 1.1f * on * flick * (1.0f + 0.25f * snow_now));
					lamp_light[i]->set_visible(on > 0.02f);
				}
				if (lamp_glass[i].is_valid()) {
					lamp_glass[i]->set("emission_energy_multiplier", 0.3f + 2.2f * on);
				}
				if (lamp_pivot[i] != nullptr) {
					lamp_pivot[i]->set_rotation(Vector3(
							std::sin(t * 0.9f + (float)i) * 0.014f, 0.0f,
							std::sin(t * 1.25f + (float)i * 1.7f) * 0.035f));
				}
				if (light_pool[i] != nullptr) {
					light_pool[i]->set_visible(on > 0.03f);
				}
				if (light_pool_mat[i].is_valid()) {
					Color c = light_pool_mat[i]->get_albedo();
					c.a = 0.11f * on * (1.0f + 0.4f * wet + 0.5f * snow_now);
					light_pool_mat[i]->set_albedo(c);
				}
			}

			// —— 雨后水洼：雨大时积水变亮，雨停后慢慢干 ——
			const float pud = Math::clamp((wet - 0.22f) / 0.5f, 0.0f, 1.0f);
			for (Puddle &p : puddles) {
				if (p.mi != nullptr) {
					p.mi->set_visible(pud > 0.03f);
				}
				if (p.mat.is_valid()) {
					Color c = p.mat->get_albedo();
					c.a = 0.06f + 0.26f * pud;
					p.mat->set_albedo(c);
				}
			}
			// 雨点打水洼：细密的小圆环
			ripple_t -= delta;
			if (ripple_t <= 0.0f) {
				if (pud > 0.15f && !puddles.empty()) {
					const Puddle &p = puddles[rng() % puddles.size()];
					spawn_ripple(p.center + Vector3(0.0f, 0.012f, 0.0f));
				}
				ripple_t = 0.45f + (float)(rng() % 100u) * 0.006f;
			}
			for (Ripple &r : ripples) {
				if (!r.active || r.mi == nullptr) {
					continue;
				}
				r.t += delta;
				float u = r.t / r.dur;
				if (u >= 1.0f) {
					r.active = false;
					r.mi->set_visible(false);
					continue;
				}
				float sc = 0.45f + u * 2.2f;
				r.mi->set_scale(Vector3(sc, 1.0f, sc));
				if (r.mat.is_valid()) {
					Color c = r.mat->get_albedo();
					c.a = (1.0f - u) * 0.40f;
					r.mat->set_albedo(c);
				}
			}
			// 踩进水洼：溅起一圈涟漪 + 一声水花
			step_t -= delta;
			if (pud > 0.3f && step_t <= 0.0f && get_parent() != nullptr) {
				Player *pl = Object::cast_to<Player>(get_parent()->get_node_or_null("Player"));
				if (pl != nullptr) {
					const Vector3 pp = pl->get_global_position();
					const Vector3 vel = pl->get_velocity();
					if (Vector2(vel.x, vel.z).length() > 0.6f) {
						for (const Puddle &p : puddles) {
							if (Vector2(pp.x - p.center.x, pp.z - p.center.z).length() < p.radius * 0.9f
									&& std::fabs(pp.y - p.center.y) < 0.6f) {
								spawn_ripple(Vector3(pp.x, p.center.y + 0.012f, pp.z));
								if (splash != nullptr) {
									splash->set_global_position(Vector3(pp.x, p.center.y + 0.1f, pp.z));
									splash->set_pitch_scale(0.9f + (float)(rng() % 100u) * 0.002f);
									splash->set_volume_db(-13.0f);
									splash->play(0.0f);
								}
								step_t = 0.34f;
								break;
							}
						}
					}
				}
			}

			// —— 路灯下的飞蛾：只在夜里、无雨时绕着灯罩转 ——
			const bool moth_show = on > 0.25f && wet < 0.25f;
			for (Moth &m : moths) {
				if (m.root == nullptr) {
					continue;
				}
				if (m.root->is_visible() != moth_show) {
					m.root->set_visible(moth_show);
				}
				if (!moth_show) {
					continue;
				}
				m.phase += delta * m.speed;
				m.root->set_global_position(m.center + Vector3(
						std::cos(m.phase) * m.radius,
						m.lift + std::sin(t * 2.1f + m.phase) * 0.045f,
						std::sin(m.phase) * m.radius));
				float flap = std::sin(t * 26.0f + m.phase * 3.0f) * 0.85f;
				if (m.wing_l != nullptr) {
					m.wing_l->set_rotation(Vector3(0.0f, 0.0f, flap));
				}
				if (m.wing_r != nullptr) {
					m.wing_r->set_rotation(Vector3(0.0f, 0.0f, -flap));
				}
			}
		} break;
		default:
			break;
	}
}

void StonePath::build_path() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	stone_mat = meshutil::toon_material(toon, Color(0.58f, 0.56f, 0.52f), 0.18f);

	std::mt19937 rng(20260808u);
	const int total = 44;
	Vector3 prev = path_point(0.0f);
	for (int i = 0; i < total; i++) {
		float u = (float)i / (float)(total - 1);
		Vector3 p = path_point(u);
		Vector3 dir = (path_point(Math::min(0.999f, u + 0.008f)) - prev);
		dir.y = 0.0f;
		if (dir.length_squared() > 0.0001f) {
			dir = dir.normalized();
		}
		// 左右轻微错开，像人走出来的路
		Vector3 side(-dir.z, 0.0f, dir.x);
		p += side * ((float)(rng() % 100u) * 0.006f - 0.30f);
		float ground = 0.0f;
		if (terrain != nullptr) {
			ground = terrain->get_terrain_height(p.x, p.z);
		}
		p.y = ground + 0.035f;

		Ref<CylinderMesh> stone;
		stone.instantiate();
		float r = 0.28f + (float)(rng() % 100u) * 0.0014f;
		stone->set_top_radius(r);
		stone->set_bottom_radius(r * 0.94f);
		stone->set_height(0.055f);
		stone->set_radial_segments(8);
		stone->set_rings(1);
		stone->set_material(stone_mat);
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name(String("Stone") + String::num_int64(i));
		mi->set_mesh(stone);
		mi->set_position(p);
		mi->set_scale(Vector3(1.0f, 1.0f, 0.82f + (float)(rng() % 100u) * 0.0018f));
		mi->set_rotation(Vector3(0.0f, (float)(rng() % 628u) * 0.01f, (float)(rng() % 100u) * 0.0008f - 0.03f));
		add_child(mi);
		stone_count++;
		if (i % 3 == 0 && winter_snow != nullptr) {
			Ref<CylinderMesh> cap;
			cap.instantiate();
			cap->set_top_radius(r * 0.78f);
			cap->set_bottom_radius(r * 0.78f);
			cap->set_height(0.02f);
			cap->set_radial_segments(8);
			cap->set_rings(1);
			cap->set_material(meshutil::toon_material(toon, Color(0.92f, 0.95f, 1.0f), 0.1f));
			MeshInstance3D *ci = memnew(MeshInstance3D);
			ci->set_name(String("StoneSnow") + String::num_int64(i));
			ci->set_mesh(cap);
			ci->set_position(p + Vector3(0.0f, 0.045f, 0.0f));
			winter_snow->add_child(ci);
		}
		prev = p;
	}
}

void StonePath::build_lamps() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const float ts[3] = {0.20f, 0.50f, 0.78f};
	const Color bark(0.33f, 0.22f, 0.12f);

	for (int i = 0; i < 3; i++) {
		Vector3 p = path_point(ts[i]);
		Vector3 next = path_point(Math::min(0.999f, ts[i] + 0.02f));
		Vector3 dir = next - p;
		dir.y = 0.0f;
		if (dir.length_squared() > 0.0001f) {
			dir = dir.normalized();
		}
		Vector3 side(-dir.z, 0.0f, dir.x);
		// 选地势更高的一侧立杆，免得灯柱插进湖里
		Vector3 pa = p + side * 0.85f;
		Vector3 pb = p - side * 0.85f;
		float ha = (terrain != nullptr) ? terrain->get_terrain_height(pa.x, pa.z) : 0.0f;
		float hb = (terrain != nullptr) ? terrain->get_terrain_height(pb.x, pb.z) : 0.0f;
		p = (ha >= hb) ? pa : pb;
		float ground = (ha >= hb) ? ha : hb;
		p.y = ground;

		meshutil::add_box(this, toon, Vector3(0.10f, 2.05f, 0.10f), p + Vector3(0, 1.02f, 0), bark);
		meshutil::add_box(this, toon, Vector3(0.42f, 0.06f, 0.06f), p + dir * 0.18f + Vector3(0, 1.98f, 0), bark);

		Node3D *pivot = memnew(Node3D);
		pivot->set_name(String("LampPivot") + String::num_int64(i));
		pivot->set_position(p + dir * 0.34f + Vector3(0, 1.97f, 0));
		add_child(pivot);
		lamp_pivot[i] = pivot;
		meshutil::add_box(pivot, toon, Vector3(0.22f, 0.05f, 0.22f), Vector3(0, 0, 0), bark);
		meshutil::add_box(pivot, toon, Vector3(0.22f, 0.04f, 0.22f), Vector3(0, -0.31f, 0), bark);

		Ref<BoxMesh> glass_mesh;
		glass_mesh.instantiate();
		glass_mesh->set_size(Vector3(0.17f, 0.24f, 0.17f));
		lamp_glass[i].instantiate();
		lamp_glass[i]->set_albedo(Color(1.0f, 0.80f, 0.48f));
		lamp_glass[i]->set("emission_enabled", true);
		lamp_glass[i]->set("emission", Color(1.0f, 0.68f, 0.32f));
		lamp_glass[i]->set("emission_energy_multiplier", 0.3f);
		glass_mesh->set_material(Ref<Material>(lamp_glass[i]));
		MeshInstance3D *glass = memnew(MeshInstance3D);
		glass->set_name(String("LampGlass") + String::num_int64(i));
		glass->set_mesh(glass_mesh);
		glass->set_position(Vector3(0, -0.16f, 0));
		pivot->add_child(glass);

		// 冬日：灯顶雪帽 + 灯柱脚下霜圈
		MeshInstance3D *cap = meshutil::add_box(pivot, toon, Vector3(0.24f, 0.035f, 0.24f), Vector3(0.0f, 0.040f, 0.0f), Color(0.92f, 0.95f, 1.0f));
		cap->set_name(String("LampSnow") + String::num_int64(i));
		lamp_snow_caps.push_back(cap);
		if (winter_snow != nullptr) {
			Ref<TorusMesh> ring;
			ring.instantiate();
			ring->set_inner_radius(0.11f);
			ring->set_outer_radius(0.17f);
			ring->set_rings(18);
			ring->set_ring_segments(5);
			ring->set_material(meshutil::toon_material(toon, Color(0.88f, 0.93f, 1.0f), 0.2f));
			MeshInstance3D *rmi = memnew(MeshInstance3D);
			rmi->set_name(String("FrostRing") + String::num_int64(i));
			rmi->set_mesh(ring);
			rmi->set_position(p + Vector3(0.0f, 0.02f, 0.0f));
			rmi->set_scale(Vector3(1.0f, 0.6f, 1.0f));
			winter_snow->add_child(rmi);
		}

		OmniLight3D *light = memnew(OmniLight3D);
		light->set_name(String("LampLight") + String::num_int64(i));
		light->set_position(Vector3(0, -0.14f, 0));
		light->set_color(Color(1.0f, 0.70f, 0.36f));
		light->set_param(Light3D::PARAM_RANGE, 7.5f);
		light->set_param(Light3D::PARAM_ENERGY, 0.0f);
		light->set_visible(false);
		pivot->add_child(light);
		lamp_light[i] = light;
	}
}

void StonePath::build_gate() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Vector3 p0 = path_point(0.0f);
	Vector3 dir = path_point(0.03f) - p0;
	dir.y = 0.0f;
	dir = dir.normalized();
	Vector3 side(-dir.z, 0.0f, dir.x);
	float ground = 0.0f;
	if (terrain != nullptr) {
		ground = terrain->get_terrain_height(p0.x, p0.z);
	}
	Vector3 base(p0.x, ground, p0.z);
	const Color bark(0.36f, 0.25f, 0.14f);
	meshutil::add_box(this, toon, Vector3(0.12f, 2.35f, 0.12f), base + side * 0.95f + Vector3(0, 1.17f, 0), bark);
	meshutil::add_box(this, toon, Vector3(0.12f, 2.35f, 0.12f), base - side * 0.95f + Vector3(0, 1.17f, 0), bark);
	meshutil::add_box(this, toon, Vector3(2.1f, 0.14f, 0.12f), base + Vector3(0, 2.30f, 0), bark);
	meshutil::add_box(this, toon, Vector3(1.55f, 0.42f, 0.07f), base + Vector3(0, 2.02f, 0), Color(0.62f, 0.47f, 0.29f));
	meshutil::add_box(winter_snow, toon, Vector3(2.16f, 0.04f, 0.16f), base + Vector3(0, 2.38f, 0), Color(0.92f, 0.95f, 1.0f));

	Ref<Font> cjk = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	Label3D *sign = memnew(Label3D);
	sign->set_name("PathSign");
	sign->set_text(String::utf8("湖 岸 小 径"));
	sign->set_font_size(40);
	sign->set_pixel_size(0.004f);
	sign->set_modulate(Color(0.22f, 0.14f, 0.07f, 1.0f));
	if (cjk.is_valid()) {
		sign->set_font(cjk);
	}
	sign->set_position(base + Vector3(0, 2.03f, 0) + dir * 0.05f);
	sign->set_rotation(Vector3(0.0f, std::atan2(-dir.x, -dir.z), 0.0f));
	add_child(sign);
}


void StonePath::build_puddles() {
	const float us[6] = {0.16f, 0.28f, 0.42f, 0.55f, 0.68f, 0.82f};
	for (int i = 0; i < 6; i++) {
		Vector3 p = path_point(us[i]);
		// 往路外侧挪一点，像是路边洼地
		Vector3 next = path_point(Math::min(0.999f, us[i] + 0.01f));
		Vector3 dir = next - p;
		dir.y = 0.0f;
		if (dir.length_squared() > 0.0001f) {
			dir = dir.normalized();
		}
		Vector3 side(-dir.z, 0.0f, dir.x);
		float off = ((i % 2 == 0) ? 1.0f : -1.0f) * (0.55f + (float)(i % 3) * 0.12f);
		p += side * off;
		float ground = 0.0f;
		if (terrain != nullptr) {
			ground = terrain->get_terrain_height(p.x, p.z);
		}
		p.y = ground + 0.012f;

		Ref<CylinderMesh> disc;
		disc.instantiate();
		float r = 0.45f + (float)(i % 3) * 0.12f;
		disc->set_top_radius(1.0f);
		disc->set_bottom_radius(1.0f);
		disc->set_height(0.008f);
		disc->set_radial_segments(20);
		disc->set_rings(1);
		Puddle pd;
		pd.mat.instantiate();
		pd.mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		pd.mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		pd.mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		pd.mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		pd.mat->set_albedo(Color(0.40f, 0.52f, 0.60f, 0.0f));
		pd.mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, make_soft_disc_texture());
		disc->set_material(Ref<Material>(pd.mat));
		pd.mi = memnew(MeshInstance3D);
		pd.mi->set_name(String("Puddle") + String::num_int64(i));
		pd.mi->set_mesh(disc);
		pd.mi->set_position(p);
		pd.mi->set_scale(Vector3(r, 1.0f, r * 0.72f));
		pd.mi->set_visible(false);
		add_child(pd.mi);
		pd.center = p;
		pd.radius = r;
		puddles.push_back(pd);
	}

	// 涟漪圆环池
	for (int i = 0; i < 6; i++) {
		Ref<TorusMesh> torus;
		torus.instantiate();
		torus->set_inner_radius(0.16f);
		torus->set_outer_radius(0.20f);
		torus->set_rings(24);
		torus->set_ring_segments(4);
		Ripple r;
		r.mat.instantiate();
		r.mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		r.mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		r.mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		r.mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		r.mat->set_albedo(Color(0.85f, 0.93f, 0.96f, 0.0f));
		torus->set_material(Ref<Material>(r.mat));
		r.mi = memnew(MeshInstance3D);
		r.mi->set_mesh(torus);
		r.mi->set_visible(false);
		add_child(r.mi);
		ripples.push_back(r);
	}
}

void StonePath::spawn_ripple(const Vector3 &p_pos) {
	for (Ripple &r : ripples) {
		if (!r.active) {
			r.active = true;
			r.t = 0.0f;
			r.dur = 0.9f + (float)(rng() % 100u) * 0.004f;
			r.mi->set_position(p_pos);
			r.mi->set_scale(Vector3(0.45f, 1.0f, 0.45f));
			r.mi->set_visible(true);
			if (r.mat.is_valid()) {
				r.mat->set_albedo(Color(0.85f, 0.93f, 0.96f, 0.40f));
			}
			return;
		}
	}
}

void StonePath::build_moths() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	for (int i = 0; i < 3; i++) {
		if (lamp_pivot[i] == nullptr) {
			continue;
		}
		Vector3 center = lamp_pivot[i]->get_global_position() + Vector3(0.0f, -0.10f, 0.0f);
		for (int j = 0; j < 3; j++) {
			Moth m;
			m.center = center;
			m.phase = (float)((i * 3 + j) * 37 % 628) * 0.01f;
			m.radius = 0.30f + (float)((i + j) % 3) * 0.06f;
			m.speed = 0.75f + (float)((i * 2 + j) % 4) * 0.12f;
			m.lift = 0.05f + (float)(j % 2) * 0.06f;
			m.root = memnew(Node3D);
			m.root->set_name(String("LampMoth") + String::num_int64(i * 3 + j));
			add_child(m.root);
			m.root->set_visible(false);
			meshutil::add_sphere(m.root, toon, 0.014f, Vector3(0, 0, 0), Color(0.32f, 0.24f, 0.16f));
			Ref<BoxMesh> wing;
			wing.instantiate();
			wing->set_size(Vector3(0.065f, 0.002f, 0.045f));
			wing->set_material(meshutil::toon_material(toon, Color(0.78f, 0.72f, 0.52f), 0.3f));
			m.wing_l = memnew(MeshInstance3D);
			m.wing_l->set_mesh(wing);
			m.wing_l->set_position(Vector3(-0.034f, 0.005f, 0));
			m.root->add_child(m.wing_l);
			m.wing_r = memnew(MeshInstance3D);
			m.wing_r->set_mesh(wing);
			m.wing_r->set_position(Vector3(0.034f, 0.005f, 0));
			m.root->add_child(m.wing_r);
			moths.push_back(m);
		}
	}
}

void StonePath::build_light_pools() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	(void)toon;
	for (int i = 0; i < 3; i++) {
		if (lamp_pivot[i] == nullptr) {
			continue;
		}
		Vector3 p = lamp_pivot[i]->get_global_position();
		p.y = (terrain != nullptr) ? terrain->get_terrain_height(p.x, p.z) + 0.015f : p.y - 1.9f;
		Ref<CylinderMesh> disc;
		disc.instantiate();
		disc->set_top_radius(1.0f);
		disc->set_bottom_radius(1.0f);
		disc->set_height(0.006f);
		disc->set_radial_segments(24);
		disc->set_rings(1);
		light_pool_mat[i].instantiate();
		light_pool_mat[i]->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		light_pool_mat[i]->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		light_pool_mat[i]->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
		light_pool_mat[i]->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		light_pool_mat[i]->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		light_pool_mat[i]->set_albedo(Color(1.0f, 0.72f, 0.38f, 0.0f));
		light_pool_mat[i]->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, make_soft_disc_texture());
		disc->set_material(Ref<Material>(light_pool_mat[i]));
		light_pool[i] = memnew(MeshInstance3D);
		light_pool[i]->set_name(String("LightPool") + String::num_int64(i));
		light_pool[i]->set_mesh(disc);
		light_pool[i]->set_position(p);
		light_pool[i]->set_scale(Vector3(1.25f, 1.0f, 1.25f));
		light_pool[i]->set_visible(false);
		add_child(light_pool[i]);
	}
}

void StonePath::make_splash() {
	snd_splash = make_splash_wav();
	splash = memnew(AudioStreamPlayer3D);
	splash->set_name("PuddleSplash");
	splash->set_stream(snd_splash);
	splash->set_volume_db(-13.0f);
	splash->set_max_distance(14.0f);
	add_child(splash);
}

// 0.24 秒的踩水声：短促的低频“啪”+ 水花噪声
Ref<AudioStreamWAV> StonePath::make_splash_wav() {
	const int sr = 22050;
	const float dur = 0.24f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float u = t / dur;
		float f = 190.0f * std::exp(-9.0f * t) + 90.0f;
		float env = Math::clamp(t / 0.006f, 0.0f, 1.0f) * std::exp(-9.0f * t);
		float body = std::sin(6.2831853f * f * t) * env * 0.6f;
		float noise = ((float)((i * 1103515245 + 12345) & 0x7FFF) / 32767.0f - 0.5f) * std::exp(-16.0f * t) * 0.45f;
		float samp = (body + noise) * (1.0f - 0.25f * u);
		int16_t q = (int16_t)Math::clamp(samp * 13000.0f, -32000.0f, 32000.0f);
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
