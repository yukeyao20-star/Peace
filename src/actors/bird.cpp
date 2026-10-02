#include "bird.h"

#include <cmath>
#include <random>

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "mesh_util.h"
#include "sky_cycle.h" // 雨雪因子 getter（与 terrain_builder 的夜光联动同一路数）

using namespace godot;

void Bird::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build();
			// 以世界坐标哈希为种：每只鸟参数不同，但整张地图可复现
			Vector3 gp = get_global_position();
			unsigned seed = (unsigned)(gp.x * 73.0f + gp.y * 7.0f + gp.z * 131.0f) + 17u;
			std::mt19937 rng(seed);
			auto rnd = [&rng](float lo, float hi) -> float {
				std::uniform_real_distribution<float> d(lo, hi);
				return d(rng);
			};
			center = gp;
			radius = rnd(7.0f, 13.0f);
			speed = rnd(0.28f, 0.50f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
			phase = rnd(0.0f, 6.2831f);
			flap = rnd(7.0f, 10.0f);
			rng.seed(seed); // 鸣叫节奏也走同一世界坐标哈希，整图可复现
			chirp_t = 5.0f;
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();
			phase += (float)delta * speed;
			anim_t += (float)delta;

			// —— 昼夜/雨雪感知：读兄弟 SkyCycle 的因子（找不到缺省白天/晴）——
			if (sky_cycle == nullptr) {
				Node *parent = get_parent();
				if (parent != nullptr) {
					sky_cycle = parent->get_node_or_null(NodePath("SkyCycle"));
				}
			}
			SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);
			// 夜幕因子取 SkyCycle::get_night_factor()（公式唯一来源）；
			// 缺省 0 = 正午白天，与旧版 tod 缺省 0.5 的换算结果一致。
			night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			float leave = night;

			// —— 雨雪感知：读 SkyCycle 的 rain/snow 因子（找不到缺省 0 = 晴）——
			float wet = 0.0f;
			if (sky != nullptr) {
				wet = Math::clamp(sky->get_rain_amount() + sky->get_snow_amount(), 0.0f, 1.0f);
			}

			// 夜里沿轨道边放大边爬升（+22m），离场后隐藏；黎明平滑回归；
			// 雨雪天（wet→1）压低高度贴湖面飞行（最低约 -3.5m，仍高于树冠与桥面）
			float r_eff = radius * (1.0f + leave * 1.2f);
			Vector3 p = center + Vector3(std::cos(phase) * r_eff,
					std::sin(phase * 2.1f) * 0.9f, std::sin(phase) * r_eff);
			p.y += leave * 22.0f;
			p.y -= wet * 3.5f;
			bool vis = leave < 0.98f;
			if (vis != is_visible()) {
				set_visible(vis);
			}
			Vector3 dir = first_frame ? Vector3(0.0f, 0.0f, -1.0f) : p - last_pos;
			first_frame = false;
			last_pos = p;
			set_global_position(p);
			if (dir.length_squared() > 0.000001f) {
				Vector3 fwd = p + dir;
				if (Math::abs(dir.normalized().dot(Vector3(0.0f, 1.0f, 0.0f))) < 0.99f) {
					look_at(fwd, Vector3(0.0f, 1.0f, 0.0f));
				}
			}
			// 扑翼：左右翼绕 Z 反向摆动（离场爬升时收翼滑翔；雨雪天小幅收翼省力）
			float a = std::sin(anim_t * flap) * 0.65f * (1.0f - leave * 0.7f) * (1.0f - wet * 0.25f);
			if (wing_r != nullptr) {
				wing_r->set_rotation(Vector3(0.0f, 0.0f, a));
			}
			if (wing_l != nullptr) {
				wing_l->set_rotation(Vector3(0.0f, 0.0f, -a));
			}

			// —— 鸟鸣：白天随机间隔鸣叫一声；夜里归巢（leave>=0.5）不叫；
			//      雨雪天（wet>=0.25）闭嘴躲雨，湿度越大下次尝试间隔越长 ——
			if (voice != nullptr && chirps[0].is_valid()) {
				chirp_t -= (float)delta;
				if (chirp_t <= 0.0f) {
					if (vis && leave < 0.5f && wet < 0.25f) {
						int pick = (int)(rng() % 3u);
						voice->set_stream(chirps[pick]);
						voice->set_pitch_scale(0.92f + (float)(rng() % 1000u) * 0.00016f); // 0.92~1.08
						voice->play(0.0f);
					}
					chirp_t = 4.0f + (float)(rng() % 1000u) * 0.007f + wet * 5.0f; // 4~11s，雨雪拉长
				}
			}
		} break;
		default:
			break;
	}
}

void Bird::build() {
	const Color body_c(0.35f, 0.30f, 0.28f);   // 灰褐羽毛
	const Color belly_c(0.62f, 0.57f, 0.52f);  // 浅色腹羽
	const Color beak_c(0.92f, 0.62f, 0.18f);   // 橙喙
	const Color wing_c(0.27f, 0.23f, 0.22f);   // 深色翼羽

	// 身体（椭球，沿 -Z 拉长）+ 头 + 喙 + 尾
	{
		Ref<SphereMesh> body;
		body.instantiate();
		body->set_radius(0.11f);
		body->set_height(0.2f);
		body->set_radial_segments(8);
		body->set_rings(4);
		body->set_material(meshutil::toon_material(meshutil::load_toon_shader(), body_c));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(body);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.9f, 0.85f, 1.7f)); // 沿 Z 拉长
		t.set_basis(b);
		mi->set_transform(t);
		add_child(mi);
	}
	meshutil::add_box(this, meshutil::load_toon_shader(), Vector3(0.13f, 0.12f, 0.13f),
			Vector3(0.0f, 0.05f, -0.16f), belly_c);
	{
		// 喙：圆锥朝 -Z
		Ref<SphereMesh> beak;
		beak.instantiate();
		beak->set_radius(0.028f);
		beak->set_height(0.05f);
		beak->set_radial_segments(6);
		beak->set_rings(2);
		beak->set_material(meshutil::toon_material(meshutil::load_toon_shader(), beak_c));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(beak);
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(-1.5707964f, 0.0f, 0.0f), Vector3(0.7f, 1.6f, 0.7f)); // 拉长成锥状指向 -Z
		t.set_basis(b);
		t.set_origin(Vector3(0.0f, 0.05f, -0.24f));
		mi->set_transform(t);
		add_child(mi);
	}
	meshutil::add_box(this, meshutil::load_toon_shader(), Vector3(0.09f, 0.02f, 0.15f),
			Vector3(0.0f, 0.02f, 0.17f), wing_c);

	// 双翼：枢轴在肩部，翼板向外伸展
	wing_r = memnew(Node3D);
	wing_r->set_name("WingR");
	wing_r->set_position(Vector3(0.09f, 0.03f, -0.02f));
	add_child(wing_r);
	meshutil::add_box(wing_r, meshutil::load_toon_shader(), Vector3(0.30f, 0.02f, 0.14f),
			Vector3(0.15f, 0.0f, 0.0f), wing_c);
	wing_l = memnew(Node3D);
	wing_l->set_name("WingL");
	wing_l->set_position(Vector3(-0.09f, 0.03f, -0.02f));
	add_child(wing_l);
	meshutil::add_box(wing_l, meshutil::load_toon_shader(), Vector3(0.30f, 0.02f, 0.14f),
			Vector3(-0.15f, 0.0f, 0.0f), wing_c);

	// 声源：3D 定位，随鸟移动；音量/衰减按湖面空旷场景调过
	voice = memnew(AudioStreamPlayer3D);
	voice->set_name("Voice");
	voice->set_volume_db(-4.0f);
	voice->set_max_distance(48.0f);
	voice->set_unit_size(12.0f);
	add_child(voice);
	for (int i = 0; i < 3; ++i) {
		chirps[i] = make_chirp(i);
	}
}

Ref<AudioStreamWAV> Bird::make_chirp(int kind) {
	// 三种鸣叫（MC 式短哨）：0 上扬短哨 / 1 下滑长哨 / 2 连音双哨。
	// 正弦基频 + 35% 二次谐波提亮，快速起音 + 指数衰减，听感是"啾/啾啾"。
	const int sr = 22050;
	const float dur = (kind == 2) ? 0.26f : (kind == 1 ? 0.24f : 0.18f);
	const float seg_len = (kind == 2) ? 0.10f : dur; // kind2 每声 0.10s，中间 0.06s 停顿
	const int n = (int)(dur * (float)sr);

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	float phase = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		// kind2 用段内局部时间（第二声从 t=0.16 重新起音、相位归零）
		float lt = (kind == 2 && t >= 0.16f) ? t - 0.16f : t;
		float s = 0.0f;
		if (lt >= 0.0f && lt < seg_len) {
			float u = Math::clamp(lt / seg_len, 0.0f, 1.0f);
			float f = 0.0f;
			if (kind == 0) {
				f = 1900.0f + 1000.0f * u * u;  // 上扬加速
			} else if (kind == 1) {
				f = 2700.0f - 1200.0f * u;      // 下滑
			} else {
				f = 2100.0f + 600.0f * u;       // 双哨各自小幅上扬
			}
			phase += 6.2831853f * f / (float)sr;
			float env = Math::clamp(lt * 60.0f, 0.0f, 1.0f); // ~16ms 起音防爆音
			env *= std::exp(-lt * 14.0f);                    // 指数衰减出"哨"感
			s = (std::sin(phase) + 0.35f * std::sin(2.0f * phase)) * 0.42f * env;
		} else {
			phase = 0.0f;
		}
		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF); // 小端 16-bit
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	return wav;
}
