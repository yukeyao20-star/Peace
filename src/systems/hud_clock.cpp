#include "hud_clock.h"

#include "sky_cycle.h"

#include <cmath>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

void HUDClock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sky_cycle_path", "path"), &HUDClock::set_sky_cycle_path);
	ClassDB::bind_method(D_METHOD("get_sky_cycle_path"), &HUDClock::get_sky_cycle_path);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "sky_cycle_path"), "set_sky_cycle_path", "get_sky_cycle_path");
}

void HUDClock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不出 HUD
			}
			// 一行水平容器：[月相图标][时钟文字]，整组水平居中——
			// 比 CENTER_TOP 稳（不用算自身宽度），图标紧跟文字左侧
			box = memnew(HBoxContainer);
			box->set_anchors_and_offsets_preset(Control::PRESET_TOP_WIDE);
			box->set_offset(SIDE_TOP, 12.0f);
			box->set_alignment(BoxContainer::ALIGNMENT_CENTER);
			add_child(box);

			// 月相图标：64px 程序化贴图缩到 34px 显示（见 update_moon_icon）
			moon_icon = memnew(TextureRect);
			moon_icon->set_custom_minimum_size(Vector2(34, 34));
			moon_icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
			box->add_child(moon_icon);

			label = memnew(Label);
			label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			label->add_theme_font_size_override("font_size", 30);
			// Godot 内置默认字体不含 CJK 字形（中文全渲染成方框），SystemFont 的
			// 系统字体查找在本机实测也不可用（FreeType 加载空路径）——
			// 改用随工程打包的 Noto Sans SC 可变字体（OFL 开源许可，三平台表现一致）。
			Ref<Font> cjk = ResourceLoader::get_singleton()->load(
					"res://assets/fonts/NotoSansSC-Regular.ttf");
			if (cjk.is_valid()) {
				label->add_theme_font_override("font", cjk);
			}
			label->add_theme_color_override("font_color", Color(1.0f, 1.0f, 1.0f, 1.0f));
			label->add_theme_color_override("font_outline_color", Color(0.04f, 0.06f, 0.10f, 0.85f));
			label->add_theme_constant_override("outline_size", 8);
			box->add_child(label);
			refresh();
			set_process(true);
			break;
		}
		case NOTIFICATION_PROCESS: {
			refresh();
			break;
		}
		default:
			break;
	}
}

void HUDClock::refresh() {
	if (!label) {
		return;
	}
	// SkyCycle 可能晚于本节点就绪（树序在后）——每帧懒查找兜底
	if (!cycle) {
		cycle = Object::cast_to<SkyCycle>(get_node_or_null(sky_cycle_path));
	}
	if (!cycle) {
		if (box) {
			box->hide(); // 图标 + 文字一起藏
		}
		return;
	}
	box->show();

	const float tod = cycle->get_time_of_day();
	const int total_min = (int)(tod * 1440.0f) % 1440;
	const int hh = total_min / 60;
	const int mm = total_min % 60;
	const String clock = String::num_int64(hh).pad_zeros(2) + ":" + String::num_int64(mm).pad_zeros(2);

	// 白天数（SkyCycle 跨午夜自增）+ 装饰性气温：按太阳高度估算，正午最暖、午夜最冷。
	// 气温需要 sun_h 的完整 -1..1 摆幅（11~23°C 全天连续变化），而 get_night_factor()
	// 在整个白天饱和为 0、深夜饱和为 1，无法无损反推——此处保留独立的 sin 换算。
	const int day_n = cycle->get_day_count();
	const float sun_h = std::sin((tod - 0.25f) * 6.2831853f);
	const int temp = (int)std::lround(17.0f + 6.0f * sun_h); // 约 11°C（夜）~ 23°C（正午）

	// 月相名称：phase 0..1（0 新月 0.5 满月），取 SkyCycle::get_moon_phase()
	// （公式唯一来源，第 1 天满月、8 天一个朔望循环）；八相等分取最近命名。
	static const char *MOON_NAMES[8] = {
		"新月", "娥眉月", "上弦月", "盈凸月", "满月", "亏凸月", "下弦月", "残月"
	};
	const float mph = cycle->get_moon_phase();
	const int midx = (int)std::lround(mph * 8.0f) % 8;

	// 显示值全部未变就不重建文本、不 set_text（时段/天气短语一并用
	// phase_name 字符串比较兜住，保证天气切换当帧仍能刷新）。
	const String phase = cycle->get_phase_name();
	if (total_min == last_min && day_n == last_day && temp == last_temp
			&& midx == last_midx && phase == last_phase) {
		update_moon_icon(mph); // 图标自身按 256 级相位 key 门控，开销极低
		return;
	}
	last_min = total_min;
	last_day = day_n;
	last_temp = temp;
	last_midx = midx;
	last_phase = phase;

	// 注意：godot-cpp 的 String(const char*) 是逐字节 latin-1 展宽、不解 UTF-8，
	// 中文字面量必须走 String::utf8()，否则运行期全是乱码。
	const String tail = String::utf8("  ·  ") + String::utf8(MOON_NAMES[midx])
			+ String::utf8("  ·  第 ") + String::num_int64(day_n) + String::utf8(" 天  ·  ")
			+ String::num_int64(temp) + String::utf8("\xC2\xB0" "C");

	label->set_text(clock + String::utf8("  ·  ") + phase + tail);
	update_moon_icon(mph);
}

// 程序化绘制月相图标：64×64 圆盘，明暗界线按月相精确建模——
// phase 0..1（0 新月 0.25 上弦 0.5 满月 0.75 下弦，与 SkyCycle::get_moon_phase 同源）：
// 界线是半椭圆 x_term = cos(2π·phase)·√(1−v²)，盈月（phase<0.5）右侧受光、
// 亏月左侧受光。暗面保留深蓝灰"地照"残影，与 3D 月盘 shader 的表现一致。
// 只在月相量化 key 变化时重绘（实际只跨天才变），不逐帧重画。
void HUDClock::update_moon_icon(float phase) {
	const int key = (int)std::lround(phase * 256.0f);
	if (key == moon_key && moon_tex.is_valid()) {
		return;
	}
	moon_key = key;

	constexpr int SZ = 64;
	Ref<Image> img = Image::create_empty(SZ, SZ, false, Image::FORMAT_RGBA8);
	const float s = std::cos(phase * 6.2831853f);
	const bool waxing = phase < 0.5f;
	const Color lit(0.95f, 0.93f, 0.86f);   // 受光面：淡暖白
	const Color dark(0.24f, 0.28f, 0.38f);  // 暗面：地照残影蓝灰
	for (int y = 0; y < SZ; ++y) {
		const float v = ((y + 0.5f) / SZ) * 2.0f - 1.0f;
		for (int x = 0; x < SZ; ++x) {
			const float u = ((x + 0.5f) / SZ) * 2.0f - 1.0f;
			const float r = std::sqrt(u * u + v * v);
			float a = (1.0f - r) * 24.0f; // 圆缘 1~2px 羽化抗锯齿
			if (a <= 0.0f) {
				continue;
			}
			a = a > 1.0f ? 1.0f : a;
			const float term = s * std::sqrt(std::fmax(0.0f, 1.0f - v * v));
			const bool is_lit = waxing ? (u > term) : (u < -term);
			Color c = is_lit ? lit : dark;
			c.a = a;
			img->set_pixel(x, y, c);
		}
	}
	moon_tex = ImageTexture::create_from_image(img);
	if (moon_icon) {
		moon_icon->set_texture(moon_tex);
	}
}

} // namespace godot
