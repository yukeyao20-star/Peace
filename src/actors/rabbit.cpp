#include "rabbit.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/animatable_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "footprint_manager.h"
#include "mesh_util.h"
#include "sky_cycle.h" // get_rain_amount/get_snow_amount（第 25 轮加的内联 getter）

using namespace godot;

namespace {

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

} // namespace

void Rabbit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			home = get_global_position();
			unsigned seed = (unsigned)(home.x * 73856093.0) ^ ((unsigned)(home.z * 19349663.0) << 13);
			std::mt19937 rng(seed);
			std::uniform_real_distribution<float> d01(0.0f, 1.0f);
			is_sleeper = (d01(rng) < 0.50f); // 初始一半睡眠，一半夜行漫游
			sleep_cycle_t = 12.0f + d01(rng) * 20.0f;
			idle_t = 1.0f;
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();
			Vector3 pos = get_global_position();

			// —— 昼夜/天气感知：读兄弟 SkyCycle 的因子（找不到缺省白天/晴）——
			if (sky_cycle == nullptr) {
				Node *parent = get_parent();
				if (parent != nullptr) {
					sky_cycle = Object::cast_to<Node3D>(parent->get_node_or_null(NodePath("SkyCycle")));
				}
			}
			SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);
			float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			float wet = 0.0f;
			if (sky != nullptr) {
				wet = Math::clamp(sky->get_rain_amount() + sky->get_snow_amount(), 0.0f, 1.0f);
			}

			bool is_night = (night > 0.45f);
			if (is_night) {
				sleep_cycle_t -= (float)delta;
				if (sleep_cycle_t <= 0.0f) {
					unsigned seed = (unsigned)(pos.x * 61.0f + pos.z * 23.0f + delta * 1000.0f);
					std::mt19937 rng(seed);
					std::uniform_real_distribution<float> d01(0.0f, 1.0f);
					is_sleeper = (d01(rng) < 0.55f);
					sleep_cycle_t = 14.0f + d01(rng) * 24.0f;
				}
			} else {
				is_sleeper = false;
			}

			bool should_sleep = (is_night && is_sleeper) || (wet > 0.5f);
			if (should_sleep) {
				// 夜幕卧睡 / 大雨大雪躲避：回窝心卧下睡觉
				Vector3 to_home = home - pos;
				to_home.y = 0.0f;
				if (to_home.length() > 0.25f) {
					if (!hopping) { // 半路被夜色打断也在返途上：目标强制回窝
						target = home;
						hopping = true;
						hop_t = 0.0f;
					}
				} else {
					hopping = false;
					pos = Vector3(home.x, home.y, home.z);
					if (!sleeping) {
						sleeping = true;
					}
					// 真实卧睡姿态：身体贴地、四爪缩于腹底、微扁安详深呼吸
					float b = 1.0f + 0.024f * std::sin((float)delta * 2.0f + pos.x);
					if (model != nullptr) {
						model->set_position(Vector3(0.0f, -0.05f, 0.0f));
						model->set_scale(Vector3(1.06f * b, 0.74f * b, 1.02f));
					}
					set_global_position(pos);
					break; // 睡觉跳过游走
				}
			} else if (sleeping) { // 醒来或天亮：起身恢复
				sleeping = false;
				if (model != nullptr) {
					model->set_position(Vector3(0.0f, 0.0f, 0.0f));
					model->set_scale(Vector3(1.0f, 1.0f, 1.0f));
				}
			}

			if (!hopping) {
				idle_t -= (float)delta;
				// 待机轻微呼吸起伏/动鼻子
				if (model != nullptr && !sleeping) {
					float wiggle = std::sin(idle_t * 10.0f) * 0.008f;
					model->set_position(Vector3(0.0f, wiggle, 0.0f));
				}
				// 玩家靠近惊起：在 1.35m 内受惊立刻起跳蹦开
				if (!sleeping) {
					Node *parent = get_parent();
					if (parent != nullptr) {
						Node *pl = parent->get_node_or_null(NodePath("Player"));
						if (pl != nullptr) {
							Node3D *pl3d = Object::cast_to<Node3D>(pl);
							if (pl3d != nullptr) {
								Vector3 pp = pl3d->get_global_position();
								if (Vector2(pos.x - pp.x, pos.z - pp.z).length() < 1.35f) {
									idle_t = 0.0f;
								}
							}
						}
					}
				}
				if (idle_t <= 0.0f) {
					// 挑一个院内的目标点（用世界坐标哈希做种，可复现）
					unsigned seed = (unsigned)(pos.x * 61.0f + pos.z * 23.0f + idle_t * -1000.0f) + 5u;
					std::mt19937 rng(seed);
					auto rnd = [&rng](float lo, float hi) -> float {
						std::uniform_real_distribution<float> d(lo, hi);
						return d(rng);
					};
					float a = rnd(0.0f, 6.2831f);
					float r = rnd(1.0f, wander_radius);
					target = home + Vector3(std::cos(a) * r, 0.0f, std::sin(a) * r);
					hopping = true;
					hop_t = 0.0f;
				}
			} else {
				float prev_phase = Math::fmod(hop_t, hop_dur) / hop_dur;
				hop_t += (float)delta;
				float curr_phase = Math::fmod(hop_t, hop_dur) / hop_dur;
				Vector3 to_t = target - pos;
				to_t.y = 0.0f;
				float remain = to_t.length();
				if (remain < 0.12f) { // 到达目标点，短暂小憩
					hopping = false;
					idle_t = lerpf(0.5f, 1.6f, Math::fmod(pos.x * 7.3f + pos.z * 3.1f, 1.0f));
					pos = Vector3(target.x, home.y, target.z); // 落回地面
					if (model != nullptr) {
						model->set_position(Vector3(0.0f, 0.0f, 0.0f));
					}
					if (FootprintManager::get_singleton() != nullptr && to_t.length_squared() > 0.001f) {
						float yaw = Math::atan2(to_t.x, to_t.z);
						FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_RABBIT, pos, yaw, 0.85f);
					}
				} else {
					// 每次起落接地触地留印
					if (curr_phase < prev_phase && FootprintManager::get_singleton() != nullptr) {
						float yaw = Math::atan2(to_t.x, to_t.z);
						FootprintManager::get_singleton()->spawn_footprint(FootprintManager::TYPE_RABBIT, pos, yaw, 0.85f);
					}
					float step = Math::min(2.4f * (float)delta, remain);
					pos += to_t.normalized() * step;
					// 跳跃弧线：单跳周期内的半波 sin
					float arc = std::sin(Math::fmod(hop_t, hop_dur) / hop_dur * 3.1415926f) * 0.26f;
					pos.y = home.y + arc;
					// 朝向前进方向（model 转向，节点保持不转）
					if (model != nullptr) {
						Vector3 fwd = pos + to_t.normalized();
						if (Math::abs(to_t.normalized().dot(Vector3(0.0f, 1.0f, 0.0f))) < 0.99f) {
							model->look_at(fwd, Vector3(0.0f, 1.0f, 0.0f));
						}
					}
				}
			}
			set_global_position(pos);
		} break;
		default:
			break;
	}
}

void Rabbit::build() {
	const Color fur(0.70f, 0.63f, 0.55f);     // 灰褐毛色
	const Color fur_dark(0.58f, 0.51f, 0.44f);
	const Color tail_c(0.92f, 0.90f, 0.88f);  // 白尾
	const Color ear_in(0.85f, 0.65f, 0.65f);  // 耳内粉

	Ref<Shader> shader = meshutil::load_toon_shader();
	model = memnew(Node3D);
	model->set_name("Model");
	add_child(model);

	// 身体（椭球）+ 头 + 尾
	{
		Ref<SphereMesh> body;
		body.instantiate();
		body->set_radius(0.15f);
		body->set_height(0.28f);
		body->set_radial_segments(9);
		body->set_rings(4);
		body->set_material(meshutil::toon_material(shader, fur));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(body);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.9f, 0.85f, 1.3f)); // 沿 -Z 拉长
		t.set_basis(b);
		t.set_origin(Vector3(0.0f, 0.15f, 0.02f));
		mi->set_transform(t);
		model->add_child(mi);
	}
	meshutil::add_box(model, shader, Vector3(0.19f, 0.18f, 0.19f), Vector3(0.0f, 0.24f, -0.16f), fur);
	{
		Ref<SphereMesh> tail;
		tail.instantiate();
		tail->set_radius(0.05f);
		tail->set_height(0.09f);
		tail->set_radial_segments(6);
		tail->set_rings(3);
		tail->set_material(meshutil::toon_material(shader, tail_c));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(tail);
		mi->set_position(Vector3(0.0f, 0.17f, 0.22f));
		model->add_child(mi);
	}
	// 长耳朵：两片竖板，微微外撇
	meshutil::add_box(model, shader, Vector3(0.045f, 0.22f, 0.02f), Vector3(-0.055f, 0.42f, -0.14f),
			fur_dark, Vector3(-0.15f, 0.0f, -0.12f));
	meshutil::add_box(model, shader, Vector3(0.045f, 0.22f, 0.02f), Vector3(0.055f, 0.42f, -0.14f),
			fur_dark, Vector3(-0.15f, 0.0f, 0.12f));
	meshutil::add_box(model, shader, Vector3(0.02f, 0.16f, 0.01f), Vector3(-0.053f, 0.42f, -0.128f),
			ear_in, Vector3(-0.15f, 0.0f, -0.12f));
	meshutil::add_box(model, shader, Vector3(0.02f, 0.16f, 0.01f), Vector3(0.053f, 0.42f, -0.128f),
			ear_in, Vector3(-0.15f, 0.0f, 0.12f));
	// 前爪一对
	meshutil::add_box(model, shader, Vector3(0.05f, 0.06f, 0.05f), Vector3(-0.07f, 0.04f, -0.16f), fur);
	meshutil::add_box(model, shader, Vector3(0.05f, 0.06f, 0.05f), Vector3(0.07f, 0.04f, -0.16f), fur);

	// 物理碰撞体：挂载在 model 节点下，随朝向实时旋转；设置在 Layer 2
	collider_body = memnew(AnimatableBody3D);
	collider_body->set_name("Collider");
	collider_body->set_sync_to_physics(false);
	collider_body->set_collision_layer_value(1, false);
	collider_body->set_collision_layer_value(2, true);
	collider_body->set_collision_mask(0);

	// 1. 身体躯干圆柱碰撞
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<CylinderShape3D> rcy;
	rcy.instantiate();
	rcy->set_radius(0.18f);
	rcy->set_height(0.35f);
	cs->set_shape(rcy);
	cs->set_position(Vector3(0.0f, 0.18f, 0.0f));
	collider_body->add_child(cs);

	// 2. 兔头专用前向碰撞体（防止兔头穿进脚底）
	CollisionShape3D *head_cs = memnew(CollisionShape3D);
	Ref<SphereShape3D> rsp;
	rsp.instantiate();
	rsp->set_radius(0.14f);
	head_cs->set_shape(rsp);
	head_cs->set_position(Vector3(0.0f, 0.22f, -0.16f));
	collider_body->add_child(head_cs);

	model->add_child(collider_body);
}
