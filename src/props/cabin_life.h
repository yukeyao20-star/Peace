#pragma once

// CabinLife —— 木屋的"有人住"细节：烟囱炊烟、入夜窗灯、门口小灯。
//
// 以 Homestead 的局部坐标摆放（木屋整体绕 Y 转了 45°）：
//   * 屋顶石烟囱常年冒一缕被风带偏的青烟；
//   * 入夜后四扇窗透出暖光、门口小灯亮起，火光轻微跳动；
//   * 雨雪天窗户蒙上一层湿雾（辉光略降）。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <vector>

namespace godot {

class CPUParticles3D;
class ImageTexture;
class MeshInstance3D;
class OmniLight3D;
class ShaderMaterial;
class SkyCycle;
class StandardMaterial3D;

class CabinLife : public Node3D {
	GDCLASS(CabinLife, Node3D)

private:
	SkyCycle *sky = nullptr;
	Ref<StandardMaterial3D> window_glow[4];
	OmniLight3D *window_light = nullptr;
	OmniLight3D *porch_light = nullptr;
	Ref<StandardMaterial3D> porch_glass;
	CPUParticles3D *smoke = nullptr;
	float t = 0.0f;
	float wet = 0.0f;

	// —— 冬日细节：屋顶积雪 / 屋檐垂冰 / 窗霜 / 门花环 ——
	MeshInstance3D *roof_snow[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	std::vector<MeshInstance3D *> icicles;
	std::vector<Vector3> icicle_scale; // 每根冰柱的基准缩放 (sx, 长度, sz)
	std::vector<Vector3> icicle_base;  // 每根冰柱的檐口挂点
	MeshInstance3D *frost[4] = { nullptr, nullptr, nullptr, nullptr };
	Ref<StandardMaterial3D> frost_mat;
	Ref<ShaderMaterial> wreath_mat;
	Node3D *wreath = nullptr;
	float cold = 0.0f;        // 平滑后的"冷度"（雪 + 夜）
	float snow_cover = 0.0f;  // 屋顶积雪量 0..1
	float frost_level = 0.0f; // 窗霜浓度 0..1
	float icicle_level = 0.0f; // 冰柱长度比 0..1
	float wreath_sway = 0.0f;
	int smoke_amount = 22;

	Ref<ImageTexture> make_frost_texture(unsigned p_seed);

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	float get_window_glow_energy(int p_index) const;
	float get_porch_energy() const;
	bool get_smoke_enabled() const;
	int get_smoke_amount() const { return smoke_amount; }
	float get_roof_snow_level() const { return snow_cover; }
	float get_frost_level() const { return frost_level; }
	float get_icicle_level() const { return icicle_level; }
	int get_icicle_count() const { return (int)icicles.size(); }
	float get_wreath_sway() const { return wreath_sway; }
};

} // namespace godot
