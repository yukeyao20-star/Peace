#include "farm_animal.h"

#include <cmath>

#include <godot_cpp/classes/animatable_body3d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
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
#include "../world/terrain_builder.h"

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

	ADD_SIGNAL(MethodInfo("animal_fed")); // 喂食成功时发出（GameDirector 每日待办计分用）
}

const char *FarmAnimal::kind_name(int p_kind) {
	switch (p_kind) {
		case 0:
			return "小猫";
		case 1:
			return "小狗";
		case 2:
			return "奶牛";
		case 3:
		default:
			return "绵羊";
	}
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
			add_to_group("interactables"); // Player E 键喂食交互用
			// 喂食回应声源：奶牛用真牛哞，其余用清铃（按存在性优雅降级）
			voice = memnew(AudioStreamPlayer3D);
			voice->set_name("Voice");
			voice->set_volume_db(-7.0f);
			voice->set_max_distance(26.0f);
			add_child(voice);
			// 喂食回应声：猫=喵、狗=汪、羊=咩（程序化合成）；牛=真牛哞资产。
			// 合成失败再降级清铃——总之喂了必有回应。
			switch (kind) {
				case 0:
					snd_voice = make_meow();
					break;
				case 1:
					snd_voice = make_woof();
					break;
				case 3:
					snd_voice = make_baa();
					break;
				case 2:
				default: {
					ResourceLoader *rl = ResourceLoader::get_singleton();
					if (rl != nullptr && rl->exists("res://assets/audio/cow_moo.mp3")) {
						Ref<Resource> r = rl->load("res://assets/audio/cow_moo.mp3");
						if (r.is_valid()) {
							snd_voice = Object::cast_to<AudioStream>(r.ptr());
						}
					}
					break;
				}
			}
			if (snd_voice.is_null()) {
				ResourceLoader *rl = ResourceLoader::get_singleton();
				if (rl != nullptr && rl->exists("res://assets/audio/organic_chime.mp3")) {
					Ref<Resource> r = rl->load("res://assets/audio/organic_chime.mp3");
					if (r.is_valid()) {
						snd_voice = Object::cast_to<AudioStream>(r.ptr());
					}
				}
			}
			if (snd_voice.is_valid()) {
				voice->set_stream(snd_voice);
			}
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
			const float dt = (float)get_process_delta_time();
			tick_behavior(dt, night, wet);
			// —— 喂食反馈：开心跳一下（身体根做抛物线小跳）——
			if (hop_t > 0.0f) {
				hop_t = Math::max(hop_t - dt, 0.0f);
				float prog = 1.0f - hop_t / 0.6f; // 0→1
				if (body != nullptr) {
					body->set_position(Vector3(0.0f, Math::sin(prog * Math::PI) * 0.22f, 0.0f));
				}
			}
			if (fed_t > 0.0f) {
				fed_t = Math::max(fed_t - dt, 0.0f);
			}
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
		// —— 奶牛：自然荷斯坦有机黑白花斑 + 歪眼罩斑块 + 额头白流星 + 腹底粉色乳房 + 双色小牛角 ——
		Ref<Material> hide = mat_of(Color(0.95f, 0.94f, 0.91f));   // 温暖纯正牛奶白底色
		Ref<Material> patch = mat_of(Color(0.13f, 0.12f, 0.13f));  // 柔和炭黑有机花斑
		Ref<Material> nose = mat_of(Color(0.92f, 0.68f, 0.66f));   // 软嫩浅粉鼻口
		Ref<Material> udder = mat_of(Color(0.93f, 0.74f, 0.72f));  // 腹下乳房粉色
		Ref<Material> horn = mat_of(Color(0.85f, 0.80f, 0.68f));   // 象牙黄小短角
		Ref<Material> hoof = mat_of(Color(0.25f, 0.23f, 0.22f));   // 蹄子

		// 1. 躯干主干（白底）
		part(body, box(0.72f, 0.72f, 1.25f), hide, Vector3(0.0f, 0.82f, 0.0f));

		// 2. 有机黑白花斑（多层错落微椭圆包络，自然贴合躯干与背脊，消除硬切方块感）：
		// (a) 左后臀大型花斑（由主斑与侧向小斑融合而成）
		part(body, ball(0.26f, 0.48f), patch, Vector3(-0.355f, 0.85f, -0.22f), Vector3(0.25f, 1.15f, 1.35f));
		part(body, ball(0.18f, 0.32f), patch, Vector3(-0.360f, 0.72f, -0.42f), Vector3(0.22f, 1.0f, 1.1f));
		// (b) 右肩胛与颈前大鞍斑（从脊背自然斜跨右前胸）
		part(body, ball(0.28f, 0.52f), patch, Vector3(0.355f, 0.88f, 0.26f), Vector3(0.25f, 1.15f, 1.35f));
		part(body, ball(0.20f, 0.36f), patch, Vector3(0.360f, 0.70f, 0.05f), Vector3(0.22f, 0.95f, 1.1f));
		// (c) 脊背中段不规则马鞍形黑斑（跨越背中线）
		part(body, ball(0.24f, 0.44f), patch, Vector3(-0.06f, 1.17f, -0.05f), Vector3(1.35f, 0.25f, 1.25f));
		part(body, ball(0.16f, 0.30f), patch, Vector3(0.12f, 1.17f, 0.15f), Vector3(1.15f, 0.25f, 1.1f));
		// (d) 腹侧与肋骨处灵动的点缀圆斑（打破呆板对称）
		part(body, ball(0.11f, 0.20f), patch, Vector3(-0.362f, 0.68f, 0.25f), Vector3(0.20f, 1.0f, 1.0f));
		part(body, ball(0.09f, 0.16f), patch, Vector3(0.362f, 0.80f, -0.38f), Vector3(0.20f, 1.0f, 1.0f));

		// 3. 腹底粉色乳房（奶牛标志性特征）
		part(body, ball(0.16f, 0.24f), udder, Vector3(0.0f, 0.46f, -0.28f), Vector3(1.1f, 0.85f, 1.1f));
		for (int ti = 0; ti < 4; ti++) {
			Vector3 tp((ti % 2 == 0 ? -0.06f : 0.06f), 0.38f, (ti < 2 ? -0.23f : -0.33f));
			part(body, stick(0.018f, 0.06f), udder, tp);
		}

		// 4. 奶牛头部
		head = memnew(Node3D);
		body->add_child(head);
		head->set_position(Vector3(0.0f, 0.98f, 0.72f));
		part(head, box(0.40f, 0.42f, 0.42f), hide, Vector3(0.0f, 0.0f, 0.05f));
		// 经典左侧歪眼罩黑斑（右侧保留大片白额与白星，生动可爱）
		part(head, ball(0.16f, 0.28f), patch, Vector3(-0.14f, 0.09f, 0.16f), Vector3(1.0f, 1.1f, 0.4f));
		// 黑色小牛角底座 + 奶黄小尖角
		part(head, stick(0.045f, 0.22f), horn, Vector3(-0.20f, 0.26f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), -0.5f);
		part(head, stick(0.045f, 0.22f), horn, Vector3(0.20f, 0.26f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), 0.5f);
		// 憨厚圆润的粉色鼻头
		part(head, box(0.32f, 0.20f, 0.14f), nose, Vector3(0.0f, -0.10f, 0.28f));
		// 鼻孔细节
		part(head, ball(0.024f, 0.03f), patch, Vector3(-0.08f, -0.10f, 0.355f));
		part(head, ball(0.024f, 0.03f), patch, Vector3(0.08f, -0.10f, 0.355f));

		// 5. 四肢与蹄子（左前腿黑、右前腿白袜，黑白交错）
		for (int i = 0; i < 4; i++) {
			Vector3 lp(i % 2 == 0 ? -0.24f : 0.24f, 0.23f, i < 2 ? 0.42f : -0.42f);
			Ref<Material> leg_mat = (i == 0 || i == 3) ? patch : hide;
			legs[i] = part(body, stick(0.075f, 0.46f), leg_mat, lp);
			// 黑色小蹄子
			part(body, box(0.14f, 0.08f, 0.15f), hoof, lp + Vector3(0.0f, -0.21f, 0.01f));
		}
		// 尾巴（顶端白、尾梢黑绒球）
		tail = part(body, stick(0.035f, 0.52f), hide, Vector3(0.0f, 0.85f, -0.68f), Vector3(1.0f, 1.0f, 1.0f), 0.6f);
		part(tail, ball(0.055f, 0.10f), patch, Vector3(0.0f, -0.28f, 0.0f));
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
	if (head != nullptr) {
		head_rest_pos = head->get_position();
	}

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
	for (int tries = 0; tries < 12; tries++) {
		float a = ang(rng);
		float r = rad(rng);
		Vector2 t = home + Vector2(std::cos(a), std::sin(a)) * r;

		// 严禁小动物进入木屋室内与门廊区域 (中心 12, 8，半径 6.2m 内)
		float d_cabin = Vector2(t.x - 12.0f, t.y - 8.0f).length();
		if (d_cabin < 6.2f) {
			continue;
		}

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
			// 严格禁止穿墙进木屋：前方 0.8m 若进入木屋半径 6.0m 内，立即停步重新觅径
			Vector2 next_p(gp.x + dir.x * 0.8f, gp.z + dir.y * 0.8f);
			if (Vector2(next_p.x - 12.0f, next_p.y - 8.0f).length() < 6.0f) {
				state = 0;
				state_t = 0.5f;
				pick_target();
				break;
			}

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

	// 踩过发光蘑菇：带走荧光微粒在身子周围缓慢飘动，4秒后逐渐消失
	update_shroom_glow_trail(p_delta);
}

void FarmAnimal::build_shroom_glow_trail() {
	if (shroom_glow_trail != nullptr) {
		return;
	}
	shroom_glow_trail = memnew(CPUParticles3D);
	shroom_glow_trail->set_name("AnimalShroomGlowTrail");
	shroom_glow_trail->set_use_local_coordinates(false); // 世界坐标系发射：动物走动时荧光微粒在身周和身后缓浮飘动

	Ref<SphereMesh> mote;
	mote.instantiate();
	mote->set_radius(0.026f);
	mote->set_height(0.052f);
	mote->set_radial_segments(6);
	mote->set_rings(3);

	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_albedo(Color(0.65f, 1.0f, 0.75f, 1.0f));
	mote->set_material(m);

	Ref<Gradient> ramp;
	ramp.instantiate();
	ramp->add_point(0.0f, Color(0.70f, 1.0f, 0.80f, 0.0f));
	ramp->add_point(0.20f, Color(0.70f, 1.0f, 0.80f, 0.95f));
	ramp->add_point(0.65f, Color(0.55f, 0.95f, 0.70f, 0.75f));
	ramp->add_point(1.0f, Color(0.40f, 0.85f, 0.60f, 0.0f));

	shroom_glow_trail->set_amount(22);
	shroom_glow_trail->set_lifetime(2.6f);
	shroom_glow_trail->set_lifetime_randomness(0.35f);
	shroom_glow_trail->set_randomness_ratio(0.4f);
	shroom_glow_trail->set_mesh(mote);
	shroom_glow_trail->set_color_ramp(ramp);
	shroom_glow_trail->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	shroom_glow_trail->set_spread(90.0f);
	shroom_glow_trail->set_gravity(Vector3(0.0f, 0.04f, 0.0f));
	shroom_glow_trail->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.05f);
	shroom_glow_trail->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.18f);
	shroom_glow_trail->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	shroom_glow_trail->set_param_max(CPUParticles3D::PARAM_SCALE, 1.35f);
	shroom_glow_trail->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	float r = (kind == 2) ? 0.85f : ((kind == 3) ? 0.65f : 0.45f);
	float body_y = (kind == 2) ? 0.75f : ((kind == 3) ? 0.55f : 0.35f);
	shroom_glow_trail->set_emission_sphere_radius(r);
	shroom_glow_trail->set_position(Vector3(0.0f, body_y, 0.0f));
	shroom_glow_trail->set_emitting(false);
	add_child(shroom_glow_trail);
}

void FarmAnimal::update_shroom_glow_trail(float p_delta) {
	TerrainBuilder *tb = nullptr;
	if (get_parent() != nullptr) {
		tb = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("Terrain")));
		if (tb == nullptr) {
			tb = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("TerrainBuilder")));
		}
	}
	Vector3 gp = get_global_position();
	if (tb != nullptr && tb->is_near_glowing_mushroom(gp, 1.35f)) {
		shroom_glow_timer = 4.0f;
	}

	if (shroom_glow_timer > 0.0f) {
		shroom_glow_timer -= p_delta;
		if (shroom_glow_trail == nullptr) {
			build_shroom_glow_trail();
		}
		if (shroom_glow_trail != nullptr) {
			shroom_glow_trail->set_emitting(true);
			float ratio = Math::clamp(shroom_glow_timer / 4.0f, 0.0f, 1.0f);
			shroom_glow_trail->set_amount(Math::max(2, (int)(12.0f * ratio)));
		}
	} else if (shroom_glow_trail != nullptr && shroom_glow_trail->is_emitting()) {
		shroom_glow_trail->set_emitting(false);
	}
}

// —— 喂食交互（Interactable）——

String FarmAnimal::get_prompt() const {
	if (fed_t > 0.0f) {
		return String::utf8("❤ ") + String::utf8(kind_name(kind)) + String::utf8("开心进食中…");
	}
	return String::utf8("按 E 喂") + String::utf8(kind_name(kind));
}

Vector3 FarmAnimal::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.6f, 0.0f);
}

void FarmAnimal::interact(const Vector3 &p_player_pos) {
	if (fed_t > 0.0f) {
		return; // 进食冷却中
	}
	fed_t = 8.0f;
	hop_t = 0.6f; // 开心跳一下
	if (voice != nullptr && snd_voice.is_valid()) {
		voice->play();
	}
	emit_signal("animal_fed"); // 通知每日待办计分（GameDirector）
}


// —— 程序化叫声（22050Hz 16-bit 单声道，无外部资源）——

// 猫 v3：「喵」= 加法合成 + 共振峰 EQ(先合成后用静态带通逐帧滤波,系数稳定)。
// 简化方案:声源谐波列直接按共振峰加权(每帧算谐波幅度权重),免滤波器发散。
// F1/F2 滑动 → 对 1..5 次谐波按与共振峰的距离高斯加权,即"共振峰塑形"。
Ref<AudioStreamWAV> FarmAnimal::make_meow() {
	const int sr = 22050;
	const float dur = 0.75f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	float phase = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float u = t / dur;
		float f0 = (u < 0.55f) ? Math::lerp(620.0f, 660.0f, u / 0.55f)
				: Math::lerp(660.0f, 240.0f, (u - 0.55f) / 0.45f);
		float env = Math::clamp(t / 0.04f, 0.0f, 1.0f)
				* Math::clamp((dur - t) / 0.16f, 0.0f, 1.0f);
		float vib = 1.0f + 0.018f * Math::sin(t * Math::TAU * 12.0f);
		phase += Math::TAU * f0 * vib / sr;
		float f1 = (u < 0.5f) ? Math::lerp(340.0f, 480.0f, u / 0.5f)
				: Math::lerp(480.0f, 880.0f, (u - 0.5f) / 0.5f);
		float f2 = (u < 0.5f) ? Math::lerp(2150.0f, 1700.0f, u / 0.5f)
				: Math::lerp(1700.0f, 880.0f, (u - 0.5f) / 0.5f);
		// 共振峰塑形:谐波 k(幅度 1/k)按落在 F1/F2 附近加权(高斯带宽~15%)
		float samp = 0.0f;
		for (int k = 1; k <= 5; k++) {
			float hf = f0 * k;
			float g1 = Math::exp(-Math::pow((hf - f1) / (f1 * 0.35f), 2.0f));
			float g2 = Math::exp(-Math::pow((hf - f2) / (f2 * 0.35f), 2.0f));
			float amp = (1.0f / k) * (0.5f + 1.6f * g1 + 1.1f * g2);
			samp += amp * Math::sin(phase * k);
		}
		samp *= 0.28f;
		// 尾段气声
		if (u > 0.72f) {
			samp += (d01(rng) - 0.5f) * (u - 0.72f) * 1.4f;
		}
		samp *= env;
		int16_t q = (int16_t)Math::clamp(samp * 30000.0f, -32000.0f, 32000.0f);
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

// 狗：「汪！汪！」两声短吠，间隔 150ms。每声 = 120Hz 粗基频 + 开口爆音
// (10ms 噪声冲击) + 快速衰减，第二声比第一声高 8%(兴奋上扬)。
Ref<AudioStreamWAV> FarmAnimal::make_woof() {
	const int sr = 22050;
	const float dur = 0.42f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int b = 0; b < 2; b++) {
			float bt = t - b * 0.19f; // 第二声 190ms 后
			if (bt < 0.0f || bt > 0.15f) {
				continue;
			}
			float f0 = 118.0f * (b == 1 ? 1.08f : 1.0f);
			float env = Math::clamp(bt / 0.008f, 0.0f, 1.0f)
					* Math::exp(-bt * 26.0f); // 极快衰减=吠
			float ph = Math::TAU * f0 * bt;
			float v = 0.7f * Math::sin(ph) + 0.3f * Math::sin(ph * 2.0f);
			float burst = (bt < 0.012f) ? (d01(rng) - 0.5f) * 0.9f * (1.0f - bt / 0.012f) : 0.0f;
			samp += (v * 0.85f + burst) * env;
		}
		int16_t q = (int16_t)Math::clamp(samp * 30000.0f, -32000.0f, 32000.0f);
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

// 羊 v3：「咩↔」高亢哭腔,共振峰塑形(免滤波器,稳定)。
// 430→520→380Hz 基频 + 24Hz 快浅唇颤(±14%),谐波按 F1 820 / F2 1900 加权。
Ref<AudioStreamWAV> FarmAnimal::make_baa() {
	const int sr = 22050;
	const float dur = 1.0f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	float phase = 0.0f;
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float u = t / dur;
		float env = Math::clamp(t / 0.03f, 0.0f, 1.0f)
				* Math::clamp((dur - t) / 0.25f, 0.0f, 1.0f);
		float f0 = (u < 0.4f) ? Math::lerp(430.0f, 520.0f, u / 0.4f)
				: Math::lerp(520.0f, 380.0f, (u - 0.4f) / 0.6f);
		float vib = 1.0f + 0.14f * Math::sin(t * Math::TAU * 24.0f);
		phase += Math::TAU * f0 * vib / sr;
		float samp = 0.0f;
		for (int k = 1; k <= 6; k++) {
			float hf = f0 * k;
			float g1 = Math::exp(-Math::pow((hf - 820.0f) / 300.0f, 2.0f));
			float g2 = Math::exp(-Math::pow((hf - 1900.0f) / 500.0f, 2.0f));
			float amp = (1.0f / k) * (0.4f + 1.7f * g1 + 0.9f * g2);
			// 偶次谐波掺一点方波感(喉音粗粝)
			float osc = Math::sin(phase * k) + ((k % 2 == 0) ? 0.18f : 0.0f) * Math::sin(phase * k * 1.02f);
			samp += amp * osc;
		}
		samp *= 0.26f * env;
		int16_t q = (int16_t)Math::clamp(samp * 30000.0f, -32000.0f, 32000.0f);
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
