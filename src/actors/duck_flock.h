#pragma once

// DuckFlock —— 湖面鸭群（程序化低模水鸟，氛围生物）。
//
// 场景里放一个空节点，运行时在湖心周围生成几只鸭子：
//   * 游泳：各自绕湖心做不同半径/角速度的圆弧巡游，朝向沿切线；
//   * 起伏：轻微上下浮动 + 缓慢转向，像真的在水上漂；
//   * 外观：程序化拼装（椭球身体 + 颈 + 头 + 圆锥喙 + 尾巴），
//     公鸭绿头、母鸭褐头，颜色取自低饱和配色，与低模风格一致。
// 固定种子：按世界坐标哈希，每次启动分布可复现。
// 注意：水面海拔 -0.55 与 terrain_builder.h water_level 联动，改水位须同步这里。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

class DuckFlock : public Node3D {
	GDCLASS(DuckFlock, Node3D)

private:
	struct Duck {
		Node3D *root = nullptr;
		Node3D *wake = nullptr; // 尾迹水圈（扁平圆环）
		float angle = 0.0f;    // 当前巡游角
		float radius = 8.0f;   // 巡游半径
		float speed = 0.04f;   // 角速度 rad/s
		float bob_w = 1.1f;    // 浮动频率
		float bob_phase = 0.0f;
		float yaw_offset = 0.0f; // 朝向微偏（不完全沿切线）
	};

	int duck_count = 4;
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;

	std::vector<Duck> ducks;
	float anim_t = 0.0f;
	bool first_frame = true;
	std::vector<Vector3> last_pos;

	void build();
	void spawn_duck(int p_index, unsigned p_seed);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
