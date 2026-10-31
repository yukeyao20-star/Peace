#pragma once

// SkyCycle —— 天色循环 + 天气系统。
//
// 天色：time_of_day ∈ [0,1) 随时间推进（day_length 秒 = 一整天），
//   0.25 = 日出（清晨），0.5 = 正午，0.75 = 日落（傍晚），0 = 午夜。
//   每帧按太阳高度插值：天空盒四色、太阳旋转/颜色/能量（夜晚自动切换成月光）、
//   环境光强度、雾色与雾浓度。
//
// 天气：状态机 晴天 ⇄ 多云 ⇄ 小雨 ⇄ 大雨 ⇄ 雪，随机时长流转；
//   cloud / rain / snow 三个 0..1 因子平滑过渡：压暗天空、加浓雾、削弱阳光，
//   并开关两层跟随主角的雨粒子（毛毛雨 / 倾盆大雨）与雪花粒子层。
//
// 场景里挂在 World 根下，sun_path / env_path 指向 Sun 与 WorldEnvironment。

#include <cmath>

#include <godot_cpp/classes/audio_effect_low_pass_filter.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector4.hpp>

namespace godot {

class CPUParticles3D;
class CanvasLayer;
class ColorRect;
class DirectionalLight3D;
class Environment;
class CSGBox3D;
class MeshInstance3D;
class MultiMeshInstance3D;
class Node3D;
class ProceduralSkyMaterial;
class ShaderMaterial;
class StandardMaterial3D;
class WorldEnvironment;

class SkyCycle : public Node3D {
	GDCLASS(SkyCycle, Node3D)

private:
	NodePath sun_path;   // 指向 DirectionalLight3D（太阳/月亮）
	NodePath env_path;   // 指向 WorldEnvironment
	NodePath fireflies_path; // 指向 CPUParticles3D（萤火虫，仅夜间晴天出现）

	DirectionalLight3D *sun = nullptr;
	WorldEnvironment *world_env = nullptr;
	Ref<ProceduralSkyMaterial> sky_mat;
	Ref<Environment> env;
	Ref<ShaderMaterial> water_mat; // 水面材质（Terrain/Lake，懒查找；rain 驱动其 rain_amount）
	Node3D *spray_root = nullptr;  // 礁石激浪水花容器（Terrain/ShoreSpray，懒查找；结冰封湖时淡出）
	float spray_ratio_applied = -1.0f; // 已写入的粒子发射比例（避免每帧无脑刷 32 个节点）
	// 太阳/月亮的世界方向（指向光源）。每帧 apply() 里更新，供需要"从哪边照过来"的
	// 材质读取 —— 目前是树冠叶片透光（背光面透出的暖光）和水面碎金高光。
	Vector3 sun_dir = Vector3(0.3f, 0.8f, 0.5f);

	float time_of_day = 0.32f;   // 0..1，开局在上午
	float day_length = 240.0f;   // 一整天的现实秒数
	int day_count = 1;           // 第几天（time_of_day 跨过午夜时 +1）

	// —— 天气状态机 ——
	int weather = 0;             // 0 晴 1 多云 2 小雨 3 大雨 4 雪
	float weather_timer = 40.0f; // 当前天气剩余秒数
	float cloud = 0.0f, cloud_target = 0.0f; // 云量因子 0..1
	float rain = 0.0f, rain_target = 0.0f;   // 雨强因子 0..1
	float snow = 0.0f, snow_target = 0.0f;   // 雪强因子 0..1

	Node3D *rain_root = nullptr;       // 降水粒子容器（每帧跟随主角）
	CPUParticles3D *drizzle = nullptr; // 毛毛雨层
	CPUParticles3D *downpour = nullptr; // 大雨层
	CPUParticles3D *snowflakes = nullptr; // 雪花层
	CPUParticles3D *fireflies = nullptr; // 萤火虫（夜间 + 非雨雪天）
	Ref<ShaderMaterial> precip_mat;      // 雨丝材质（fixed_y = true，带屋檐遮蔽）
	Ref<ShaderMaterial> precip_mat_snow; // 雪花材质（fixed_y = false；billboard 模式不同必须分开）
	// —— 屋檐遮蔽体：全部在木屋局部空间描述（木屋绕 Y 转了 45°，
	//    用世界 AABB 会在屋子周围削出一块菱形"无雨区"）。
	//    既给着色器剔除粒子，也给 is_sheltered() 判断"玩家在不在屋檐下"。 ——
	struct ShelterBox {
		Vector3 lo;
		Vector3 hi;
		Vector4 slope; // (slope_x, x0, slope_z, z0)：沿局部 X / Z 的下斜率与基准点
	};
	static const int MAX_SHELTERS = 3;
	ShelterBox shelters[MAX_SHELTERS];
	int shelter_count = 0;
	Vector3 shelter_origin;
	float shelter_cos = 1.0f;
	float shelter_sin = 0.0f;
	bool precip_shelter_ready = false; // 是否已采集并写入遮蔽体
	MultiMeshInstance3D *stars = nullptr;    // 星空穹顶（夜空晴天可见）
	Ref<StandardMaterial3D> star_mat;        // 星星材质（每帧只调 albedo 透明度）
	MeshInstance3D *moon = nullptr;          // 月盘（夜空可见，高度跟随 -sun_h）
	Ref<StandardMaterial3D> moon_mat;        // 月盘兜底材质（着色器加载失败时用，每帧只调透明度）
	Ref<ShaderMaterial> moon_shader_mat;     // 月盘月相材质（moon.gdshader：陨石坑+相位+地照）
	int moon_phase_day = -1;                 // 已写入相位的 day_count（只在跨天时更新 phase uniform）
	Node3D *player = nullptr;          // 懒查找的主角
	CanvasLayer *uw_layer = nullptr;   // 水下氛围覆盖层（相机入水时淡入）
	ColorRect *uw_tint = nullptr;
	float uw_alpha = 0.0f;             // 当前不透明度（平滑过渡）
	Ref<AudioEffectLowPassFilter> uw_lp; // 水下低通（Master 总线）
	int uw_lp_idx = -1;
	float uw_cutoff = 20000.0f;
	Ref<StandardMaterial3D> lamp_glow_mat;   // 桥头石灯灯芯材质（两盏灯共用 LampGlowMat，懒查找，改一次全生效）
	float lamp_energy = 0.0f;                // 石灯当前自发光强度（apply() 里 move_toward 平滑逼近目标）

	void build_rain();
	void build_snow();
	void build_stars();
	void build_moon();
	void build_underwater();
	void build_precip_shelter(); // 采集木屋屋顶遮蔽体，写进降水着色器
	void pick_next_weather();
	void apply(); // 把 time_of_day / cloud / rain 写进灯光与天空

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void set_sun_path(const NodePath &p) { sun_path = p; }
	NodePath get_sun_path() const { return sun_path; }
	void set_env_path(const NodePath &p) { env_path = p; }
	NodePath get_env_path() const { return env_path; }
	void set_fireflies_path(const NodePath &p) { fireflies_path = p; }
	NodePath get_fireflies_path() const { return fireflies_path; }
	void set_time_of_day(float p) { time_of_day = p; }
	float get_time_of_day() const { return time_of_day; }
	void set_day_length(float p) { day_length = p; }
	float get_day_length() const { return day_length; }
	void set_day_count(int p) { day_count = p; } // 存档恢复用（正常游戏只读自增）
	int get_day_count() const { return day_count; }
	float get_rain_amount() const { return rain; } // 雨强因子 0..1（供夜光/水面等系统联动）
	float get_snow_amount() const { return snow; } // 雪强因子 0..1
	Vector3 get_sun_dir() const { return sun_dir; } // 指向太阳/月亮的世界方向（植被透光等用）

	// 某世界坐标点是否在屋檐下（屋里 / 檐下）。与降水粒子剔除用同一套遮蔽体，
	// 供 Ambience 判断"人在室内就该把雨雪声压掉"。
	bool is_sheltered(const Vector3 &p_world) const;

	// —— 天气状态快照（SaveSystem 存读用）——
	int get_weather() const { return weather; }
	void set_weather(int p) { weather = p; }
	float get_cloud() const { return cloud; }
	void set_cloud(float p) { cloud = p; }
	float get_cloud_target() const { return cloud_target; }
	void set_cloud_target(float p) { cloud_target = p; }
	void set_rain(float p) { rain = p; }
	float get_rain_target() const { return rain_target; }
	void set_rain_target(float p) { rain_target = p; }
	void set_snow(float p) { snow = p; }
	float get_snow_target() const { return snow_target; }
	void set_snow_target(float p) { snow_target = p; }

	// 月相 0..1（0 新月 0.25 上弦 0.5 满月 0.75 下弦）：第 1 天满月、8 天一个朔望循环。
	// 公式的唯一来源——apply() 写 shader uniform 与 HUD 月相名都从这里取。
	float get_moon_phase() const {
		return std::fmod(0.5f + (float)(day_count - 1) * 0.125f, 1.0f);
	}

	// 夜幕因子 0..1（白天 0 → 深夜 1）：sun_h 从 -0.02 降到 -0.12 线性映射 0→1。
	// 公式唯一来源——Bird/Rabbit/ButterflySwarm/TerrainBuilder/Ambience 的昼夜
	// 阈值都从这里取（此前各抄一份 sin 公式已漂移）；找不到 SkyCycle 的调用方
	// 自行按 0（= 正午白天）兜底，与本函数在 time_of_day=0.5 时的返回值一致。
	float get_night_factor() const;

	String get_phase_name() const; // 如 "白天 · 小雨"（调试用）
};

} // namespace godot
