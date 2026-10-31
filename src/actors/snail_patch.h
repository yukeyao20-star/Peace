#pragma once

// SnailPatch —— 雨后从草丛里爬出来的几只小蜗牛。
//
// 只在雨里露面：背着小螺壳慢慢挪，身后留一条亮亮的黏液痕，过一会儿淡掉；
// 人一凑近就把身子缩回壳里，走远了再探出来。雨停后陆续缩回去。
//
// 参考：
//   * Godot 官方 Decal 文档 —— 足迹/水渍类贴花可以随时间淡出再移除；
//   * godotshaders「Decal shader (+Y_Fading)」—— 用 alpha 渐变做地面痕迹；
//   * Wandering Door Games / So Little Slime —— 用 Line2D 做尾端更淡更细的
//     黏液痕，整条在设定时长内淡出（本作用一条 MultiMesh 薄片带近似）；
//   * pollen-not-included snail.gd —— 蜗牛移动/进食/睡眠的状态划分参考。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class MultiMeshInstance3D;
class MultiMesh;
class TerrainBuilder;

class SnailPatch : public Node3D {
	GDCLASS(SnailPatch, Node3D)

	static const int SNAIL_COUNT = 4;
	static const int TRAIL_SLOTS = 40;

	struct Snail {
		Node3D *root = nullptr;
		Node3D *body = nullptr;
		Vector3 home;
		Vector3 pos;
		float heading = 0.0f;
		float emerge = 0.0f;
		float retract = 0.0f;
		float trail_acc = 0.0f;
		float phase = 0.0f;
	};

	Snail snails[SNAIL_COUNT];
	MultiMeshInstance3D *trail_mm = nullptr;
	Ref<MultiMesh> trail_mesh;
	Vector3 trail_pos[TRAIL_SLOTS];
	float trail_yaw[TRAIL_SLOTS] = {};
	float trail_age[TRAIL_SLOTS] = {};
	float trail_life[TRAIL_SLOTS] = {};
	int trail_next = 0;
	TerrainBuilder *terrain = nullptr;
	Node3D *player = nullptr;

	float t = 0.0f;
	bool active = false;
	int emerge_count = 0;
	int retract_count = 0;
	int trail_active = 0;

	void build();
	void add_trail_segment(const Vector3 &p_pos, float p_yaw);
	float ground_height(float p_x, float p_z);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_snail_count() const { return SNAIL_COUNT; }
	int get_emerge_count() const { return emerge_count; }
	int get_retract_count() const { return retract_count; }
	int get_trail_active() const { return trail_active; }
	bool get_active() const { return active; }
};

} // namespace godot
