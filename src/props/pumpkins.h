#pragma once

// Pumpkins —— 田边的南瓜堆。
//
// 四只大小不一的南瓜趴在藤叶间，最大的一只刻了脸；傍晚后南瓜灯里透出
// 暖光。清晨瓜皮上挂着露水，雪天顶着一层薄雪。
//
// 参考：
//   * pixor.art「南瓜地」低模（780 三角面）—— 南瓜地与藤叶的整体构成；
//   * Flockbay / Kenney「Crop Pumpkin」CC0 低模 —— 南瓜比例与瓣状轮廓；
//   * Synty POLYGON Farm Pack —— 南瓜/藤叶/茎的配色参考。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class SkyCycle;

class Pumpkins : public Node3D {
	GDCLASS(Pumpkins, Node3D)

	static const int COUNT = 4;
	static const int DEW_COUNT = 5;

	Node3D *pumpkins[COUNT] = {};
	MeshInstance3D *snow_caps[COUNT] = {};
	MeshInstance3D *glow = nullptr;
	MeshInstance3D *face_glow[3] = {};
	MeshInstance3D *dewdrops[DEW_COUNT] = {};
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float dew = 0.0f;
	int dew_visible = 0;
	bool glow_on = false;
	bool snow_covered = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_pumpkin_count() const { return COUNT; }
	int get_dew_visible() const { return dew_visible; }
	bool get_glow_on() const { return glow_on; }
	bool get_snow_covered() const { return snow_covered; }
};

} // namespace godot
