#pragma once

// WeatherStation —— 草地上的小气象站。
//
// 风向标、三杯风速计、雨量筒、温度计和风向袋；风越大风速计转得越快，
// 雨多时雨量筒水位上升，下雪时温度计的红柱缩下去。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
namespace godot {
class MeshInstance3D;
class Node3D;
class SkyCycle;
class WeatherStation : public Node3D {
	GDCLASS(WeatherStation, Node3D)
	static const int ITEM_TYPES = 5;
	Node3D *cups = nullptr;
	Node3D *sock = nullptr;
	Node3D *vane = nullptr;
	MeshInstance3D *rain_water = nullptr;
	MeshInstance3D *mercury = nullptr;
	SkyCycle *sky = nullptr;
	float t = 0.0f;
	float wind = 0.0f;
	float rain_level = 0.0f;
	float temp_level = 0.7f;
	void build();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	int get_item_types() const { return ITEM_TYPES; }
	float get_wind() const { return wind; }
	float get_rain_level() const { return rain_level; }
	float get_temp_level() const { return temp_level; }
};
} // namespace godot
