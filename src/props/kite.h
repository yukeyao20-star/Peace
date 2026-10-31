#pragma once

// Kite —— 草甸上的纸风筝。
//
// 靠近按 E 放线：风筝升到半空，随风轻轻画八字，线从手里连到风筝；
// 再按 E 收线，风筝落回草地。飞着时有一点风声。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Node3D;
class TerrainBuilder;

class Kite : public Interactable {
	GDCLASS(Kite, Interactable)

private:
	Node3D *kite = nullptr;
	MeshInstance3D *string = nullptr;
	Node3D *tail = nullptr;
	AudioStreamPlayer3D *flut = nullptr;
	Ref<AudioStreamWAV> snd_flutter;
	TerrainBuilder *terrain = nullptr;
	Node3D *player = nullptr;
	Vector3 home;
	float t = 0.0f;
	bool flying = false;

	void build();
	void make_flutter();
	Ref<AudioStreamWAV> synth_flutter();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	bool get_flying() const { return flying; }
	Vector3 get_kite_pos() const;
	bool get_flutter_ready() const { return snd_flutter.is_valid(); }
};

} // namespace godot
