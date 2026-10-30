#include "soccer_ball.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/memory.hpp>

#include "../actors/player.h"

using namespace godot;

void SoccerBall::_bind_methods() {
	ClassDB::bind_method(D_METHOD("kick", "impulse"), &SoccerBall::kick);
	ClassDB::bind_method(D_METHOD("reset_to_center"), &SoccerBall::reset_to_center);

	ADD_SIGNAL(MethodInfo("goal_scored")); // 踢进任一球门时发出（GameDirector 每日待办计数用）
}

void SoccerBall::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build_model();
			set_physics_process(true);
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			_physics_process(get_physics_process_delta_time());
		} break;
		default:
			break;
	}
}

void SoccerBall::build_model() {
	set_mass(0.43f);
	set_use_continuous_collision_detection(true);

	Ref<PhysicsMaterial> pmat;
	pmat.instantiate();
	pmat->set_bounce(0.72f);
	pmat->set_friction(0.38f);
	set_physics_material_override(pmat);

	// 碰撞体（半径 0.24m 标准5号足球）
	Ref<SphereShape3D> shape;
	shape.instantiate();
	shape->set_radius(0.24f);

	CollisionShape3D *col = memnew(CollisionShape3D);
	col->set_name("Col");
	col->set_shape(shape);
	add_child(col);

	// 足球主球体（采用截角二十面体 32 面数学 Voronoi 经典皮球着色器）
	Ref<SphereMesh> sm;
	sm.instantiate();
	sm->set_radius(0.24f);
	sm->set_height(0.48f);
	sm->set_radial_segments(48);
	sm->set_rings(24);

	Ref<ShaderMaterial> ball_mat;
	ball_mat.instantiate();
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl != nullptr && rl->exists("res://assets/soccer_ball.gdshader")) {
		Ref<Shader> ball_shader = rl->load("res://assets/soccer_ball.gdshader");
		if (ball_shader.is_valid()) {
			ball_mat->set_shader(ball_shader);
		}
	}

	MeshInstance3D *ball_mesh = memnew(MeshInstance3D);
	ball_mesh->set_mesh(sm);
	if (ball_mat->get_shader().is_valid()) {
		ball_mesh->set_material_override(ball_mat);
	} else {
		Ref<StandardMaterial3D> mat_white;
		mat_white.instantiate();
		mat_white->set_albedo(Color(0.96f, 0.96f, 0.95f));
		ball_mesh->set_material_override(mat_white);
	}
	add_child(ball_mesh);

	// 草地滚动与旋转阻尼（防止踢飞没影，草地上滚动自然受阻降速停下）
	set_linear_damp(1.85f);
	set_angular_damp(2.3f);

	// 声音组件
	audio_player = memnew(AudioStreamPlayer3D);
	audio_player->set_unit_size(10.0f);
	audio_player->set_max_distance(45.0f);
	audio_player->set_bus(StringName("SFX"));
	add_child(audio_player);

	if (rl != nullptr) {
		if (rl->exists("res://assets/audio/soccer_kick.mp3")) {
			Ref<Resource> res = rl->load("res://assets/audio/soccer_kick.mp3");
			if (res.is_valid()) {
				snd_kick = Object::cast_to<AudioStream>(res.ptr());
			}
		} else if (rl->exists("res://assets/audio/zen_wood_drop.mp3")) {
			Ref<Resource> res = rl->load("res://assets/audio/zen_wood_drop.mp3");
			if (res.is_valid()) {
				snd_kick = Object::cast_to<AudioStream>(res.ptr());
			}
		}
		if (rl->exists("res://assets/audio/zen_bell.mp3")) {
			Ref<Resource> res = rl->load("res://assets/audio/zen_bell.mp3");
			if (res.is_valid()) {
				snd_goal = Object::cast_to<AudioStream>(res.ptr());
			}
		}
	}
}

void SoccerBall::_physics_process(double p_delta) {
	if (kick_cooldown > 0.0f) {
		kick_cooldown -= (float)p_delta;
	}
	if (goal_cooldown > 0.0f) {
		goal_cooldown -= (float)p_delta;
	}

	Vector3 pos = get_global_position();
	Vector3 vel = get_linear_velocity();

	// 1. 玩家触球与踢球判定
	Node *root = get_tree()->get_current_scene();
	if (root == nullptr) {
		root = get_tree()->get_root();
	}
	if (root != nullptr && kick_cooldown <= 0.0f) {
		Player *pl = Object::cast_to<Player>(root->find_child("Player", true, false));
		if (pl != nullptr) {
			Vector3 pl_pos = pl->get_global_position();
			Vector3 delta_pos = pos - pl_pos;
			float dist_2d = Vector2(delta_pos.x, delta_pos.z).length();

			if (dist_2d < 0.88f && Math::abs(pos.y - (pl_pos.y + 0.35f)) < 0.9f) {
				Vector3 pl_vel = pl->get_velocity();
				float pl_speed = pl_vel.length();

				Vector3 fwd = pl->get_facing();
				// 贴地踢球：水平分量为主，垂直仅保留 0.08f 轻微弹跳，避免直接踢飞上天无影
				Vector3 kick_dir = Vector3(delta_pos.x, 0.08f * dist_2d, delta_pos.z).normalized();
				if (pl_speed > 0.8f) {
					kick_dir = (kick_dir * 0.4f + fwd * 1.6f + Vector3(0.0f, 0.12f, 0.0f)).normalized();
				}

				// 速度与冲量：下调冲量（约 -28%），增加草地滚阻，避免一脚踢飞过远
				float impulse_str = 1.45f + pl_speed * 0.95f;
				apply_central_impulse(kick_dir * impulse_str);
				kick_cooldown = 0.22f;

				if (audio_player != nullptr && snd_kick.is_valid()) {
					audio_player->set_stream(snd_kick);
					audio_player->set_pitch_scale(1.15f + ((float)(rand() % 20) - 10.0f) * 0.01f);
					audio_player->set_volume_db(-4.0f);
					audio_player->play();
				}
			}
		}
	}

	// 2. 出界判定与静止超时自动复位（需求：可以踢出球场，但是离开球场很久且静止，会自动刷新在球场中间）
	float dx = Math::abs(pos.x - pitch_center.x);
	float dz = Math::abs(pos.z - pitch_center.z);
	bool out_of_pitch = (dx > (pitch_half_w + 1.2f) || dz > (pitch_half_d + 1.2f));

	if (out_of_pitch) {
		if (vel.length() < 0.25f || pos.y < -0.2f) { // 离开球场且静止（或滚入湖中/掉落）
			out_of_bounds_timer += (float)p_delta;
			if (out_of_bounds_timer >= 8.5f || pos.y < -0.2f) { // 静止超时 8.5 秒或掉入水中立即复位
				reset_to_center();
			}
		} else {
			out_of_bounds_timer = 0.0f; // 还在高速滚动则不急着重置
		}
	} else {
		out_of_bounds_timer = 0.0f;
	}

	// 3. 进球判定（西球门 x ≈ 36.0, 东球门 x ≈ 84.0, z ∈ [-42.8, -37.2], y ∈ [0.1, 2.5]）
	if (goal_cooldown <= 0.0f) {
		bool in_goal_z = Math::abs(pos.z - pitch_center.z) < 2.8f;
		bool in_goal_y = (pos.y >= 0.1f && pos.y <= 2.5f);
		if (in_goal_y && in_goal_z) {
			if (pos.x < 36.2f && pos.x > 33.8f) { // 进西球门！
				goal_cooldown = 4.0f;
				emit_signal("goal_scored"); // 通知每日待办计分（GameDirector）
				if (audio_player != nullptr && snd_goal.is_valid()) {
					audio_player->set_stream(snd_goal);
					audio_player->set_volume_db(2.0f);
					audio_player->play();
				}
			} else if (pos.x > 83.8f && pos.x < 86.2f) { // 进东球门！
				goal_cooldown = 4.0f;
				emit_signal("goal_scored"); // 通知每日待办计分（GameDirector）
				if (audio_player != nullptr && snd_goal.is_valid()) {
					audio_player->set_stream(snd_goal);
					audio_player->set_volume_db(2.0f);
					audio_player->play();
				}
			}
		}
	}
}

void SoccerBall::kick(const Vector3 &p_impulse) {
	apply_central_impulse(p_impulse);
	if (audio_player != nullptr && snd_kick.is_valid()) {
		audio_player->set_stream(snd_kick);
		audio_player->set_volume_db(-2.0f);
		audio_player->play();
	}
}

void SoccerBall::reset_to_center() {
	out_of_bounds_timer = 0.0f;
	set_linear_velocity(Vector3(0.0f, 0.0f, 0.0f));
	set_angular_velocity(Vector3(0.0f, 0.0f, 0.0f));

	Transform3D t = get_global_transform();
	t.set_origin(pitch_center + Vector3(0.0f, 0.5f, 0.0f));
	set_global_transform(t);

	if (audio_player != nullptr && snd_goal.is_valid()) {
		audio_player->set_stream(snd_goal);
		audio_player->set_pitch_scale(1.4f);
		audio_player->set_volume_db(-6.0f);
		audio_player->play();
	}
}
