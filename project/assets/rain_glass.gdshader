shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;

// 窗玻璃雨痕：网格里的水滴把屏幕纹理轻微折射。
// 参考 GehirnEimer 的 drop_layer 教程与 godotshaders.com 的 Raindrops glass
// （hint_screen_texture + UV 偏移），去掉外部法线贴图，全部程序化。

uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;
uniform float wet = 0.0;
uniform float grid_x = 20.0;
uniform float grid_y = 26.0;
uniform float strength = 0.035;

float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

// 一层水滴：格子内随机位置的小水滴，整体缓慢向下滑
vec2 drop_layer(vec2 uv, vec2 grid, float t, float seed) {
	vec2 scaled = uv * grid;
	scaled.y += t * (0.35 + seed * 0.25);
	vec2 id = floor(scaled);
	vec2 f = fract(scaled) * 2.0 - 1.0;
	float h = hash21(id + seed);
	float h2 = hash21(id + seed + 7.13);
	// 只让一部分格子有水滴
	if (h < 0.35) {
		return vec2(0.0);
	}
	vec2 center = vec2((h2 - 0.5) * 0.6, 0.0);
	vec2 d = f - center;
	d.y *= 0.72;
	float r = 0.10 + 0.14 * h2;
	float drop = smoothstep(r, r * 0.35, length(d));
	// 水滴下方拖一点水痕
	float trail = smoothstep(0.06, 0.0, abs(d.x)) * smoothstep(-1.0, 0.2, d.y) * step(d.y, 0.0) * 0.35;
	return (vec2(d.x, d.y) * 0.5 + vec2(0.0, -0.15)) * (drop + trail);
}

void fragment() {
	vec2 delta = vec2(0.0);
	delta += drop_layer(UV, vec2(grid_x, grid_y), TIME * 0.06, 0.0);
	delta += drop_layer(UV * 1.37, vec2(grid_x * 0.8, grid_y * 1.1), TIME * 0.045, 3.7) * 0.8;
	vec2 uv = SCREEN_UV + delta * strength * wet;
	vec3 c = texture(screen_tex, uv).rgb;
	// 水滴边缘的一点点冷色高光
	float hl = clamp(length(delta) * 2.0, 0.0, 1.0);
	c += vec3(0.45, 0.55, 0.65) * hl * 0.10 * wet;
	ALBEDO = c;
	ALPHA = 1.0;
}
