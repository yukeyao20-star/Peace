#include "farm_animal.h"

#include <cmath>

#include <godot_cpp/classes/animatable_body3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "footprint_manager.h"
#include "sky_cycle.h"

using namespace godot;

namespace {
inline float lerpf(float a, float b, float t) {
	return a + (b - a) * t;
}
} // namespace

void FarmAnimal::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_kind", "k"), &FarmAnimal::set_kind);
	ClassDB::bind_method(D_METHOD("get_kind"), &FarmAnimal::get_kind);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "kind", PROPERTY_HINT_RANGE, "0,3,1"),
			"set_kind", "get_kind");
}

void FarmAnimal::set_kind(int p_kind) {
	kind = Math::clamp(p_kind, 0, 3);
}

Ref<StandardMaterial3D> FarmAnimal::mat_of(const Color &p_c) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(p_c);
	m->set_roughness(0.95f);
	return m;
}

void FarmAnimal::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return; // 编辑器里不生成模型（GDExtension 类永远 @tool）
			}
			Vector3 gp = get_global_position();
			home = Vector2(gp.x, gp.z);
			target = home;
			uint32_t seed = (uint32_t)std::llround(gp.x * 73856093.0)
					^ ((uint32_t)std::llround(gp.z * 19349663.0) << 13) ^ (uint32_t)kind;
			rng.seed(seed);
			speed = (kind <= 1) ? 1.65f : 0.95f; // 猫狗轻快巡游，牛羊从容漫步

			std::uniform_real_distribution<float> d01(0.0f, 1.0f);
			// 初始错开个体睡眠节律（约 55% 卧睡，45% 夜行漫游，避免全员同频）
			is_sleeper = (d01(rng) < 0.55f);
			sleep_cycle_t = 15.0f + d01(rng) * 25.0f;

			build_model();
			add_to_group("farm_animals"); // TerrainBuilder 收集"踩草者"用
			set_process(true);
		} break;

		case NOTIFICATION_PROCESS: {
			float night = 0.0f, wet = 0.0f;
			if (sky == nullptr) {
				Node *p = get_parent();
				if (p != nullptr) {
					sky = Object::cast_to<SkyCycle>(p->get_node_or_null(NodePath("SkyCycle")));
				}
			}
			if (sky != nullptr) {
				night = sky->get_night_factor();
				wet = Math::clamp(sky->get_rain_amount() + sky->get_snow_amount(), 0.0f, 1.0f);
			}
			tick_behavior((float)get_process_delta_time(), night, wet);
		} break;

		default:
			break;
	}
}

// 拼一只低多边形小动物：全部基础网格 + 纯色材质，无外部资源。
// 模型正面朝 +Z（与 Player 模型同一约定，朝向直接用 look_at 思路）。
void FarmAnimal::build_model() {
	body = memnew(Node3D);
	add_child(body);

	auto part = [this](Node3D *parent, Ref<Mesh> p_mesh, const Ref<Material> &p_mat,
			const Vector3 &p_pos, const Vector3 &p_scale = Vector3(1.0, 1.0, 1.0),
			float p_tilt_x = 0.0f) -> MeshInstance3D * {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(p_mesh);
		mi->set_material_override(p_mat);
		mi->set_position(p_pos);
		mi->set_scale(p_scale);
		if (p_tilt_x != 0.0f) {
			mi->set_rotation(Vector3(p_tilt_x, 0.0f, 0.0f));
		}
		parent->add_child(mi);
		return mi;
	};
	auto box = [](float sx, float sy, float sz) {
		Ref<BoxMesh> m;
		m.instantiate();
		m->set_size(Vector3(sx, sy, sz));
		return m;
	};
	auto ball = [](float r, float h) {
		Ref<SphereMesh> m;
		m.instantiate();
		m->set_radius(r);
		m->set_height(h);
		m->set_radial_segments(10);
		m->set_rings(6);
		return m;
	};
	auto stick = [](float r, float h) {
		Ref<CylinderMesh> m;
		m.instantiate();
		m->set_top_radius(r * 0.8f);
		m->set_bottom_radius(r);
		m->set_height(h);
		m->set_radial_segments(6);
		return m;
	};

	if (kind == 3) {
		// —— 羊：蓬松白球身体 + 深灰脸 + 四根黑细腿（MC 绵羊即视感） ——
		Ref<Material> wool = mat_of(Color(0.92f, 0.90f, 0.86f));
		Ref<Material> face = mat_of(Color(0.28f, 0.26f, 0.25f));
		part(body, ball(0.55f, 0.95f), wool, Vector3(0.0, 0.72, 0.0), Vector3(1.0, 0.9, 1.25));
		head = memnew(Node3D);
		body->add_child(head);
		head->set_position(Vector3(0.0, 0.78, 0.62));
		part(head, ball(0.22f, 0.34f), face, Vector3(0.0, 0.0, 0.06), Vector3(0.85, 1.0, 1.0));
		part(head, ball(0.10f, 0.16f), wool, Vector3(0.0, 0.16, -0.02)); // 头顶一撮毛
		for (int i = 0; i < 4; i++) {
			Vector3 lp(i % 2 == 0 ? -0.26f : 0.26f, 0.24, i < 2 ? 0.32f : -0.32f);
			legs[i] = part(body, stick(0.055f, 0.5f), face, lp);
		}
		tail = part(body, ball(0.09f, 0.15f), wool, Vector3(0.0, 0.72, -0.68)); // 小尾巴球
	} else if (kind == 2) {
		// —— 牛：白底箱体 + 黑色斑块 + 粉色鼻头 + 小短角 ——
		Ref<Material> hide = mat_of(Color(0.90f, 0.88f, 0.84f));
		Ref<Material> patch = mat_of(Color(0.16f, 0.15f, 0.15f));
		Ref<Material> nose = mat_of(Color(0.85f, 0.62f, 0.60f));
		Ref<Material> horn = mat_of(Color(0.80f, 0.74f, 0.60f));
		part(body, box(0.72f, 0.72f, 1.25f), hide, Vector3(0.0, 0.82, 0.0));
		part(body, box(0.30f, 0.34f, 0.02f), patch, Vector3(-0.365f, 0.86, 0.18)); // 左斑块
		part(body, box(0.36f, 0.30f, 0.02f), patch, Vector3(0.365f, 0.78, -0.22)); // 右斑块
		part(body, box(0.24f, 0.26f, 0.30f), patch, Vector3(0.10f, 1.22, -0.30)); // 背斑
		head = memnew(Node3D);
		body->add_child(head);
		head->set_position(Vector3(0.0, 0.98, 0.72));
		part(head, box(0.40f, 0.42f, 0.42f), hide, Vector3(0.0, 0.0, 0.05));
		part(head, box(0.30f, 0.20f, 0.12f), nose, Vector3(0.0, -0.10, 0.28)); // 粉鼻
		part(head, stick(0.045f, 0.22f), horn, Vector3(-0.20f, 0.26, 0.0), Vector3(1, 1, 1), -0.5f);
		part(head, stick(0.045f, 0.22f), horn, Vector3(0.20f, 0.26, 0.0), Vector3(1, 1, 1), 0.5f);
		for (int i = 0; i < 4; i++) {
			Vector3 lp(i % 2 == 0 ? -0.24f : 0.24f, 0.23, i < 2 ? 0.42f : -0.42f);
			legs[i] = part(body, stick(0.075f, 0.46f), patch, lp);
		}
		tail = part(body, stick(0.035f, 0.55f), patch, Vector3(0.0, 0.85, -0.68), Vector3(1, 1, 1), 0.6f); // 尾
	} else {
		// —— 猫 / 狗：小号身材，猫橘狗棕；狗垂耳猫尖耳，狗摇尾上翘 ——
		bool cat = (kind == 0);
		Ref<Material> fur = mat_of(cat ? Color(0.85f, 0.55f, 0.25f) : Color(0.55f, 0.38f, 0.22f));
		Ref<Material> chest = mat_of(cat ? Color(0.95f, 0.90f, 0.82f) : Color(0.80f, 0.70f, 0.55f));
		float body_y = cat ? 0.34f : 0.42f;
		part(body, ball(0.20f, 0.55f), fur, Vector3(0.0, body_y, 0.0), Vector3(0.9, 1.0, 1.35));
		part(body, ball(0.13f, 0.22f), chest, Vector3(0.0, body_y - 0.02f, 0.16), Vector3(0.8, 0.9, 1.0));
		head = memnew(Node3D);
		body->add_child(head);
		head->set_position(Vector3(0.0, body_y + 0.14f, 0.30f));
		part(head, ball(0.15f, 0.24f), fur, Vector3(0.0, 0.0, 0.02));
		if (cat) { // 尖耳朵
			part(head, stick(0.045f, 0.12f), fur, Vector3(-0.09f, 0.15, 0.0));
			part(head, stick(0.045f, 0.12f), fur, Vector3(0.09f, 0.15, 0.0));
		} else { // 垂耳朵
			part(head, box(0.06f, 0.16f, 0.10f), fur, Vector3(-0.15f, 0.02, 0.0));
			part(head, box(0.06f, 0.16f, 0.10f), fur, Vector3(0.15f, 0.02, 0.0));
		}
		for (int i = 0; i < 4; i++) {
			Vector3 lp(i % 2 == 0 ? -0.10f : 0.10f, body_y * 0.5f - 0.02f, i < 2 ? 0.18f : -0.18f);
			legs[i] = part(body, stick(0.030f, body_y), fur, lp);
		}
		tail = part(body, stick(0.022f, 0.30f), fur, Vector3(0.0, body_y + 0.10f, -0.30),
				Vector3(1, 1, 1), cat ? -0.9f : -0.6f); // 猫尾高翘、狗尾斜举
	}

	if (tail != nullptr) {
		tail_rest_rot = tail->get_rotation();
	}
	head_rest_pos = head->get_position();

	// 物理碰撞体：挂载在 body 节点下，随身体旋转实时同步朝向；设置在 Layer 2
	collider_body = memnew(AnimatableBody3D);
	collider_body->set_name("Collider");
	collider_body->set_sync_to_physics(false);
	collider_body->set_collision_layer_value(1, false);
	collider_body->set_collision_layer_value(2, true);
	collider_body->set_collision_mask(0);

	// 1. 躯干主碰撞体
	collider_shape = memnew(CollisionShape3D);
	Ref<CylinderShape3D> shape;
	shape.instantiate();
	if (kind == 2) { // 牛
		shape->set_radius(0.44f);
		shape->set_height(1.10f);
		collider_shape->set_position(Vector3(0.0f, 0.60f, -0.05f));
	} else if (kind == 3) { // 羊
		shape->set_radius(0.38f);
		shape->set_height(0.85f);
		collider_shape->set_position(Vector3(0.0f, 0.48f, -0.05f));
	} else if (kind == 1) { // 狗
		shape->set_radius(0.24f);
		shape->set_height(0.55f);
		collider_shape->set_position(Vector3(0.0f, 0.30f, -0.04f));
	} else { // 猫
		shape->set_radius(0.18f);
		shape->set_height(0.42f);
		collider_shape->set_position(Vector3(0.0f, 0.22f, -0.03f));
	}
	collider_shape->set_shape(shape);
	collider_body->add_child(collider_shape);

	// 2. 头部专用前向碰撞体（圆球包络头颅、口鼻与犄角，随朝向彻底消除头部穿模）
	head_shape = memnew(CollisionShape3D);
	Ref<SphereShape3D> h_shape;
	h_shape.instantiate();
	if (kind == 2) { // 牛头与口鼻
		h_shape->set_radius(0.42f);
		head_shape->set_position(Vector3(0.0f, 0.92f, 0.70f));
	} else if (kind == 3) { // 羊脸
		h_shape->set_radius(0.34f);
		head_shape->set_position(Vector3(0.0f, 0.78f, 0.58f));
	} else if (kind == 1) { // 狗头
		h_shape->set_radius(0.22f);
		head_shape->set_position(Vector3(0.0f, 0.44f, 0.32f));
	} else { // 猫头
		h_shape->set_radius(0.18f);
		head_shape->set_position(Vector3(0.0f, 0.36f, 0.26f));
	}
	head_shape->set_shape(h_shape);
	collider_body->add_child(head_shape);

	body->add_child(collider_body);
}

// 向下射线求地面高度；打不到（悬空/出界）返回 1e9 表示不可站。
float FarmAnimal::ground_y(float p_x, float p_z) {
	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	if (space == nullptr) {
		return 1e9f;
	}
	Ref<PhysicsRayQueryParameters3D> ray = PhysicsRayQueryParameters3D::create(
			Vector3(p_x, 8.0, p_z), Vector3(p_x, -12.0, p_z));
	ray->set_collision_mask(1); // 仅检测第 1 层地形与建筑，绝不误击动物自身的 Layer 2
	Dictionary hit = space->intersect_ray(ray);
	if (hit.is_empty()) {
		return 1e9f;
	}
	Vector3 hp = hit["position"];
	return hp.y;
}

void FarmAnimal::pick_target() {
	std::uniform_real_distribution<float> ang(0.0f, 6.2831f);
	std::uniform_real_distribution<float> rad(2.0f, home_radius);
	for (int tries = 0; tries < 8; tries++) {
		float a = ang(rng);
		float r = rad(rng);
		Vector2 t = home + Vector2(std::cos(a), std::sin(a)) * r;
		float gy = ground_y(t.x, t.y);
		// 不下水（水面 -0.55 留 0.1 余量）、不上房顶（不高于出生地 1.5m）
		if (gy < 1e8f && gy > -0.45f && (home_ground_y < -1e8f || gy < home_ground_y + 1.5f)) {
			target = t;
			return;
		}
	}
	// 找不到就原地再歇一轮
}

void FarmAnimal::tick_behavior(float p_delta, float p_night, float p_wet) {
	if (body == nullptr) {
		return;
	}
	Vector3 gp = get_global_position();

	// —— 贴地（每帧一次射线，代价可忽略）；拒绝贴上明显高于出生地的面
	// （房顶/桥栏等结构：出生时锚定地面，之后只接受 ±1.5m 以内的命中） ——
	float gy = ground_y(gp.x, gp.z);
	if (gy < 1e8f) {
		if (!grounded_once) {
			home_ground_y = gy;
		}
		if (gy < home_ground_y + 1.5f) {
			gp.y = gy;
			set_global_position(gp);
			grounded_once = true;
		}
	}
	if (!grounded_once) {
		return;
	}

	anim_time += p_delta;

	// —— 昼夜与天气节律：一部分卧睡，一部分走动漫游（绝非全部站着不动） ——
	bool is_night = (p_night > 0.45f);
	bool huddling = (!is_night && p_wet > 0.5f);

	if (is_night) {
		sleep_cycle_t -= p_delta;
		if (sleep_cycle_t <= 0.0f) {
			std::uniform_real_distribution<float> pick(0.0f, 1.0f);
			if (is_sleeper) {
				// 睡醒了：70% 概率站起来走动伸懒腰，30% 继续睡会儿
				if (pick(rng) < 0.70f) {
					is_sleeper = false;
					sleep_cycle_t = 18.0f + pick(rng) * 28.0f; // 夜间游走 18~46 秒
				} else {
					sleep_cycle_t = 15.0f + pick(rng) * 25.0f;
				}
			} else {
				// 走累了：70% 概率找平地卧下入睡，30% 继续闲逛
				if (pick(rng) < 0.70f) {
					is_sleeper = true;
					sleep_cycle_t = 22.0f + pick(rng) * 35.0f; // 卧睡 22~57 秒
				} else {
					sleep_cycle_t = 15.0f + pick(rng) * 25.0f;
				}
			}
		}
	} else {
		// 白天全部醒着
		is_sleeper = false;
	}

	bool should_sleep = (is_night && is_sleeper) || huddling;

	// 平滑卧倒/起身权重过渡 (0 站立 ~ 1 卧倒)
	float target_lie = should_sleep ? 1.0f : 0.0f;
	lie_down_weight = Math::move_toward(lie_down_weight, target_lie, (float)(p_delta * (should_sleep ? 1.5f : 2.2f)));

	// 身体下落高度（卧倒时腹部贴地）
	float drop_amount = (kind == 2) ? 0.36f : ((kind == 3) ? 0.30f : ((kind == 1) ? 0.18f : 0.14f));
	body->set_position(Vector3(0.0f, -drop_amount * lie_down_weight, 0.0f));

	// 腿部折叠与收拢（趴下卧姿：前腿后折、后腿前折缩于腹下）
	for (int i = 0; i < 4; i++) {
		if (legs[i] != nullptr) {
			float fold_pitch = (i < 2) ? -1.25f : 1.15f;
			float out_yaw = (i % 2 == 0) ? -0.12f : 0.12f;
			Vector3 target_rot = Vector3(fold_pitch * lie_down_weight, out_yaw * lie_down_weight, 0.0f);
			legs[i]->set_rotation(legs[i]->get_rotation().lerp(target_rot, Math::clamp(p_delta * 4.0f, 0.0f, 1.0f)));
			legs[i]->set_scale(Vector3(1.0f, lerpf(1.0f, 0.35f, lie_down_weight), 1.0f));
		}
	}

	// 头部卧姿姿态
	if (head != nullptr) {
		Vector3 hp = head_rest_pos;
		hp.y -= (drop_amount * 0.45f + 0.10f) * lie_down_weight;
		hp.z += 0.04f * lie_down_weight;
		head->set_position(hp);
		float tilt_dir = (kind % 2 == 0) ? 1.0f : -1.0f;
		Vector3 lie_rot(0.24f * lie_down_weight, 0.20f * tilt_dir * lie_down_weight, 0.08f * tilt_dir * lie_down_weight);
		head->set_rotation(head->get_rotation().lerp(lie_rot, Math::clamp(p_delta * 4.0f, 0.0f, 1.0f)));
	}

	// 呼吸与躯干体态（卧睡时腹部贴地微鼓、呼吸更舒缓安详）
	float sleep_breathe = 1.0f + 0.024f * std::sin(anim_time * 1.35f);
	float awake_breathe = 1.0f + 0.022f * std::sin(anim_time * 2.6f);
	float b_scale = lerpf(awake_breathe, sleep_breathe, lie_down_weight);
	body->set_scale(Vector3(
			body_rest_scale.x * b_scale * lerpf(1.0f, 1.06f, lie_down_weight),
			body_rest_scale.y * b_scale * lerpf(1.0f, 0.82f, lie_down_weight),
			body_rest_scale.z * lerpf(1.0f, 1.02f, lie_down_weight)));

	if (tail != nullptr) {
		if (lie_down_weight > 0.6f) {
			// 卧睡时尾巴贴地静止
			tail->set_rotation(tail->get_rotation().lerp(tail_rest_rot, 0.15f));
		} else {
			// 清醒时自然摇动
			if (kind == 1) { // 狗：兴奋摇尾
				tail->set_rotation(Vector3(tail_rest_rot.x, tail_rest_rot.y + std::sin(anim_time * 7.5f) * 0.42f, tail_rest_rot.z));
			} else if (kind == 0) { // 猫：尾尖轻缓卷翘甩动
				tail->set_rotation(Vector3(tail_rest_rot.x + std::sin(anim_time * 2.0f) * 0.12f,
						tail_rest_rot.y + std::sin(anim_time * 2.4f) * 0.22f, tail_rest_rot.z));
			} else { // 牛 / 羊：周期性甩尾驱蝇
				float sw = std::sin(anim_time * 1.5f);
				if (std::abs(sw) > 0.65f) {
					tail->set_rotation(Vector3(tail_rest_rot.x, tail_rest_rot.y + std::sin(anim_time * 8.0f) * 0.28f, tail_rest_rot.z));
				} else {
					tail->set_rotation(tail_rest_rot);
				}
			}
		}
	}

	// 若处于深度卧睡状态，不执行迈步移动，保持安睡
	if (lie_down_weight > 0.85f) {
		return;
	}

	// —— 状态机 ——
	state_t -= p_delta;
	switch (state) {
		case 0: { // 待机发呆/四处张望（缩短静止时间，增加头部转动）
			if (head != nullptr) {
				float look_yaw = std::sin(anim_time * 1.3f) * 0.22f;
				float look_pitch = std::cos(anim_time * 0.85f) * 0.08f;
				head->set_rotation(Vector3(look_pitch, look_yaw, 0.0f));
				head->set_position(head_rest_pos);
			}
			if (state_t <= 0.0f) {
				std::uniform_real_distribution<float> pick(0.0f, 1.0f);
				if (kind <= 1) {
					// 猫狗不吃草：80% 几率走动，20% 几率小憩嗅闻
					if (pick(rng) < 0.80f) {
						pick_target();
						state = 1;
					} else {
						state_t = 0.8f + pick(rng) * 0.9f;
					}
				} else {
					// 牛羊：70% 几率走动，30% 几率低头吃草
					if (pick(rng) < 0.70f) {
						pick_target();
						state = 1;
					} else {
						state = 2; // 低头吃草
						state_t = 1.6f + pick(rng) * 1.8f;
					}
				}
			}
		} break;

		case 1: { // 走向目标
			if (head != nullptr) {
				head->set_rotation(head->get_rotation().lerp(Vector3(), 0.15f));
			}
			Vector2 cur(gp.x, gp.z);
			Vector2 to = target - cur;
			float dist = to.length();
			if (dist < 0.35f || state_t < -7.0f) { // 到达目标点，短暂小歇
				state = 0;
				std::uniform_real_distribution<float> idle_dur(kind <= 1 ? 0.6f : 0.9f, kind <= 1 ? 1.6f : 2.2f);
				state_t = idle_dur(rng);
				break;
			}
			// 避让主角：离主角过近时停步或重新觅径，防止推挤人
			Node *pl_node = get_parent() ? get_parent()->get_node_or_null(NodePath("Player")) : nullptr;
			if (pl_node != nullptr) {
				Node3D *pl3d = Object::cast_to<Node3D>(pl_node);
				if (pl3d != nullptr) {
					Vector3 pp = pl3d->get_global_position();
					float d_pl = Vector2(gp.x - pp.x, gp.z - pp.z).length();
					float min_dist = (kind == 2) ? 1.85f : ((kind == 3) ? 1.45f : 1.10f);
					if (d_pl < min_dist) {
						state = 0;
						state_t = 1.0f;
						break;
					}
				}
			}
			Vector2 dir = to / dist;
			// 前方落点可站才迈步（防走下水/悬崖）
			float ahead_y = ground_y(gp.x + dir.x * 0.6f, gp.z + dir.y * 0.6f);
			if (ahead_y > 1e8f || ahead_y < -0.45f) {
				state = 0;
				state_t = 0.8f;
				break;
			}
			float cur_speed = speed * (is_night ? 0.78f : 1.0f);
			gp.x += dir.x * cur_speed * p_delta;
			gp.z += dir.y * cur_speed * p_delta;
			set_global_position(gp);
			// 朝向平滑转向移动方向（模型正面 +Z）
			float want_yaw = std::atan2(dir.x, dir.y);
			float yaw = body->get_rotation().y;
			float dy = want_yaw - yaw;
			while (dy > 3.14159f) dy -= 6.2831f;
			while (dy < -3.14159f) dy += 6.2831f;
			body->set_rotation(Vector3(0.0f, yaw + dy * Math::clamp(p_delta * 6.0f, 0.0f, 1.0f), 0.0f));

			// 动物足迹留痕：按物种体型在地面留下偶蹄或爪印
			footprint_acc += cur_speed * p_delta;
			float step_dist = (kind == 2) ? 0.85f : ((kind == 3) ? 0.65f : ((kind == 1) ? 0.48f : 0.36f));
			if (footprint_acc >= step_dist) {
				footprint_acc = 0.0f;
				if (FootprintManager::get_singleton() != nullptr) {
					FootprintManager::FootprintType ft = (kind >= 2) ? FootprintManager::TYPE_HOOF : FootprintManager::TYPE_PAW;
					float sc = (kind == 2) ? 1.05f : ((kind == 3) ? 0.82f : ((kind == 1) ? 0.90f : 0.72f));
					Vector2 perp(-dir.y, dir.x);
					float lat = (foot_side ? 0.12f : -0.12f) * sc;
					Vector3 fp_pos(gp.x + perp.x * lat, gp.y, gp.z + perp.y * lat);
					FootprintManager::get_singleton()->spawn_footprint(ft, fp_pos, want_yaw, sc);
					foot_side = !foot_side;
				}
			}

			// 腿交替摆动
			walk_phase += p_delta * speed * 9.0f;
			float sw = std::sin(walk_phase) * 0.55f;
			for (int i = 0; i < 4; i++) {
				if (legs[i] != nullptr) {
					float s = (i % 2 == 0) ? sw : -sw;
					legs[i]->set_rotation(Vector3(s, 0.0f, 0.0f));
				}
			}
		} break;

		case 2: { // 吃草：头压下带咀嚼微颤
			if (head != nullptr) {
				float chew = std::sin(anim_time * 6.5f) * 0.035f;
				Vector3 hp = head_rest_pos;
				hp.y -= 0.30f + chew;
				hp.z += 0.08f;
				head->set_position(head->get_position().lerp(hp, Math::clamp(p_delta * 5.0f, 0.0f, 1.0f)));
				head->set_rotation(Vector3(0.18f + chew, 0.0f, 0.0f));
			}
			if (state_t <= 0.0f) {
				pick_target(); // 吃完草直接启程漫步，不再长期发呆
				state = 1;
			}
		} break;
	}
}
