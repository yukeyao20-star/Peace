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
	// 球面采 2D 噪声模拟真实月海 (Lunar Maria) 与陨石坑 (Craters)
	float m1 = vnoise(n.xy * 6.5 + 3.1) * 0.6 + vnoise(n.zy * 5.5 + 11.7) * 0.4;
	float m2 = vnoise(n.xy * 18.0 + 7.7) * 0.5 + vnoise(n.zy * 16.0 + 2.9) * 0.5;

	float mare = smoothstep(0.42, 0.72, m1) * 0.28;          // 月海：低频青灰玄武岩大块暗斑
	float spot = smoothstep(0.66, 0.82, m2);                  // 环形山/撞击坑：高频斑点
	float rim = max(smoothstep(0.60, 0.66, m2) - spot, 0.0);  // 撞击坑边缘反光亮环
	float shade = 1.0 - mare - spot * 0.16 + rim * 0.12;

	// 月相几何明暗界线 (Terminator)：
	// 真实月相是阳光入射角与视线方向的交叠投影。
	// 当 phase = 0.5 时为满月（整个可见圆面全亮）；
	// 当 phase 在 0.25/0.75 时为上弦月/下弦月（明暗半圆直平分线）；
	// 当 phase 在 0.0 时为新月（全暗，仅透出地照）。
	// 在朝向相机的可见半球上（n.z 越大越靠近中心）：
	// 经度角 lambda：x = cos(phi)*sin(lambda), z = cos(phi)*cos(lambda)
	// 晨昏线：sun_dir 与法线夹角 dot(n, light_dir) > 0。
	// 光源在轨道面上绕 Y 轴旋转：
	float sun_angle = (phase - 0.5) * 6.2831853; // phase=0.5 -> angle=0 (正面照亮)
	vec3 light_dir = vec3(sin(sun_angle), 0.0, cos(sun_angle));
	float NdotL = dot(n, light_dir);

	// 自适应反走样过渡
	float aa = max(fwidth(NdotL) * 1.5, 0.025);
	float lit = smoothstep(-aa, aa, NdotL);

	// 亮面基色与柔和地照 (Earthshine)：
	// 真实月球暗部在晴朗夜空中并非死黑突兀，而是带有微弱通透的蓝灰冷光地照；
	// 亮面是温暖皎洁的银白色月华。
	vec3 lit_albedo = vec3(0.96, 0.97, 1.0) * shade;
	vec3 earthshine_albedo = vec3(0.12, 0.15, 0.22) * shade * 0.35;
	vec3 final_color = mix(earthshine_albedo, lit_albedo, lit);

	// 球面圆盘边缘自然柔化（消除 3D 网格边缘像素台阶与生硬折角）
	// n.z 在正面圆心处为 1.0，在圆盘边缘切线处为 0.0
	float disk_edge = smoothstep(0.01, 0.10, abs(n.z));

	ALBEDO = final_color;
	EMISSION = lit_albedo * lit * 0.32; // 皎洁月光自发光
	ALPHA = clamp(alpha * mix(0.18, 1.0, lit) * disk_edge, 0.0, 1.0);
}
