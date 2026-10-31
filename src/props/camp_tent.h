#pragma once

// CampTent —— 火堆附近的小帆布帐篷。
//
// A 字木杆撑起两片帆布，里面有一条铺好的床卷和小枕头；门口挂一盏小灯，
// 入夜亮起；风大时前襟会轻轻掀动。纯氛围，不参与交互。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class MeshInstance3D;
class OmniLight3D;
class SkyCycle;
class StandardMaterial3D;
class TerrainBuilder;

class CampTent : public Node3D {
	GDCLASS(CampTent, Node3D)

private:
	Node3D *flap_l = nullptr;
	Node3D *flap_r = nullptr;
	OmniLight3D *lamp = nullptr;
	Ref<StandardMaterial3D> lamp_glass;
	SkyCycle *sky = nullptr;
	float t = 0.0f;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
