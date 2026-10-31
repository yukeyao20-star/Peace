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

inline Ref<StyleBoxTexture> make_wood_textured(float /*p_radius*/ = 14.0f, float p_alpha = 0.97f) {
	Ref<StyleBoxTexture> sb;
	sb.instantiate();
	sb->set_texture(wood_texture());
	sb->set_texture_margin_all(12.0f);
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
