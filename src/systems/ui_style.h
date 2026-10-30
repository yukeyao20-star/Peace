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
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>
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
