shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_disabled;

// CS2 级高拟真高山飞瀑着色器
// 1. 真实屏幕空间折射（Screen Refraction）：透过清澈奔流窥见背后扭曲晃动的湿润悬崖岩壁
// 2. 物理光线吸收（Beer-Lambert Absorption）：浅水清透见底，湍流核心呈现饱满深邃的碧蓝水色
// 3. 编织湍流流场（Domain-Warped Braided Flow）：双层差速对冲剪切，水流撕裂成有机奔腾的水索与飞溅浪丝
// 4. CS2 碎金高光与钻石闪烁（Sun Specular & Sparkle）：日光在飞瀑水索上激起耀眼的高光耀斑与微闪
// 5. 逆光次表面散射（Subsurface Translucency）：迎光/逆光时透出晶莹通透的翡翠色内光
// 6. 触岩软接触沸腾白沫（Soft Contact Froth）：水帘与岩石、水潭交界处激起浓郁白沫
// 7. 飞瀑两翼自然流苏撕裂（Ragged Edge Erosion）：消灭任何多边形方边，宛如天然飞流

uniform vec4 shallow_color : source_color = vec4(0.20, 0.78, 0.88, 0.80);
uniform vec4 deep_color : source_color = vec4(0.04, 0.35, 0.58, 0.95);
uniform vec4 foam_color : source_color = vec4(0.98, 1.0, 1.0, 1.0);
uniform vec3 sun_dir = vec3(0.35, 0.82, 0.45);

uniform float fall_speed : hint_range(1.0, 15.0) = 6.8;
uniform float foam_amount : hint_range(0.0, 1.0) = 0.65;
uniform float streak_scale : hint_range(0.5, 5.0) = 1.35;
uniform float refraction_strength : hint_range(0.0, 0.15) = 0.055;
uniform float sparkle_intensity : hint_range(0.0, 5.0) = 2.4;
uniform float emission_strength : hint_range(0.0, 1.0) = 0.32;

uniform sampler2D depth_tex : hint_depth_texture, filter_linear, repeat_disable;
uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap, repeat_disable;

varying vec3 world_pos;

float hash21(vec2 p) {
	p = fract(p * vec2(127.1, 311.7));
	p += dot(p, p + 34.123);
	return fract(p.x * p.y);
}

float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	float a = hash21(i);
	float b = hash21(i + vec2(1.0, 0.0));
	float c = hash21(i + vec2(0.0, 1.0));
	float d = hash21(i + vec2(1.0, 1.0));
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	// 激流巨大动能引起的高频微颤与纵向波浪涌动
	float wave1 = sin(TIME * 24.0 + VERTEX.y * 5.5 + VERTEX.x * 2.8) * 0.025;
	float wave2 = cos(TIME * 16.0 + VERTEX.y * 3.2 - VERTEX.z * 1.5) * 0.018;
	VERTEX += NORMAL * (wave1 + wave2);
}

void fragment() {
	float u = UV.x;
	float v = UV.y;
	float t = TIME * fall_speed;

	// ── 1. 编织湍流域扭曲（Domain Warping 流场） ──
	// 用低频滚动的涡流噪声扭曲高频坐标，使水丝不再像机械平铺贴图，而是如真实飞瀑般扭动交织
	vec2 warp = vec2(
		vnoise(vec2(u * 2.2, v * 0.8 - t * 0.35)) - 0.5,
		vnoise(vec2(u * 3.5 + 17.0, v * 1.2 - t * 0.55)) - 0.5
	) * 0.22;

	vec2 uv_coarse = vec2(u * 3.6 * streak_scale, v * 0.85 - t * 0.85) + warp * 0.5;
	vec2 uv_fast   = vec2(u * 9.2 * streak_scale, v * 2.4 - t * 1.65) + warp;
	vec2 uv_fine   = vec2(u * 18.5 * streak_scale, v * 5.4 - t * 2.8) + warp * 1.5;

	float n_coarse = vnoise(uv_coarse);
	float n_fast   = vnoise(uv_fast);
	float n_fine   = vnoise(uv_fine);

	// 垂直落差区段水态演变：
	// 崖顶 (v < 0.08)：刚刚越过悬崖边缘，水体平滑清透，浪花初现
	// 中段 (0.08 < v < 0.80)：剧烈撕裂加速，纯白浪丝与碧蓝水柱紧密编织
	// 底部 (v > 0.80)：撞入深潭，剧烈翻滚暴起 100% 浓厚沸腾白沫
	float crest_calm = smoothstep(0.01, 0.10, v);
	float impact_foam = smoothstep(0.70, 0.98, v) * 0.85;

	float foam_pattern = n_coarse * 0.38 + n_fast * 0.42 + n_fine * 0.20;
	// 激流纯白浪丝：呈现在奔腾水柱表面，保留清透湛蓝水体基底，不再整面全白
	float whitewater = smoothstep(0.55 - foam_amount * 0.12, 0.85, foam_pattern) * crest_calm;
	whitewater = clamp(whitewater + impact_foam * 0.75, 0.0, 1.0);

	// ── 2. 水帘两翼自然流苏撕裂边缘 ──
	float center_dist = abs(u - 0.5) * 2.0;
	float edge_noise = (n_fast - 0.5) * 0.45 + (n_fine - 0.5) * 0.30;
	float ragged_edge = center_dist + edge_noise;
	float alpha_edge = 1.0 - smoothstep(0.72, 1.02, ragged_edge);

	// ── 3. 场景深度与软接触判定 ──
	float depth_raw = texture(depth_tex, SCREEN_UV).r;
	float scene_depth = 10.0;
	if (depth_raw < 0.9999) {
		vec3 ndc = vec3(SCREEN_UV * 2.0 - 1.0, depth_raw);
		vec4 view_pos = INV_PROJECTION_MATRIX * vec4(ndc, 1.0);
		view_pos.xyz /= view_pos.w;
		scene_depth = max(-view_pos.z - (-VERTEX.z), 0.0);
	}
	float soft_contact = smoothstep(0.0, 0.20, scene_depth);

	// 触岩与落水暴起白沫（Contact Froth）：水帘切入岩壁或潭面处爆开一圈白沫
	float contact_froth = (1.0 - smoothstep(0.0, 0.28, scene_depth)) * 0.65;
	whitewater = clamp(whitewater + contact_froth, 0.0, 1.0);

	// ── 4. 高频水丝法线扰动 ──
	vec3 norm_pert = vec3(
		(n_fine - 0.5) * 0.45,
		0.0,
		(n_fast - 0.5) * 0.38
	);
	vec3 perturbed_normal = normalize(NORMAL + (VIEW_MATRIX * vec4(norm_pert, 0.0)).xyz);
	NORMAL = perturbed_normal;

	// ── 5. 屏幕空间真实折射与光线吸收（CS2 光学核心） ──
	vec2 refr_offset = perturbed_normal.xy * refraction_strength * (1.0 - whitewater * 0.6);
	vec2 refr_uv = clamp(SCREEN_UV + refr_offset, vec2(0.002), vec2(0.998));
	vec3 scene_bg = texture(screen_tex, refr_uv).rgb;

	// 物理吸收（Beer-Lambert）：深层呈现通透的高山翡翠碧蓝
	vec3 absorb = exp(-clamp(scene_depth, 0.0, 4.0) * vec3(0.65, 0.20, 0.08));
	vec3 clear_water_refr = scene_bg * absorb;

	// 基础水色调制：层次丰富的清冽深蓝与碧蓝水幕
	vec3 water_tint = mix(shallow_color.rgb, deep_color.rgb, smoothstep(0.05, 0.65, v));
	vec3 clear_water = mix(clear_water_refr, water_tint, 0.72);

	// 水体与激流白沫混合
	vec3 col = mix(clear_water, foam_color.rgb, whitewater * 0.88);

	// ── 6. 菲涅尔与逆光次表面漫射（Subsurface Translucency） ──
	vec3 view_dir = -VIEW;
	vec3 l_sun = normalize((VIEW_MATRIX * vec4(normalize(sun_dir), 0.0)).xyz);
	float fresnel = pow(1.0 - clamp(dot(view_dir, NORMAL), 0.0, 1.0), 3.0);
	col += vec3(0.85, 0.95, 1.0) * fresnel * 0.40;

	// 逆光翡翠透光感：阳光穿透跌水水幕产生的散射透亮
	float backlight = pow(max(dot(-view_dir, l_sun), 0.0), 2.5);
	col += shallow_color.rgb * backlight * 0.45 * (1.0 - whitewater);

	// ── 7. CS2 碎金高光与白沫钻石微闪（Specular Glint & Sparkles） ──
	vec3 refl_dir = reflect(-view_dir, NORMAL);
	float spec_sun = pow(max(dot(refl_dir, l_sun), 0.0), 160.0);
	float spec_broad = pow(max(dot(refl_dir, l_sun), 0.0), 24.0);
	col += vec3(1.0, 0.98, 0.90) * (spec_sun * 2.8 + spec_broad * 0.25) * (1.0 - whitewater * 0.3);

	// 白沫上的钻石微闪（Sun Sparkles on Foam）
	float sparkle = pow(vnoise(vec2(u * 38.0, v * 14.0 - t * 3.2)), 5.5) * sparkle_intensity;
	col += foam_color.rgb * sparkle * whitewater;

	// 飞沫在日光下微发光（适度柔和，不刺目过曝）
	vec3 emit = foam_color.rgb * (whitewater * emission_strength * 0.45 + impact_foam * 0.18 + contact_froth * 0.15);

	ALBEDO = col;
	EMISSION = emit;
	ROUGHNESS = mix(0.04, 0.25, whitewater);
	SPECULAR = 0.92;
	ALPHA = clamp(alpha_edge * soft_contact * (0.85 + whitewater * 0.15), 0.0, 1.0);
}
