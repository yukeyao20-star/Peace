#pragma once

// FairyRing —— 林缘草地上的蘑菇圈。
//
// 八朵小蘑菇围成一圈，黄昏后伞盖下透出一点微光；清晨有孢子轻轻飘起。
// 按 E 可以采走一朵，过一阵子它又会长回来。
//
// 参考：
//   * glowlings（Godot）mushroom_glow —— 蘑菇的内外两层光晕与轻微闪烁；
//   * glowlings mushroom_glow.tscn —— CPUParticles2D 的漂浮光点做法；
//   * CGTrader「Stylized Mushroom Pack」—— Fairy Ring 群体的构图参考。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class CPUParticles3D;
class MeshInstance3D;
class MultiMesh;
class Node3D;
class SkyCycle;

class FairyRing : public Interactable {
	GDCLASS(FairyRing, Interactable)

	static const int COUNT = 8;

	Ref<MultiMesh> stem_mm;
	Ref<MultiMesh> cap_mm;
	Ref<MultiMesh> spot_mm;
	Ref<MultiMesh> glow_mm;
	Vector3 mush_pos[COUNT];
	float mush_sx[COUNT] = {};
	float mush_sz[COUNT] = {};
	CPUParticles3D *spores = nullptr;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float respawn_t = 0.0f;
	int picked_count = 0;
	int glow_visible = 0;
	bool spores_on = false;
	bool picked[COUNT] = {};

	void build();
	void refresh_glow();
	void update_mushroom(int p_index);
	void update_glow(int p_index, bool p_on);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.8f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_mushroom_count() const { return COUNT; }
	int get_picked_count() const { return picked_count; }
	int get_glow_visible() const { return glow_visible; }
	bool get_spores_on() const { return spores_on; }
};

} // namespace godot
