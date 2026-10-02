#pragma once

// CabinDoor —— 小木屋的木门（可开关）。
//
// 结构（局部坐标：原点在门洞中心的地面上，+Z 朝屋外门廊）：
//   CabinDoor
//   ├─ Frame*        门框（左右门柱 + 上槛，静态装饰）
//   └─ Pivot         合页枢轴（在门扇左缘，绕 Y 轴旋转）
//      └─ Panel      StaticBody3D 门扇（有碰撞，开关时跟着转）
//         ├─ 门板 / 凹槽饰条 / 门把手（把手两侧各一个圆头 + 穿门轴）
//
// 开门方向：interact(player_pos) 里把主角坐标换到门局部空间，
// 主角在门外（z>0）就把门往里推，在屋内就往外拉 —— 推门方向随站位变化。
// 开关过程在 _process 里做平滑补间（Hermite 缓动），约 0.33 秒完成。

#include "interactable.h"

#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CabinDoor : public Interactable {
	GDCLASS(CabinDoor, Interactable)

private:
	Node3D *pivot = nullptr;  // 合页枢轴
	float open_t = 0.0f;      // 当前开合进度 0(关)..1(开)
	float open_target = 0.0f; // 目标进度
	float open_sign = 1.0f;   // 开门旋转方向（+1 往里推，-1 往外拉）

	void build(Ref<Shader> p_shader);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
	void _process(double p_delta) override;

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual void interact(const Vector3 &p_player_pos) override;

	bool is_open() const { return open_target > 0.5f; }
};

} // namespace godot
