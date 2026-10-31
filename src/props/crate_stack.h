#pragma once

// CrateStack —— 谷仓旁堆着的木箱与酒桶，还有一只粗麻袋。
// 参考此前调研的 Cozy Farm Asset Pack 与 OpenGameArt 木桶/板条箱比例。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CrateStack : public Node3D {
	GDCLASS(CrateStack, Node3D)
	static const int ITEM_TYPES = 5;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
};

} // namespace godot
