#pragma once

// MusicBox —— 门廊上的小八音盒。
//
// 走近按 E 上弦：黄铜滚筒与摇柄转起来，飘出十几秒程序化的五声音阶小曲，
// 同时升起几点金色音符。再按 E 停下。
//
// 旋律生成参考：
//   * chicomcastro/procedural-music-generator —— 噪声/随机游走映射到五声音阶；
//   * aleatoric —— 最大跳进约束 + 拱形轮廓（前半上行、后半回落）；
//   * monome/crow First.lua —— 五声音阶 {0,2,4,7,9} + LCG 随机旋律。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;

class MusicBox : public Interactable {
	GDCLASS(MusicBox, Interactable)

private:
	Node3D *crank = nullptr;
	CPUParticles3D *notes_fx = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> tune;
	bool playing = false;
	float t = 0.0f;
	int note_count = 0;

	void build();
	Ref<AudioStreamWAV> compose_tune();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.2f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool get_playing() const { return playing; }
	bool get_tune_ready() const { return tune.is_valid(); }
	int get_note_count() const { return note_count; }
	float get_crank_angle() const;
};

} // namespace godot
