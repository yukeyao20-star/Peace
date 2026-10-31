#pragma once

// Campfire —— 码头旁的火堆（昼夜都燃着，夜里是暖光与噼啪声）。
//
// 石圈 + 交叉柴火 + 火焰/火星粒子 + 暖橙色 OmniLight（轻微闪烁）
// + 程序化柴火噼啪循环声。节点放在岸上，READY 时贴合地形高度。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class OmniLight3D;
class TerrainBuilder;

class Campfire : public Node3D {
	GDCLASS(Campfire, Node3D)

private:
	OmniLight3D *light = nullptr;
	CPUParticles3D *flame = nullptr;
	CPUParticles3D *embers = nullptr;
	AudioStreamPlayer3D *crackle = nullptr;
	Ref<AudioStreamWAV> snd_crackle;
	float t = 0.0f;

	void build();
	Ref<AudioStreamWAV> make_crackle();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
