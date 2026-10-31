#pragma once

// Shells —— 湖岸沙滩上的十枚小贝壳（可拾取、可存档）。
//
// 每枚贝壳是一个 Interactable：靠近按 E 拾起，贝壳消失并响一声清脆的风铃。
// 顶部左侧有一块小木牌记录 0/10；SaveSystem 用 10 位掩码保存已拾取状态。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <vector>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CanvasLayer;
class Label;
class MeshInstance3D;
class TerrainBuilder;

class ShellField;

class BeachShell : public Interactable {
	GDCLASS(BeachShell, Interactable)

private:
	ShellField *field = nullptr;
	int index = 0;
	bool taken = false;

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void setup(ShellField *p_field, int p_index) {
		field = p_field;
		index = p_index;
	}
	void mark_taken(bool p_taken);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
};

class ShellField : public Node3D {
	GDCLASS(ShellField, Node3D)

private:
	std::vector<BeachShell *> shells;
	std::vector<bool> collected;
	TerrainBuilder *terrain = nullptr;
	AudioStreamPlayer3D *chime = nullptr;
	Ref<AudioStreamWAV> snd_chime;
	CanvasLayer *hud = nullptr;
	Label *counter = nullptr;
	bool restored_mask = false;

	void build();
	void update_counter();
	Ref<AudioStreamWAV> make_chime();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void collect(int p_index, const Vector3 &p_player_pos);
	String get_collected_mask() const;
	void apply_collected_mask(const String &p_mask);
	int get_collected_count() const;
	int get_total() const { return (int)collected.size(); }
};

} // namespace godot
