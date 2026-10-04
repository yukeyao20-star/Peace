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
	BIND_ENUM_CONSTANT(TYPE_ROLL_TRACK);
	BIND_ENUM_CONSTANT(TYPE_HUMAN_TRACK);
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
	const int W = 128;
	const int H = 128;

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
					// 真实户外登山靴底印：
					// 1. 后跟独立大底垫 (ny ∈ [-0.85, -0.42])
					// 2. 前掌人体工学大底 (ny ∈ [-0.08, 0.85])
					// 3. 足弓内凹腰线 (ny ∈ [-0.42, -0.08])
					// 4. 清晰防滑齿花纹（齿槽沟壑深邃、边缘锐利自然）
					float sx = (t == TYPE_HUMAN_RIGHT) ? nx : -nx; // 左右脚镜像

					// 后跟轮廓（圆角方形）
					float dx_heel = std::abs(sx) / 0.28f;
					float dy_heel = std::abs(ny - (-0.62f)) / 0.22f;
					float d_heel = std::hypot(std::max(0.0f, dx_heel - 0.35f), std::max(0.0f, dy_heel - 0.35f));
					bool inside_heel = (d_heel <= 0.65f);

					// 前掌轮廓（前宽后窄，拇指侧微突）
					float fore_w = 0.34f + (0.80f - ny) * 0.08f;
					float dx_fore = (sx - 0.03f) / fore_w;
					float dy_fore = (ny - 0.35f) / 0.48f;
					float d_fore = std::hypot(dx_fore, dy_fore);
					bool inside_fore = (d_fore <= 1.0f);

					// 足弓处连接（只有外侧窄条着地，内侧完全悬空）
					bool inside_arch = false;
					if (ny >= -0.42f && ny <= -0.08f) {
						if (sx > 0.05f && sx < 0.24f) {
							inside_arch = true;
						}
					}

					if (inside_heel || inside_fore || inside_arch) {
						alpha = 0.95f;

						// 鞋底防滑齿凹凸花纹（凹槽处镂空或大幅淡化）
						if (inside_fore) {
							// 前掌横向深向防滑槽（间距均匀）
							float lug_y = std::fmod(ny + 1.0f, 0.12f);
							if (lug_y < 0.035f) {
								alpha *= 0.15f; // 齿槽凹陷
							}
							// 前掌中轴导水防滑纵槽
							if (std::abs(sx - 0.02f) < 0.03f && ny > 0.05f && ny < 0.65f) {
								alpha *= 0.20f;
							}
						}
						if (inside_heel) {
							// 后跟 U 形马蹄铁防滑块
							float r_h = std::hypot(sx / 0.22f, (ny - (-0.62f)) / 0.16f);
							if (r_h > 0.35f && r_h < 0.60f && ny < -0.55f) {
								alpha *= 0.25f;
							}
						}

						// 边缘柔和抗锯齿抗失真
						if (inside_fore) {
							alpha *= Math::clamp((1.0f - d_fore) * 12.0f, 0.0f, 1.0f);
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
				} else if (t == TYPE_ROLL_TRACK) {
					// 火腿肠/人体横向滚过压出的圆柱形宽体辗痕：
					// 宽条带 (sx ∈ [-0.85, 0.85], ny ∈ [-0.45, 0.45])，两头圆角收口，中间压实微凹
					float dx_roll = std::abs(nx) / 0.82f;
					float dy_roll = std::abs(ny) / 0.40f;
					float d_roll = std::hypot(std::max(0.0f, dx_roll - 0.25f), std::max(0.0f, dy_roll - 0.15f));
					if (d_roll <= 0.80f) {
						// 边缘柔和衰减，中间平滑压实
						float edge = (0.80f - d_roll) / 0.25f;
						alpha = Math::clamp(edge, 0.0f, 1.0f) * 0.92f;
						// 圆柱体两端环状微浅凹陷
						if (std::abs(nx) > 0.65f) {
							alpha *= (1.0f - (std::abs(nx) - 0.65f) / 0.25f);
						}
					}
				} else if (t == TYPE_HUMAN_TRACK) {
					// 用户的角色是胶囊体小人（没有脚，不应有靴子脚印）：
					// 移动时在地面留下浅浅的一道柔和圆润微弱压痕（单一自然滑痕）
					float dx_h = nx / 0.38f;
					float dy_h = ny / 0.38f;
					float d_h = std::hypot(dx_h, dy_h);
					if (d_h <= 1.0f) {
						// 中心饱满，边缘极柔和虚化，绝无鞋底齿纹/鞋跟
						float falloff = std::cos(d_h * 1.570796f);
						alpha = Math::clamp(falloff * 0.65f, 0.0f, 0.65f);
					}
				} else if (t == TYPE_SKATE_LEFT || t == TYPE_SKATE_RIGHT) {
					// 真实滑冰左一下右一下的单侧冰刀切痕：
					// 滑冰时单脚向外后侧蹬冰发力，刀刃切出一道微带内收圆滑弧度（S-curve/弧形切槽）的清脆细锐刀痕
					// 左脚向左外侧弧弯，右脚向右外侧弧弯
					float side_dir = (t == TYPE_SKATE_RIGHT) ? 1.0f : -1.0f;
					// 沿 ny（滑行纵向，-1到+1）产生自然的向外侧弧线偏移
					float curve_offset = side_dir * (0.18f * (1.0f - ny * ny) + 0.12f * ny);
					float d_blade = std::abs(nx - curve_offset);

					// 1. 冰刀极细钢刃切割核心痕（窄缝高亮，切削坚实）
					if (d_blade < 0.052f && std::abs(ny) < 0.90f) {
						float core = 1.0f - d_blade / 0.052f;
						float len_fade = 1.0f - std::pow(std::abs(ny) / 0.90f, 4.0f);
						alpha = core * len_fade * 0.96f;
					}
					// 2. 蹬冰刀刃外侧铲飞出的白霜与细微冰屑雾痕（外侧多，内侧少）
					float outer_dx = (nx - curve_offset) * side_dir;
					if (outer_dx > -0.04f && outer_dx < 0.18f && std::abs(ny) < 0.82f) {
						float frost = (1.0f - std::max(0.0f, outer_dx) / 0.18f) * 0.40f;
						float len_fade = 1.0f - std::abs(ny) / 0.82f;
						alpha = std::max(alpha, frost * len_fade);
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
	// 湖水深坑不留脚印（冰面标高在 y ≈ -0.55m，允许冰面划痕与落脚印）
	if (pos.y < -0.65f) {
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
	if (surface == 7 || type == TYPE_SKATE_LEFT || type == TYPE_SKATE_RIGHT) {
		// 冰面划痕：晶莹剔透的高亮冰晶白微带浅青冷辉（冰刀刮破深色冰体后翻出的白色微晶冰屑）
		s.base_color = Color(0.92f, 0.96f, 1.0f, 0.88f);
		s.max_age = 25.0f; // 冰痕保存 25 秒
	} else if (surface == 3) {
		// 石桥石板：干燥石面极微擦痕
		if (snowy) {
			s.base_color = Color(0.48f, 0.53f, 0.65f, 0.85f);
			s.max_age = 30.0f;
		} else {
			s.base_color = Color(0.58f, 0.57f, 0.54f, 0.22f); // 大幅淡化，轻微灰痕
			s.max_age = 7.0f;
		}
	} else if (surface == 4) {
		// 木地板：极微木色磨痕
		s.base_color = Color(0.46f, 0.34f, 0.22f, 0.25f);
		s.max_age = 8.0f;
	} else if (surface == 5) {
		// 球场草坪：草被压倒的自然微弱色差
		s.base_color = Color(0.12f, 0.22f, 0.08f, 0.32f);
		s.max_age = 12.0f;
	} else if (snowy) {
		// 雪地印痕：踩踏压实后的冷灰冰晶立体阴影色，极富下凹质感
		s.base_color = Color(0.38f, 0.44f, 0.58f, 0.95f);
		s.max_age = 35.0f; // 雪地脚印保存持久
	} else if (rainy) {
		// 雨天湿泥印痕：深润泥水浸润色
		s.base_color = Color(0.16f, 0.12f, 0.08f, 0.70f);
		s.max_age = 22.0f;
	} else {
		// 干燥普通路面：用户反馈痕迹太重像污渍，调整为极轻柔的浅淡土色，快散不刺眼
		s.base_color = Color(0.24f, 0.20f, 0.15f, 0.28f);
		s.max_age = 9.0f; // 9秒内快速自然消散
	}

	// 尺寸适配（人脚印 32cm 宽 × 48cm 长——旧尺寸在草地上几乎看不出）
	Vector3 sz;
	if (type == TYPE_SKATE_LEFT || type == TYPE_SKATE_RIGHT) {
		// 单脚冰刀潇洒长弧划痕：宽 0.38m x 超长滑行切痕 1.85m
		sz = Vector3(0.38f * scale, 0.70f, 1.85f * scale);
	} else if (type == TYPE_HUMAN_LEFT || type == TYPE_HUMAN_RIGHT) {
		sz = Vector3(0.32f * scale, 0.70f, 0.48f * scale);
	} else if (type == TYPE_HOOF) {
		sz = Vector3(0.29f * scale, 0.70f, 0.34f * scale);
	} else if (type == TYPE_PAW) {
		sz = Vector3(0.21f * scale, 0.50f, 0.23f * scale);
	} else if (type == TYPE_ROLL_TRACK) {
		// 横向身体辗痕：横宽 1.4m x 纵向 0.55m（紧随圆柱滚动方向）
		sz = Vector3(1.40f * scale, 0.65f, 0.55f * scale);
	} else if (type == TYPE_HUMAN_TRACK) {
		// 无脚小人浅浅的一道单一平滑微痕：宽 0.38m x 长 0.42m
		sz = Vector3(0.38f * scale, 0.60f, 0.42f * scale);
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
