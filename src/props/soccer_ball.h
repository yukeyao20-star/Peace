#pragma once

#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class SoccerBall : public RigidBody3D {
	GDCLASS(SoccerBall, RigidBody3D)

private:
	Vector3 pitch_center = Vector3(60.0f, 0.35f, -40.0f);
	float pitch_half_w = 24.0f;
	float pitch_half_d = 15.0f;

	float out_of_bounds_timer = 0.0f;
	float goal_cooldown = 0.0f;
	bool was_kicked_recently = false;
	float kick_cooldown = 0.0f;

	AudioStreamPlayer3D *audio_player = nullptr;
	Ref<AudioStream> snd_kick;
	Ref<AudioStream> snd_goal;
	Ref<AudioStream> snd_whistle;

	void build_model();
	void reset_to_center();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
	void _physics_process(double p_delta) override;

	void kick(const Vector3 &p_impulse);
};

} // namespace godot
