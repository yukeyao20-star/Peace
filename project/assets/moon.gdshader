// moon.gdshader —— 月盘材质（SkyCycle/Moon 专用）
//
// 月面细节：双层值噪声叠出 月海（大块暗斑）+ 陨石坑（高频斑点：中心暗、外缘微亮）；
// 相位照明：明暗界线是椭圆弧 lit: x > cos(2π·phase)·sqrt(1-y²)（圆盘坐标 x/y 取球面方向），
//   phase 0=新月 0.25=半月 0.5=满月；照亮侧固定朝局部 +X（日光方位的近似投影）。
//   暗面保留约 8% 蓝灰"地照"残影，新月夜也能隐约看见轮廓而不是凭空消失。
// alpha 由 SkyCycle 每帧按 夜幕×(1-云量×0.92) 写入，白天/浓云整组隐藏省 draw call。

shader_type spatial;
render_mode unshaded, fog_disabled, cull_back;

uniform float alpha : hint_range(0.0, 1.0) = 0.0;
uniform float phase : hint_range(0.0, 1.0) = 0.5;

varying vec3 obj_pos;

float hash21(vec2 p) {
	p = fract(p * vec2(234.34, 435.345));
	p += dot(p, p + 34.23);
	return fract(p.x * p.y);
}

float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash21(i);
	float b = hash21(i + vec2(1.0, 0.0));
	float c = hash21(i + vec2(0.0, 1.0));
	float d = hash21(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void vertex() {
	obj_pos = VERTEX; // 球体局部坐标（半径 8），fragment 里归一化成球面方向
}

void fragment() {
	vec3 n = normalize(obj_pos);
	// 球面采 2D 噪声会有接缝：把方向投到 XY / ZY 两个平面各采一层按权重平均
	float m1 = vnoise(n.xy * 7.0 + 3.1) * 0.6 + vnoise(n.zy * 6.0 + 11.7) * 0.4;
	float m2 = vnoise(n.xy * 19.0 + 7.7) * 0.5 + vnoise(n.zy * 17.0 + 2.9) * 0.5;

	float mare = smoothstep(0.45, 0.75, m1) * 0.22;          // 月海：低频大块暗斑
	float spot = smoothstep(0.68, 0.80, m2);                  // 陨石坑：高频斑点主体
	float rim = max(smoothstep(0.62, 0.68, m2) - spot, 0.0);  // 坑缘微亮
	float shade = 1.0 - mare - spot * 0.18 + rim * 0.10;

	// 相位照明（照亮侧朝 +X）：GLSL 的 smoothstep 反向参数未定义，统一正向写
	float k = cos(phase * 6.2831853);
	float limb = sqrt(max(1.0 - n.y * n.y, 0.0));
	float lit = smoothstep(-0.05, 0.05, n.x - k * limb);

	vec3 lit_col = vec3(0.93, 0.94, 1.0) * shade;
	vec3 dark_col = vec3(0.07, 0.09, 0.15) * (0.6 + 0.4 * shade);
	ALBEDO = mix(dark_col, lit_col, lit);
	EMISSION = lit_col * lit * 0.18; // 借环境 Glow 泛一点柔光晕
	ALPHA = clamp(alpha, 0.0, 1.0);
}
