#include "bed.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../actors/player.h"

using namespace godot;

void Bed::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_is_sleeping"), &Bed::get_is_sleeping);
	ClassDB::bind_method(D_METHOD("wake_up"), &Bed::wake_up);
}

void Bed::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
		} break;
		default:
			break;
	}
}

String Bed::get_prompt() const {
	return is_sleeping ? String::utf8("按 E 起床") : String::utf8("按 E 上床睡觉 (度过时光)");
}

Vector3 Bed::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.45f, 0.0f);
}

void Bed::interact(const Vector3 &p_player_pos) {
	Node *root = get_tree()->get_current_scene();
	if (root == nullptr) {
		root = get_tree()->get_root();
	}
	if (root == nullptr) {
		return;
	}
	Player *pl = Object::cast_to<Player>(root->find_child("Player", true, false));
	if (pl == nullptr) {
		return;
	}

	if (!is_sleeping) {
		is_sleeping = true;
		pl->start_sleep(get_global_position(), get_global_rotation().y);
	} else {
		is_sleeping = false;
		pl->stop_sleep();
	}
}

void Bed::wake_up() {
	if (is_sleeping) {
		is_sleeping = false;
		Node *root = get_tree()->get_current_scene();
		if (root == nullptr) {
			root = get_tree()->get_root();
		}
		if (root != nullptr) {
			Player *pl = Object::cast_to<Player>(root->find_child("Player", true, false));
			if (pl != nullptr) {
				pl->stop_sleep();
			}
		}
	}
}
