#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <map>
#include <vector>

namespace godot {

class CPUParticles3D;
class SkyCycle;

// TerrainBuilder —— 程序化世界搭建器。
//
// 场景里只放一个空的 TerrainBuilder 节点，运行时它会一次性生成整个世界：
//   1. 地形网格：多层噪声叠加出山丘，挖出湖盆，地图边缘抬升成环山；
//      顶点着色区分 草地 / 沙滩 / 水下湖床 / 岩壁，并生成三角网格碰撞。
//   2. 水面：CS2 风格着色器（滚动法线噪声、屏幕折射、深度吸收、
//      菲涅尔天空反射、岸边泡沫），见 project/assets/water.gdshader。
//   3. 植被与点缀：树（树干 + 两层树冠 + 果子）、灌木、草丛、芦苇、
//      睡莲、小花、石头 —— 全部用 MultiMesh 批量渲染（每种一次 draw call）。
//
// 布局原则：树林按"抖动网格"成片分布保证均衡，湖岸有灌木/芦苇/石头环，
// 草与小花铺满可用陆地，湖心放睡莲。所有随机数固定种子 → 每次启动地图一致。
//
// 本次细化（参照 MC 的美术风格）：
//   * 石桥引道：桥两端（x = lake_center.x ± 15，端头标高 -0.2）的地形
//     引道与 CSG 桥面严格齐平；桥台/翼墙在 world.tscn 的 StoneBridge 里。
//   * 浆果丛：林缘成簇分布，深绿矮丛缀鲜红浆果（MC 甜浆果丛风味）。
//   * 草丛颜色按噪声分片区（一坡一色），并避让泥土小径。
//   * 泥土小径：出生点 → 宅基地院门、出生点 → 石桥西岸，烙进地形顶点色。
//   * 草地聚簇 + 裸土斑块：草丛密度图与地形顶点色共用同一噪声
//     （0.14 频率、偏移 21/11）—— 稀草区自动露出干土，MC 草方块观感。
//   * 山体岩层：陡坡/高山露岩处按海拔画水平层理条带（边界噪声扰动），
//     岩壁下用 Scatter "Scree" 撒碎石坡。
//   * 倒木：林间平缓处横卧少量枯木（Scatter "FallenLogs"，两端贴地校验）。
//   * 蘑菇：树荫下林缘成簇（MC 红伞白点蘑菇 + 棕色蘑菇混生，白点贴伞盖上表面）；
//     夜光 —— 伞盖/白点带自发光，能量由昼夜驱动（夜里泛暖橘光、白点泛冷白光），
//     约 40% 的簇夜里冒淡绿"孢子光尘"粒子（白天不冒）。
//   * 湖岸湿沙带：水线上方一圈深色湿沙过渡带（湖水涨落的浸润痕迹）。
//   * 巨石：全图缓坡稀疏分布的大石头（0.8~2.2m，灰岩色带随机色差，半埋入土）。
//   * 树桩：树林区里的伐木残桩（矮圆柱 + 顶面浅色年轮盘，贴地校验防悬空）。
class TerrainBuilder : public Node3D {
	GDCLASS(TerrainBuilder, Node3D)

private:
	// —— 世界参数（改这里即可重排地图，改完重新编译） ——
	float map_size = 448.0f;                     // 地图边长（米，320→448 扩图，可用面积×1.96）
	int grid_steps = 320;                        // 地形每边网格数（顶点间距维持 ~1.4m）
	float water_level = -0.55f;                  // 水面海拔
	Vector2 lake_center = Vector2(-6.0f, 26.0f); // 湖区跨桥参考中心（x, z）
	float lake_radius = 44.0f;                   // 湖的控制半径（大湖主水域 44m）
	Vector2 spawn_point = Vector2(0.0f, -28.0f); // 出生点（自动压平成小平台）


	// —— 足球场（专属草坪：全图草毯已删，草只长在这里） ——
	Vector2 pitch_center = Vector2(60.0f, -40.0f); // 球场中心（改这两个数=挪球场）
	float pitch_half_w = 24.0f;                    // 半宽（东西），全宽 = 2×这个
	float pitch_half_d = 15.0f;                    // 半深（南北）
	float pitch_height = 0.10f;                    // 场地海拔（压平到这个高度）
	int pitch_blades = 700000;                     // 草叶卡片数（70万张 × 12根/张 = 840万根视觉草）

	// —— 石桥引道参数（与 world.tscn 里 StoneBridge 的几何保持一致：
	//     桥体沿 X 轴跨湖，桥面端头在 x = lake_center.x ± 15、标高 -0.2） ——
	float bridge_end_west = -21.0f; // 桥西端（世界坐标 x）
	float bridge_end_east = 9.0f;   // 桥东端（世界坐标 x）
	float bridge_deck_end_y = -0.26f; // 桥面引道压平到此高度（比 CSG 桥面 -0.2 低 6cm，消除 Z-fighting 频闪）

	// 宅基地：湖东南岸的木屋+篱笆院子，压平成平台（高度与 world.tscn 里
	// Homestead 节点的 y 保持一致，否则房子会悬空或陷地）
	Vector2 homestead_center = Vector2(12.0f, 8.0f);
	float homestead_height = 0.35f;

	// —— 高度场 ——
	float value_noise(float x, float z) const;    // 单层平滑值噪声（0..1）
	float fbm(float x, float z) const;            // 三层叠加（约 -1..1）
	float terrain_height(float x, float z) const; // 最终地形高度
	float terrain_slope(float x, float z) const;  // 坡度 |∇h|（1 ≈ 45°）

	// —— 生成步骤 ——
	void build_terrain();
	void build_water();
	void build_vegetation();

	// —— 昼夜/天气驱动的运行时材质（逐帧，代价极低） ——
	// 懒查找兄弟 SkyCycle → time_of_day / rain / snow：
	//   * 昼夜 → 蘑菇伞盖/白点自发光能量与孢子光尘开关；
	//   * 雪强 → 地形雪盖（terrain_mat 的 snow_amount）与分级积雪
	//     （snow_bands：每档 veg_wind 材质按自己的 [lo,hi] 区间映射雪量——
	//     贴地的草/灌木/蘑菇伞顶先积雪，树冠与树枝雪毯按树顶高度分三档递进）。
	// 找不到 SkyCycle 视为晴天白天。
	void update_sky_driven();
	SkyCycle *sky_cache = nullptr; // 懒查找一次后缓存（同 bird/rabbit 套路，不再每帧 get_node_or_null）
	Ref<StandardMaterial3D> shroom_cap_mat;  // MushroomCaps 专用（夜光暖橘）
	Ref<StandardMaterial3D> shroom_spot_mat; // MushroomSpots 专用（夜光冷白）
	std::vector<CPUParticles3D *> spore_puffs; // 孢子光尘（夜开昼关）
	Ref<ShaderMaterial> terrain_mat;           // 地形雪盖材质（snow_ground.gdshader，加载失败时为空 → 用标准材质兜底）

	// 积雪分级条目：雪强 snow ∈ [0,1] → 生效值 v = sstepf(lo, hi, snow) 写进
	// veg_wind 的 snow_amount（覆雪混色 + 梢部下坠都跟着分级）；drive_alpha 的
	// 条目（树枝/蘑菇雪毯）再以 v 写 alpha——无雪隐形，低档先现身、高档后现身。
	struct SnowBand {
		Ref<ShaderMaterial> mat;
		float lo = 0.0f;
		float hi = 1.0f;
		bool drive_alpha = false;
	};
	std::vector<SnowBand> snow_bands;

	// 踩草倒伏：草丛/高草专用 veg_wind 材质（update_sky_driven 每帧把玩家与
	// "farm_animals" 组里动物的世界坐标写进 trample_pos/trample_count）
	std::vector<Ref<ShaderMaterial>> trample_mats;
	// 踩草源状态追踪：节点 → { 上一帧位置, 移动能量(0静止~1跑动), 着地权重(0空中~1地面) }
	struct TrampleState {
		Vector3 prev_pos;
		float energy = 0.0f;
		float weight = 1.0f;
	};
	std::map<Node *, TrampleState> trample_track;

	// 灌木丛障碍物列表（用于玩家穿过时减速）
	struct BushObstacle {
		Vector3 pos;
		float radius = 0.5f;
		float height = 0.8f;
	};
	std::vector<BushObstacle> bush_obstacles;

	static float sstepf(float a, float b, float x); // 平滑阶跃（Hermite）

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
	bool in_pitch(float x, float z) const;
	float dist_to_path(float x, float z) const;
	bool is_in_bush(float x, float z, float y) const;
	bool is_near_lake(float x, float z, float margin = 0.0f) const;
};

} // namespace godot
