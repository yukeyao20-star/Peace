#pragma once

// CricketChorus —— 夏夜草丛里的三只蟋蟀。
//
// 天黑后轮流发出短促的啾啾声；走近时它们会安静下来，雪天不出声。

#include <godot_cpp/classes/node3d.hpp>
namespace godot {
class AudioStreamPlayer3D;
class AudioStreamWAV;
class Node3D;
class SkyCycle;
class CricketChorus : public Node3D {
	GDCLASS(CricketChorus, Node3D)
	static const int COUNT = 3;
	AudioStreamPlayer3D *voices[COUNT] = {};
	Ref<AudioStreamWAV> snd;
	float timers[COUNT] = { 1.0f, 2.5f, 4.0f };
	Node3D *player = nullptr;
	SkyCycle *sky = nullptr;
	bool night = false;
	bool hushed = false;
	int chirps = 0;
	void build();
	void build_sound();
protected:
	static void _bind_methods();
public:
	void _notification(int p_what);
	bool get_night() const { return night; }
	bool get_hushed() const { return hushed; }
	int get_chirps() const { return chirps; }
};
} // namespace godot
