#pragma once

#include "interactable.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Bed : public Interactable {
	GDCLASS(Bed, Interactable)

private:
	bool is_sleeping = false;

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual float get_interact_distance() const override { return 2.2f; }
	virtual void interact(const Vector3 &p_player_pos) override;

	bool get_is_sleeping() const { return is_sleeping; }
	void wake_up();
};

} // namespace godot
