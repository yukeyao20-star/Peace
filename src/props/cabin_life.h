#pragma once

// CabinLife —— 木屋的"有人住"细节：烟囱炊烟、入夜窗灯、门口小灯。
//
// 以 Homestead 的局部坐标摆放（木屋整体绕 Y 转了 45°）：
//   * 屋顶石烟囱常年冒一缕被风带偏的青烟；
//   * 入夜后四扇窗透出暖光、门口小灯亮起，火光轻微跳动；
//   * 雨雪天窗户蒙上一层湿雾（辉光略降）。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CPUParticles3D;
class OmniLight3D;
class SkyCycle;
class StandardMaterial3D;

class CabinLife : public Node3D {
	GDCLASS(CabinLife, Node3D)

private:
	SkyCycle *sky = nullptr;
	Ref<StandardMaterial3D> window_glow[4];
	OmniLight3D *window_light = nullptr;
	OmniLight3D *porch_light = nullptr;
	Ref<StandardMaterial3D> porch_glass;
	CPUParticles3D *smoke = nullptr;
	float t = 0.0f;
	float wet = 0.0f;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	float get_window_glow_energy(int p_index) const;
	float get_porch_energy() const;
	bool get_smoke_enabled() const;
};

} // namespace godot
