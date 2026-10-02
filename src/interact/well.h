#pragma once

// Well —— 院子里的石砌水井（MC 水井风味，带辘轳）。
//
// 结构（局部坐标：原点在井底地面中心，+Z 无特殊含义）：
//   Well
//   ├─ Ring          StaticBody3D 石块井圈（圆柱碰撞，人走不进去）
//   │   ├─ 8 块围成圈的石块 / 井内深色水面
//   ├─ PostL / PostR 木柱 + 横轴（辘轳架）+ 人字小顶棚
//   ├─ Crank         摇把（在横轴右端，打水时跟着转）
//   └─ Winch         绳索锚点（横轴正下方）
//      ├─ Rope       吊绳（圆柱网格，靠 y 向缩放改变长度）
//      └─ Bucket     水桶（小圆柱 + 提梁横杆）
//
// 交互：按 E 在"打水（桶升上来）"与"放桶（桶沉回井里）"间切换，
// Hermite 缓动约 1.2 秒（比门窗慢，摇辘轳本来就该费点劲）；
// 摇把随桶的升降同步旋转（升满恰好两整圈）。

#include "interactable.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Well : public Interactable {
	GDCLASS(Well, Interactable)

private:
	Node3D *crank = nullptr;  // 摇把枢轴
	Node3D *winch = nullptr;  // 绳索锚点
	MeshInstance3D *rope = nullptr; // 吊绳（靠缩放变长）
	Node3D *bucket = nullptr; // 水桶
	float up_t = 0.0f;        // 当前 0(桶在井底)..1(桶到横轴)
	float up_target = 0.0f;   // 目标进度

	void build(Ref<Shader> p_shader);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
	void _process(double p_delta) override;

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual void interact(const Vector3 &p_player_pos) override;

	bool is_up() const { return up_target > 0.5f; }
};

} // namespace godot
