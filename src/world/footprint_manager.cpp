#include "footprint_manager.h"

#include <cmath>
#include <algorithm>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/math.hpp>

#include "sky_cycle.h"

namespace godot {

FootprintManager *FootprintManager::singleton = nullptr;

void FootprintManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("spawn_footprint", "type", "pos", "yaw_rad", "scale", "surface"), &FootprintManager::spawn_footprint, DEFVAL(1.0f), DEFVAL(-1));

	BIND_ENUM_CONSTANT(TYPE_HUMAN_LEFT);
	BIND_ENUM_CONSTANT(TYPE_HUMAN_RIGHT);
	BIND_ENUM_CONSTANT(TYPE_HOOF);
	BIND_ENUM_CONSTANT(TYPE_PAW);
	BIND_ENUM_CONSTANT(TYPE_RABBIT);
	BIND_ENUM_CONSTANT(TYPE_MAX);
}

FootprintManager::FootprintManager() {
	if (singleton == nullptr) {
		singleton = this;
	}
}

FootprintManager::~FootprintManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void FootprintManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			set_process(true);
			build_textures();
			build_pool();
		} break;
		default:
			break;
	}
}

void FootprintManager::build_textures() {
	const int W = 48;
	const int H = 48;

	for (int t = 0; t < TYPE_MAX; ++t) {
		Ref<Image> img = Image::create(W, H, false, Image::FORMAT_RGBA8);
		img->fill(Color(1.0f, 1.0f, 1.0f, 0.0f));

		for (int y = 0; y < H; ++y) {
			float v = (float)(y + 0.5f) / (float)H;
			float ny = (v - 0.5f) * 2.0f; // -1（后跟）~ +1（足尖）
			for (int x = 0; x < W; ++x) {
				float u = (float)(x + 0.5f) / (float)W;
				float nx = (u - 0.5f) * 2.0f; // -1（左）~ +1（右）

				float alpha = 0.0f;

				if (t == TYPE_HUMAN_LEFT || t == TYPE_HUMAN_RIGHT) {
					// 人类靴底印：后跟圆垫 + 前掌椭圆大掌 + 足弓内凹 + 前掌防滑防滑横槽
					float sx = (t == TYPE_HUMAN_RIGHT) ? nx : -nx; // 右脚内凹在左，左脚内凹在右
					float d_heel = std::hypot(sx, ny - (-0.55f)) - 0.28f;
					float d_fore = std::hypot((sx - 0.04f) / 0.35f, (ny - 0.24f) / 0.42f) - 1.0f;
					bool inside = (d_heel <= 0.0f) || (d_fore <= 0.0f);
					if (ny > -0.38f && ny < -0.02f && sx < -0.04f) {
						inside = false; // 足弓镂空
					}
					if (inside) {
						float edge = std::min(std::abs(d_heel), std::abs(d_fore));
						alpha = Math::clamp(edge * 8.0f, 0.45f, 1.0f);
						// 前掌防滑横槽（靴底花纹凹凸感）
						if (ny > 0.02f && ny < 0.55f) {
							float groove = std::sin(ny * 26.0f);
							if (groove > 0.65f) {
								alpha *= 0.55f;
							}
						}
					}
				} else if (t == TYPE_HOOF) {
					// 牛羊偶蹄：左右对称双半月形瓣叶，中央留清晰缝隙
					float ax = std::abs(nx);
					if (ax >= 0.045f) {
						float dx = (ax - 0.20f) / 0.16f;
						float dy = ny / 0.38f;
						float d = std::hypot(dx, dy);
						if (ny > 0.0f) {
							d *= (1.0f + ny * 0.42f); // 前端向内收尖
						}
						if (d <= 1.0f) {
							alpha = Math::clamp((1.0f - d) * 7.0f, 0.35f, 1.0f);
						}
					}
				} else if (t == TYPE_PAW) {
					// 猫狗爪印：中央肉垫 + 前方 4 个弧形排列的指节肉豆
					float d_main = std::hypot(nx / 0.33f, (ny + 0.16f) / 0.26f) - 1.0f;
					const float toes[4][3] = {
						{-0.24f, 0.24f, 0.12f},
						{-0.09f, 0.42f, 0.13f},
						{0.09f, 0.42f, 0.13f},
						{0.24f, 0.24f, 0.12f}
					};
					float d_toe_min = 999.0f;
					for (int i = 0; i < 4; ++i) {
						float td = std::hypot(nx - toes[i][0], ny - toes[i][1]) - toes[i][2];
						if (td < d_toe_min) {
							d_toe_min = td;
						}
					}
					float d_pad = std::min(d_main, d_toe_min);
					if (d_pad <= 0.0f) {
						alpha = Math::clamp((-d_pad) * 7.0f, 0.35f, 1.0f);
					}
				} else if (t == TYPE_RABBIT) {
					// 野兔跳痕：两个圆圆前爪并在后，两个长条后爪并在前
					float d_f1 = std::hypot(nx - (-0.12f), ny - (-0.42f)) - 0.09f;
					float d_f2 = std::hypot(nx - 0.12f, ny - (-0.42f)) - 0.09f;
					float d_h1 = std::hypot((nx - (-0.20f)) / 0.10f, (ny - 0.18f) / 0.30f) - 1.0f;
					float d_h2 = std::hypot((nx - 0.20f) / 0.10f, (ny - 0.18f) / 0.30f) - 1.0f;
					float d_rab = std::min(std::min(d_f1, d_f2), std::min(d_h1, d_h2));
					if (d_rab <= 0.0f) {
						alpha = Math::clamp((-d_rab) * 6.5f, 0.35f, 1.0f);
					}
				}

				if (alpha > 0.001f) {
					img->set_pixel(x, y, Color(1.0f, 1.0f, 1.0f, alpha));
				}
			}
		}

		textures[t] = ImageTexture::create_from_image(img);
	}
}

void FootprintManager::build_pool() {
	for (int i = 0; i < POOL_SIZE; ++i) {
		Decal *d = memnew(Decal);
		d->set_name(vformat("Footprint_%d", i));
		d->set_cull_mask(1); // 仅投射在场景 Layer 1（地面/石拱/桥板），绝不投射在角色/动物本身
		d->set_upper_fade(0.25f);
		d->set_lower_fade(0.25f);
		d->set_normal_fade(0.45f); // 陡峭立面不留印
		d->set_enable_distance_fade(true);
		d->set_distance_fade_begin(40.0f);
		d->set_distance_fade_length(15.0f);
		d->set_size(Vector3(0.28f, 0.70f, 0.42f));
		d->set_visible(false);
		add_child(d);

		pool[i].decal = d;
		pool[i].active = false;
		pool[i].age = 0.0f;
	}
}

void FootprintManager::_process(double delta) {
	for (int i = 0; i < POOL_SIZE; ++i) {
		Slot &s = pool[i];
		if (!s.active || s.decal == nullptr) {
			continue;
		}
		s.age += (float)delta;
		if (s.age >= s.max_age) {
			s.decal->set_visible(false);
			s.active = false;
			continue;
		}
		// 渐隐阶段：前 60% 保持清晰可见，后 40% 柔和淡出消散
		float fade_start = s.max_age * 0.60f;
		if (s.age > fade_start) {
			float fade_prog = (s.age - fade_start) / (s.max_age - fade_start);
			float alpha_k = Math::clamp(1.0f - fade_prog, 0.0f, 1.0f);
			Color c = s.base_color;
			c.a *= alpha_k;
			s.decal->set_modulate(c);
		}
	}
}

void FootprintManager::spawn_footprint(FootprintType type, const Vector3 &pos, float yaw_rad, float scale, int surface) {
	// 湖水以下或悬崖深坑不留脚印
	if (pos.y < -0.45f) {
		return;
	}
	if (type < 0 || type >= TYPE_MAX || !textures[type].is_valid()) {
		return;
	}

	Slot &s = pool[next_slot];
	next_slot = (next_slot + 1) % POOL_SIZE;

	if (s.decal == nullptr) {
		return;
	}

	// 读取当前天气
	float snow = 0.0f;
	float rain = 0.0f;
	if (get_parent() != nullptr) {
		SkyCycle *sc = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
		if (sc != nullptr) {
			snow = sc->get_snow_amount();
			rain = sc->get_rain_amount();
		}
	}

	// 印痕颜色与存留寿命：先看脚下是什么地面，再看天气。
	// 硬质地面（石板/木地板）踩不出泥印，只留浅灰擦痕——不区分的话
	// 走在石桥上会印出一串泥脚印，很出戏。
	const bool snowy = snow > 0.15f;
	const bool rainy = rain > 0.20f;
	if (surface == 3) {
		// 石桥石板：干燥石面擦痕（薄薄一层灰），雪天则是压实的雪印
		if (snowy) {
			s.base_color = Color(0.52f, 0.57f, 0.68f, 0.80f);
			s.max_age = 30.0f;
		} else {
			s.base_color = Color(0.58f, 0.57f, 0.54f, 0.42f);
			s.max_age = 11.0f;
		}
	} else if (surface == 4) {
		// 木地板：浅木色磨痕
		s.base_color = Color(0.46f, 0.34f, 0.22f, 0.40f);
		s.max_age = 13.0f;
	} else if (surface == 5) {
		// 球场草坪：草被踩倒压出的深绿痕
		s.base_color = Color(0.15f, 0.27f, 0.11f, 0.46f);
		s.max_age = 15.0f;
	} else if (snowy) {
		// 雪地印痕：踩踏压实后的冷灰冰晶阴影色，极富立体下凹感
		s.base_color = Color(0.40f, 0.46f, 0.60f, 0.92f);
		s.max_age = 35.0f; // 雪地脚印保存持久
	} else if (rainy) {
		// 雨天湿泥印痕：深润泥水浸润色
		s.base_color = Color(0.17f, 0.13f, 0.09f, 0.78f);
		s.max_age = 22.0f;
	} else {
		// 干燥荒草/泥土路面：土壤夯实暗痕（比旧版更深，否则浅色沙地上看不见）
		s.base_color = Color(0.26f, 0.21f, 0.15f, 0.58f);
		s.max_age = 18.0f;
	}

	// 尺寸适配（人脚印 32cm 宽 × 48cm 长——旧尺寸在草地上几乎看不出）
	Vector3 sz;
	if (type == TYPE_HUMAN_LEFT || type == TYPE_HUMAN_RIGHT) {
		sz = Vector3(0.32f * scale, 0.70f, 0.48f * scale);
	} else if (type == TYPE_HOOF) {
		sz = Vector3(0.29f * scale, 0.70f, 0.34f * scale);
	} else if (type == TYPE_PAW) {
		sz = Vector3(0.21f * scale, 0.50f, 0.23f * scale);
	} else { // TYPE_RABBIT
		sz = Vector3(0.25f * scale, 0.50f, 0.32f * scale);
	}

	s.decal->set_texture(Decal::TEXTURE_ALBEDO, textures[type]);
	s.decal->set_size(sz);
	// 贴花中心略微上浮 0.15m，投影高度 0.7m 可完美包裹 ±0.35m 范围的地形坡度
	s.decal->set_global_position(Vector3(pos.x, pos.y + 0.15f, pos.z));
	s.decal->set_global_rotation(Vector3(0.0f, yaw_rad, 0.0f));
	s.decal->set_modulate(s.base_color);
	s.decal->set_visible(true);

	s.age = 0.0f;
	s.active = true;
}

} // namespace godot
