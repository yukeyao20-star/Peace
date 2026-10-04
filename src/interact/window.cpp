#include "window.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"

using namespace godot;

void ShutterWindow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_open"), &ShutterWindow::is_open);
}

void ShutterWindow::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build(meshutil::load_toon_shader());
			add_to_group("interactables");
			set_process(true);
		} break;
		default:
			break;
	}
}

void ShutterWindow::build(Ref<Shader> p_shader) {
	const Color plank(0.48f, 0.32f, 0.18f); // 百叶板：中深色木板
	const Color trim(0.38f, 0.25f, 0.14f);  // 边框：深木

	// 窗框四条边（静态，贴在墙面上）
	float w2 = width * 0.5f + 0.06f;
	float h2 = height * 0.5f + 0.06f;
	meshutil::add_box(this, p_shader, Vector3(width + 0.18f, 0.09f, 0.08f), Vector3(0.0f, h2, 0.02f), trim);
	meshutil::add_box(this, p_shader, Vector3(width + 0.18f, 0.09f, 0.08f), Vector3(0.0f, -h2, 0.02f), trim);
	meshutil::add_box(this, p_shader, Vector3(0.09f, height + 0.18f, 0.08f), Vector3(-w2, 0.0f, 0.02f), trim);
	meshutil::add_box(this, p_shader, Vector3(0.09f, height + 0.18f, 0.08f), Vector3(w2, 0.0f, 0.02f), trim);
	// 窗台：底边外侧多出一小条，承接雨水也更有层次
	meshutil::add_box(this, p_shader, Vector3(width + 0.26f, 0.07f, 0.18f), Vector3(0.0f, -h2 - 0.07f, 0.05f), trim);

	// —— 百叶板：铰链在顶缘，向上撑开 ——
	pivot = memnew(Node3D);
	pivot->set_name("Pivot");
	pivot->set_position(Vector3(0.0f, height * 0.5f + 0.03f, 0.08f));
	add_child(pivot);

	Ref<BoxMesh> board;
	board.instantiate();
	board->set_size(Vector3(width + 0.06f, height + 0.06f, 0.05f));
	board->set_material(meshutil::toon_material(p_shader, plank));
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name("Board");
	mi->set_mesh(board);
	mi->set_position(Vector3(0.0f, -height * 0.5f - 0.03f, 0.0f)); // 板中心相对顶端铰链下移半高
	pivot->add_child(mi);

	// 板面上的两条横向加固条
	meshutil::add_box(pivot, p_shader, Vector3(width + 0.06f, 0.08f, 0.02f),
			Vector3(0.0f, -height * 0.22f, 0.035f), trim);
	meshutil::add_box(pivot, p_shader, Vector3(width + 0.06f, 0.08f, 0.02f),
			Vector3(0.0f, -height * 0.78f, 0.035f), trim);

	// 开关窗木门合页咯吱音效（优先加载用户提供的开窗真实音频）
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr) {
		if (rl->exists("res://assets/audio/window_open_creak.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/window_open_creak.wav");
			if (r.is_valid()) {
				snd_open = Object::cast_to<AudioStream>(r.ptr());
			}
		} else if (rl->exists("res://assets/audio/wood_creak_open.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/wood_creak_open.wav");
			if (r.is_valid()) {
				snd_open = Object::cast_to<AudioStream>(r.ptr());
			}
		}
		if (rl->exists("res://assets/audio/wood_creak_close.wav")) {
			Ref<Resource> r = rl->load("res://assets/audio/wood_creak_close.wav");
			if (r.is_valid()) {
				snd_close = Object::cast_to<AudioStream>(r.ptr());
			}
		}
	}
	audio_player = memnew(AudioStreamPlayer3D);
	audio_player->set_name("WindowAudio");
	audio_player->set_unit_size(5.0f);
	audio_player->set_max_distance(20.0f);
	add_child(audio_player);
}

void ShutterWindow::_process(double p_delta) {
	if (open_t == open_target || pivot == nullptr) {
		return;
	}
	float dir = open_target > open_t ? 1.0f : -1.0f;
	open_t = Math::clamp(open_t + dir * (float)p_delta * 2.6f, 0.0f, 1.0f);
	float e = open_t * open_t * (3.0f - 2.0f * open_t);
	// 铰链在顶缘、板朝 -Y 垂下：负角度绕 X 轴把板向上、向外 (+Z) 撑开成遮阳篷
	pivot->set_rotation(Vector3(-1.45f * e, 0.0f, 0.0f)); // ≈83° 向上撑开
}

String ShutterWindow::get_prompt() const {
	return is_open() ? String::utf8("按 E 关窗") : String::utf8("按 E 开窗");
}

Vector3 ShutterWindow::get_interact_pos() const {
	// 窗户中心位置（窗框与百叶窗板所在的世界坐标）
	return get_global_position();
}

void ShutterWindow::interact(const Vector3 &p_player_pos) {
	bool will_open = !is_open();
	open_target = will_open ? 1.0f : 0.0f;
	if (audio_player != nullptr) {
		// 用户明确要求："关窗声音改成和开窗户声音一致"
		Ref<AudioStream> s = snd_open.is_valid() ? snd_open : snd_close;
		if (s.is_valid()) {
			audio_player->set_stream(s);
			audio_player->play();
		}
	}
}
