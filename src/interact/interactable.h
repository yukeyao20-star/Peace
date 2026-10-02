#pragma once

// Interactable —— 可交互物体的最小接口。
//
// 玩家（Player）每帧扫描 "interactables" 组里最近的实例：
//   * get_prompt()      返回提示文案（如 "按 E 开门"），显示在物体上方
//   * get_interact_pos() 返回交互判定点（世界坐标）
//   * interact()        玩家按下 E 时被调用，参数是主角的世界坐标
//                       （门用它判断从哪一侧推门）
//
// 具体实现：CabinDoor（木门）、ShutterWindow（百叶窗）。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Interactable : public Node3D {
	GDCLASS(Interactable, Node3D)

protected:
	static void _bind_methods() {} // 纯接口，无需绑定方法

public:
	virtual String get_prompt() const { return String(); }
	virtual Vector3 get_interact_pos() const { return get_global_position(); }
	virtual void interact(const Vector3 &p_player_pos) {}
};

} // namespace godot
