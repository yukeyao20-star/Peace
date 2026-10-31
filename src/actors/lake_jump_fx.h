#pragma once

// LakeJumpFx —— 湖面偶发鱼跃（氛围小事件）。
//
// 每隔随机数秒，在湖心附近随机一点有一条小鱼跃出水面：
//   * 抛物线出水 → 摆尾 → 落回水面；
//   * 落点生成一圈白色涟漪（扁平圆环，扩散并淡出）。
// 只在玩家附近（<45m）发生，避免远处无人看到的空演。
// 水面海拔 -0.55 与 terrain_builder.h water_level 联动。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class AudioStreamPlayer3D;
class Material;
class MeshInstance3D;

class LakeJumpFx : public Node3D {
	GDCLASS(LakeJumpFx, Node3D)

private:
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;

	Node3D *player = nullptr;
	Node3D *fish = nullptr;
	MeshInstance3D *ring = nullptr;
	AudioStreamPlayer3D *splash = nullptr;
	Ref<Material> ring_mat;

	float timer = 5.0f;
	bool leaping = false;
	float leap_t = 0.0f;
	float leap_dur = 0.8f;
	Vector3 leap_start;
	Vector3 leap_end;
	float leap_h = 0.6f;

	bool ring_active = false;
	float ring_t = 0.0f;
	float ring_dur = 0.55f;
	Vector3 ring_pos;

	void build_fish();
	void build_ring();
	void schedule_next();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
