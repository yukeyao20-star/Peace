#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class SoccerField : public Node3D {
	GDCLASS(SoccerField, Node3D)

private:
	Vector3 pitch_center = Vector3(60.0f, 0.10f, -40.0f);
	float pitch_half_w = 24.0f;
	float pitch_half_d = 15.0f;

	void build_goal(Node3D *p_parent, const Vector3 &p_pos, float p_facing_x, Ref<Shader> p_shader);
	void build_pitch_markings(Ref<Shader> p_shader);
	void build_corner_flags(Ref<Shader> p_shader);
	void spawn_ball();

protected:
	static void _bind_methods() {}

public:
	void _notification(int p_what);
};

} // namespace godot
