#include "sky_cycle.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

constexpr float LAMP_ENERGY_NIGHT = 1.6f; // 石灯夜里满亮的自发光强度（与 LampGlowMat 原值一致）
constexpr float LAMP_FADE_SPEED = 2.0f;   // 石灯明灭速度（energy 每秒变化量，晨昏约 0.5s 过渡）

} // namespace

void SkyCycle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sun_path", "p_path"), &SkyCycle::set_sun_path);
	ClassDB::bind_method(D_METHOD("get_sun_path"), &SkyCycle::get_sun_path);
	ClassDB::bind_method(D_METHOD("set_env_path", "p_path"), &SkyCycle::set_env_path);
	ClassDB::bind_method(D_METHOD("get_env_path"), &SkyCycle::get_env_path);
	ClassDB::bind_method(D_METHOD("set_fireflies_path", "p_path"), &SkyCycle::set_fireflies_path);
	ClassDB::bind_method(D_METHOD("get_fireflies_path"), &SkyCycle::get_fireflies_path);
	ClassDB::bind_method(D_METHOD("set_time_of_day", "p_t"), &SkyCycle::set_time_of_day);
	ClassDB::bind_method(D_METHOD("get_time_of_day"), &SkyCycle::get_time_of_day);
	ClassDB::bind_method(D_METHOD("set_day_length", "p_secs"), &SkyCycle::set_day_length);
	ClassDB::bind_method(D_METHOD("get_day_length"), &SkyCycle::get_day_length);
	ClassDB::bind_method(D_METHOD("get_day_count"), &SkyCycle::get_day_count);
	ClassDB::bind_method(D_METHOD("get_phase_name"), &SkyCycle::get_phase_name);
	ClassDB::bind_method(D_METHOD("get_moon_phase"), &SkyCycle::get_moon_phase);
	ClassDB::bind_method(D_METHOD("get_night_factor"), &SkyCycle::get_night_factor);
	// 雨雪因子 getter 此前只有 C++ 调用方，-s 脚本摸不到（坑 17.3），补绑
	ClassDB::bind_method(D_METHOD("get_rain_amount"), &SkyCycle::get_rain_amount);
	ClassDB::bind_method(D_METHOD("get_snow_amount"), &SkyCycle::get_snow_amount);
	// —— 天气状态快照（SaveSystem 存读用）——
	ClassDB::bind_method(D_METHOD("set_day_count", "p_day"), &SkyCycle::set_day_count);
	ClassDB::bind_method(D_METHOD("get_weather"), &SkyCycle::get_weather);
	ClassDB::bind_method(D_METHOD("set_weather", "p_weather"), &SkyCycle::set_weather);
	ClassDB::bind_method(D_METHOD("get_cloud"), &SkyCycle::get_cloud);
	ClassDB::bind_method(D_METHOD("set_cloud", "p_cloud"), &SkyCycle::set_cloud);
	ClassDB::bind_method(D_METHOD("get_cloud_target"), &SkyCycle::get_cloud_target);
	ClassDB::bind_method(D_METHOD("set_cloud_target", "p_target"), &SkyCycle::set_cloud_target);
	ClassDB::bind_method(D_METHOD("set_rain", "p_rain"), &SkyCycle::set_rain);
	ClassDB::bind_method(D_METHOD("get_rain_target"), &SkyCycle::get_rain_target);
	ClassDB::bind_method(D_METHOD("set_rain_target", "p_target"), &SkyCycle::set_rain_target);
	ClassDB::bind_method(D_METHOD("set_snow", "p_snow"), &SkyCycle::set_snow);
	ClassDB::bind_method(D_METHOD("get_snow_target"), &SkyCycle::get_snow_target);
	ClassDB::bind_method(D_METHOD("set_snow_target", "p_target"), &SkyCycle::set_snow_target);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "sun_path"), "set_sun_path", "get_sun_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "env_path"), "set_env_path", "get_env_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "fireflies_path"), "set_fireflies_path", "get_fireflies_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_of_day"), "set_time_of_day", "get_time_of_day");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "day_length"), "set_day_length", "get_day_length");
	// day_count 只读（运行时自增，不提供 setter）
	ADD_PROPERTY(PropertyInfo(Variant::INT, "day_count"), "", "get_day_count");
}

void SkyCycle::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不动场景里的灯光与天空
			}
			sun = Object::cast_to<DirectionalLight3D>(get_node_or_null(sun_path));
			WorldEnvironment *we = Object::cast_to<WorldEnvironment>(get_node_or_null(env_path));
			if (we != nullptr) {
				world_env = we;
				env = we->get_environment();
				if (env.is_valid() && env->get_sky().is_valid()) {
					sky_mat = Ref<ProceduralSkyMaterial>(Object::cast_to<ProceduralSkyMaterial>(
							env->get_sky()->get_material().ptr()));
				}
			}
			fireflies = Object::cast_to<CPUParticles3D>(get_node_or_null(fireflies_path));
			build_rain();
			build_snow();
			build_stars();
			build_moon();
			apply();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();
			// —— 时间推进 ——
			time_of_day += (float)delta / day_length;
			if (time_of_day >= 1.0f) {
				time_of_day -= 1.0f;
				day_count++; // 跨过午夜，进入新的一天
			}
			// —— 天气状态机 ——
			weather_timer -= (float)delta;
			if (weather_timer <= 0.0f) {
				pick_next_weather();
			}
			cloud = Math::move_toward(cloud, cloud_target, (float)delta * 0.22f);
			rain = Math::move_toward(rain, rain_target, (float)delta * 0.22f);
			snow = Math::move_toward(snow, snow_target, (float)delta * 0.15f); // 雪来得更缓
			apply();
			// —— 星空缓慢漂移（北极星附近的视运动，极简版绕竖轴慢转）——
			if (stars != nullptr) {
				stars->rotate_y((float)delta * 0.01f);
			}
			// —— 雨粒子跟随主角 ——
			if (player == nullptr && get_parent() != nullptr) {
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			if (rain_root != nullptr && player != nullptr) {
				rain_root->set_global_position(player->get_global_position() + Vector3(0.0f, 12.0f, 0.0f));
			}
		} break;
		default:
			break;
	}
}

// 夜幕因子 0..1：sun_h = sin((tod-0.25)·2π)，从 -0.02 降到 -0.12 线性映射 0→1。
// 公式唯一来源：Bird/Rabbit/ButterflySwarm/TerrainBuilder/Ambience 及本类
// 星空/月盘透明度都从这里取（此前各抄一份，阈值已有漂移风险）。
float SkyCycle::get_night_factor() const {
	const float sun_h = std::sin((time_of_day - 0.25f) * 6.2831853f); // -1..1 太阳高度
	return 1.0f - Math::clamp((sun_h + 0.12f) / 0.10f, 0.0f, 1.0f);
}

void SkyCycle::pick_next_weather() {
	// 固定种子：天气序列可复现（rng 常驻，lambda 只传引用不捕获）
	static std::mt19937 rng(4242);
	auto rnd = [](std::mt19937 &r, float lo, float hi) -> float {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(r);
	};
	auto dur = [&](float lo, float hi) { weather_timer = rnd(rng, lo, hi); };

	int next = weather;
	switch (weather) {
		case 0: // 晴 → 多云为主
			next = rnd(rng, 0.0f, 1.0f) < 0.55f ? 1 : 0;
			break;
		case 1: // 多云 → 多半转小雨；小概率下雪
			{
				float r = rnd(rng, 0.0f, 1.0f);
				next = r < 0.40f ? 2 : (r < 0.65f ? 0 : (r < 0.90f ? 3 : 4));
			}
			break;
		case 2: // 小雨 → 一半加大，一半转多云
			next = rnd(rng, 0.0f, 1.0f) < 0.5f ? 3 : 1;
			break;
		case 3: // 大雨 → 逐渐减弱
			next = rnd(rng, 0.0f, 1.0f) < 0.6f ? 2 : 1;
			break;
		default: // 雪 → 转回多云/晴
			next = rnd(rng, 0.0f, 1.0f) < 0.6f ? 1 : 0;
			break;
	}
	weather = next;
	switch (weather) {
		case 0:
			cloud_target = 0.0f;
			rain_target = 0.0f;
			snow_target = 0.0f;
			dur(35.0f, 60.0f);
			break;
		case 1:
			cloud_target = 0.85f;
			rain_target = 0.0f;
			snow_target = 0.0f;
			dur(20.0f, 40.0f);
			break;
		case 2:
			cloud_target = 0.92f;
			rain_target = 0.45f;
			snow_target = 0.0f;
			dur(18.0f, 30.0f);
			break;
		case 3:
			cloud_target = 1.0f;
			rain_target = 1.0f;
			snow_target = 0.0f;
			dur(14.0f, 24.0f);
			break;
		default: // 雪：阴天但不下雨，雪强满格
			cloud_target = 0.95f;
			rain_target = 0.0f;
			snow_target = 1.0f;
			dur(20.0f, 35.0f);
			break;
	}
}

void SkyCycle::apply() {
	const float t = time_of_day;
	const float sun_h = std::sin((t - 0.25f) * 6.2831853f); // -1..1 太阳高度
	const float day = clampf(sun_h * 3.0f, 0.0f, 1.0f);     // 白昼量（日出日落快速过渡）
	const float dusk = clampf(1.0f - std::abs(sun_h) * 5.0f, 0.0f, 1.0f); // 晨昏带
	const float gloom = clampf(cloud * 0.5f + rain * 0.3f, 0.0f, 0.75f);  // 阴沉量

	// —— 天空盒四色 ——
	if (sky_mat.is_valid()) {
		Color top = Color(0.02f, 0.045f, 0.11f).lerp(Color(0.32f, 0.54f, 0.88f), day);
		top = top.lerp(Color(0.40f, 0.43f, 0.47f) * (0.2f + 0.8f * day), gloom);
		Color hor = Color(0.06f, 0.08f, 0.15f).lerp(Color(0.66f, 0.76f, 0.90f), day);
		hor = hor.lerp(Color(0.98f, 0.55f, 0.30f), dusk * 0.75f * (1.0f - gloom)); // 晨昏橙霞
		hor = hor.lerp(Color(0.55f, 0.57f, 0.60f), gloom);
		Color gnd = Color(0.02f, 0.03f, 0.06f).lerp(Color(0.36f, 0.48f, 0.38f), day);
		gnd = gnd.lerp(Color(0.30f, 0.31f, 0.33f), gloom);
		sky_mat->set_sky_top_color(top);
		sky_mat->set_sky_horizon_color(hor);
		sky_mat->set_ground_bottom_color(gnd);
		sky_mat->set_ground_horizon_color(hor);
		sky_mat->set_sky_curve(0.15f);
	}

	// —— 太阳 / 月亮 ——
	if (sun != nullptr) {
		if (sun_h >= 0.0f) {
			sun->set_rotation(Vector3(-sun_h * 1.15f, 0.35f, 0.0f));
			Color c = Color(1.0f, 0.62f, 0.35f).lerp(Color(1.0f, 0.96f, 0.87f), day); // 低角度偏橙
			sun->set_color(c);
			sun->set_param(Light3D::PARAM_ENERGY, 1.08f * day * (1.0f - 0.6f * gloom));
		} else {
			sun->set_rotation(Vector3(sun_h * 1.15f, 2.2f, 0.0f)); // 月亮：高度取反、方位不同
			sun->set_color(Color(0.55f, 0.65f, 0.95f));
			sun->set_param(Light3D::PARAM_ENERGY, 0.15f * (1.0f - cloud * 0.8f));
		}
	}

	// —— 环境光与雾 ——
	if (env.is_valid()) {
		env->set_ambient_light_energy(0.42f * (0.22f + 0.78f * day) * (1.0f - 0.3f * gloom));
		if (sky_mat.is_valid()) {
			env->set_fog_light_color(sky_mat->get_sky_horizon_color());
		}
		// 晴日雾量减半：原 0.006 让 8 点的世界泡牛奶（远景全白、树干巨石洗成色块）
		env->set_fog_density(0.0032f + rain * 0.014f + snow * 0.012f + (1.0f - day) * 0.005f);
	}

	// —— 雨 ——
	if (drizzle != nullptr) {
		drizzle->set_emitting(rain > 0.12f);
	}
	if (downpour != nullptr) {
		downpour->set_emitting(rain > 0.7f);
	}

	// —— 雪 ——
	if (snowflakes != nullptr) {
		snowflakes->set_emitting(snow > 0.08f);
	}

	// —— 星空：夜里渐显，云越厚星越淡（雨雪天 cloud 都在 0.9+，自然被遮没）——
	// 夜幕因子取 get_night_factor()（与 Bird/Rabbit 同一套阈值的唯一来源）。
	float night = 0.0f;
	if (star_mat.is_valid()) {
		night = get_night_factor();
		const float alpha = night * (1.0f - cloud * 0.92f);
		star_mat->set_albedo(Color(1.0f, 1.0f, 1.0f, alpha));
		if (stars != nullptr) {
			stars->set_visible(alpha > 0.004f); // 白天/浓云整组隐藏，省一次 draw call
		}
	}

	// —— 月盘：太阳落下它升起（高度跟随 -sun_h），方位与夜里的月光灯一致（az 2.2rad）；
	// 透明度复用星空的夜幕×云量因子，雨雪阴天一起被遮没；白天整组隐藏省 draw call。
	// 月相随 day_count 走：第 1 天满月，之后每天 -1/8 相位（8 天一个朔望循环）。 ——
	if (moon != nullptr) {
		const float elev = clampf(-sun_h, 0.0f, 1.0f) * 1.2f; // 0..1.2rad，午夜最高
		if (elev > 0.01f) {
			const float az = 2.2f;
			const Vector3 dir(std::cos(az) * std::cos(elev), std::sin(elev),
					std::sin(az) * std::cos(elev));
			moon->set_position(dir * 240.0f); // 星穹 260m 之内，确保挡在星星前面
		}
		const float alpha = night * (1.0f - cloud * 0.92f);
		if (moon_shader_mat.is_valid()) {
			if (moon_phase_day != day_count) { // 相位只在跨天时变，不必每帧写 uniform
				moon_phase_day = day_count;
				moon_shader_mat->set_shader_parameter("phase", get_moon_phase());
			}
			moon_shader_mat->set_shader_parameter("alpha", alpha);
		} else if (moon_mat.is_valid()) {
			moon_mat->set_albedo(Color(0.93f, 0.94f, 1.0f, alpha));
		}
		moon->set_visible(alpha > 0.004f);
	}

	// —— 水面雨量联动：把 rain 写进 water.gdshader 的 rain_amount ——
	// Lake 是 TerrainBuilder 运行时生成的（Terrain/Lake），懒查找一次并缓存。
	if (water_mat.is_null() && get_parent() != nullptr) {
		MeshInstance3D *lake = Object::cast_to<MeshInstance3D>(
				get_parent()->get_node_or_null(NodePath("Terrain/Lake")));
		if (lake != nullptr && lake->get_mesh().is_valid()) {
			PrimitiveMesh *pm = Object::cast_to<PrimitiveMesh>(lake->get_mesh().ptr());
			if (pm != nullptr) {
				water_mat = Ref<ShaderMaterial>(
						Object::cast_to<ShaderMaterial>(pm->get_material().ptr()));
			}
		}
	}
	if (water_mat.is_valid()) {
		water_mat->set_shader_parameter("rain_amount", rain);
		// 结冰随雪量推进（雪停回温也走同一条平滑曲线自然消融）
		water_mat->set_shader_parameter("freeze_amount", snow);
		// 太阳/月亮方向 + 高光强度：CS2 水面碎金高光的方向源（夜里=月光水路）
		if (sun != nullptr) {
			Vector3 sd = sun->get_global_transform().basis.xform(
					Vector3(0.0f, 0.0f, 1.0f)).normalized();
			sun_dir = sd; // 存下来供植被透光等其它系统读（不受 water_mat 是否就绪影响）
			water_mat->set_shader_parameter("sun_dir", sd);
			float glint = (sun_h >= 0.0f) ? (day * (1.0f - 0.6f * gloom))
					: (0.15f * (1.0f - cloud * 0.8f));
			water_mat->set_shader_parameter("sun_glint", glint);
		}
	}

	// —— 礁石激浪联动：湖面结冰封湖时，拍岸水花也得停 ——
	// ShoreSpray 是 TerrainBuilder 撒完湖岸礁石后生成的，懒查找一次并缓存。
	// 用 amount_ratio 平滑淡出（不是直接隐藏），避免封湖瞬间水花"啪"地消失。
	if (spray_root == nullptr && get_parent() != nullptr) {
		spray_root = Object::cast_to<Node3D>(
				get_parent()->get_node_or_null(NodePath("Terrain/ShoreSpray")));
	}
	if (spray_root != nullptr) {
		float ratio = 1.0f - snow * 1.6f; // 雪量过半就把水花压没
		ratio = ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
		if (Math::abs(ratio - spray_ratio_applied) > 0.01f) {
			spray_ratio_applied = ratio;
			for (int i = 0; i < spray_root->get_child_count(); i++) {
				GPUParticles3D *g = Object::cast_to<GPUParticles3D>(spray_root->get_child(i));
				if (g != nullptr) {
					g->set_amount_ratio(ratio);
				}
			}
		}
	}

	// —— 昼夜联动：桥头石灯 + 萤火虫（统一用 night = 1 - day 驱动）——
	// 石灯灯芯材质懒查找：READY 时 StoneBridge 可能尚未入树，找不到就下一帧再试。
	// 两盏灯（LampW_Glow / LampE_Glow）共用 LampGlowMat 子资源，改一次材质即全生效。
	if (lamp_glow_mat.is_null()) {
		CSGBox3D *glow = Object::cast_to<CSGBox3D>(
				get_node_or_null(NodePath("../StoneBridge/LampW_Glow")));
		if (glow != nullptr) {
			lamp_glow_mat = Ref<StandardMaterial3D>(
					Object::cast_to<StandardMaterial3D>(glow->get_material().ptr()));
		}
	}
	if (lamp_glow_mat.is_valid()) {
		const float night_f = 1.0f - day; // 夜幕因子：白天 0 → 深夜 1
		lamp_energy = Math::move_toward(lamp_energy, LAMP_ENERGY_NIGHT * night_f,
				(float)get_process_delta_time() * LAMP_FADE_SPEED);
		lamp_glow_mat->set_emission_energy_multiplier(lamp_energy);
	}

	// —— 萤火虫：天黑足量才出没，雨雪天躲藏（set_emitting 直接开关，简单优先）——
	if (fireflies != nullptr) {
		const float night_f = 1.0f - day;
		fireflies->set_emitting(night_f > 0.6f && rain < 0.25f && snow < 0.25f);
	}
}

void SkyCycle::build_rain() {
	// 雨丝材质：竖直细长面片，Y 轴朝向相机
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_FIXED_Y);
	mat->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);

	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(0.03f, 0.3f));
	quad->set_material(mat);

	rain_root = memnew(Node3D);
	rain_root->set_name("RainRoot");
	add_child(rain_root);

	auto make_rain = [&](const char *p_name, int p_amount, float p_vmin, float p_vmax,
				float p_grav, const Vector3 &p_extents, const Color &p_color) {
		CPUParticles3D *p = memnew(CPUParticles3D);
		p->set_name(p_name);
		p->set_amount(p_amount);
		p->set_lifetime(2.0f);
		p->set_mesh(quad);
		p->set_color(p_color);
		p->set_direction(Vector3(0.0f, -1.0f, 0.0f));
		p->set_spread(3.0f);
		p->set_gravity(Vector3(0.0f, -p_grav, 0.0f));
		p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, p_vmin);
		p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, p_vmax);
		p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
		p->set_emission_box_extents(p_extents);
		p->set_visibility_aabb(AABB(Vector3(-24.0f, -40.0f, -24.0f), Vector3(48.0f, 56.0f, 48.0f)));
		p->set_emitting(false);
		rain_root->add_child(p);
		return p;
	};
	drizzle = make_rain("Drizzle", 320, 6.0f, 8.0f, 8.0f, Vector3(13.0f, 0.4f, 13.0f),
			Color(0.65f, 0.72f, 0.85f, 0.32f));
	downpour = make_rain("Downpour", 900, 11.0f, 14.0f, 14.0f, Vector3(15.0f, 0.4f, 15.0f),
			Color(0.60f, 0.68f, 0.82f, 0.5f));
}

void SkyCycle::build_snow() {
	// 雪花：小圆片、满幅 billboard、慢落 + 四散飘（spread 大制造横漂）
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	mat->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);

	Ref<QuadMesh> flake;
	flake.instantiate();
	flake->set_size(Vector2(0.07f, 0.07f));
	flake->set_material(mat);

	snowflakes = memnew(CPUParticles3D);
	snowflakes->set_name("Snowflakes");
	snowflakes->set_amount(480);
	snowflakes->set_lifetime(12.0f); // 慢落，需要长寿命铺满整段下落路径
	snowflakes->set_mesh(flake);
	snowflakes->set_color(Color(0.96f, 0.97f, 1.0f, 0.85f));
	snowflakes->set_direction(Vector3(0.0f, -1.0f, 0.0f));
	snowflakes->set_spread(25.0f); // 大散布角 → 横向漂移
	snowflakes->set_gravity(Vector3(0.0f, -0.55f, 0.0f));
	snowflakes->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.4f);
	snowflakes->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.1f);
	snowflakes->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	snowflakes->set_param_max(CPUParticles3D::PARAM_SCALE, 1.5f);
	snowflakes->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	snowflakes->set_emission_box_extents(Vector3(14.0f, 7.0f, 14.0f));
	snowflakes->set_visibility_aabb(AABB(Vector3(-24.0f, -40.0f, -24.0f), Vector3(48.0f, 64.0f, 48.0f)));
	snowflakes->set_emitting(false);
	if (rain_root != nullptr) {
		rain_root->add_child(snowflakes); // 跟 rain_root 一起每帧跟随主角
	} else {
		add_child(snowflakes);
	}
}

void SkyCycle::build_stars() {
	// 星空穹顶：MultiMesh 一组小面片贴在大半球上（半径 260m，只留地平线以上），
	// 每颗星随机大小/亮度/微偏蓝白；材质 unshaded + 禁雾，夜里调 albedo 透明度显隐。
	star_mat.instantiate();
	star_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	star_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	star_mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	star_mat->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	star_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true); // 实例色当亮度
	star_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true); // 别让雾把星星洗白
	star_mat->set_albedo(Color(1.0f, 1.0f, 1.0f, 0.0f)); // 初始全透明，apply() 每帧接管

	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(0.9f, 0.9f));
	quad->set_material(star_mat);

	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_use_colors(true);
	mm->set_mesh(quad);

	// 固定种子：星图可复现
	static std::mt19937 rng(777);
	std::uniform_real_distribution<float> rnd(0.0f, 1.0f);

	const int STAR_COUNT = 220;
	const float R = 260.0f;
	mm->set_instance_count(STAR_COUNT);
	int placed = 0;
	while (placed < STAR_COUNT) {
		// 均匀取球面方向，只留仰角 > ~8° 的上半球（低于地平线的丢弃）
		float x = rnd(rng) * 2.0f - 1.0f;
		float y = rnd(rng) * 2.0f - 1.0f;
		float z = rnd(rng) * 2.0f - 1.0f;
		float len = std::sqrt(x * x + y * y + z * z);
		if (len < 0.001f || y / len < 0.14f) {
			continue;
		}
		Transform3D xf;
		xf.origin = Vector3(x, y, z) * (R / len);
		const float s = lerpf(0.45f, 1.6f, rnd(rng)); // 大小错落
		xf.basis = Basis().scaled(Vector3(s, s, s));
		const float b = lerpf(0.35f, 1.0f, rnd(rng)); // 亮度错落，微偏冷白
		mm->set_instance_color(placed, Color(b, b, clampf(b * 1.08f, 0.0f, 1.0f)));
		placed++;
	}

	stars = memnew(MultiMeshInstance3D);
	stars->set_name("Stars");
	stars->set_multimesh(mm);
	stars->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(stars);
}

void SkyCycle::build_moon() {
	// 月盘：淡蓝白小球贴在星穹内侧（240m），unshaded + 禁雾（同星星，防雾洗白）；
	// 优先用 moon.gdshader（陨石坑月面 + 月相明暗 + 暗面地照残影），加载失败回退
	// 纯色 StandardMaterial3D；透明度由 apply() 每帧按夜幕×云量接管。
	Ref<Shader> moon_shader = ResourceLoader::get_singleton()->load(
			"res://assets/moon.gdshader");
	if (moon_shader.is_valid()) {
		moon_shader_mat.instantiate();
		moon_shader_mat->set_shader(moon_shader);
		moon_shader_mat->set_shader_parameter("alpha", 0.0f); // 初始全透明
		moon_shader_mat->set_shader_parameter("phase", 0.5f); // 第 1 天满月
	}

	if (moon_shader_mat.is_valid()) {
		Ref<SphereMesh> sphere;
		sphere.instantiate();
		sphere->set_radius(8.0f); // 240m 外张角约 2.4°，比真实月亮略大、夜里足够醒目
		sphere->set_height(16.0f);
		sphere->set_radial_segments(48); // 月相明暗界线平滑需要更高细分
		sphere->set_rings(24);
		sphere->set_material(moon_shader_mat);

		moon = memnew(MeshInstance3D);
		moon->set_name("Moon");
		moon->set_mesh(sphere);
		moon->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		moon->set_visible(false); // 白天隐藏，apply() 夜里接管
		add_child(moon);
		return;
	}

	// —— 兜底：无着色器时维持原纯色月盘 ——
	moon_mat.instantiate();
	moon_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	moon_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	moon_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	moon_mat->set_albedo(Color(0.93f, 0.94f, 1.0f, 0.0f)); // 初始全透明
	moon_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	moon_mat->set_emission(Color(0.50f, 0.55f, 0.72f));
	moon_mat->set_emission_energy_multiplier(0.30f);

	Ref<SphereMesh> sphere;
	sphere.instantiate();
	sphere->set_radius(8.0f);  // 240m 外张角约 2.4°，比真实月亮略大、夜里足够醒目
	sphere->set_height(16.0f);
	sphere->set_radial_segments(24);
	sphere->set_rings(12);
	sphere->set_material(moon_mat);

	moon = memnew(MeshInstance3D);
	moon->set_name("Moon");
	moon->set_mesh(sphere);
	moon->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	moon->set_visible(false); // 白天隐藏，apply() 夜里接管
	add_child(moon);
}

String SkyCycle::get_phase_name() const {
	const char *phase;
	if (time_of_day >= 0.20f && time_of_day < 0.30f) {
		phase = "清晨";
	} else if (time_of_day >= 0.30f && time_of_day < 0.70f) {
		phase = "白天";
	} else if (time_of_day >= 0.70f && time_of_day < 0.80f) {
		phase = "傍晚";
	} else {
		phase = "黑夜";
	}
	const char *w;
	switch (weather) {
		case 0:
			w = "晴";
			break;
		case 1:
			w = "多云";
			break;
		case 2:
			w = "小雨";
			break;
		case 3:
			w = "大雨";
			break;
		default:
			w = "雪";
			break;
	}
	// 中文字面量必须 String::utf8()（String(const char*) 是 latin-1 展宽，会乱码）
	return String::utf8(phase) + String::utf8(" · ") + String::utf8(w);
}
