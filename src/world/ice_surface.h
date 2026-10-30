#pragma once

// IceSurface —— 结冰湖面可行走碰撞（ROADMAP #12：雪天上冰的惊喜时刻）。
//
// 视觉上的结冰由 water.gdshader 的 freeze_amount 完成（SkyCycle 每帧把雪量
// 直接写进 freeze_amount，浅水先冻、湖心后封）；本类只负责"冰面能不能站"：
// 一个无限大水平碰撞面（WorldBoundaryShape3D，法线 +Y、单向托举），平时
// disabled，雪量够大才启用——不删不加节点，只切形状开关，天然规避
// "在 _process 里增删物理体"的物理帧同步坑。
//
// 由 SkyCycle READY 时 spawn 为子节点（与 build_rain/build_snow 同套路），
// 自己沿祖先链懒查找 SkyCycle 读雪量，不在场景文件里出现。
//
// 阈值带滞回：启用 0.85 / 解除 0.70，防止雪量在临界值附近抖动导致碰撞
// 忽有忽无。0.85 时湖心可见冰面已大面积成形（视觉与脚下大致对齐）。
// 本类是湖面冰碰撞的唯一归属——terrain_builder 的旧湖面 box（雪量 0.20 即
// 激活，让人走上视觉未封冻的湖面）已于 2026-10-24 移除；溪流/深潭冰仍在
// terrain_builder（高处溪段高于本平面，无限平面盖不住）。
//
// 行为语义：冰面消融（雪停）时直接解除碰撞——站在冰上的玩家会落回水里
// 蹚水（湖底地形原本就可以走）；解冻后再重新冻结，人若在水下不会被托出
// 冰面（单向平面只从上方托举），需从岸边重新走上冰面，符合直觉。

#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

class CollisionShape3D;
class SkyCycle;

class IceSurface : public StaticBody3D {
	GDCLASS(IceSurface, StaticBody3D)

private:
	CollisionShape3D *col = nullptr; // 唯一的碰撞形状（WorldBoundary，启停即开关冰面）
	SkyCycle *sky = nullptr;         // 懒查找（父链向上），拿到雪量驱动源
	bool active = false;             // 冰面当前是否可行走

	void set_active(bool p);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool is_ice_active() const { return active; } // 测试与调试用
};

} // namespace godot
