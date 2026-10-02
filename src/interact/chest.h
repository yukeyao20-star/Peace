#pragma once

// Chest —— 可以打开/关闭的木箱（MC 储物箱风味）。
//
// 结构（局部坐标：原点在箱底中心，+Z 朝屋内一侧即"正面"）：
//   Chest
//   ├─ Body          StaticBody3D 箱体（有碰撞）
//   │   ├─ 箱体木板 / 顶部内衬（打开后能看到深色内腔）/ 黄铜锁扣座 / 四角包边
//   └─ LidPivot      盖板合页枢轴（在后上边缘，绕 X 轴向后旋开）
//      └─ Lid        StaticBody3D 盖板（有碰撞，开关时跟着转）
//         ├─ 盖板 / 黄铜锁鼻 / 两道横向加固条
//
// 开关动画与 CabinDoor 相同：Hermite 缓动约 0.33 秒；盖板向后旋开约 109°。

#include "interactable.h"

#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Chest : public Interactable {
	GDCLASS(Chest, Interactable)

private:
	Node3D *lid_pivot = nullptr; // 盖板合页枢轴
	float open_t = 0.0f;         // 当前开合进度 0(关)..1(开)
	float open_target = 0.0f;    // 目标进度

	class AudioStreamPlayer3D *audio_player = nullptr;
	Ref<class AudioStream> snd_open;
	Ref<class AudioStream> snd_close;

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
