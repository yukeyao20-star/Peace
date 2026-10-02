#pragma once

// Rabbit —— 在宅基地院子里蹦跶的小兔子。
//
// 行为：随机游走状态机——原地歇一会儿 → 挑个目标点 → 连续小跳过去 →
// 再歇。跳跃用 sin 半波做抛物线弧度，朝向始终对准前进方向。
// 天气/昼夜感知：夜里（night>0.5）或雨雪天（wet=rain+snow>0.5）蹦回窝心
// 趴着躲，天亮雨停恢复游走（找兄弟 SkyCycle 读因子，缺省视为晴朗白天）。
// 节点放在地面上（origin = 脚底），home 取 READY 时的世界坐标；
// 院子平台是压平的（高 0.35），所以直接用 home.y 当地面高度。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class AnimatableBody3D;

class Rabbit : public Node3D {
	GDCLASS(Rabbit, Node3D)

private:
	Node3D *model = nullptr;
	AnimatableBody3D *collider_body = nullptr;
	Node3D *sky_cycle = nullptr; // 懒查找的天色节点（读 time_of_day 感知昼夜）
	Vector3 home;          // 活动中心（READY 时的世界坐标）
	float wander_radius = 5.0f; // 游走半径（院子平台内）
	float idle_t = 0.0f;   // 休息剩余秒数
	float hop_t = 0.0f;    // 当前这一跳的进度
	float hop_dur = 0.38f; // 单跳时长
	Vector3 target;        // 当前目标点
	bool hopping = false;
	bool sleeping = false; // 卧睡体态（贴地微扁、安详呼吸）
	bool is_sleeper = true; // 夜间是否处于卧睡倾向（错开节律，一部分睡觉一部分走动）
	float sleep_cycle_t = 0.0f; // 昼夜动态睡眠轮换时钟

	void build();

protected:
	static void _bind_methods() {}

public:
	void _notification(int p_what);
};

} // namespace godot
