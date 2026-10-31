#pragma once

// DewWebs —— 篱笆边的三张蜘蛛网。
//
// 清晨网线上挂着一串串露珠，太阳升高后慢慢消失；小蜘蛛趴在网心，人一靠近
// 就缩到网角，走远再回来。风会让网轻轻晃，暴雨会把网打烂、雨停后再织好。
//
// 参考：
//   * Pixor「墙角蛛网」素材 —— 蛛网径向 + 同心圈的构成；
//   * bzdghf/godot-spider —— 程序化蜘蛛的关节/姿态参考；
//   * Godot 官方 Environment glow 文档 —— 清晨露珠用自发光小点表现。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class SkyCycle;

class DewWebs : public Node3D {
	GDCLASS(DewWebs, Node3D)

	static const int WEB_COUNT = 3;
	static const int DEW_PER_WEB = 7;

	Node3D *webs[WEB_COUNT] = {};
	Node3D *spider = nullptr;
	MeshInstance3D *dewdrops[WEB_COUNT][DEW_PER_WEB] = {};
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;

	float t = 0.0f;
	float dew = 0.0f;
	float spider_t = 0.0f;
	bool webs_visible = true;
	int dew_visible = 0;
	bool spider_out = false;

	void build();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	int get_web_count() const { return WEB_COUNT; }
	int get_dew_visible() const { return dew_visible; }
	bool get_spider_out() const { return spider_out; }
	bool get_webs_visible() const { return webs_visible; }
};

} // namespace godot
