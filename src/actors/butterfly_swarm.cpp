#include "butterfly_swarm.h"

#include "sky_cycle.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <vector>

#include "mesh_util.h"

using namespace godot;

// —— 圆润翼形网格（2026-10-31 用户反馈"翅膀太方方正正"）——
// 把 2D 轮廓（局部 x/z）挤出成一枚薄翼片：顶面 + 底面 + 侧壁，法线全部显式给，
// 走卡通材质（cull_back 也能两面都实心）。轮廓由椭圆 + 叶根收窄生成，天然圆润。
static Ref<ArrayMesh> make_wing_mesh(const std::vector<Vector2> &p_outline, float p_thickness,
		const Ref<Shader> &p_toon, const Color &p_color) {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	const int n = (int)p_outline.size();
	if (n < 3) {
		return mesh;
	}
	const float h = p_thickness * 0.5f;
	Vector2 c(0, 0);
	for (const Vector2 &p : p_outline) {
		c += p;
	}
	c /= (float)n;

	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);
	st->set_material(meshutil::toon_material(p_toon, p_color));

	const Vector3 up_n(0.0f, 1.0f, 0.0f);
	const Vector3 down_n(0.0f, -1.0f, 0.0f);
	for (int i = 0; i < n; i++) {
		const Vector2 &a = p_outline[i];
		const Vector2 &b = p_outline[(i + 1) % n];
		// 顶面（法线 +Y）：绕序 c,b,a 才朝上
		st->set_normal(up_n);
		st->add_vertex(Vector3(c.x, h, c.y));
		st->set_normal(up_n);
		st->add_vertex(Vector3(b.x, h, b.y));
		st->set_normal(up_n);
		st->add_vertex(Vector3(a.x, h, a.y));
		// 底面（法线 -Y）：绕序 c,a,b
		st->set_normal(down_n);
		st->add_vertex(Vector3(c.x, -h, c.y));
		st->set_normal(down_n);
		st->add_vertex(Vector3(a.x, -h, a.y));
		st->set_normal(down_n);
		st->add_vertex(Vector3(b.x, -h, b.y));
		// 侧壁：外法线 = (dz, 0, -dx)
		Vector3 side_n = Vector3(b.y - a.y, 0.0f, -(b.x - a.x)).normalized();
		st->set_normal(side_n);
		st->add_vertex(Vector3(a.x, h, a.y));
		st->set_normal(side_n);
		st->add_vertex(Vector3(b.x, h, b.y));
		st->set_normal(side_n);
		st->add_vertex(Vector3(b.x, -h, b.y));
		st->set_normal(side_n);
		st->add_vertex(Vector3(a.x, h, a.y));
		st->set_normal(side_n);
		st->add_vertex(Vector3(b.x, -h, b.y));
		st->set_normal(side_n);
		st->add_vertex(Vector3(a.x, -h, a.y));
	}
	st->commit(mesh);
	return mesh;
}

// 叶形轮廓：椭圆采样 + 内缘收窄（根部尖、外缘饱满）；mirror 用于左翅
static std::vector<Vector2> wing_outline(float p_cx, float p_cz, float p_rx, float p_rz,
		bool p_mirror, int p_n = 18) {
	std::vector<Vector2> pts;
	pts.reserve(p_n);
	for (int i = 0; i < p_n; i++) {
		float a = (float)Math::TAU * (float)i / (float)p_n;
		float taper = 0.45f + 0.55f * (0.5f + 0.5f * std::cos(a));
		float x = p_cx + std::cos(a) * p_rx;
		float z = p_cz + std::sin(a) * p_rz * taper;
		if (p_mirror) {
			x = -x;
		}
		pts.push_back(Vector2(x, z));
	}
	return pts;
}

void ButterflySwarm::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();
			anim_t += (float)delta;

			// —— 昼夜 + 天气感知（昼夜阈值统一走 SkyCycle::get_night_factor） ——
			if (sky_cycle == nullptr) {
				Node *parent = get_parent();
				if (parent != nullptr) {
					sky_cycle = parent->get_node_or_null(NodePath("SkyCycle"));
				}
			}
			SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);
			// 夜幕因子缺省 0 = 正午白天、wet 缺省 0 = 干燥（找不到 SkyCycle 时）
			float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			float wet = (sky != nullptr)
					? Math::clamp(sky->get_rain_amount() + sky->get_snow_amount(), 0.0f, 1.0f)
					: 0.0f;

			bool vis = night < 0.98f;
			if (vis != is_visible()) {
				set_visible(vis);
			}
			if (!vis) {
				return; // 夜里整体隐藏，省掉逐只更新
			}

			for (size_t i = 0; i < flyers.size(); ++i) {
				Flyer &f = flyers[i];
				// 雨雪天（wet→1）几乎停止漫游，落到出生平台的草丛里静栖
				f.phase += (float)delta * f.orbit_w * (1.0f - 0.85f * wet);
				float bob = std::sin(anim_t * f.bob_w + f.phase * 0.7f) * f.bob_a
						* (1.0f - 0.8f * wet);
				// 高度：晴湿之间在"正常飞行高度 ↔ 贴地草丛"之间过渡
				//（群节点 y=0.9，出生平台地面约 0.06 → 落点取本地 -0.75 ≈ 离地 9cm）
				float anchor_y = Math::lerp(f.anchor.y, -0.75f, wet);
				Vector3 p = f.anchor + Vector3(
						std::cos(f.phase) * f.orbit_r,
						anchor_y - f.anchor.y + bob + night * 26.0f, // 离场爬升（与鸟一致 +22m 量级，取 26 增加层次）
						std::sin(f.phase) * f.orbit_r);
				// 朝向：沿漫游切线方向；机翼随转弯侧倾（蝴蝶味的关键小细节）
				Vector3 tangent(-std::sin(f.phase) * (f.orbit_w >= 0.0f ? 1.0f : -1.0f), 0.0f,
						std::cos(f.phase) * (f.orbit_w >= 0.0f ? 1.0f : -1.0f));
				if (!first_frame && last_pos[i] != p) {
					// look_at 用世界坐标目标：当前位置（世界）+ 切线方向
					f.root->look_at(f.root->get_global_position() + tangent, Vector3(0.0f, 1.0f, 0.0f));
				}
				// p 是相对群节点的局部坐标（群节点在出生点压平区上空 y=0.9 z=-24）
				f.root->set_position(p);
				last_pos[i] = p;
				// 扑翼：频率快、幅度适中；离场爬升时收翼滑翔，雨雪天蹲在草里收翼
				float a = std::sin(anim_t * f.flap) * 0.95f
						* (1.0f - night * 0.7f) * (1.0f - 0.85f * wet);
				f.wing_r->set_rotation(Vector3(0.0f, 0.0f, a));
				f.wing_l->set_rotation(Vector3(0.0f, 0.0f, -a));
				// 后翅：幅度略小 + 相位滞后，四翅扑动有层次（不是整块板一起扇）
				float ah = std::sin(anim_t * f.flap - 0.55f) * 0.78f
						* (1.0f - night * 0.7f) * (1.0f - 0.85f * wet);
				if (f.hind_r != nullptr) {
					f.hind_r->set_rotation(Vector3(0.0f, 0.0f, ah));
				}
				if (f.hind_l != nullptr) {
					f.hind_l->set_rotation(Vector3(0.0f, 0.0f, -ah));
				}
			}
			first_frame = false;
		} break;
		default:
			break;
	}
}

void ButterflySwarm::build() {
	// 以世界坐标哈希为种：每只蝴蝶参数不同，但整张地图可复现
	Vector3 gp = get_global_position();
	unsigned base = (unsigned)(gp.x * 91.0f + gp.y * 7.0f + gp.z * 57.0f) + 2026u;
	std::mt19937 rng(base);
	auto rnd = [&rng](float lo, float hi) -> float {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};

	flyers.clear();
	last_pos.clear();
	for (int i = 0; i < butterfly_count; ++i) {
		Vector2 off(rnd(-swarm_radius, swarm_radius), rnd(-swarm_radius, swarm_radius));
		Vector3 anchor(off.x, rnd(0.45f, 1.15f), off.y);
		int variant = (int)rnd(0.0f, 3.0f); // 0 橙(君主斑蝶) / 1 白(粉蝶) / 2 蓝斑
		spawn_flyer(anchor, variant, base + (unsigned)i * 131u + 7u);
	}
}

void ButterflySwarm::spawn_flyer(const Vector3 &p_anchor, int p_variant, unsigned p_seed) {
	std::mt19937 rng(p_seed);
	auto rnd = [&rng](float lo, float hi) -> float {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};

	// —— MC 式扁平亮色块配色 ——
	Color wing_hi, wing_lo, body_c(0.22f, 0.18f, 0.15f);
	switch (p_variant) {
		case 0: // 君主斑蝶：亮橙 + 深橙缘
			wing_hi = Color(0.93f, 0.52f, 0.14f);
			wing_lo = Color(0.62f, 0.28f, 0.08f);
			break;
		case 1: // 粉蝶：奶白 + 灰缘
			wing_hi = Color(0.95f, 0.94f, 0.86f);
			wing_lo = Color(0.66f, 0.65f, 0.60f);
			body_c = Color(0.30f, 0.30f, 0.26f);
			break;
		default: // 蓝斑蝶：天蓝 + 深蓝缘
			wing_hi = Color(0.36f, 0.62f, 0.95f);
			wing_lo = Color(0.16f, 0.30f, 0.62f);
			break;
	}

	Flyer f;
	f.anchor = p_anchor;
	f.orbit_r = rnd(0.35f, 1.30f);
	f.orbit_w = rnd(0.55f, 1.15f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
	f.phase = rnd(0.0f, 6.2831f);
	f.flap = rnd(11.0f, 16.0f); // 蝴蝶扑频比鸟快
	f.bob_a = rnd(0.06f, 0.16f);
	f.bob_w = rnd(1.7f, 3.1f);

	f.root = memnew(Node3D);
	f.root->set_name("Butterfly" + String::num_int64(flyers.size()));
	add_child(f.root);

	Ref<Shader> toon = meshutil::load_toon_shader();

	// 身体：细长小柱（沿 -Z 前进方向）
	{
		Ref<CapsuleMesh> body;
		body.instantiate();
		body->set_radius(0.011f);
		body->set_height(0.075f);
		body->set_material(meshutil::toon_material(toon, body_c));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(body);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(1.5707964f, 0.0f, 0.0f), Vector3(1, 1, 1)); // 躺平沿 Z
		t.set_basis(b);
		mi->set_transform(t);
		f.root->add_child(mi);
	}
	// 触角：两根极细短须（MC 简化成一个 V 字观感即可）
	meshutil::add_box(f.root, toon, Vector3(0.008f, 0.002f, 0.008f),
			Vector3(0.012f, 0.008f, -0.045f), body_c, Vector3(0.0f, 0.0f, 0.5f));
	meshutil::add_box(f.root, toon, Vector3(0.008f, 0.002f, 0.008f),
			Vector3(-0.012f, 0.008f, -0.045f), body_c, Vector3(0.0f, 0.0f, -0.5f));

	// —— 四翅：圆润叶形翼片（2026-10-31 用户反馈"翅膀太方方正正"）——
	// 前翅 + 后翅各一对；翼片用"椭圆 + 叶根收窄"的轮廓挤出薄片，走卡通材质，
	// 深色斑纹用小一号的椭圆薄片叠在翼面之上（y 抬高 0.7mm 防 z-fighting）。
	auto add_wing = [&](Node3D *p_parent, const std::vector<Vector2> &p_outline,
			const Color &p_color, float p_y) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(make_wing_mesh(p_outline, 0.0024f, toon, p_color));
		mi->set_position(Vector3(0.0f, p_y, 0.0f));
		p_parent->add_child(mi);
	};

	f.wing_r = memnew(Node3D);
	f.wing_r->set_name("WingR");
	f.wing_r->set_position(Vector3(0.012f, 0.004f, 0.0f));
	f.root->add_child(f.wing_r);
	add_wing(f.wing_r, wing_outline(0.046f, -0.006f, 0.044f, 0.034f, false), wing_hi, 0.0f);
	add_wing(f.wing_r, wing_outline(0.058f, -0.002f, 0.015f, 0.012f, false), wing_lo, 0.0019f);

	f.wing_l = memnew(Node3D);
	f.wing_l->set_name("WingL");
	f.wing_l->set_position(Vector3(-0.012f, 0.004f, 0.0f));
	f.root->add_child(f.wing_l);
	add_wing(f.wing_l, wing_outline(0.046f, -0.006f, 0.044f, 0.034f, true), wing_hi, 0.0f);
	add_wing(f.wing_l, wing_outline(0.058f, -0.002f, 0.015f, 0.012f, true), wing_lo, 0.0019f);

	// 后翅一对（更小、偏尾侧，圆润小叶形）
	f.hind_r = memnew(Node3D);
	f.hind_r->set_name("HindWingR");
	f.hind_r->set_position(Vector3(0.011f, 0.0035f, 0.030f));
	f.root->add_child(f.hind_r);
	add_wing(f.hind_r, wing_outline(0.028f, 0.000f, 0.030f, 0.022f, false), wing_hi, 0.0f);
	add_wing(f.hind_r, wing_outline(0.040f, 0.004f, 0.010f, 0.009f, false), wing_lo, 0.0019f);

	f.hind_l = memnew(Node3D);
	f.hind_l->set_name("HindWingL");
	f.hind_l->set_position(Vector3(-0.011f, 0.0035f, 0.030f));
	f.root->add_child(f.hind_l);
	add_wing(f.hind_l, wing_outline(0.028f, 0.000f, 0.030f, 0.022f, true), wing_hi, 0.0f);
	add_wing(f.hind_l, wing_outline(0.040f, 0.004f, 0.010f, 0.009f, true), wing_lo, 0.0019f);

	flyers.push_back(f);
	last_pos.push_back(p_anchor);
}

void ButterflySwarm::_bind_methods() {
	// 纯运行时表现组件，无对外方法；保留占位便于后续加密度/半径属性
}
