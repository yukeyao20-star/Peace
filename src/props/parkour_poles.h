#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class ParkourPoles : public Node3D {
	GDCLASS(ParkourPoles, Node3D)

private:
	void build_poles(Ref<Shader> p_shader);

protected:
	static void _bind_methods() {}

public:
	void _notification(int p_what);
};

} // namespace godot
