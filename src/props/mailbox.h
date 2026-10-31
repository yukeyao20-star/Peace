#pragma once

// Mailbox —— 木屋门前的信箱。每天一封短短的「湖畔来信」：
// 走近按 E，箱门放下、抽出一张信纸，读一段随天数与天气变化的小记。
// 读过的当天信箱空了（旗子落下），睡一觉后又有新的信。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CanvasLayer;
class Control;
class Label;
class MeshInstance3D;
class SkyCycle;

class Mailbox : public Interactable {
	GDCLASS(Mailbox, Interactable)

private:
	Node3D *door = nullptr;       // 箱门枢轴（底部铰链）
	Node3D *flag_pivot = nullptr; // 小红旗
	MeshInstance3D *envelope = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_paper;
	CanvasLayer *ui_layer = nullptr;
	Control *ui_root = nullptr;
	Label *ui_title = nullptr;
	Label *ui_body = nullptr;
	Label *ui_sign = nullptr;
	bool ui_open = false;
	bool read_today = false;
	int day_seen = -1;
	float door_t = 0.0f;
	float door_target = 0.0f;
	SkyCycle *sky = nullptr;
	Node3D *player = nullptr;

	void build();
	Ref<AudioStreamWAV> make_paper();
	void build_ui();
	void open_ui();
	void close_ui();
	String letter_for(int p_day, int p_weather) const;

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool get_ui_open() const { return ui_open; }
	bool get_read_today() const { return read_today; }
	int get_letter_day() const { return day_seen; }
};

} // namespace godot
