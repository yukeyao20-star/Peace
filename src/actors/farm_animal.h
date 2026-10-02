#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <random>

namespace godot {

class AnimatableBody3D;
class CollisionShape3D;
class Node3D;
class SkyCycle;

// FarmAnimal —— 农场小动物（猫 / 狗 / 牛 / 羊）。
//
// 一个类四种形态（kind 属性）：程序化低多边形模型（身/头/腿/耳尾），
// MC 牧场风味——羊是蓬松白球+黑脸，牛是白底黑斑+短角，猫橘狗棕。
//
// 行为（状态机，每帧驱动）：
//   * 白天：发呆(2~5s) → 选家点半径内一个落点 → 慢走过去（腿交替摆动）
//     → 低头吃草(2~4s) → 循环；落点先向下射线贴地，落水/悬空就重选；
//   * 夜里（夜幕因子 > 0.5）：原地趴下睡觉（头埋低、身体压扁 12%）；
//   * 雨雪天（wet>0.5）：不再漫游，原地缩着偶尔甩头。
// 随机源按出生世界坐标哈希播种，整图行为可复现。
class FarmAnimal : public Node3D {
	GDCLASS(FarmAnimal, Node3D)

	int kind = 2;            // 0 猫 / 1 狗 / 2 牛 / 3 羊
	float home_radius = 10.0f; // 漫游半径（相对出生点）

	Node3D *body = nullptr;        // 整体模型根（朝向旋转挂这）
	Node3D *head = nullptr;        // 头（吃草/睡觉时下低）
	Node3D *legs[4] = {};          // 四条腿（行走摆动）
	Node3D *tail = nullptr;        // 尾巴（摇尾/摆动）
	Vector3 head_rest_pos;         // 头的静置局部位置（压睡姿时用）
	Vector3 body_rest_scale = Vector3(1.0, 1.0, 1.0);
	Vector3 tail_rest_rot;         // 尾巴静置旋转
	float anim_time = 0.0f;        // 持续微动作与呼吸时钟

	AnimatableBody3D *collider_body = nullptr; // 物理碰撞体（Layer 2，杜绝人与动物穿模）
	CollisionShape3D *collider_shape = nullptr;
	CollisionShape3D *head_shape = nullptr;    // 头部专用碰撞体（彻底杜绝头部穿模）

	bool is_sleeper = true;                    // 当前夜间是否处于卧睡倾向
	float sleep_cycle_t = 0.0f;                // 睡眠/清醒动态轮换时钟
	float lie_down_weight = 0.0f;              // 卧倒过渡权重 (0.0 站立 ~ 1.0 完全卧倒)

	SkyCycle *sky = nullptr; // 懒查找一次
	std::mt19937 rng;

	// 状态机：0 发呆 / 1 走向目标 / 2 吃草
	int state = 0;
	float state_t = 1.0f;       // 当前状态剩余秒数
	Vector2 target;             // 漫游目标（世界 XZ）
	Vector2 home;               // 出生点（世界 XZ）
	float walk_phase = 0.0f;    // 腿部摆动相位
	float speed = 0.7f;         // 移动速度（按 kind 定）
	float footprint_acc = 0.0f; // 脚印步距累计
	bool foot_side = false;     // 左右脚印交替
	bool grounded_once = false; // 首次贴地完成前不做决策
	float home_ground_y = -1e9f; // 出生点地面高度（拒绝贴上房顶等结构面的锚点）

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	FarmAnimal() = default;
	~FarmAnimal() override = default;

	void set_kind(int p_kind);
	int get_kind() const { return kind; }

private:
	void build_model();                    // 按 kind 拼程序化模型
	Ref<StandardMaterial3D> mat_of(const Color &p_c); // 简单彩色材质
	float ground_y(float p_x, float p_z);  // 向下射线求地面高度（失败返回 1e9）
	void pick_target();                    // 在 home_radius 内选一个可站立的落点
	void tick_behavior(float p_delta, float p_night, float p_wet);
};

} // namespace godot
