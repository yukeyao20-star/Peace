#pragma once

// Clothesline —— 木屋旁的晾衣绳。
//
// 两根木桩之间一根下垂的麻绳，挂着四件洗好的衣物；风一吹轻轻摆，
// 下雨时布料变深、衣角滴水；下雪时绳上积一层薄雪、衣物摆得更拘谨。
// 走到跟前按 E 可以把衣服收起来，再按 E 晾回去。
//
// 参考：
//   * Donitzo/godot-catenary —— 悬链线做下垂的绳/链（本作用二次曲线近似）；
//   * godotshaders「Waving flag」/「Wind sway for foliage and flags」——
//     顶部固定、底边摆幅最大的悬挂布摆动思路；
//   * Yves Scherdin「Shader for Waving Flag」—— 顶部留一条不打扰动的固定带；
//   * GPU Cloth Simulation（Godot Asset Library）—— 顶部整排钉住、风与湍流
//     的布料模拟；本作衣物是低模，用挂点旋转近似，不跑布料物理。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class CPUParticles3D;
class MultiMesh;
class MultiMeshInstance3D;
class Node3D;
class ShaderMaterial;
class SkyCycle;

class Clothesline : public Interactable {
	GDCLASS(Clothesline, Interactable)

	static const int GARMENT_COUNT = 4;
	static const int ROPE_SEGMENTS = 10;

	Node3D *hangers[GARMENT_COUNT] = {};
	Ref<ShaderMaterial> cloth_mats[GARMENT_COUNT];
	Color cloth_dry[GARMENT_COUNT];
	float phases[GARMENT_COUNT] = {};
	float line_x[GARMENT_COUNT] = {};

	MultiMeshInstance3D *rope_mm = nullptr;
	MultiMeshInstance3D *snow_mm = nullptr;
	Ref<MultiMesh> rope_multimesh;
	Ref<MultiMesh> snow_multimesh;
	Ref<ShaderMaterial> rope_mat;
	Ref<ShaderMaterial> snow_mat;

	CPUParticles3D *drips = nullptr;

	SkyCycle *sky = nullptr;
	bool hung = true;
	float t = 0.0f;
	float wet = 0.0f;
	float snow_lvl = 0.0f;
	float last_wet_draw = -1.0f;

	void build();
	float rope_y(float p_x) const;

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool get_clothes_hung() const { return hung; }
	float get_wet_level() const { return wet; }
	float get_snow_level() const { return snow_lvl; }
	bool get_drips_emitting() const;
};

} // namespace godot
