#pragma once

// 统一 UI 设计规范 ——「湖畔玻璃」设计语言（与地图 UI 同源定稿）。
//
// 顶栏时钟 / 每日待办 / 底部提示 / 钓鱼 HUD / 暂停菜单 等一切 HUD 面板
// 一律从这里取样式令牌，禁止各写各的底色圆角——那是风格漂移的开始。
//
//   底板：深色半透明玻璃 rgba(0.08,0.07,0.06,~0.5)，圆角 8~18，1px 淡奶油描边
//   文字：奶油白 (1.0,0.97,0.88) / 柔白 (0.95,0.95,0.92)，Noto Sans SC
//   描边：墨色 (0.05,0.05,0.08)，6~8px
//   强调：暖金 (1.0,0.92,0.55)（结算/达成），警示暖红 (0.85,0.40,0.30)

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/style_box_texture.hpp>
#include <godot_cpp/classes/system_font.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>

#include <cmath>
#include <godot_cpp/variant/color.hpp>

namespace godot {
namespace uistyle {

inline Color text_cream() { return Color(1.0f, 0.97f, 0.88f); }
inline Color text_soft() { return Color(0.95f, 0.95f, 0.92f); }
inline Color outline_ink() { return Color(0.05f, 0.05f, 0.08f); }
inline Color accent_gold() { return Color(1.0f, 0.92f, 0.55f); }
inline Color chalk_gold() { return Color(0.95f, 0.87f, 0.58f); }
inline Color ink_brown() { return Color(0.30f, 0.23f, 0.16f); }
inline Color paper_cream() { return Color(0.94f, 0.90f, 0.81f); }

// 玻璃底板样式：radius 圆角，alpha 不透明度（0.45 轻 / 0.55 常规 / 0.75 条托槽）
inline Ref<StyleBoxFlat> make_glass(float p_radius = 14.0f, float p_alpha = 0.55f) {
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(0.08f, 0.07f, 0.06f, p_alpha));
	sb->set_corner_radius_all((int)p_radius);
	sb->set_border_color(Color(1.0f, 0.97f, 0.88f, 0.10f)); // 一根极淡的奶油发丝边
	sb->set_border_width_all(1);
	sb->set_content_margin_all(Math::max(6.0f, p_radius * 0.5f));
	return sb;
}

// —— 木质令牌（与木屋/篱笆/菜园木牌同一材质语言）——
inline Color wood_light() { return Color(0.62f, 0.47f, 0.30f); } // 浅松木
inline Color wood_mid() { return Color(0.45f, 0.33f, 0.20f); }   // 中原木（门框同款）
inline Color wood_dark() { return Color(0.28f, 0.20f, 0.12f); }  // 深胡桃（描边）
inline Color wood_text() { return Color(0.97f, 0.93f, 0.84f); }  // 木刻奶油字

// 悬浮木牌底板：原木底+深胡桃厚边+板缝高光（上亮下暗的倒角错觉）。
// 用双 StyleBox 叠不了，靠 border + shadow 模拟木板厚度。
inline Ref<StyleBoxFlat> make_wood(float p_radius = 14.0f, float p_alpha = 0.97f) {
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(wood_mid().r, wood_mid().g, wood_mid().b, p_alpha));
	sb->set_corner_radius_all((int)p_radius);
	sb->set_border_color(Color(wood_dark().r, wood_dark().g, wood_dark().b, 0.95f));
	sb->set_border_width_all(3);
	sb->set_shadow_color(Color(0.06f, 0.04f, 0.02f, 0.45f));
	sb->set_shadow_size(14);
	sb->set_shadow_offset(Vector2(0, 6));
	sb->set_content_margin_all(Math::max(8.0f, p_radius * 0.6f));
	return sb;
}

// 统一文字妆造：字号 + 颜色 + Noto 中文字体 + 墨色描边
inline void dress_label(Label *p_label, int p_font_size, const Color &p_color,
		const Ref<Font> &p_font = Ref<Font>(), int p_outline = 6) {
	if (p_label == nullptr) {
		return;
	}
	if (p_font.is_valid()) {
		p_label->add_theme_font_override("font", p_font);
	}
	p_label->add_theme_font_size_override("font_size", p_font_size);
	p_label->add_theme_color_override("font_color", p_color);
	p_label->add_theme_color_override("font_outline_color", outline_ink());
	p_label->add_theme_constant_override("outline_size", p_outline);
}


// —— 小黑板令牌（按键提示卡）——
inline Color chalk_white() { return Color(0.93f, 0.96f, 0.91f); }
inline Color board_green() { return Color(0.09f, 0.15f, 0.11f); }

// 小黑板：墨绿底 + 原木边框 + 轻微投影；粉笔字用 dress_chalk_label
inline Ref<StyleBoxFlat> make_blackboard(float p_radius = 8.0f, float p_alpha = 0.96f) {
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(board_green().r, board_green().g, board_green().b, p_alpha));
	sb->set_corner_radius_all((int)p_radius);
	sb->set_border_color(Color(wood_mid().r, wood_mid().g, wood_mid().b, 0.96f));
	sb->set_border_width_all(5);
	sb->set_shadow_color(Color(0.05f, 0.03f, 0.02f, 0.4f));
	sb->set_shadow_size(10);
	sb->set_shadow_offset(Vector2(0, 4));
	sb->set_content_margin_all(Math::max(10.0f, p_radius * 0.9f));
	return sb;
}


// —— 程序化材质纹理（真木纹 / 黑板粉笔灰）——
// 运行期生成一次 128×128 贴图并缓存；StyleBoxTexture 九宫格拉伸，边框保持锐利。
inline float ui_hash01(int x, int y) {
	unsigned h = (unsigned)(x * 374761393 + y * 668265263);
	h = (h ^ (h >> 13)) * 1274126177u;
	return (float)((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}
inline float ui_vnoise(float x, float y) {
	int xi = (int)std::floor(x), yi = (int)std::floor(y);
	float fx = x - xi, fy = y - yi;
	float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy);
	float a = ui_hash01(xi, yi), b = ui_hash01(xi + 1, yi);
	float c = ui_hash01(xi, yi + 1), d = ui_hash01(xi + 1, yi + 1);
	return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy;
}

inline Ref<ImageTexture> wood_texture() {
	const int S = 128;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	const Color mid = wood_mid();
	const Color dark = wood_dark();
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			// 纵向木纹：低频弯曲 + 高频细纹 + 偶发板缝
			float warp = (ui_vnoise((float)x * 0.06f, (float)y * 0.012f) - 0.5f) * 7.0f;
			float band = std::sin((float)x * 0.42f + warp + (float)y * 0.03f);
			float grain = 0.80f + 0.15f * band
					+ 0.10f * (ui_vnoise((float)x * 0.11f, (float)y * 0.05f) - 0.5f);
			int seam = (int)(((float)y + 8.0f * std::sin((float)x * 0.05f))) % 46;
			if (seam < 2) {
				grain *= 0.72f;
			}
			Color c(mid.r * grain, mid.g * grain, mid.b * grain, 1.0f);
			int edge = std::min(std::min(x, y), std::min(S - 1 - x, S - 1 - y));
			if (edge < 10) {
				c = Color(dark.r, dark.g, dark.b, 1.0f);
				if (edge == 9) {
					c = Color(dark.r * 1.5f, dark.g * 1.38f, dark.b * 1.25f, 1.0f); // 内亮线（倒角高光）
				} else if (edge == 0) {
					c = Color(dark.r * 0.55f, dark.g * 0.55f, dark.b * 0.55f, 1.0f); // 外沿阴影
				}
			}
			img->set_pixel(x, y, c);
		}
	}
	return ImageTexture::create_from_image(img);
}

inline Ref<ImageTexture> blackboard_texture() {
	const int S = 128;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	const Color base = board_green();
	const Color mid = wood_mid();
	const Color dark = wood_dark();
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			Color c;
			int edge = std::min(std::min(x, y), std::min(S - 1 - x, S - 1 - y));
			if (edge < 10) {
				// 原木边框（复用木纹观感）
				float g = 0.82f + 0.12f * std::sin((float)x * 0.42f + (float)y * 0.03f);
				c = Color(mid.r * g, mid.g * g, mid.b * g, 1.0f);
				if (edge == 9) {
					c = Color(dark.r * 1.4f, dark.g * 1.3f, dark.b * 1.2f, 1.0f);
				} else if (edge == 0) {
					c = Color(dark.r * 0.55f, dark.g * 0.55f, dark.b * 0.55f, 1.0f);
				}
			} else {
				// 墨绿板面：粉尘噪点 + 擦痕 + 淡粉笔灰
				float n1 = ui_vnoise((float)x * 0.09f, (float)y * 0.09f);
				float n2 = ui_vnoise((float)x * 0.31f + 17.0f, (float)y * 0.31f);
				float v = 0.90f + 0.14f * n1 + 0.06f * (n2 - 0.5f);
				c = Color(base.r * v, base.g * v, base.b * v, 1.0f);
				// 擦拭痕：横向拉长的浅色雾带
				float smudge = ui_vnoise((float)x * 0.05f, (float)y * 0.35f + 41.0f);
				if (smudge > 0.62f) {
					float k = (smudge - 0.62f) * 0.22f;
					c = c.lerp(Color(0.72f, 0.78f, 0.72f, 1.0f), k);
				}
				// 细粉笔灰颗粒
				if (ui_hash01(x * 3 + 7, y * 3 + 11) > 0.985f) {
					c = c.lerp(Color(0.85f, 0.90f, 0.85f, 1.0f), 0.35f);
				}
			}
			img->set_pixel(x, y, c);
		}
	}
	return ImageTexture::create_from_image(img);
}



// 暗场晕影：中心透明、四周压暗，替代一整块平铺黑幕
inline Ref<ImageTexture> vignette_texture() {
	const int S = 256;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			float u = ((float)x + 0.5f) / S * 2.0f - 1.0f;
			float v = ((float)y + 0.5f) / S * 2.0f - 1.0f;
			float r = std::sqrt(u * u + v * v) / 1.41421356f;
			float a = Math::smoothstep(0.35f, 1.0f, r) * 0.72f;
			img->set_pixel(x, y, Color(0.035f, 0.040f, 0.032f, a));
		}
	}
	return ImageTexture::create_from_image(img);
}


inline Color deep_green() { return Color(0.16f, 0.31f, 0.21f); }
inline Color deep_green_dark() { return Color(0.08f, 0.17f, 0.11f); }


// 手绘墨线：标题下的一笔，粗细与深浅略微起伏
inline Ref<ImageTexture> ink_underline_texture() {
	const int W = 192, H = 14;
	Ref<Image> img = Image::create(W, H, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			float t = (float)x / (float)(W - 1);
			float taper = Math::clamp(Math::min(t * 12.0f, (1.0f - t) * 12.0f), 0.0f, 1.0f);
			float wave = std::sin(t * 9.0f + 0.7f) * 0.9f + ui_vnoise(t * 18.0f, 0.0f) * 0.8f;
			float cy = H * 0.5f + wave;
			float thick = 1.2f + 0.8f * ui_vnoise(t * 9.0f, 4.0f);
			float a = Math::clamp(thick - std::fabs((float)y + 0.5f - cy), 0.0f, 1.0f) * taper;
			img->set_pixel(x, y, Color(0.26f, 0.19f, 0.13f, a * 0.85f));
		}
	}
	return ImageTexture::create_from_image(img);
}

// —— 实体按钮：带倒角的木牌 / 墨绿漆牌，三种状态（normal / hover / pressed）——
inline Ref<ImageTexture> button_texture(bool p_primary, int p_state) {
	const int S = 96;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	Color base = p_primary ? deep_green() : Color(0.34f, 0.23f, 0.13f, 1.0f);
	if (p_state == 1) { // hover 提亮
		base = base.lightened(0.12f);
	} else if (p_state == 2) { // pressed 压暗
		base = base.darkened(0.18f);
	}
	const Color outline = p_primary ? deep_green_dark() : Color(0.13f, 0.09f, 0.05f, 1.0f);
	const Color brass(0.70f, 0.55f, 0.30f, 1.0f);
	const float r = 7.0f;
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			// 圆角遮罩
			float cx = Math::clamp((float)x, r, (float)S - r);
			float cy = Math::clamp((float)y, r, (float)S - r);
			float d = std::sqrt(((float)x - cx) * ((float)x - cx) + ((float)y - cy) * ((float)y - cy));
			float a = Math::clamp(r - d + 0.5f, 0.0f, 1.0f);
			if (a <= 0.0f) {
				img->set_pixel(x, y, Color(0, 0, 0, 0));
				continue;
			}
			// 木纹 / 漆面细微起伏
			float grain = 0.96f + 0.05f * std::sin((float)y * 0.55f + ui_vnoise((float)x * 0.2f, (float)y * 0.05f) * 4.0f);
			Color c(base.r * grain, base.g * grain, base.b * grain, 1.0f);
			// 倒角：正常态左上亮、右下暗；按下态反过来
			bool pressed = p_state == 2;
			int edge = std::min(std::min(x, y), std::min(S - 1 - x, S - 1 - y));
			bool top_left = (x < 7) || (y < 7);
			bool bottom_right = (x >= S - 7) || (y >= S - 7);
			if (top_left && !bottom_right) {
				c = pressed ? c.darkened(0.18f) : c.lightened(0.16f);
			}
			if (bottom_right && !top_left) {
				c = pressed ? c.lightened(0.10f) : c.darkened(0.22f);
			}
			if (edge == 0) {
				c = outline;
			} else if (p_primary && edge <= 2) {
				c = c.lerp(brass, 0.55f);
			}
			img->set_pixel(x, y, c);
		}
	}
	return ImageTexture::create_from_image(img);
}

inline Ref<StyleBoxTexture> make_button_style(bool p_primary, int p_state) {
	Ref<StyleBoxTexture> sb;
	sb.instantiate();
	sb->set_texture(button_texture(p_primary, p_state));
	sb->set_texture_margin_all(18.0f);
	sb->set_content_margin(SIDE_LEFT, 20.0f);
	sb->set_content_margin(SIDE_RIGHT, 20.0f);
	sb->set_content_margin(SIDE_TOP, 9.0f);
	sb->set_content_margin(SIDE_BOTTOM, 9.0f);
	return sb;
}


// —— 装饰小图（钉子 / 叶片 / 火漆印），全部程序化，免外部资源 ——
inline Ref<ImageTexture> pin_texture() {
	const int S = 32;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	const float cx = 16.0f, cy = 16.0f;
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
			float r = std::sqrt(dx * dx + dy * dy);
			float a = Math::clamp(12.0f - r, 0.0f, 1.0f);
			if (a <= 0.0f) {
				img->set_pixel(x, y, Color(0, 0, 0, 0));
				continue;
			}
			float shade = 0.55f + 0.45f * Math::clamp(1.0f - r / 12.0f, 0.0f, 1.0f);
			// 左上高光
			float hl = Math::clamp(1.0f - std::sqrt((dx + 4.0f) * (dx + 4.0f) + (dy + 4.0f) * (dy + 4.0f)) / 6.0f, 0.0f, 1.0f);
			Color c(0.20f * shade + hl * 0.45f, 0.17f * shade + hl * 0.42f, 0.14f * shade + hl * 0.38f, a);
			img->set_pixel(x, y, c);
		}
	}
	return ImageTexture::create_from_image(img);
}

inline Ref<ImageTexture> leaf_icon_texture() {
	const int S = 72;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	const float cx = S * 0.5f;
	for (int y = 0; y < S; y++) {
		float t = (float)y / (float)(S - 1);              // 0 顶尖 → 1 叶柄
		float half = 0.40f * (float)S * std::pow(std::sin(t * 3.1415926f), 0.72f);
		for (int x = 0; x < S; x++) {
			float dx = (float)x + 0.5f - cx;
			float a = Math::clamp(half - std::fabs(dx), 0.0f, 1.0f);
			float stem = (t > 0.86f && std::fabs(dx) < 1.6f) ? 1.0f : 0.0f;
			if (a <= 0.0f && stem <= 0.0f) {
				img->set_pixel(x, y, Color(0, 0, 0, 0));
				continue;
			}
			float edge = Math::clamp(1.0f - std::fabs(dx) / Math::max(half, 0.001f), 0.0f, 1.0f);
			float g = 0.55f + 0.45f * edge;
			Color c(0.18f * g, 0.40f * g, 0.22f * g, Math::max(a, stem));
			// 主叶脉
			if (std::fabs(dx) < 1.1f && t < 0.9f) {
				c = c.lerp(Color(0.12f, 0.28f, 0.15f, 1.0f), 0.7f);
			}
			img->set_pixel(x, y, c);
		}
	}
	return ImageTexture::create_from_image(img);
}

inline Ref<ImageTexture> seal_texture() {
	const int S = 80;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	const float cx = S * 0.5f, cy = S * 0.5f;
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
			float r = std::sqrt(dx * dx + dy * dy);
			float wob = 1.0f + 0.055f * std::sin(std::atan2(dy, dx) * 7.0f + 0.8f)
					+ 0.025f * std::sin(std::atan2(dy, dx) * 13.0f);
			float a = Math::clamp((29.0f * wob - r), 0.0f, 1.0f);
			if (a <= 0.0f) {
				img->set_pixel(x, y, Color(0, 0, 0, 0));
				continue;
			}
			// 蜡面：边缘更深、中心略亮
			float k = Math::clamp(r / 29.0f, 0.0f, 1.0f);
			Color c = Color(0.22f, 0.38f, 0.26f, a).lerp(Color(0.12f, 0.24f, 0.16f, a), k * 0.85f);
			// 压印叶片（左上受光的浅浮雕）
			float lt = (dy + 20.0f) / 40.0f;
			if (lt > 0.04f && lt < 0.96f) {
				float half = 9.5f * std::pow(std::sin(lt * 3.1415926f), 0.72f);
				float lx = std::fabs(dx);
				if (lx < half) {
					float emb = Math::clamp(1.0f - (lx + std::fabs(dy - 1.0f) * 0.25f) / 10.0f, 0.0f, 1.0f);
					c = c.lerp(Color(0.46f, 0.62f, 0.44f, a), emb * 0.55f);
					if (lx < 0.9f) {
						c = c.lerp(Color(0.16f, 0.29f, 0.19f, a), 0.6f);
					}
				}
			}
			img->set_pixel(x, y, c);
		}
	}
	return ImageTexture::create_from_image(img);
}

// 大面积木板：平铺木纹而不是把 128px 纹理拉伸成粗条纹
inline Ref<StyleBoxTexture> make_wood_tiled(float p_alpha = 0.99f) {
	Ref<StyleBoxTexture> sb;
	sb.instantiate();
	sb->set_texture(wood_texture());
	sb->set_texture_margin_all(12.0f);
	sb->set_h_axis_stretch_mode(StyleBoxTexture::AXIS_STRETCH_MODE_TILE_FIT);
	sb->set_v_axis_stretch_mode(StyleBoxTexture::AXIS_STRETCH_MODE_TILE_FIT);
	sb->set_content_margin_all(24.0f);
	sb->set_modulate(Color(1.0f, 1.0f, 1.0f, p_alpha));
	return sb;
}

inline Ref<StyleBoxTexture> make_wood_textured(float /*p_radius*/ = 14.0f, float p_alpha = 0.97f) {
	Ref<StyleBoxTexture> sb;
	sb.instantiate();
	sb->set_texture(wood_texture());
	sb->set_texture_margin_all(12.0f);
	sb->set_content_margin_all(14.0f);
	sb->set_modulate(Color(1.0f, 1.0f, 1.0f, p_alpha));
	return sb;
}

// 羊皮纸/手账纸：暖白底 + 横向纤维 + 细碎纸屑 + 边缘微暗
inline Ref<ImageTexture> parchment_texture() {
	const int S = 128;
	Ref<Image> img = Image::create(S, S, false, Image::FORMAT_RGBA8);
	const Color base = paper_cream();
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			float fiber = ui_vnoise((float)x * 0.04f + 7.0f, (float)y * 0.42f);
			float grain = 0.955f + 0.035f * fiber + 0.015f * (ui_vnoise((float)x * 0.30f, (float)y * 0.30f) - 0.5f);
			Color c(base.r * grain, base.g * grain, base.b * grain, 1.0f);
			// 纸屑：极少量深色小点
			if (ui_hash01(x * 5 + 3, y * 5 + 9) > 0.994f) {
				c = c.lerp(Color(0.72f, 0.64f, 0.51f, 1.0f), 0.22f);
			}
			// 边缘微暗，像一张被摸旧的纸
			int edge = std::min(std::min(x, y), std::min(S - 1 - x, S - 1 - y));
			if (edge < 8) {
				float k = (1.0f - (float)edge / 8.0f) * 0.12f;
				c = c.lerp(Color(0.80f, 0.73f, 0.61f, 1.0f), k);
			}
			img->set_pixel(x, y, c);
		}
	}
	return ImageTexture::create_from_image(img);
}

inline Ref<StyleBoxTexture> make_parchment_textured(float p_alpha = 0.98f) {
	Ref<StyleBoxTexture> sb;
	sb.instantiate();
	sb->set_texture(parchment_texture());
	sb->set_texture_margin_all(10.0f);
	sb->set_content_margin_all(14.0f);
	sb->set_modulate(Color(1.0f, 1.0f, 1.0f, p_alpha));
	return sb;
}

inline Ref<StyleBoxTexture> make_blackboard_textured(float p_alpha = 0.98f) {
	Ref<StyleBoxTexture> sb;
	sb.instantiate();
	sb->set_texture(blackboard_texture());
	sb->set_texture_margin_all(14.0f);
	sb->set_content_margin_all(16.0f);
	sb->set_modulate(Color(1.0f, 1.0f, 1.0f, p_alpha));
	return sb;
}

// 粉笔手写字体：优先系统手写体，缺字形时回退系统 CJK（黑板上写中文也成立）


// 粗体中文：NotoSansSC 是可变字体，用 wght 轴 + embolden 叠出"木板刻字"的分量
inline Ref<Font> heavy_cjk(int p_weight = 900, float p_embolden = 0.25f) {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	Ref<Font> base = Ref<Font>(Object::cast_to<Font>(res.ptr()));
	if (base.is_null()) {
		return Ref<Font>();
	}
	Ref<FontVariation> fv;
	fv.instantiate();
	fv->set_base_font(base);
	Dictionary coords;
	coords["wght"] = p_weight;
	fv->set_variation_opentype(coords);
	fv->set_variation_embolden(p_embolden);
	return Ref<Font>(fv.ptr());
}

inline Ref<Font> chalk_font() {
	Ref<SystemFont> sf;
	sf.instantiate();
	PackedStringArray names;
	names.push_back("Chalkboard SE");
	names.push_back("Marker Felt");
	names.push_back("Comic Sans MS");
	names.push_back("Segoe Print");
	sf->set_font_names(names);
	sf->set_allow_system_fallback(true);
	return Ref<Font>(sf.ptr());
}

// 粉笔字：米白 + 极淡的粉笔灰描边（不压黑，保留黑板感）
inline void dress_chalk_label(Label *p_label, int p_font_size, const Ref<Font> &p_font = Ref<Font>()) {
	dress_label(p_label, p_font_size, chalk_white(), p_font, 1);
}

// 木牌上的深雕字色（刻在木头里的字）
inline void dress_wood_label(Label *p_label, int p_font_size, const Ref<Font> &p_font) {
	dress_label(p_label, p_font_size, wood_text(), p_font, 0);
}

inline Ref<Font> load_cjk() {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/fonts/NotoSansSC-Regular.ttf");
	return Ref<Font>(Object::cast_to<Font>(res.ptr()));
}

} // namespace uistyle
} // namespace godot
