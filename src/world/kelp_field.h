#pragma once

// KelpField —— 湖底水草（氛围植被）。
//
// 在桥下水域与西北主湖的湖床上撒一片窄长水草：底部深绿、顶端浅绿，
// 随水流缓慢摆动。叶片用 MultiMesh + 自定义着色器（按世界坐标错相），
// 只在玩家靠近湖边时显示。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

class MultiMeshInstance3D;
class TerrainBuilder;

class KelpField : public Node3D {
	GDCLASS(KelpField, Node3D)

private:
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;
	MultiMeshInstance3D *mmi = nullptr;
	TerrainBuilder *terrain = nullptr;
	Node3D *player = nullptr;

	void build();
	void build_mesh(Ref<class ArrayMesh> &r_mesh);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
