#pragma once

// ShutterWindow —— 可开合的木质百叶窗。
//
// 局部坐标约定：原点在窗户中心，+Z 朝屋外（玻璃所在墙面外法线方向），
// 窗户宽度沿 X。百叶板铰链在左缘（-X 侧），打开时绕 Y 轴向外旋开 ~115°。
//
// 玻璃与窗框由场景里的 CSG 窗负责；本节点只负责"木板百叶 + 合页"，
// 关上时百叶板盖住玻璃（MC 木窗观感），打开后露出玻璃。

#include "interactable.h"

#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>

namespace godot {

class ShutterWindow : public Interactable {
	GDCLASS(ShutterWindow, Interactable)

private:
	Node3D *pivot = nullptr;  // 合页枢轴
	float open_t = 0.0f;      // 开合进度 0(关)..1(开)
	float open_target = 0.0f; // 目标进度
	float width = 1.14f;      // 窗洞宽度（决定百叶板尺寸）
	float height = 0.94f;     // 窗洞高度

	AudioStreamPlayer3D *audio_player = nullptr;
	Ref<AudioStream> snd_open;
	Ref<AudioStream> snd_close;

	void build(Ref<Shader> p_shader);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
	void _process(double p_delta) override;

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual float get_interact_distance() const override { return 2.4f; }
	virtual bool requires_line_of_sight() const override { return false; }
	virtual void interact(const Vector3 &p_player_pos) override;

	bool is_open() const { return open_target > 0.5f; }
};

} // namespace godot
