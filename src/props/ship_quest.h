#pragma once

// ShipQuest —— 主线「离岸」：在旧船坞里一点点找回零件，把一艘大船重新拼起来。
// 零件散落在木屋、谷仓、沙滩、码头和花圃附近；拾到后回船坞按 E 安装。
// 全部装好后登船出发，进入离岸结局。

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../interact/interactable.h"

namespace godot {

class CanvasLayer;
class Camera3D;
class AudioStreamPlayer3D;
class CPUParticles3D;
class Curve3D;
class ShaderMaterial;
class SubViewport;
class ColorRect;
class Label;
class Label3D;
class MeshInstance3D;
class Node3D;
class StandardMaterial3D;
class TerrainBuilder;
class ShipQuest;

class ShipPart : public Interactable {
	GDCLASS(ShipPart, Interactable)
protected:
	static void _bind_methods();
	ShipQuest *owner = nullptr;
	int index = 0;
	String part_name;
	int kind = 0;
	Node3D *marker = nullptr;
	Vector3 base_pos;
	float t = 0.0f;
	void build_visual();
public:
	void init(ShipQuest *p_owner, int p_index, const String &p_name, int p_kind);
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;
};

class ShipQuest : public Interactable {
	GDCLASS(ShipQuest, Interactable)
public:
	static const int PART_COUNT = 32;
private:
	ShipPart *parts[PART_COUNT] = {};
	bool collected[PART_COUNT] = {};
	bool installed_flag[PART_COUNT] = {};
	Node3D *scaffold = nullptr;
	Node3D *ship_root = nullptr;
	Node3D *stage_nodes[7] = {};
	CanvasLayer *ui_layer = nullptr;
	Label *tracker = nullptr;
	CanvasLayer *cine_layer = nullptr;
	CanvasLayer *title_layer = nullptr;
	ColorRect *fade = nullptr;
	Label *end_title = nullptr;
	Label *end_sub = nullptr;
	Camera3D *cine_cam = nullptr;
	TerrainBuilder *terrain = nullptr;
	Node3D *player_node = nullptr;
	Ref<Curve3D> voyage_curve;
	Ref<ShaderMaterial> sail_mat;
	SubViewport *sail_vp = nullptr;
	CPUParticles3D *wake = nullptr;
	AudioStreamPlayer3D *sfx = nullptr;
	Ref<AudioStream> snd_pick;
	Ref<AudioStream> snd_install;
	Ref<Texture2D> sail_tex; // 必须由本节点持有：Variant 参数不会替 ViewportTexture 续命
	float ship_yaw = 0.0f;
	bool yaw_init = false;
	bool started = false;
	bool complete = false;
	bool departing = false;
	bool intro_hint_shown = false;
	int carried = 0;
	int installed = 0;
	int stage = 0;
	float t = 0.0f;
	float dep_t = 0.0f;
	float dep_s = 0.0f;
	void build();
	void build_ship();
	void build_ui();
	void build_cinematic();
	void make_part(int p_index);
	void update_ui();
	void update_stage(bool p_announce = true);
	void flash(const String &p_msg) const;
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 3.8f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	void collect_part(int p_index);
	bool is_started() const { return started; }
	bool is_complete() const { return complete; }
	bool is_departing() const { return departing; }
	int get_part_count() const { return PART_COUNT; }
	int get_carried() const { return carried; }
	int get_installed() const { return installed; }
	int get_stage() const { return stage; }
	bool is_part_taken(int p_index) const { return p_index >= 0 && p_index < PART_COUNT && collected[p_index]; }
	int get_parts_visible() const;

	// 存档：started|32位拾取掩码|已安装数（安装只记数量，安装顺序不影响船体）
	String get_save_serial() const;
	void apply_save_serial(const String &p_serial);
};
} // namespace godot
