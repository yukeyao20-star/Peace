#pragma once

// FarmPlot —— 家园农田：种地小循环 + 作物多样性。
//
// 一块菜畦：E 播种（嫩苗破土）→ 生长（苗逐渐长大）→ 成熟挂果
// → E 收获（发 berry_picked ×产量，GameDirector 每日待办自动累计，
// 采摘音效由导演层反馈）→ 回到空畦，可再种。
//
// v2 三件套：
//   * 井水浇灌：打水（Well 发 water_drawn）后 20 秒内对生长中菜畦按 E
//     → 生长 +30%（一茬最多浇一次），土垄变深色（湿土）作反馈；
//   * 状态存档：get_plot_serial()/apply_plot_serial() 输出/恢复
//     "state,进度%,已浇灌,作物"，由 GameDirector 聚合成 farm_plots 存档键；
//   * 雪天盖雪：SkyCycle 雪强 > 阈值时土垄盖一层薄雪（停雪自动消）。
//
// v3 作物多样性：空畦旁按 R 换种（带缩小预览），三种作物各有节奏——
//   * 浆果：90 秒 / +2（默认，与林间采丛同款）
//   * 萝卜：60 秒 / +3（快产）
//   * 南瓜：150 秒 / +5（慢熟高产）
//
// 定位：给"采浆果"目标提供一条家园内的稳定产出口（不用满岛找丛），
// 也用快/慢作物给每日节奏提供选择。

#include "interactable.h"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class ShaderMaterial;
class SkyCycle;
class Well;

class FarmPlot : public Interactable {
	GDCLASS(FarmPlot, Interactable)

public:
	enum State {
		STATE_EMPTY = 0,   // 空畦（可播种；R 换种）
		STATE_GROWING = 1, // 生长中（可浇水加速）
		STATE_RIPE = 2,    // 成熟（可收获）
	};
	enum Crop {
		CROP_BERRY = 0,   // 浆果：90s / +2
		CROP_RADISH = 1,  // 萝卜：60s / +3
		CROP_PUMPKIN = 2, // 南瓜：150s / +5
		CROP_COUNT = 3,
	};

private:
	int state = STATE_EMPTY;
	int crop = CROP_BERRY;         // 本茬（及空畦时预选的下一茬）作物
	float grow_t = 0.0f;          // 生长进度 0..1

	bool watered = false;         // 本茬已浇灌（一茬一次，翻土/收获时复位）
	float water_carry_t = 0.0f;   // 打水后的浇灌窗口倒计时（秒，>0 手里有水）
	static constexpr float WATER_CARRY_SECONDS = 20.0f;
	static constexpr float WATER_BOOST = 0.30f;  // 浇灌一次的生长增量
	static constexpr float SNOW_SHOW = 0.45f;    // 雪强超过此值土垄盖雪

	bool r_prev = false;          // R 键边沿检测（空畦旁换种）
	class Node3D *player_cache = nullptr; // 懒查找的主角（换种的距离门控）

	Node3D *sprouts = nullptr;    // 苗组（随 grow_t 长大）
	Node3D *fruit_groups[CROP_COUNT] = { nullptr, nullptr, nullptr }; // 各作物的果组
	MeshInstance3D *soil = nullptr;    // 土垄（浇灌改其材质 albedo 变湿土色）
	Ref<ShaderMaterial> soil_mat;      // 土垄独立材质（meshutil 每块新建，可安全改）
	MeshInstance3D *snow_cap = nullptr; // 雪盖薄板（雪天才显示）
	Ref<ShaderMaterial> snow_mat;

	SkyCycle *sky = nullptr;      // 懒查找（读雪强）；World 下唯一的 SkyCycle
	Well *well = nullptr;         // 懒查找 + 连 water_drawn（有水窗口的来源）

	void build();
	void refresh_visual();
	void try_connect_well();      // 逐帧补连井的 water_drawn（READY 顺序无关）
	void set_soil_wet(bool p_wet);
	void _on_water_drawn();       // Well.water_drawn 槽：开启浇灌窗口
	void handle_seed_cycle();     // 空畦 + 玩家在旁时 R 键换种（含预览）

public:
	static float grow_seconds(int p_crop); // 各作物一茬时长
	static int crop_yield(int p_crop);     // 各作物收获产量
	static const char *crop_name(int p_crop); // UTF-8 中文名（提示文案用）

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual float get_interact_distance() const override { return 2.0f; }
	virtual void interact(const Vector3 &p_player_pos) override;

	int get_state() const { return state; }
	int get_crop() const { return crop; }
	void cycle_crop(); // 空畦换种（正常流程走 R 键；公开便于测试）
	// 测试/调试用：直接设定状态与进度
	void set_state(int p_state, float p_progress);
	// 测试/调试用：直接置浇灌标记（正常流程走 interact）
	void set_watered(bool p_watered);

	// —— 存档序列（GameDirector 聚合为 farm_plots 键）——
	String get_plot_serial() const;       // "state,进度%,已浇灌,作物"
	void apply_plot_serial(const String &p_serial);

	bool is_snow_covered() const { return snow_cap != nullptr && snow_cap->is_visible(); }
};

} // namespace godot
