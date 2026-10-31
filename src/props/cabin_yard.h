#pragma once

// CabinYard —— 木屋门口的生活杂物：柴堆、劈柴墩与斧头、信箱、花盆、门垫。
//
// 全部挂在 Homestead 局部坐标下，和木屋一起转 45°；纯装饰，不参与交互。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CabinYard : public Node3D {
	GDCLASS(CabinYard, Node3D)

private:
	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
