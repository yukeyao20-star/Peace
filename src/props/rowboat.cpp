#include "rowboat.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../actors/player.h"
#include "../systems/mesh_util.h"
#include "../world/terrain_builder.h"

using namespace godot;

void Rowboat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt"), &Rowboat::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &Rowboat::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &Rowboat::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &Rowboat::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &Rowboat::interact);
	ClassDB::bind_method(D_METHOD("is_occupied"), &Rowboat::is_occupied);
}

void Rowboat::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			add_to_group("interactables");
			set_global_position(home);
			build_dock();
			build_hull();
			if (get_parent() != nullptr) {
				terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("Terrain"));
				if (terrain == nullptr) {
					terrain = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null("TerrainBuilder"));
				}
			}
			set_physics_process(true);
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			float delta = (float)get_physics_process_delta_time();
			bob_t += delta;
			Input *in = Input::get_singleton();
			InputMap *im = InputMap::get_singleton();
			auto act = [&](const char *p_action, Key p_key) {
				if (im != nullptr && im->has_action(StringName(p_action))) {
					return in->is_action_pressed(StringName(p_action));
				}
				return in->is_key_pressed(p_key);
			};

			if (occupied && rider != nullptr) {
				float f = 0.0f;
				float t = 0.0f;
				if (act("move_forward", KEY_W)) {
					f += 1.0f;
				}
				if (act("move_back", KEY_S)) {
					f -= 1.0f;
				}
				if (act("move_left", KEY_A)) {
					t += 1.0f;
				}
				if (act("move_right", KEY_D)) {
					t -= 1.0f;
				}
				speed = Math::lerp(speed, f * 2.3f, Math::min(1.0f, delta * 2.2f));
				yaw += t * delta * 0.85f * (0.45f + Math::min(1.0f, std::fabs(speed) / 2.3f));
				Vector3 fwd(std::sin(yaw), 0.0f, std::cos(yaw));
				Vector3 next = get_global_position() + fwd * speed * delta;
				if (terrain != nullptr && terrain->water_depth_at(next.x, next.z) > 0.65f) {
					set_global_position(Vector3(next.x, get_global_position().y, next.z));
				} else {
					speed = 0.0f;
				}
				// 下船：E 边沿（上船时把 e_prev 置真，避免同一次按键立刻下船）
				bool e = act("interact", KEY_E);
				if (e && !e_prev) {
					exit_boat();
				}
				e_prev = e;
			} else {
				speed = Math::lerp(speed, 0.0f, Math::min(1.0f, delta * 2.0f));
				e_prev = in->is_key_pressed(KEY_E);
			}

			// 贴水浮动 + 轻微摇晃
			float bob = std::sin(bob_t * 1.35f) * 0.03f;
			Vector3 p = get_global_position();
			p.y = water_level + 0.035f + bob;
			set_global_position(p);
			set_rotation(Vector3(0.0f, yaw, 0.0f));
			if (hull != nullptr) {
				hull->set_rotation(Vector3(std::sin(bob_t * 1.1f) * 0.012f, 0.0f,
						std::sin(bob_t * 0.85f) * 0.022f));
			}
			if (wake != nullptr) {
				wake->set_emitting(occupied && std::fabs(speed) > 0.2f);
			}
			// 划桨动画：有速度时前后扫、桨叶入水；停船时收回船舷
			float moving = Math::clamp(std::fabs(speed) / 1.2f, 0.0f, 1.0f);
			float phase = bob_t * 2.2f;
			float stroke = std::sin(phase) * 0.42f * moving;
			float dip = (0.10f + 0.18f * std::cos(phase)) * moving;
			if (oar_r != nullptr) {
				oar_r->set_rotation(Vector3(0.0f, stroke, -dip));
			}
			if (oar_l != nullptr) {
				oar_l->set_rotation(Vector3(0.0f, -stroke, dip));
			}
		} break;
		default:
			break;
	}
}

String Rowboat::get_prompt() const {
	return occupied ? String() : String::utf8("按 E 上船");
}

Vector3 Rowboat::get_interact_pos() const {
	return get_global_position() + Vector3(0.0f, 0.35f, 0.0f);
}

float Rowboat::get_interact_distance() const {
	return 3.6f;
}

void Rowboat::interact(const Vector3 &p_player_pos) {
	if (occupied) {
		return;
	}
	Node *parent = get_parent();
	if (parent == nullptr) {
		return;
	}
	Player *p = Object::cast_to<Player>(parent->get_node_or_null("Player"));
	if (p == nullptr) {
		return;
	}
	board(p);
}

void Rowboat::board(Player *p_rider) {
	if (p_rider == nullptr || occupied) {
		return;
	}
	rider = p_rider;
	occupied = true;
	e_prev = true;
	p_rider->board_boat(this);
	p_rider->set_global_position(get_global_position() + Vector3(0.0f, 0.45f, 0.0f));
}

void Rowboat::exit_boat() {
	if (rider == nullptr) {
		return;
	}
	// 从船侧下水：右舷方向偏移一点，落到水面以下 → 自动进入游泳
	Vector3 side(std::cos(yaw), 0.0f, -std::sin(yaw));
	Vector3 p = get_global_position() + side * 1.25f;
	p.y = water_level - 0.35f;
	rider->set_global_position(p);
	rider->set_velocity(Vector3());
	rider->leave_boat();
	rider = nullptr;
	occupied = false;
	speed = 0.0f;
}

void Rowboat::build_hull() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.47f, 0.32f, 0.18f);
	const Color wood_dark(0.30f, 0.19f, 0.10f);
	const Color trim(0.62f, 0.46f, 0.27f);

	hull = memnew(Node3D);
	hull->set_name("Hull");
	add_child(hull);

	// 船底 + 两侧船舷 + 船尾
	meshutil::add_box(hull, toon, Vector3(0.92f, 0.08f, 2.15f), Vector3(0.0f, 0.02f, 0.0f), wood_dark);
	meshutil::add_box(hull, toon, Vector3(0.08f, 0.30f, 2.15f), Vector3(0.46f, 0.18f, 0.0f), wood);
	meshutil::add_box(hull, toon, Vector3(0.08f, 0.30f, 2.15f), Vector3(-0.46f, 0.18f, 0.0f), wood);
	meshutil::add_box(hull, toon, Vector3(0.92f, 0.30f, 0.08f), Vector3(0.0f, 0.18f, -1.04f), wood);

	// 船头：两块内收的斜板
	meshutil::add_box(hull, toon, Vector3(0.08f, 0.30f, 0.72f), Vector3(0.29f, 0.18f, 1.10f), wood,
			Vector3(0.0f, 0.55f, 0.0f));
	meshutil::add_box(hull, toon, Vector3(0.08f, 0.30f, 0.72f), Vector3(-0.29f, 0.18f, 1.10f), wood,
			Vector3(0.0f, -0.55f, 0.0f));
	meshutil::add_box(hull, toon, Vector3(0.20f, 0.10f, 0.20f), Vector3(0.0f, 0.30f, 1.42f), trim);
	meshutil::add_box(hull, toon, Vector3(0.64f, 0.08f, 0.62f), Vector3(0.0f, 0.02f, 1.30f), wood_dark);

	// 两道横座板
	meshutil::add_box(hull, toon, Vector3(0.84f, 0.06f, 0.26f), Vector3(0.0f, 0.30f, 0.42f), trim);
	meshutil::add_box(hull, toon, Vector3(0.84f, 0.06f, 0.26f), Vector3(0.0f, 0.30f, -0.36f), trim);

	// 两支可动的桨：枢轴在船舷内侧，桨身沿 ±X 伸出
	for (int i = 0; i < 2; i++) {
		float side = (i == 0) ? 1.0f : -1.0f;
		Node3D *pivot = memnew(Node3D);
		pivot->set_name(i == 0 ? "OarR" : "OarL");
		pivot->set_position(Vector3(side * 0.40f, 0.34f, 0.05f));
		hull->add_child(pivot);
		Ref<CylinderMesh> oar;
		oar.instantiate();
		oar->set_top_radius(0.022f);
		oar->set_bottom_radius(0.028f);
		oar->set_height(1.65f);
		oar->set_radial_segments(6);
		oar->set_material(meshutil::toon_material(toon, trim));
		MeshInstance3D *omi = memnew(MeshInstance3D);
		omi->set_mesh(oar);
		omi->set_position(Vector3(side * 0.80f, 0.0f, 0.0f));
		omi->set_rotation(Vector3(0.0f, 0.0f, 1.5707964f));
		pivot->add_child(omi);
		// 桨叶
		meshutil::add_box(pivot, toon, Vector3(0.36f, 0.03f, 0.16f),
				Vector3(side * 1.52f, 0.0f, 0.0f), trim, Vector3(0.0f, 0.0f, 0.0f));
		if (i == 0) {
			oar_r = pivot;
		} else {
			oar_l = pivot;
		}
	}

	// 船尾水花
	Ref<SphereMesh> drop;
	drop.instantiate();
	drop->set_radius(0.028f);
	drop->set_height(0.056f);
	drop->set_radial_segments(5);
	drop->set_rings(3);
	Ref<StandardMaterial3D> wmat;
	wmat.instantiate();
	wmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	wmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wmat->set_albedo(Color(0.94f, 0.98f, 1.0f, 0.55f));
	wmat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	drop->set_material(wmat);
	wake = memnew(CPUParticles3D);
	wake->set_name("Wake");
	wake->set_amount(26);
	wake->set_lifetime(1.1f);
	wake->set_mesh(drop);
	wake->set_use_local_coordinates(false);
	wake->set_direction(Vector3(0.0f, 0.3f, 1.0f));
	wake->set_spread(70.0f);
	wake->set_gravity(Vector3(0.0f, -0.25f, 0.0f));
	wake->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.25f);
	wake->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.65f);
	wake->set_param_min(CPUParticles3D::PARAM_SCALE, 0.5f);
	wake->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
	wake->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	wake->set_emission_sphere_radius(0.18f);
	wake->set_position(Vector3(0.0f, 0.06f, -1.12f));
	wake->set_emitting(false);
	hull->add_child(wake);
}

void Rowboat::build_dock() {
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.43f, 0.30f, 0.18f);
	const Color wood_light(0.55f, 0.40f, 0.24f);
	const Color wood_dark(0.26f, 0.17f, 0.09f);

	// 码头必须挂到世界（不能挂在船下，否则会跟着船一起漂走）
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("Dock");
	Node *world = get_parent();
	if (world != nullptr) {
		// READY 期间直接 add_child 会被场景装配流程吞掉（实测节点随后消失），
		// 延后到本帧末再挂到世界上。
		world->call_deferred("add_child", body);
	} else {
		add_child(body);
		printf("[Rowboat] dock added to boat (no parent)\n");
	}

	Ref<BoxShape3D> shape;
	shape.instantiate();
	shape->set_size(Vector3(1.9f, 0.14f, 5.6f));
	CollisionShape3D *col = memnew(CollisionShape3D);
	col->set_shape(shape);
	col->set_position(Vector3(-6.0f, -0.31f, -9.1f));
	body->add_child(col);

	// 桥面 + 一条条木板（中间用浅色窄板做出板缝）
	meshutil::add_box(body, toon, Vector3(1.9f, 0.10f, 5.6f), Vector3(-6.0f, -0.34f, -9.1f), wood);
	for (int i = 0; i < 9; i++) {
		float z = -11.6f + (float)i * 0.62f;
		meshutil::add_box(body, toon, Vector3(1.72f, 0.028f, 0.48f), Vector3(-6.0f, -0.28f, z),
				(i % 2 == 0) ? wood_light : wood);
	}

	// 四根入水木桩
	for (int i = 0; i < 4; i++) {
		float x = (i % 2 == 0) ? -6.85f : -5.15f;
		float z = (i < 2) ? -11.6f : -6.65f;
		Ref<CylinderMesh> post;
		post.instantiate();
		post->set_top_radius(0.075f);
		post->set_bottom_radius(0.085f);
		post->set_height(3.0f);
		post->set_radial_segments(8);
		post->set_material(meshutil::toon_material(toon, wood_dark));
		MeshInstance3D *pmi = memnew(MeshInstance3D);
		pmi->set_mesh(post);
		pmi->set_position(Vector3(x, -1.75f, z));
		body->add_child(pmi);
	}

	// 系船柱
	meshutil::add_box(body, toon, Vector3(0.14f, 0.42f, 0.14f), Vector3(-5.1f, -0.05f, -7.2f), wood_dark);
}
