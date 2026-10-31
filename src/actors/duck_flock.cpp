#include "duck_flock.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void DuckFlock::_bind_methods() {
	ADD_SIGNAL(MethodInfo("animal_fed"));
	// 覆写 Interactable 的虚方法：脚本侧也走鸭子自己的实现（-s 探针可测）
	ClassDB::bind_method(D_METHOD("get_prompt"), &DuckFlock::get_prompt);
	ClassDB::bind_method(D_METHOD("get_interact_pos"), &DuckFlock::get_interact_pos);
	ClassDB::bind_method(D_METHOD("get_interact_distance"), &DuckFlock::get_interact_distance);
	ClassDB::bind_method(D_METHOD("requires_line_of_sight"), &DuckFlock::requires_line_of_sight);
	ClassDB::bind_method(D_METHOD("interact", "player_pos"), &DuckFlock::interact);
}

void DuckFlock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			add_to_group("interactables"); // E 键撒面包屑
			add_to_group("farm_animals"); // 复用"喂动物"每日待办计分（导演按组连 animal_fed）
			if (get_parent() != nullptr) {
				sky_cycle = get_parent()->get_node_or_null("SkyCycle");
				player = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Player"));
			}
			// 面包屑小球的共享网格/材质
			crumb_mesh.instantiate();
			crumb_mesh->set_radius(0.022f);
			crumb_mesh->set_height(0.044f);
			crumb_mesh->set_radial_segments(6);
			crumb_mesh->set_rings(3);
			crumb_mat.instantiate();
			crumb_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			crumb_mat->set_albedo(Color(0.94f, 0.83f, 0.56f));
			crumb_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
			crumb_mesh->set_material(crumb_mat);
			// 嘎嘎声源（程序化合成，无外部音频依赖）
			quack_stream = make_quack();
			if (quack_stream.is_valid()) {
				voice = memnew(AudioStreamPlayer3D);
				voice->set_name("DuckVoice");
				voice->set_stream(quack_stream);
				voice->set_volume_db(-10.0f);
				voice->set_max_distance(30.0f);
				add_child(voice);
			}
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();
			anim_t += (float)delta;
			// 喂食冷却/聚拢计时
			if (feed_cd > 0.0f) {
				feed_cd = Math::max(0.0f, feed_cd - (float)delta);
			}
			if (feed_t > 0.0f) {
				feed_t = Math::max(0.0f, feed_t - (float)delta);
				feed_elapsed += (float)delta;
			}
			update_crumbs((float)delta);
			// 偶尔嘎两声：玩家在 30m 内才叫，避免远处空响
			quack_timer -= (float)delta;
			if (quack_timer <= 0.0f && voice != nullptr && !ducks.empty()) {
				bool near = true;
				if (player != nullptr && ducks[0].root != nullptr) {
					near = player->get_global_position().distance_to(
							ducks[0].root->get_global_position()) < 30.0f;
				}
				if (near) {
					static std::mt19937 qrng(424242u);
					std::uniform_int_distribution<int> pick(0, (int)ducks.size() - 1);
					std::uniform_real_distribution<float> wait(9.0f, 22.0f);
					Duck &d = ducks[pick(qrng)];
					if (d.root != nullptr) {
						voice->set_global_position(d.root->get_global_position() + Vector3(0.0f, 0.15f, 0.0f));
						voice->play();
					}
					quack_timer = wait(qrng);
				} else {
					quack_timer = 4.0f;
				}
			}
			// 夜晚：先放慢巡游，再渐渐靠向南岸歇息（栖岸）；清晨自动散回巡游
			float night_mul = 1.0f;
			float rest_target = 0.0f;
			if (sky_cycle != nullptr) {
				SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);
				if (sky != nullptr) {
					float td = sky->get_time_of_day();
					bool night = td < 0.22f || td > 0.78f;
					night_mul = night ? 0.35f : 1.0f;
					if (sky->get_night_factor() > 0.65f) {
						rest_target = 1.0f;
					}
				}
			}
			if (feed_t > 0.0f) {
				rest_target = 0.0f; // 投喂时优先聚拢
			}
			rest_k = Math::move_toward(rest_k, rest_target, (float)delta * 0.30f);
			// 喂食聚拢权重：1.2s 游过去、最后 1.5s 散开
			float gather_k = 0.0f;
			if (feed_t > 0.0f) {
				float kin = Math::clamp(feed_elapsed / 1.2f, 0.0f, 1.0f);
				float kout = Math::clamp(feed_t / 1.5f, 0.0f, 1.0f);
				float k = kin * kout;
				gather_k = k * k * (3.0f - 2.0f * k);
			}
			for (int di = 0; di < (int)ducks.size(); di++) {
				Duck &d = ducks[di];
				if (d.root == nullptr) {
					continue;
				}
				// 小鸭：不参与环形巡游，贴着母鸭侧后方跟随（投喂/栖岸时也一起走）
				if (d.follow >= 0 && d.follow < (int)ducks.size()
						&& ducks[d.follow].root != nullptr) {
					Duck &m = ducks[d.follow];
					Transform3D mt = m.root->get_global_transform();
					Vector3 mfwd = -mt.basis.get_column(2); // 母鸭朝前（-Z）
					Vector3 mright = mt.basis.get_column(0);
					float bob_l = std::sin(anim_t * d.bob_w + d.bob_phase) * 0.010f;
					Vector3 target = m.root->get_global_position()
							- mfwd * 0.55f + mright * (d.follow_side * 0.55f)
							+ Vector3(0.0f, bob_l, 0.0f);
					Vector3 cur = d.root->get_global_position();
					Vector3 np = cur.lerp(target, Math::min(1.0f, (float)delta * 5.0f));
					d.root->set_global_position(np);
					if (mfwd.length_squared() > 0.0001f) {
						d.root->look_at(np + mfwd.normalized(), Vector3(0.0f, 1.0f, 0.0f));
					}
					if (d.wake != nullptr) {
						float ws_l = 1.0f + 0.10f * std::sin(anim_t * 3.0f + d.bob_phase);
						d.wake->set_scale(Vector3(ws_l, 1.0f, ws_l));
					}
					continue;
				}
				d.angle += d.speed * night_mul * (float)delta * (1.0f - rest_k * 0.92f);
				float bob = std::sin(anim_t * d.bob_w + d.bob_phase) * 0.012f;
				Vector3 pos(lake_center.x + std::cos(d.angle) * d.radius,
						water_level + 0.02f + bob,
						lake_center.y + std::sin(d.angle) * d.radius);
				if (gather_k > 0.001f) {
					float ga = Math::TAU * (float)di / (float)ducks.size();
					float gr = 0.5f + 0.22f * (float)di;
					Vector3 gather(feed_point.x + std::cos(ga) * gr,
							water_level + 0.02f + bob,
							feed_point.z + std::sin(ga) * gr);
					pos = pos.lerp(gather, gather_k);
				} else if (rest_k > 0.001f) {
					// 南岸一溜儿歇脚点：绕着桥南浅水，各自错开避免叠在一起
					float ra = 2.05f + ((float)di - 0.5f * (float)(ducks.size() - 1)) * 0.17f;
					float rr = 13.2f + 0.35f * (float)(di % 3);
					Vector3 rest(lake_center.x + std::cos(ra) * rr,
							water_level + 0.02f + bob * 0.35f,
							lake_center.y + std::sin(ra) * rr);
					pos = pos.lerp(rest, rest_k * 0.92f);
				}
				d.root->set_global_position(pos);
				Vector3 gpos = pos;
				Vector3 dir;
				if (gather_k > 0.03f) {
					// 聚拢时面朝食物
					dir = Vector3(feed_point.x - gpos.x, 0.0f, feed_point.z - gpos.z);
					if (dir.length_squared() < 0.0001f) {
						dir = Vector3(0.0f, 0.0f, 1.0f);
					} else {
						dir = dir.normalized();
					}
				} else {
					// 朝向 = 切线方向（角速度符号决定顺/逆时针），再叠一个固定微偏
					float sgn = d.speed >= 0.0f ? 1.0f : -1.0f;
					Vector3 tangent(-std::sin(d.angle) * sgn, 0.0f, std::cos(d.angle) * sgn);
					float yaw = std::atan2(tangent.x, tangent.z) + d.yaw_offset;
					dir = Vector3(std::sin(yaw), 0.0f, std::cos(yaw));
					if (rest_k > 0.02f) {
						// 栖岸时大多面向湖心，安静漂着
						Vector3 to_center(lake_center.x - gpos.x, 0.0f, lake_center.y - gpos.z);
						if (to_center.length_squared() > 0.0001f) {
							dir = dir.lerp(to_center.normalized(), Math::min(1.0f, rest_k)).normalized();
						}
					}
				}
				d.root->look_at(gpos + dir, Vector3(0.0f, 1.0f, 0.0f));
				// 轻微左右摇晃（像踩水）
				d.root->rotate_object_local(Vector3(0.0f, 0.0f, 1.0f),
						std::sin(anim_t * 1.6f + d.bob_phase) * 0.045f);
				if (d.wake != nullptr) {
					float ws = 1.0f + 0.08f * std::sin(anim_t * 2.1f + d.bob_phase);
					d.wake->set_scale(Vector3(ws, 1.0f, ws));
				}
			}
		} break;
		default:
			break;
	}
}

void DuckFlock::build() {
	ducks.clear();
	spawn_duck(0, 20261031u);
	for (int i = 1; i < duck_count; i++) {
		spawn_duck(i, 20261031u + (unsigned)i * 7919u);
	}
	// 两只小鸭跟着 0 号母鸭
	spawn_duck(duck_count, 20261031u + 991u, true, 0, -0.62f);
	spawn_duck(duck_count + 1, 20261031u + 1993u, true, 0, 0.62f);
}

void DuckFlock::spawn_duck(int p_index, unsigned p_seed, bool p_duckling, int p_follow, float p_side) {
	std::mt19937 rng(p_seed);
	auto rnd = [&rng](float lo, float hi) {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};

	Duck d;
	d.duckling = p_duckling;
	d.follow = p_follow;
	d.follow_side = p_side;
	if (p_duckling) {
		d.bob_w = rnd(1.6f, 2.3f);
		d.bob_phase = rnd(0.0f, 6.2831853f);
	} else {
		d.angle = rnd(0.0f, 6.2831853f);
		d.radius = rnd(6.2f, 9.4f);
		d.speed = rnd(0.022f, 0.052f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
		d.bob_w = rnd(0.9f, 1.7f);
		d.bob_phase = rnd(0.0f, 6.2831853f);
		d.yaw_offset = (rnd(0.0f, 1.0f) - 0.5f) * 0.35f;
	}

	d.root = memnew(Node3D);
	d.root->set_name((p_duckling ? "Duckling" : "Duck") + String::num_int64(p_index));
	add_child(d.root);

	Ref<Shader> toon = meshutil::load_toon_shader();
	const bool male = !p_duckling && rnd(0.0f, 1.0f) < 0.55f;
	const Color body_c = p_duckling ? Color(0.86f, 0.76f, 0.42f)
			: (male ? Color(0.64f, 0.62f, 0.57f) : Color(0.56f, 0.47f, 0.35f));
	const Color head_c = p_duckling ? Color(0.91f, 0.82f, 0.50f)
			: (male ? Color(0.10f, 0.36f, 0.22f) : Color(0.44f, 0.35f, 0.25f));
	const Color beak_c(0.93f, 0.58f, 0.12f);
	if (p_duckling) {
		d.root->set_scale(Vector3(0.62f, 0.62f, 0.62f));
	}

	auto add_sphere = [&](const Vector3 &p_pos, float p_radius, float p_height,
			const Vector3 &p_scale, const Color &p_color) {
		Ref<SphereMesh> sp;
		sp.instantiate();
		sp->set_radius(p_radius);
		sp->set_height(p_height);
		sp->set_radial_segments(16);
		sp->set_rings(8);
		sp->set_material(meshutil::toon_material(toon, p_color));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(sp);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(0.0f, 0.0f, 0.0f), p_scale);
		t.set_basis(b);
		t.set_origin(p_pos);
		mi->set_transform(t);
		d.root->add_child(mi);
	};

	auto add_cone = [&](const Vector3 &p_pos, float p_bottom, float p_top, float p_height,
			float p_rot_x, const Color &p_color) {
		Ref<CylinderMesh> cy;
		cy.instantiate();
		cy->set_bottom_radius(p_bottom);
		cy->set_top_radius(p_top);
		cy->set_height(p_height);
		cy->set_radial_segments(12);
		cy->set_material(meshutil::toon_material(toon, p_color));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(cy);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(p_rot_x, 0.0f, 0.0f), Vector3(1, 1, 1));
		t.set_basis(b);
		t.set_origin(p_pos);
		mi->set_transform(t);
		d.root->add_child(mi);
	};

	// 身体：压扁的椭球（沿 -Z 朝前）
	add_sphere(Vector3(0.0f, 0.10f, 0.0f), 0.15f, 0.30f,
			Vector3(1.05f, 0.72f, 1.5f), body_c);
	// 颈
	Ref<CylinderMesh> neck;
	neck.instantiate();
	neck->set_bottom_radius(0.045f);
	neck->set_top_radius(0.034f);
	neck->set_height(0.16f);
	neck->set_radial_segments(10);
	neck->set_material(meshutil::toon_material(toon, head_c));
	MeshInstance3D *neck_mi = memnew(MeshInstance3D);
	neck_mi->set_mesh(neck);
	neck_mi->set_position(Vector3(0.0f, 0.19f, -0.09f));
	neck_mi->set_rotation(Vector3(0.25f, 0.0f, 0.0f));
	d.root->add_child(neck_mi);
	// 头
	add_sphere(Vector3(0.0f, 0.275f, -0.15f), 0.062f, 0.124f,
			Vector3(1.0f, 1.0f, 1.12f), head_c);
	// 喙（圆锥，朝 -Z）
	add_cone(Vector3(0.0f, 0.262f, -0.225f), 0.024f, 0.004f, 0.075f,
			-1.5707964f, beak_c);
	// 尾（圆锥，朝 +Z 略上翘）
	add_cone(Vector3(0.0f, 0.135f, 0.195f), 0.055f, 0.012f, 0.13f,
			1.35f, body_c);
	// 眼睛
	add_sphere(Vector3(0.040f, 0.292f, -0.185f), 0.010f, 0.020f, Vector3(1, 1, 1),
			Color(0.06f, 0.06f, 0.06f));
	add_sphere(Vector3(-0.040f, 0.292f, -0.185f), 0.010f, 0.020f, Vector3(1, 1, 1),
			Color(0.06f, 0.06f, 0.06f));

	// 尾迹水圈：贴在吃水线附近的一圈淡白圆环（随浮动轻微缩放）
	Ref<TorusMesh> torus;
	torus.instantiate();
	torus->set_inner_radius(0.165f);
	torus->set_outer_radius(0.195f);
	torus->set_rings(8);
	torus->set_ring_segments(44); // 圆环更顺滑（低段数近看像白色多边形）
	Ref<StandardMaterial3D> wake_mat;
	wake_mat.instantiate();
	wake_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	wake_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wake_mat->set_albedo(Color(0.92f, 0.96f, 1.0f, 0.16f));
	wake_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	torus->set_material(wake_mat);
	MeshInstance3D *wake_mi = memnew(MeshInstance3D);
	wake_mi->set_mesh(torus);
	wake_mi->set_position(Vector3(0.0f, -0.018f, 0.02f));
	d.root->add_child(wake_mi);
	d.wake = wake_mi;

	ducks.push_back(d);
}

// —— 喂食交互（Interactable）——

String DuckFlock::get_prompt() const {
	if (feed_t > 0.0f) {
		return String::utf8("🦆 鸭群游过来了");
	}
	if (feed_cd > 0.0f) {
		return String::utf8("鸭子吃饱了，待会儿再来");
	}
	return String::utf8("按 E 撒面包屑");
}

Vector3 DuckFlock::get_interact_pos() const {
	if (ducks.empty()) {
		return Vector3(lake_center.x, water_level, lake_center.y);
	}
	// 取离玩家最近的那只鸭（玩家在岸上，避免用节点原点/固定半径的鸭）
	Vector3 ref = player != nullptr ? player->get_global_position() : get_global_position();
	Vector3 best = get_global_position();
	float best_d = 1e9f;
	for (const Duck &d : ducks) {
		if (d.root == nullptr) {
			continue;
		}
		Vector3 p = d.root->get_global_position();
		float dd = p.distance_squared_to(ref);
		if (dd < best_d) {
			best_d = dd;
			best = p;
		}
	}
	best.y = water_level + 0.15f;
	return best;
}

float DuckFlock::get_interact_distance() const {
	return 8.0f; // 鸭子离岸 5~7m，比常规交互半径大
}

void DuckFlock::interact(const Vector3 &p_player_pos) {
	if (feed_cd > 0.0f) {
		return;
	}
	feed_cd = 12.0f;
	feed_t = 7.0f;
	feed_elapsed = 0.0f;
	// 投喂点落在玩家这一侧的湖面上（离湖心不超过 12m，保证在水里）
	Vector2 dir(p_player_pos.x - lake_center.x, p_player_pos.z - lake_center.y);
	float len = dir.length();
	if (len < 0.01f) {
		dir = Vector2(0.0f, 1.0f);
		len = 0.0f;
	} else {
		dir /= len;
	}
	float reach = Math::min(len, 12.0f);
	feed_point = Vector3(lake_center.x + dir.x * reach, water_level + 0.02f,
			lake_center.y + dir.y * reach);
	spawn_crumbs();
	if (voice != nullptr) {
		voice->set_global_position(feed_point + Vector3(0.0f, 0.2f, 0.0f));
		voice->play();
	}
	emit_signal("animal_fed"); // 每日待办"喂动物"计分
}

void DuckFlock::spawn_crumbs() {
	static std::mt19937 rng(515151u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < 7; i++) {
		if (!crumb_mesh.is_valid()) {
			break;
		}
		float a = d01(rng) * Math::TAU;
		float r = 0.12f + d01(rng) * 0.55f;
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name("Crumb" + String::num_int64(i));
		mi->set_mesh(crumb_mesh);
		mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(mi);
		mi->set_global_position(feed_point + Vector3(std::cos(a) * r, 0.012f, std::sin(a) * r));
		Crumb c;
		c.mi = mi;
		c.life = 4.6f + d01(rng) * 0.8f;
		crumbs.push_back(c);
	}
}

void DuckFlock::update_crumbs(float p_delta) {
	for (size_t i = 0; i < crumbs.size();) {
		Crumb &c = crumbs[i];
		c.life -= p_delta;
		if (c.life <= 0.0f) {
			if (c.mi != nullptr) {
				c.mi->queue_free();
			}
			crumbs.erase(crumbs.begin() + (long)i);
		} else {
			if (c.mi != nullptr) {
				float s = Math::clamp(c.life / 1.2f, 0.0f, 1.0f);
				c.mi->set_scale(Vector3(s, s, s));
			}
			i++;
		}
	}
}

// 程序化鸭叫：「嘎-嘎」两声短促鼻音，基频下滑 + 共振峰加权（免滤波器，稳定）。
Ref<AudioStreamWAV> DuckFlock::make_quack() {
	const int sr = 22050;
	const float dur = 0.55f;
	const int n = (int)(dur * sr);
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	static std::mt19937 rng(909090u);
	std::uniform_real_distribution<float> d01(0.0f, 1.0f);
	for (int i = 0; i < n; i++) {
		float t = (float)i / sr;
		float samp = 0.0f;
		for (int b = 0; b < 2; b++) {
			float bt = t - b * 0.24f;
			if (bt < 0.0f || bt > 0.19f) {
				continue;
			}
			// 基频下滑：第一声 520→330，第二声略低
			float f0 = (b == 0 ? 520.0f : 470.0f) * (1.0f - 0.36f * (bt / 0.19f));
			float env = Math::clamp(bt / 0.012f, 0.0f, 1.0f)
					* Math::exp(-bt * 14.0f);
			float ph = Math::TAU * f0 * bt;
			// 加法合成：谐波按共振峰 F1≈1250 / F2≈2500 加权
			float v = 0.0f;
			for (int k = 1; k <= 10; k++) {
				float fk = f0 * (float)k;
				float w1 = Math::exp(-Math::pow((fk - 1250.0f) / 700.0f, 2.0f));
				float w2 = Math::exp(-Math::pow((fk - 2500.0f) / 900.0f, 2.0f));
				float amp = (0.25f / (float)k) + w1 * 0.55f + w2 * 0.25f;
				v += amp * Math::sin(ph * (float)k);
			}
			float breath = (d01(rng) - 0.5f) * 0.10f * env;
			samp += (v * 0.7f + breath) * env;
		}
		int16_t q = (int16_t)Math::clamp(samp * 26000.0f, -32000.0f, 32000.0f);
		w[i * 2] = (uint8_t)(q & 0xFF);
		w[i * 2 + 1] = (uint8_t)((q >> 8) & 0xFF);
	}
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}
