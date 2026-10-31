#pragma once

// StewPot —— 火堆旁咕嘟作响的小汤锅。
//
// 铸铁锅架在几块石头上，锅里橙色浓汤慢慢打转，热气与气泡不断冒；
// 靠近按 E 用木勺搅一搅：蒸汽腾起、汤转得更快，配一声轻轻的搅汤声。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class CPUParticles3D;
class MeshInstance3D;
class TerrainBuilder;

class StewPot : public Interactable {
	GDCLASS(StewPot, Interactable)

private:
	TerrainBuilder *terrain = nullptr;
	MeshInstance3D *soup = nullptr;
	CPUParticles3D *steam = nullptr;
	CPUParticles3D *bubbles = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_stir;
	float t = 0.0f;
	float stir_t = 0.0f;

	void build();
	Ref<AudioStreamWAV> make_stir();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override;
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	float get_stir_time() const { return stir_t; }
	bool get_steam_emitting() const;
	bool get_stir_ready() const { return snd_stir.is_valid(); }
};

} // namespace godot
