#pragma once

// WillowTree —— 湖东岸的老柳树：树干斜向水面，垂下的柳条在风里一起一伏。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class Node3D;
class WillowTree : public Node3D {
	GDCLASS(WillowTree, Node3D)
	Node3D *crown = nullptr;
	float t = 0.0f;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_strand_count() const { return 40; }
};
} // namespace godot
