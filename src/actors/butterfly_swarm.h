#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

// ButterflySwarm —— 蝴蝶群（MC 风格的平原小生物）。
//
// 场景里放一个空节点，运行时在周围草地生成一群蝴蝶（每只是
// "身体 + 左右翼"的小 Node3D 组合，MC 式扁平亮色块）：
//   * 游走：各自绕小锚点做圆弧漫游 + 上下轻浮，忽快忽慢像被风推着走；
//   * 扑翼：高速小幅扑动（蝴蝶扑频明显比鸟快），转向时机翼略倾；
//   * 昼夜：从兄弟 SkyCycle 读 time_of_day，黄昏像鸟一样升空离场、
//     黎明回归（找不到 SkyCycle 视为白天）；
//   * 天气：雨/雪因子和（wet）越大越"躲"——落到草丛里收翼静栖、
//     漫游与扑翼近乎停止（MC 蝴蝶雨天消失的等价演绎）。
// 固定种子（按世界坐标哈希），每次启动地图可复现。
class ButterflySwarm : public Node3D {
	GDCLASS(ButterflySwarm, Node3D)

private:
	// 单只蝴蝶的运行时状态（节点指针挂在场景树里，这里只存参数）
	struct Flyer {
		Node3D *root = nullptr;
		Node3D *wing_l = nullptr;
		Node3D *wing_r = nullptr;
		Node3D *hind_l = nullptr; // 后翅（2026-10-31：四翅蝴蝶，前翅 + 后翅各一对）
		Node3D *hind_r = nullptr;
		Vector3 anchor;   // 游走锚点（相对群节点，含离地高度）
		float orbit_r = 0.0f;  // 绕锚点的漫游半径
		float orbit_w = 0.0f;  // 漫游角速度（含方向）
		float phase = 0.0f;    // 漫游初相位
		float flap = 0.0f;     // 扑翼频率
		float bob_a = 0.0f;    // 上下轻浮幅度
		float bob_w = 0.0f;    // 轻浮频率
	};

	int butterfly_count = 9;   // 蝴蝶数量
	float swarm_radius = 5.5f; // 锚点分布半径（米）

	std::vector<Flyer> flyers;
	Node *sky_cycle = nullptr;
	float anim_t = 0.0f;
	bool first_frame = true;
	std::vector<Vector3> last_pos;

	void build();
	void spawn_flyer(const Vector3 &p_anchor, int p_variant, unsigned p_seed);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
