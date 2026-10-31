#pragma once

// RainGear —— 屋侧的雨水收集角。
//
// 一根落水管把屋檐水引进木雨桶，桶边靠着一把洒水壶和一双胶靴；雨大时
// 落水口会溅起细小水花，桶里的水位也跟着涨落。
//
// 参考：
//   * pixor.art「雨水桶」CC0（352 三角面）—— 桶 + 檐槽 + 弯管落水管；
//   * yoiyami「Rain Barrel」CC0 —— 短雨樋下的木桶；
//   * polyfork.dev「Watering Can」—— 洒水壶比例（0.18×0.35×0.46 m）。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CPUParticles3D;
class MeshInstance3D;
class SkyCycle;

class RainGear : public Node3D {
	GDCLASS(RainGear, Node3D)

	static const int ITEM_TYPES = 5;

	MeshInstance3D *water = nullptr;
	CPUParticles3D *splash = nullptr;
	SkyCycle *sky = nullptr;

	float water_level = 0.4f;
	bool splashing = false;
	float t = 0.0f;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_item_types() const { return ITEM_TYPES; }
	float get_water_level() const { return water_level; }
	bool get_splashing() const { return splashing; }
};

} // namespace godot
