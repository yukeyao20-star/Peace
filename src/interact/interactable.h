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
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_prompt"), &Interactable::get_prompt);
		ClassDB::bind_method(D_METHOD("get_interact_pos"), &Interactable::get_interact_pos);
		ClassDB::bind_method(D_METHOD("get_interact_distance"), &Interactable::get_interact_distance);
		ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Interactable::interact);
	}

public:
	virtual String get_prompt() const { return String(); }
	virtual Vector3 get_interact_pos() const { return get_global_position(); }
	virtual float get_interact_distance() const { return 2.2f; }
	virtual bool requires_line_of_sight() const { return true; }
	virtual void interact(const Vector3 &p_player_pos) {}
};

} // namespace godot
