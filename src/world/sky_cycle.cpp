#include "sky_cycle.h"

#include "ice_surface.h" // 结冰可行走面（本节点 READY 时 spawn，ROADMAP #12）

#include <cmath>
#include <random>

#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/os.hpp>
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
#include <godot_cpp/variant/string_name.hpp>
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
	// QA / 调试用：查询某个世界坐标点是否在屋檐下（与降水粒子剔除同一套遮蔽体）
	ClassDB::bind_method(D_METHOD("is_sheltered", "world_pos"), &SkyCycle::is_sheltered);
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
			build_precip_shelter(); // 必须在两份降水材质都建好之后
			build_stars();
			build_moon();
			build_underwater();
			// 调试：ZCODE_FORCE_METEOR=1 时首次流星提前到 0.4s（探针截图用）
			if (OS::get_singleton() != nullptr
					&& OS::get_singleton()->get_environment("ZCODE_FORCE_METEOR") == "1") {
				star_timer = 0.4f;
			}
			// 结冰可行走面（ROADMAP #12）：与雨雪星空同为本节点营造的世界元素。
			// IceSurface 自己沿祖先链找到本节点读雪量，雪量 ≥0.85 启用湖面碰撞、
			// ≤0.70 解除（滞回防抖）；场景文件里不出现。
			IceSurface *ice = memnew(IceSurface);
			ice->set_name("IceSurface");
			add_child(ice);
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
			cloud = Math::move_toward(cloud, cloud_target, (float)delta * 0.18f);
			rain = Math::move_toward(rain, rain_target, (float)delta * 0.18f);
			snow = Math::move_toward(snow, snow_target, (float)delta * 0.045f); // 积雪与消融过渡放缓3倍，从容自然
			apply();
			// —— 水下氛围（相机低于水面 0.5m 内淡入青蓝色）——
			if (uw_tint != nullptr) {
				float target = 0.0f;
				Camera3D *cam = get_viewport() != nullptr
						? Object::cast_to<Camera3D>(get_viewport()->get_camera_3d()) : nullptr;
				bool under = cam != nullptr && cam->get_global_position().y < -0.50f;
				if (under) {
					// 水越深，水体吸收越明显：覆盖层随相机深度从 0.34 加浓到 0.55，
					// 否则湖底加深后依然"一眼见底"、看不出水深。
					float depth_below = Math::max(0.0f, -cam->get_global_position().y - 0.5f);
					target = Math::clamp(0.34f + depth_below * 0.032f, 0.34f, 0.55f);
				}
				if (uw_bubbles != nullptr) {
					uw_bubbles->set_emitting(under);
					if (under && cam != nullptr) {
						uw_bubbles->set_global_position(cam->get_global_position());
					}
				}
				uw_alpha = Math::move_toward(uw_alpha, target, (float)delta * 1.8f);
				Color col = uw_tint->get_color();
				col.a = uw_alpha;
				uw_tint->set_color(col);
				// 水下距离雾：深度雾把远处湖底/对岸压暗成深青，水体才有"厚度"；
				// apply() 每帧已按天气重置雾底色与浓度，这里只在其上按水下权重叠加，不会累积。
				if (env.is_valid() && uw_alpha > 0.001f) {
					float k = uw_alpha / 0.55f;
					Color fog_c = env->get_fog_light_color();
					env->set_fog_light_color(fog_c.lerp(Color(0.045f, 0.27f, 0.31f), k * 0.85f));
					float d = env->get_fog_density();
					env->set_fog_density(d + (Math::max(d, 0.075f) - d) * k);
				}
				// 光柱/焦散叠加层：强度跟水下浓度走，完全出水时隐藏省一次全屏绘制
				if (uw_rays != nullptr && uw_rays_mat.is_valid()) {
					uw_rays->set_visible(uw_alpha > 0.002f);
					uw_rays_mat->set_shader_parameter("uw_amount", uw_alpha / 0.55f);
				}
				// 音频低通随水下状态平滑过渡（指数逼近，避免线性扫频的"呼呼"声）
				float cut_target = target > 0.0f ? 700.0f : 20000.0f;
				uw_cutoff += (cut_target - uw_cutoff) * Math::min(1.0f, (float)delta * 4.0f);
				if (uw_lp.is_valid()) {
					uw_lp->set_cutoff(uw_cutoff);
				}
			}
			// —— 星空缓慢漂移（北极星附近的视运动，极简版绕竖轴慢转）——
			if (stars != nullptr) {
				stars->rotate_y((float)delta * 0.01f);
			}
			// —— 流星：晴夜深空偶发一颗，从视野前上方斜掠而过 ——
			if (shooting_star != nullptr) {
				if (star_life > 0.0f) {
					star_life -= (float)delta;
					shooting_star->set_position(shooting_star->get_position() + star_vel * (float)delta);
					if (star_trail_mat.is_valid()) {
						float u = 1.0f - Math::clamp(star_life / star_dur, 0.0f, 1.0f);
						float a = Math::min(u * 6.0f, 1.0f) * (1.0f - u);
						star_trail_mat->set_albedo(Color(0.82f, 0.92f, 1.0f, a));
					}
					if (star_life <= 0.0f) {
						shooting_star->set_visible(false);
					}
				} else {
					star_timer -= (float)delta;
					if (star_timer <= 0.0f) {
						// 调试：ZCODE_FORCE_METEOR=1 时无视夜色/天气、每 0.6s 循环（探针截图用）
						bool force = OS::get_singleton() != nullptr
								&& OS::get_singleton()->get_environment("ZCODE_FORCE_METEOR") == "1";
						bool night_ok = force || (get_night_factor() > 0.72f && cloud < 0.45f
								&& rain < 0.15f && snow < 0.15f);
						Camera3D *cam = get_viewport() != nullptr
								? Object::cast_to<Camera3D>(get_viewport()->get_camera_3d()) : nullptr;
						if (night_ok && cam != nullptr) {
							static std::mt19937 srng(313131u);
							auto rnd = [&](float lo, float hi) {
								std::uniform_real_distribution<float> d(lo, hi);
								return d(srng);
							};
							Transform3D gt = cam->get_global_transform();
							Vector3 fwd = -gt.basis.get_column(2);
							Vector3 right = gt.basis.get_column(0);
							Vector3 pos = gt.origin + fwd * rnd(30.0f, 44.0f)
									+ right * rnd(-14.0f, 14.0f)
									+ Vector3(0.0f, rnd(13.0f, 22.0f), 0.0f);
							Vector3 dir = (Vector3(rnd(-1.0f, 1.0f), rnd(-0.75f, -0.28f), rnd(-1.0f, 1.0f))).normalized();
							star_vel = dir * rnd(34.0f, 52.0f);
							shooting_star->set_position(pos);
							shooting_star->look_at(pos + dir, Vector3(0.0f, 1.0f, 0.0f));
							shooting_star->set_visible(true);
							star_dur = force ? 2.0f : rnd(0.7f, 1.1f);
							star_life = star_dur;
							star_timer = force ? 0.6f : rnd(18.0f, 45.0f);
						} else {
							star_timer = 4.0f;
						}
					}
				}
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
			dur(45.0f, 85.0f);
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
		// 夜间方向光改用"月亮"方向，天空盒会把光源当太阳画成一颗亮盘 → 关掉天空侧太阳，
		// 夜里只留 DirectionalLight 照明（月盘由 Moon 网格单独呈现）。
		sun->set_sky_mode(sun_h >= 0.0f
				? DirectionalLight3D::SKY_MODE_LIGHT_AND_SKY
				: DirectionalLight3D::SKY_MODE_LIGHT_ONLY);
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
			Vector3 mpos = dir * 240.0f;
			moon->set_position(mpos); // 星穹 260m 之内，确保挡在星星前面
			moon->look_at(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
			// look_at 让 -Z 指向地心，而 moon.gdshader 以局部 +Z 为可见面（n.z>0 才是被照亮的一半），
			// 不补这个 180° 翻转，看到的永远是暗面（旧截图里月盘呈灰盘）。
			moon->rotate_object_local(Vector3(0.0f, 1.0f, 0.0f), Math::PI);
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

// 降水粒子材质：手写 billboard + 屋檐遮蔽，见 assets/precip.gdshader 文件头。
// 加载失败时退回 StandardMaterial3D（雨雪会穿屋顶，但不至于没有雨）。
// fixed_y 是材质级 uniform，所以雨丝与雪花必须各持一份材质。
static Ref<Material> make_precip_material(Ref<ShaderMaterial> &r_out, bool p_fixed_y, const char *p_tag) {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/precip.gdshader");
	Ref<Shader> shader = Ref<Shader>(Object::cast_to<Shader>(res.ptr()));
	if (shader.is_valid()) {
		r_out.instantiate();
		r_out->set_shader(shader);
		r_out->set_shader_parameter("fixed_y", p_fixed_y);
		r_out->set_shader_parameter("tint", Color(1.0f, 1.0f, 1.0f, 1.0f));
		return r_out;
	}
	WARN_PRINT(String("SkyCycle: res://assets/precip.gdshader 加载失败（") + String(p_tag)
			+ "），降水退回 StandardMaterial3D —— 雨雪会穿屋顶落进屋里。");
	Ref<StandardMaterial3D> fb;
	fb.instantiate();
	fb->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	fb->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	fb->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	fb->set_billboard_mode(p_fixed_y ? BaseMaterial3D::BILLBOARD_FIXED_Y : BaseMaterial3D::BILLBOARD_ENABLED);
	fb->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	return fb;
}

// 采集木屋屋顶遮蔽体（局部空间盒体），写进降水着色器。
// 木屋挂在 World/Homestead/Cabin；SkyCycle 与它同级挂在 World 下。
void SkyCycle::build_precip_shelter() {
	if (precip_shelter_ready || precip_mat.is_null()) {
		return;
	}
	Node *world = get_parent();
	Node3D *cabin = (world != nullptr)
			? Object::cast_to<Node3D>(world->get_node_or_null(NodePath("Homestead/Cabin")))
			: nullptr;
	if (cabin == nullptr) {
		WARN_PRINT("SkyCycle: 找不到 Homestead/Cabin，屋檐遮蔽未启用（雨雪会落进屋里）");
		return;
	}

	Transform3D gt = cabin->get_global_transform();
	shelter_origin = gt.get_origin();
	// 木屋只有绕 Y 的旋转：局部 +X 在世界里的方向是 (cos yaw, 0, -sin yaw)
	Vector3 xw = gt.basis.get_column(0);
	float yaw = Math::atan2(-xw.z, xw.x);
	shelter_cos = Math::cos(yaw);
	shelter_sin = Math::sin(yaw);

	auto add_box = [&](const Vector3 &lo, const Vector3 &hi, const Vector4 &slope) {
		if (shelter_count >= MAX_SHELTERS) {
			return;
		}
		shelters[shelter_count].lo = lo;
		shelters[shelter_count].hi = hi;
		shelters[shelter_count].slope = slope;
		shelter_count++;
	};

	// 0 号：主屋 gable 坡屋顶。PrismMesh 的屋脊沿局部 Z 挤出，所以屋面沿 ±X 下斜：
	//        屋脊 (x=0) y=5.8、檐口 (x=±4.4) y=3.8 → 斜率 2.0/4.4。
	//        lo.y 取到地面以下，保证"从屋面到地板"整段都被剔除。
	add_box(Vector3(-4.4f, -0.6f, -3.8f), Vector3(4.4f, 5.8f, 3.8f),
			Vector4(2.0f / 4.4f, 0.0f, 0.0f, 0.0f));
	// 1 号：东侧柴棚（单坡顶 12°，用平顶保守值；棚子小，0.3m 误差看不出来）
	add_box(Vector3(4.0f, -0.6f, -3.4f), Vector3(7.46f, 3.2f, 0.6f),
			Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	// 2 号：南侧门廊雨棚（PorchRoof 中心 y=3.0、板厚 0.14）
	add_box(Vector3(-2.0f, -0.6f, 3.3f), Vector3(2.0f, 3.07f, 5.4f),
			Vector4(0.0f, 0.0f, 0.0f, 0.0f));

	// 把遮蔽体写进两份降水材质（雨丝 + 雪花）
	Ref<ShaderMaterial> mats[2] = { precip_mat, precip_mat_snow };
	const char *names_min[MAX_SHELTERS] = { "smin0", "smin1", "smin2" };
	const char *names_max[MAX_SHELTERS] = { "smax0", "smax1", "smax2" };
	const char *names_slope[MAX_SHELTERS] = { "sslope0", "sslope1", "sslope2" };
	for (int m = 0; m < 2; ++m) {
		if (mats[m].is_null()) {
			continue;
		}
		mats[m]->set_shader_parameter("shelter_count", shelter_count);
		mats[m]->set_shader_parameter("shelter_origin", shelter_origin);
		mats[m]->set_shader_parameter("shelter_cos", shelter_cos);
		mats[m]->set_shader_parameter("shelter_sin", shelter_sin);
		for (int i = 0; i < shelter_count; ++i) {
			mats[m]->set_shader_parameter(StringName(names_min[i]), shelters[i].lo);
			mats[m]->set_shader_parameter(StringName(names_max[i]), shelters[i].hi);
			mats[m]->set_shader_parameter(StringName(names_slope[i]), shelters[i].slope);
		}
	}
	precip_shelter_ready = true;
}

bool SkyCycle::is_sheltered(const Vector3 &p_world) const {
	if (shelter_count <= 0) {
		return false;
	}
	Vector3 rel = p_world - shelter_origin;
	// 世界 → 木屋局部（绕 Y 反转 yaw），与 precip.gdshader 里完全同一套算法
	Vector3 lp(rel.x * shelter_cos - rel.z * shelter_sin,
			rel.y,
			rel.x * shelter_sin + rel.z * shelter_cos);
	for (int i = 0; i < shelter_count; ++i) {
		const ShelterBox &b = shelters[i];
		if (lp.x <= b.lo.x || lp.x >= b.hi.x || lp.z <= b.lo.z || lp.z >= b.hi.z || lp.y <= b.lo.y) {
			continue;
		}
		float top = b.hi.y - b.slope.x * std::abs(lp.x - b.slope.y) - b.slope.z * std::abs(lp.z - b.slope.w);
		if (lp.y < top) {
			return true;
		}
	}
	return false;
}

void SkyCycle::build_rain() {
	// 雨丝材质：竖直细长面片，Y 轴朝向相机（fixed_y = true）
	Ref<Material> mat = make_precip_material(precip_mat, true, "雨丝");

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
	// fixed_y = false：雪花在各个角度看都是同一片（雨丝才需要锁竖）
	Ref<Material> mat = make_precip_material(precip_mat_snow, false, "雪花");

	Ref<QuadMesh> flake;
	flake.instantiate();
	flake->set_size(Vector2(0.085f, 0.085f)); // 略放大：雪白背景下更拎得清形状（2026-10-31）
	flake->set_material(mat);

	snowflakes = memnew(CPUParticles3D);
	snowflakes->set_name("Snowflakes");
	snowflakes->set_amount(480);
	snowflakes->set_lifetime(12.0f); // 慢落，需要长寿命铺满整段下落路径
	snowflakes->set_mesh(flake);
	snowflakes->set_color(Color(0.95f, 0.97f, 1.0f, 0.92f));
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

void SkyCycle::build_underwater() {
	// 水下氛围：相机低于水面时，全屏淡入一层青蓝色（配合水面着色器的"水下穹顶"）。
	// 用 CanvasLayer + ColorRect 覆盖，不改 Environment —— 与天气/昼夜的雾色计算互不干扰。
	uw_layer = memnew(CanvasLayer);
	uw_layer->set_name("UnderwaterFx");
	uw_layer->set_layer(90);
	add_child(uw_layer);
	uw_tint = memnew(ColorRect);
	uw_tint->set_anchors_preset(Control::PRESET_FULL_RECT);
	uw_tint->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	uw_tint->set_color(Color(0.04f, 0.26f, 0.30f, 0.0f));
	uw_layer->add_child(uw_tint);

	// 光柱/焦散：在青蓝色之上叠一层加色光效（不透明层遮住水下细节的问题由它补出"水感"）
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr && rl->exists("res://assets/uw_rays.gdshader")) {
		Ref<Shader> rays_shader = rl->load("res://assets/uw_rays.gdshader");
		if (rays_shader.is_valid()) {
			uw_rays_mat.instantiate();
			uw_rays_mat->set_shader(rays_shader);
			uw_rays_mat->set_shader_parameter("uw_amount", 0.0f);
			uw_rays = memnew(ColorRect);
			uw_rays->set_name("UnderwaterRays");
			uw_rays->set_anchors_preset(Control::PRESET_FULL_RECT);
			uw_rays->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			uw_rays->set_color(Color(1.0f, 1.0f, 1.0f, 1.0f));
			uw_rays->set_material(uw_rays_mat);
			uw_rays->set_visible(false);
			uw_layer->add_child(uw_rays);
		}
	}

	// 水下音频：Master 总线挂一个低通，入水时截止频率滑到 ~700Hz（声音发闷），
	// 出水滑回 20kHz（近似直通）。始终启用、只改截止频率，避免开关时的咔哒声。
	AudioServer *as = AudioServer::get_singleton();
	if (as != nullptr) {
		uw_lp.instantiate();
		uw_lp->set_cutoff(20000.0f);
		uw_lp->set_resonance(0.35f);
		as->add_bus_effect(0, uw_lp);
		uw_lp_idx = as->get_bus_effect_count(0) - 1;
	}

	// 水下气泡：相机周围缓慢上升的细小气泡（只在入水时发射）
	Ref<SphereMesh> bubble_mesh;
	bubble_mesh.instantiate();
	bubble_mesh->set_radius(0.014f);
	bubble_mesh->set_height(0.028f);
	bubble_mesh->set_radial_segments(8);
	bubble_mesh->set_rings(4);
	Ref<StandardMaterial3D> bubble_mat;
	bubble_mat.instantiate();
	bubble_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	bubble_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	bubble_mat->set_albedo(Color(0.85f, 0.94f, 1.0f, 0.55f));
	bubble_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	bubble_mesh->set_material(bubble_mat);
	uw_bubbles = memnew(CPUParticles3D);
	uw_bubbles->set_name("UnderwaterBubbles");
	uw_bubbles->set_amount(36);
	uw_bubbles->set_lifetime(2.6f);
	uw_bubbles->set_mesh(bubble_mesh);
	uw_bubbles->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	uw_bubbles->set_spread(28.0f);
	uw_bubbles->set_gravity(Vector3(0.0f, 1.6f, 0.0f));
	uw_bubbles->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.10f);
	uw_bubbles->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.35f);
	uw_bubbles->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	uw_bubbles->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
	uw_bubbles->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	uw_bubbles->set_emission_box_extents(Vector3(1.6f, 0.6f, 1.6f));
	uw_bubbles->set_visibility_aabb(AABB(Vector3(-4, -2, -4), Vector3(8, 8, 8)));
	uw_bubbles->set_emitting(false);
	add_child(uw_bubbles);
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
	// 程序化圆形光斑纹理：不贴纹理时 0.9m 方片在夜空里会显成小方块
	Ref<Image> star_img = Image::create(32, 32, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 32; x++) {
			float dx = ((float)x + 0.5f) / 16.0f - 1.0f;
			float dy = ((float)y + 0.5f) / 16.0f - 1.0f;
			float d = std::sqrt(dx * dx + dy * dy);
			float a = Math::clamp(1.0f - d, 0.0f, 1.0f);
			a = std::pow(a, 0.75f); // 柔边但保住中心亮度
			star_img->set_pixel(x, y, Color(1.0f, 1.0f, 1.0f, a));
		}
	}
	Ref<ImageTexture> star_tex = ImageTexture::create_from_image(star_img);
	star_mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, star_tex);

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
		const float s = lerpf(0.7f, 2.2f, rnd(rng)); // 大小错落（圆形纹理边缘透明，实际亮点略小）
		xf.basis = Basis().scaled(Vector3(s, s, s));
		const float b = lerpf(0.35f, 1.0f, rnd(rng)); // 亮度错落，微偏冷白
		mm->set_instance_transform(placed, xf); // 此前只算 xf 未写入 → 220 颗星全堆在原点
		mm->set_instance_color(placed, Color(b, b, clampf(b * 1.08f, 0.0f, 1.0f)));
		placed++;
	}

	stars = memnew(MultiMeshInstance3D);
	stars->set_name("Stars");
	stars->set_multimesh(mm);
	stars->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(stars);

	// 流星：一根细长加色光条（拖尾）。平时隐藏，夜间偶发。
	Ref<BoxMesh> streak;
	streak.instantiate();
	streak->set_size(Vector3(0.07f, 0.07f, 5.0f));
	star_trail_mat.instantiate();
	star_trail_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	star_trail_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	star_trail_mat->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	star_trail_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	star_trail_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, false);
	star_trail_mat->set_albedo(Color(0.82f, 0.92f, 1.0f, 1.0f));
	streak->set_material(star_trail_mat);
	shooting_star = memnew(MeshInstance3D);
	shooting_star->set_name("ShootingStar");
	shooting_star->set_mesh(streak);
	shooting_star->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	shooting_star->set_visible(false);
	add_child(shooting_star);
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
		sphere->set_radius(5.2f); // 再缩小约 23%，精致紧凑，绝不臃肿
		sphere->set_height(10.4f);
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
	sphere->set_radius(6.0f);  // 缩小兜底尺寸
	sphere->set_height(12.0f);
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
