#include "ambience.h"

#include <cmath>
#include <cstdint>
#include <random>

#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "sky_cycle.h" // 昼夜/天气因子 getter
#include "terrain_builder.h"

using namespace godot;

void Ambience::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sky_cycle_path", "path"), &Ambience::set_sky_cycle_path);
	ClassDB::bind_method(D_METHOD("get_sky_cycle_path"), &Ambience::get_sky_cycle_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "sky_cycle_path"), "set_sky_cycle_path", "get_sky_cycle_path");

	ClassDB::bind_method(D_METHOD("set_master", "value"), &Ambience::set_master);
	ClassDB::bind_method(D_METHOD("get_master"), &Ambience::get_master);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "master", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_master", "get_master");
}

void Ambience::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不合成不发声，保持场景干净
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			shutdown_audio();
		} break;
		case NOTIFICATION_PROCESS: {
			double delta = get_process_delta_time();

			// —— 懒查找 SkyCycle：按 sky_cycle_path 解析，失败退回兄弟节点
			//     （同 bird 的套路；找不到时按白天/无雨处理）——
			if (sky_cycle == nullptr) {
				Node *found = get_node_or_null(sky_cycle_path);
				if (found == nullptr) {
					Node *parent = get_parent();
					if (parent != nullptr) {
						found = parent->get_node_or_null(NodePath("SkyCycle"));
					}
				}
				sky_cycle = Object::cast_to<SkyCycle>(found);
			}
			SkyCycle *sky = Object::cast_to<SkyCycle>(sky_cycle);

			// —— 懒查找 Player 与 Terrain ——
			if (player_cache == nullptr && get_parent() != nullptr) {
				player_cache = Object::cast_to<Node3D>(get_parent()->get_node_or_null(NodePath("Player")));
			}
			if (terrain_cache == nullptr && get_parent() != nullptr) {
				terrain_cache = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("Terrain")));
				if (terrain_cache == nullptr) {
					terrain_cache = Object::cast_to<TerrainBuilder>(get_parent()->get_node_or_null(NodePath("TerrainBuilder")));
				}
			}

			// —— 昼夜/天气感知（找不到 SkyCycle 缺省：正午、晴）——
			float rain_amt = 0.0f;
			float snow_amt = 0.0f;
			if (sky != nullptr) {
				rain_amt = sky->get_rain_amount();
				snow_amt = sky->get_snow_amount();
			}
			// 夜幕因子取 SkyCycle::get_night_factor()（公式唯一来源）；
			// 缺省 0 = 正午白天，与旧版 tod 缺省 0.5 的换算结果一致。
			const float night_f = (sky != nullptr) ? sky->get_night_factor() : 0.0f;

			// 天气因子平滑（lerp 0.04，约半秒跟上，防状态切换跳变）
			rain_s = Math::lerp(rain_s, rain_amt, 0.04f);
			snow_s = Math::lerp(snow_s, snow_amt, 0.04f);
			float wet = Math::clamp(rain_amt + snow_amt, 0.0f, 1.0f);
			wet_s = Math::lerp(wet_s, wet, 0.04f);

			const float mg = Math::clamp(master, 0.0f, 1.0f); // master 线性

			// 室内判定（屋里 / 门廊）：人在屋里时，室外刮风、下雪、下雨的声音显著减弱
			float outdoor_atten = 1.0f;
			if (sky != nullptr && player_cache != nullptr
					&& sky->is_sheltered(player_cache->get_global_position())) {
				outdoor_atten = 0.12f; // 室内衰减至 12%（降低约 18.4 dB），门窗室内听室外风雨雪沉闷幽微
			}

			// —— 风声：基线 -18dB → 雨雪天（wet>0.15 起）线性升到 -12dB ——
			//     （峰值压在 -12dB：背景垫底，不压鸟鸣 3D 声源与脚步声；进屋后乘 outdoor_atten 减弱）
			float wet_k = Math::clamp((wet_s - 0.15f) / 0.85f, 0.0f, 1.0f);
			float wind_db = Math::lerp(-18.0f, -12.0f, wet_k);
			float wind_lin = std::pow(10.0f, wind_db / 20.0f) * mg * outdoor_atten;
			// 雪天（snow>rain）主导时换更闷的雪风音色：两台播放器交叉淡化
			float snow_mix = Math::clamp((snow_s - rain_s) * 1.5f + 0.5f, 0.0f, 1.0f);
			wind_gain = Math::lerp(wind_gain, wind_lin * (1.0f - snow_mix), 0.05f);
			snow_gain = Math::lerp(snow_gain, wind_lin * snow_mix, 0.05f);

			// —— 雨声层：纯雨量 rain_s 驱动！下雪天完全静音！进屋后显著减弱！——
			// 有雪时被雪压制（防雨雪过渡期残留雨声，雪天只留宁静雪风）
			float rain_effective = Math::max(0.0f, rain_s - snow_s * 0.8f);
			float rain_k = Math::clamp((rain_effective - 0.10f) / 0.90f, 0.0f, 1.0f);
			float rain_target_db = Math::lerp(-24.0f, -12.0f, rain_k);
			float rain_lin = (rain_k > 0.01f) ? (std::pow(10.0f, rain_target_db / 20.0f) * mg * outdoor_atten) : 0.0f;
			rain_gain = Math::lerp(rain_gain, rain_lin, 0.05f);

			// —— 晨曦鸟鸣层：白天（night_f < 0.35）且晴朗无大雨雪（wet_s < 0.30）——
			float bird_day_k = Math::clamp(1.0f - (night_f - 0.10f) / 0.25f, 0.0f, 1.0f);
			float bird_weather_k = Math::clamp(1.0f - (wet_s - 0.10f) / 0.20f, 0.0f, 1.0f);
			float bird_target_k = bird_day_k * bird_weather_k;
			float bird_lin = (bird_target_k > 0.01f) ? (std::pow(10.0f, -17.0f / 20.0f) * bird_target_k * mg * (outdoor_atten * 0.6f + 0.4f)) : 0.0f;
			birds_gain = Math::lerp(birds_gain, bird_lin, 0.04f);

			// —— 湖水拍岸层：玩家靠近湖水时响起，冬天湖面结冰（snow_s 高）时减弱 ——
			float lake_proximity = 0.0f;
			if (player_cache != nullptr) {
				Vector3 pp = player_cache->get_global_position();
				if (terrain_cache != nullptr) {
					if (terrain_cache->is_near_lake(pp.x, pp.z, 2.0f)) {
						lake_proximity = 1.0f;
					} else if (terrain_cache->is_near_lake(pp.x, pp.z, 16.0f)) {
						lake_proximity = 0.5f;
					}
				} else {
					float d_lake = Vector2(pp.x - (-6.0f), pp.z - 26.0f).length();
					lake_proximity = Math::clamp(1.0f - (d_lake - 8.0f) / 16.0f, 0.0f, 1.0f);
				}
			}
			float lake_unfrozen = Math::clamp(1.0f - snow_s * 1.5f, 0.0f, 1.0f);
			float water_k = lake_proximity * lake_unfrozen;
			float water_lin = (water_k > 0.01f) ? (std::pow(10.0f, -16.0f / 20.0f) * water_k * mg * outdoor_atten) : 0.0f;
			water_gain = Math::lerp(water_gain, water_lin, 0.05f);

			// —— 壁炉柴火层：靠近木屋壁炉时响起 ——
			// 声源中心取**真实壁炉位置**，不是院子中心 (12,8)：
			// 壁炉在 Cabin 局部 (-3.42, 0.38, -0.6)，木屋整体绕 Y 转了 45°、
			// 原点在 (12, 0.35, 8) → 世界 XZ ≈ (9.16, 9.99)。
			float hearth_k = 0.0f;
			if (player_cache != nullptr) {
				Vector3 pp = player_cache->get_global_position();
				float d_fire = Vector2(pp.x - 9.16f, pp.z - 9.99f).length();
				if (d_fire < 5.0f) {
					hearth_k = 1.0f; // 屋里（含门廊）满响
				} else if (d_fire < 18.0f) {
					hearth_k = 1.0f - (d_fire - 5.0f) / 13.0f;
				}
			}
			// 响应用户反馈：壁炉燃烧的声音太小了，大幅提升壁炉音效增益（由 -6.5dB 提升至 +2.0dB），确保在室内木柴燃烧噼啪声清晰清脆温润
			float hearth_lin = (hearth_k > 0.01f) ? (std::pow(10.0f, 2.0f / 20.0f) * hearth_k * mg) : 0.0f;
			hearth_gain = Math::lerp(hearth_gain, hearth_lin, 0.05f);

			if (wind != nullptr) {
				apply_gain(wind, wind_gain, wind_paused);
			}
			if (wind_snow != nullptr) {
				apply_gain(wind_snow, snow_gain, snow_paused);
			}
			if (rain != nullptr) {
				apply_gain(rain, rain_gain, rain_paused);
			}
			if (birds != nullptr) {
				apply_gain(birds, birds_gain, birds_paused);
			}
			if (water != nullptr) {
				apply_gain(water, water_gain, water_paused);
			}
			if (hearth != nullptr) {
				apply_gain(hearth, hearth_gain, hearth_paused);
			}

			// —— 夜间虫鸣：night>0.3（等价旧阈值 sun_h<-0.05，night=1-(sun_h+0.12)/0.10
			//     在 sun_h=-0.05 处恰为 0.3）且 wet<0.3 时每 3~8s 随机一声 ——
			if (cricket != nullptr && snd_chirp.is_valid()) {
				bool night_ok = (night_f > 0.3f) && (wet_s < 0.3f);
				cricket_t -= (float)delta;
				if (cricket_t <= 0.0f) {
					if (night_ok && !cricket->is_playing()) {
						// 音高微随机：载波 4500Hz × 0.92~1.08 ≈ 4140~4860Hz
						cricket->set_pitch_scale(0.92f + (float)(rng() % 1000u) * 0.00016f);
						// -22dB 很轻，master 线性叠上去（防 linear_to_db(0) 出 -inf）
						cricket->set_volume_db(-22.0f + Math::linear_to_db(Math::max(mg, 0.0001f)));
						cricket->play(0.0f);
					}
					cricket_t = 3.0f + (float)(rng() % 1000u) * 0.005f; // 3~8s
				}
				if (!night_ok) {
					cricket_t = Math::max(cricket_t, 1.5f); // 白天不叫，入夜也不立即叫
				}
			}
		} break;
		default:
			break;
	}
}

void Ambience::build() {
	ResourceLoader *rl = ResourceLoader::get_singleton();

	auto load_looping_mp3 = [&](const String &p_path) -> Ref<AudioStream> {
		if (rl != nullptr && rl->exists(p_path)) {
			Ref<Resource> r = rl->load(p_path);
			if (r.is_valid()) {
				Ref<AudioStreamMP3> mp3 = Object::cast_to<AudioStreamMP3>(r.ptr());
				if (mp3.is_valid()) {
					mp3->set_loop(true);
					return mp3;
				}
				// WAV 的循环必须显式设 loop_mode，否则播一遍就停
				Ref<AudioStreamWAV> wav = Object::cast_to<AudioStreamWAV>(r.ptr());
				if (wav.is_valid()) {
					wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
					return wav;
				}
				Ref<AudioStream> s = Object::cast_to<AudioStream>(r.ptr());
				if (s.is_valid()) {
					return s;
				}
			}
		}
		return Ref<AudioStream>();
	};

	snd_wind = load_looping_mp3("res://assets/audio/wind_breeze.mp3");
	if (!snd_wind.is_valid()) {
		snd_wind = make_wind(false);
	}
	snd_wind_snow = make_wind(true);

	snd_rain = load_looping_mp3("res://assets/audio/rain_gentle.mp3");
	if (!snd_rain.is_valid()) {
		snd_rain = make_rain();
	}

	snd_chirp = make_chirp();
	snd_birds = load_looping_mp3("res://assets/audio/birds_morning.mp3");
	snd_water = load_looping_mp3("res://assets/audio/water_lap.mp3");
	if (!snd_water.is_valid()) {
		snd_water = make_water_lap();
	}
	// 壁炉柴火：优先用离线归一化过的无缝循环（tools/make_fireplace_loop.py 生成）。
	// 原始 cabin_fireplace.mp3 实测 RMS 只有 -50.8 dB（峰值 -4.9 dB），
	// 按原样播再乘 -14dB 增益后实际只剩 -65dB —— 比风声(-49.7dB)还低 20dB 以上，
	// 玩家站在炉边也完全听不见。归一化版把 RMS 拉到 -20dB 并做了软限幅。
	snd_hearth = load_looping_mp3("res://assets/audio/cabin_fireplace_loop.wav");
	if (!snd_hearth.is_valid()) {
		WARN_PRINT("Ambience: 缺少 assets/audio/cabin_fireplace_loop.wav"
				"（用 tools/make_fireplace_loop.py 生成），退回原始 cabin_fireplace.mp3 ——"
				"它 RMS 只有 -50.8dB，基本听不见。");
		snd_hearth = load_looping_mp3("res://assets/audio/cabin_fireplace.mp3");
	}

	// 背景音乐（BGM）：柔和空灵治愈，循环播放，绝不喧宾夺主（-24dB）
	if (rl != nullptr && rl->exists("res://assets/audio/bgm_kalimba_night.wav")) {
		Ref<Resource> r = rl->load("res://assets/audio/bgm_kalimba_night.wav");
		if (r.is_valid()) {
			Ref<AudioStreamWAV> wav = Object::cast_to<AudioStreamWAV>(r.ptr());
			if (wav.is_valid()) {
				wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
				snd_bgm = wav;
			} else {
				snd_bgm = Object::cast_to<AudioStream>(r.ptr());
			}
		}
	}

	auto setup_loop_player = [&](AudioStreamPlayer *&p_player, const char *p_name, Ref<AudioStream> p_stream) {
		p_player = memnew(AudioStreamPlayer);
		p_player->set_name(p_name);
		if (p_stream.is_valid()) {
			p_player->set_stream(p_stream);
		}
		p_player->set_volume_db(-60.0f);
		add_child(p_player);
		p_player->play();
		p_player->set_stream_paused(true);
	};

	setup_loop_player(wind, "Wind", snd_wind);
	setup_loop_player(wind_snow, "WindSnow", snd_wind_snow);
	setup_loop_player(rain, "RainLayer", snd_rain);
	setup_loop_player(birds, "Birds", snd_birds);
	setup_loop_player(water, "WaterLap", snd_water);
	setup_loop_player(hearth, "Fireplace", snd_hearth);

	if (snd_bgm.is_valid()) {
		bgm = memnew(AudioStreamPlayer);
		bgm->set_name("BGM");
		bgm->set_stream(snd_bgm);
		bgm->set_volume_db(-24.0f); // 保持很小声很舒适
		add_child(bgm);
		bgm->play();
	}

	// 虫鸣是一次性短音：不循环、不预播放，到点才响
	cricket = memnew(AudioStreamPlayer);
	cricket->set_name("Cricket");
	cricket->set_stream(snd_chirp);
	cricket->set_volume_db(-22.0f);
	add_child(cricket);
}

// 退出收口（NOTIFICATION_EXIT_TREE 触发）：stop 全部播放器并清掉成员上的音频流引用
void Ambience::shutdown_audio() {
	set_process(false);
	AudioStreamPlayer *players[8] = {wind, wind_snow, rain, cricket, birds, water, hearth, bgm};
	for (int i = 0; i < 8; ++i) {
		if (players[i] != nullptr) {
			players[i]->stop();
			players[i]->set_stream(Ref<AudioStream>());
		}
	}
	snd_wind = Ref<AudioStream>();
	snd_wind_snow = Ref<AudioStreamWAV>();
	snd_rain = Ref<AudioStream>();
	snd_chirp = Ref<AudioStreamWAV>();
	snd_birds = Ref<AudioStream>();
	snd_water = Ref<AudioStream>();
	snd_hearth = Ref<AudioStream>();
	snd_bgm = Ref<AudioStream>();
	OS::get_singleton()->delay_usec(50000); // 给音频线程留清扫窗口
}

// 增益写入 + 停混音省电：低于 -55dB 暂停、高于 -50dB 恢复（带迟滞防抖动）。
void Ambience::apply_gain(AudioStreamPlayer *p, float gain, bool &paused) {
	// 线性增益转 dB；接近 0 时钳到 -60，防 linear_to_db(0) 出 -inf
	float db = (gain > 0.0005f) ? Math::linear_to_db(gain) : -60.0f;
	p->set_volume_db(db);
	if (db < -55.0f && !paused) {
		paused = true;
		p->set_stream_paused(true);
	} else if (db > -50.0f && paused) {
		paused = false;
		p->set_stream_paused(false);
	}
}

// 风声（22050Hz 16-bit 单声道 5s 无缝循环）：
// 白噪声过一阶低通（y += (x-y)*alpha），alpha 随"阵风因子"g 缓慢随机游走
// （向中值回拉防漂死），幅度同 g 起伏 → 阵风"呼——呼——"。
// muffled=true 为雪天变体：alpha 小一个量级（更闷）、游走更慢（雪风更平缓）。
// 无缝循环：主段 n 样本后用同一滤波器状态续写 0.5s"尾巴"，开头 0.5s 与
// 尾巴做等功率交叉淡化（w=i/X），滤波状态在循环点连续，无爆音。
Ref<AudioStreamWAV> Ambience::make_wind(bool muffled) {
	const int sr = 22050;
	const float dur = 5.0f;
	const int n = (int)(dur * (float)sr);
	const int x = (int)(0.5f * (float)sr); // 首尾交叉淡化长度

	std::mt19937 rng(muffled ? 4021u : 1337u);
	std::uniform_real_distribution<float> d(-1.0f, 1.0f);

	const float step = muffled ? 0.0008f : 0.0015f; // 阵风随机游走步长
	const float a_lo = muffled ? 0.012f : 0.045f;   // 低通系数下限（更小=更闷）
	const float a_hi = muffled ? 0.065f : 0.320f;
	const float out_gain = muffled ? 1.15f : 0.70f; // 闷音色高频损失大，补增益

	float *mainf = new float[n];
	float *tail = new float[x];

	float y = 0.0f;  // 滤波器状态（跨主段与尾巴保持连续）
	float g = 0.5f;  // 阵风因子 0..1
	for (int i = 0; i < n + x; ++i) {
		g += d(rng) * step + (0.5f - g) * 0.0004f; // 随机游走 + 向中值回拉
		g = Math::clamp(g, 0.0f, 1.0f);
		float alpha = a_lo + (a_hi - a_lo) * g;
		y += (d(rng) - y) * alpha; // 一阶低通
		float s = y * (0.30f + 0.65f * g) * out_gain;
		if (i < n) {
			mainf[i] = s;
		} else {
			tail[i - n] = s;
		}
	}

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; ++i) {
		float s = mainf[i];
		if (i < x) {
			float u = (float)i / (float)x; // 等功率交叉淡化：开头混入尾巴
			s = tail[i] * (1.0f - u) + mainf[i] * u;
		}
		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF); // 小端 16-bit
	}
	delete[] mainf;
	delete[] tail;

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
	wav->set_loop_begin(0);
	wav->set_loop_end(n);
	return wav;
}

// 雨声（22050Hz 16-bit 单声道 4.0s 无缝循环）：
// 彻底告别金属铁棚敲击感！采用双层自然户外雨幕模型：
// 1. 宽频柔和雨幕（两极温润低通 1100Hz，慢速微阵风起伏，如湖畔林梢轻雨）
// 2. 润泽雨滴打叶/落水声（650~1250Hz 柔和圆润水滴微粒，5ms 软起音，彻底消除金属敲击铁皮感）
// 首尾 0.5s 等功率交叉淡化，整周无缝循环。
Ref<AudioStreamWAV> Ambience::make_rain() {
	const int sr = 22050;
	const float dur = 4.0f;
	const int n = (int)(dur * (float)sr);
	const int x = (int)(0.5f * (float)sr);

	std::mt19937 rng(909u);
	std::uniform_real_distribution<float> d(-1.0f, 1.0f);
	std::uniform_real_distribution<float> u(0.0f, 1.0f);

	float *buf = new float[n + x]();

	// 1. 雨幕基底：连续双极低通粉化滤波
	float lp1 = 0.0f, lp2 = 0.0f;
	for (int i = 0; i < n + x; ++i) {
		float t = (float)i / (float)sr;
		float raw = d(rng);
		lp1 += 0.26f * (raw - lp1);
		lp2 += 0.26f * (lp1 - lp2);
		float gust = 0.85f + 0.15f * std::sin(6.2831853f * 0.3f * t);
		buf[i] = lp2 * gust * 1.5f;
	}

	// 2. 润泽雨滴打湿地面与林梢微粒（用平滑带通噪声水滴微粒，彻底消除纯正弦波金属铁棚敲击感）
	const int drops = 50;
	for (int dp = 0; dp < drops; ++dp) {
		int start = (int)(u(rng) * (float)dur * (float)sr);
		float amp = 0.12f + u(rng) * 0.16f;
		float len = 0.020f + u(rng) * 0.025f;
		int plen = (int)(len * (float)sr);
		float d_lp = 0.0f;
		for (int k = 0; k < plen; ++k) {
			float dt = (float)k / (float)sr;
			float raw_d = d(rng);
			d_lp += 0.20f * (raw_d - d_lp);
			float env = Math::clamp(dt / 0.003f, 0.0f, 1.0f) * std::exp(-dt * 60.0f);
			float drop = (raw_d - d_lp) * env * amp;
			int idx1 = start + k;
			if (idx1 < n + x) {
				buf[idx1] += drop;
			}
			// 循环回卷
			if (start + k >= n) {
				int idx2 = (start + k) - n;
				if (idx2 < x) {
					buf[idx2] += drop;
				}
			}
		}
	}

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	for (int i = 0; i < n; ++i) {
		float s = buf[i];
		if (i < x) {
			float u_fade = (float)i / (float)x;
			s = buf[n + i] * (1.0f - u_fade) + buf[i] * u_fade;
		}
		s *= 0.50f; // 整体增益
		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}
	delete[] buf;

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
	wav->set_loop_begin(0);
	wav->set_loop_end(n);
	return wav;
}

// 蟋蟀短颤音（22050Hz 16-bit 单声道 0.3s，一次性不循环）：
// 4500Hz 正弦载波（运行期 set_pitch_scale 0.92~1.08 → 4140~4860Hz）× 30Hz
// 正弦幅度调制（平方让脉冲颗粒更分明，~9 声"唧"）× 10ms 起收门限防爆音。
// 首尾样本包络为 0，天然无爆音。
Ref<AudioStreamWAV> Ambience::make_chirp() {
	const int sr = 22050;
	const float dur = 0.3f;
	const int n = (int)(dur * (float)sr);
	const float gate = 0.01f; // 起收各 10ms

	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();

	float phase = 0.0f;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / (float)sr;
		float env = Math::clamp(Math::min(t / gate, (dur - t) / gate), 0.0f, 1.0f);
		float am = 0.5f + 0.5f * std::sin(6.2831853f * 30.0f * t);
		am *= am;
		phase += 6.2831853f * 4500.0f / (float)sr;
		float s = (std::sin(phase) + 0.15f * std::sin(2.0f * phase)) * am * env * 0.40f;
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
	// 不设 loop：蟋蟀是一声一声的短音，由 PROCESS 定时触发 play
	return wav;
}

// 湖水拍岸程序化合成（22050Hz 16-bit 单声道 5.5s 无缝循环）：
// 当 water_lap.mp3 不存在时的自动兜底，确保靠近湖水时始终有拍岸声。
//
// 声学模型（三层叠加，模拟真实湖泊浅水区浪涌拍岸）：
//   1. 涌浪基底：极低频 (~0.55Hz) 缓慢起伏的柔化白噪声，模拟大体量水面
//      的整体呼吸感（水体在岸边的升降推拉）。三极低通 550Hz 截止，
//      幅度随正弦包络起伏——涌浪推上来时声浪渐响，退去时渐轻。
//   2. 破碎浪花：每个涌浪周期的前沿叠加一小段高频碎沫声（单极低通
//      2200Hz，包络快起慢收），模拟浪头翻卷破碎成白沫拍上沙滩的"哗——"声。
//   3. 细碎回流：极轻的高频沙沙声（带通 3000~5500Hz 近似），持续低音量
//      铺底，模拟湖水退回时沙砾/卵石间的细碎水声。
//
// 首尾 0.8s 等功率交叉淡化，滤波器状态在循环点连续，无爆音。
Ref<AudioStreamWAV> Ambience::make_water_lap() {
	const int sr = 22050;
	const float dur = 5.5f;
	const int n = (int)(dur * (float)sr);
	const int x = (int)(0.8f * (float)sr); // 首尾交叉淡化长度

	std::mt19937 rng(7788u);
	std::uniform_real_distribution<float> d(-1.0f, 1.0f);

	float *buf = new float[n + x]();

	// 1. 涌浪基底（三极低通 550Hz 柔化白噪声 × 正弦包络起伏）
	float lp1 = 0.0f, lp2 = 0.0f, lp3 = 0.0f;
	const float alpha_base = 550.0f / (float)sr * 6.2831853f; // 三极低通系数近似
	for (int i = 0; i < n + x; ++i) {
		float t = (float)i / (float)sr;
		float raw = d(rng);
		lp1 += alpha_base * (raw - lp1);
		lp2 += alpha_base * (lp1 - lp2);
		lp3 += alpha_base * (lp2 - lp3);
		// 涌浪起伏包络：0.55Hz 主节拍 + 0.23Hz 慢调制
		float surge = 0.55f + 0.45f * std::sin(6.2831853f * 0.55f * t)
				* (0.7f + 0.3f * std::sin(6.2831853f * 0.23f * t + 1.2f));
		buf[i] += lp3 * surge * 1.8f;
	}

	// 2. 破碎浪花（高频碎沫，快起慢收包络，每个涌浪周期前沿触发）
	const float wave_period = 1.0f / 0.55f; // ~1.82s
	const int num_waves = (int)(dur / wave_period) + 2;
	float bp_state = 0.0f;
	for (int w = 0; w < num_waves; ++w) {
		int start = (int)(((float)w * wave_period + 0.1f) * (float)sr);
		int burst_len = (int)(0.65f * (float)sr);
		for (int k = 0; k < burst_len && start + k < n + x; ++k) {
			float dt = (float)k / (float)sr;
			float raw = d(rng);
			bp_state += 0.52f * (raw - bp_state); // 单极低通 ~2200Hz
			// 快起慢收包络
			float env = Math::clamp(dt / 0.035f, 0.0f, 1.0f) * std::exp(-dt * 4.5f);
			buf[start + k] += bp_state * env * 0.75f;
		}
	}

	// 3. 细碎回流铺底（轻柔高频沙沙声）
	float hp_state = 0.0f;
	for (int i = 0; i < n + x; ++i) {
		float raw = d(rng);
		hp_state += 0.38f * (raw - hp_state);
		float hi = raw - hp_state; // 简易高通
		buf[i] += hi * 0.08f;
	}

	// 编码 + 首尾交叉淡化
	PackedByteArray bytes;
	bytes.resize(n * 2);
	uint8_t *w = bytes.ptrw();
	for (int i = 0; i < n; ++i) {
		float s = buf[i];
		if (i < x) {
			float u = (float)i / (float)x;
			s = buf[n + i] * (1.0f - u) + buf[i] * u;
		}
		s *= 0.55f; // 整体增益
		int v = (int)std::lround(s * 32767.0f);
		v = Math::clamp(v, -32768, 32767);
		w[i * 2] = (uint8_t)(v & 0xFF);
		w[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
	}
	delete[] buf;

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(sr);
	wav->set_stereo(false);
	wav->set_data(bytes);
	wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
	wav->set_loop_begin(0);
	wav->set_loop_end(n);
	return wav;
}
