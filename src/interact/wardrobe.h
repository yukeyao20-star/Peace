#pragma once

#include "interactable.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Wardrobe : public Interactable {
	GDCLASS(Wardrobe, Interactable)

private:
	int current_costume = 0;
	void build(Ref<Shader> p_shader);

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual float get_interact_distance() const override { return 2.0f; }
	virtual void interact(const Vector3 &p_player_pos) override;

	int get_costume() const { return current_costume; }
};

} // namespace godot
