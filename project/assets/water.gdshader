// CS2 风格真实水面着色器
//
// 与"半透明蓝塑料"那种假水的区别在于它把水当成一个"光学机器"：
//   1. 顶点波浪：多个不同频率/方向的正弦波叠加，让水面整体起伏；
//   2. 法线噪声：两层反向滚动的程序噪声求梯度 → 细碎波光，替代贴图；
//   3. 折射：用屏幕纹理按法线"掰弯"采样，看到水下的东西是扭曲的；
//   4. 吸收：从深度缓冲算出水深——浅处透底、深处变浓（深水蓝）；
//   5. 菲涅尔：视角越平，反射越强——远处的水面映出天空的颜色；
//   6. 泡沫：水很浅的岸边混入白色，用噪声打碎边缘，模拟浪花残留。
// 低粗糙度 + 高镜面反射让阳光在波纹上形成 CS2 标志性的碎金高光。

shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_back;

uniform vec4 shallow_color : source_color = vec4(0.18, 0.55, 0.58, 1.0);
uniform vec4 deep_color : source_color = vec4(0.015, 0.13, 0.27, 1.0);
uniform vec4 foam_color : source_color = vec4(0.93, 0.97, 1.0, 1.0);
uniform float wave_height : hint_range(0.0, 0.5) = 0.05;
uniform float wave_speed : hint_range(0.0, 4.0) = 1.1;
uniform float normal_strength : hint_range(0.0, 3.0) = 1.0;
uniform float noise_scale : hint_range(0.1, 4.0) = 1.0;
uniform float refraction_distort : hint_range(0.0, 0.15) = 0.045;
uniform float foam_depth : hint_range(0.0, 2.0) = 0.32;
uniform float depth_reach : hint_range(0.5, 6.0) = 2.6;
uniform float caustics_strength : hint_range(0.0, 2.0) = 0.45;
uniform float rain_amount : hint_range(0.0, 1.0) = 0.0; // 雨强 0..1（SkyCycle 天气机每帧驱动）
uniform vec2 flow_dir = vec2(0.0, -1.0);                // 水流方向（世界 XZ，需归一化）
uniform float freeze_amount : hint_range(0.0, 1.0) = 0.0; // 结冰量 0..1（SkyCycle 每帧驱动；冰从岸边浅水先结）
uniform vec3 ice_color : source_color = vec3(0.72, 0.85, 0.93); // 冰面底色
uniform vec3 sun_dir = vec3(0.3, 0.8, 0.5);      // 指向太阳/月亮的方向（SkyCycle 每帧写入）
uniform float sun_glint : hint_range(0.0, 1.0) = 1.0; // 高光强度（白昼×晴天度；夜里=月光残量）
uniform float shore_wave_speed : hint_range(0.0, 5.0) = 2.8;  // 拍岸浪推进速度（向岸翻滚周期）
uniform float shore_wave_freq : hint_range(1.0, 20.0) = 7.5;  // 破浪深度频率（浪卷紧凑度）
uniform float shore_wave_reach : hint_range(0.2, 4.0) = 1.9;  // 拍岸浪作用水深范围（米，越大影响越深处）

uniform sampler2D depth_tex : hint_depth_texture, filter_linear, repeat_disable;
uniform sampler2D screen_tex : hint_screen_texture, filter_linear, repeat_disable;

varying vec3 world_pos;

// ---- 程序噪声（值噪声，替代法线贴图） ----

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

// 多方向多频率的正弦波叠加（顶点波浪）；末项为长波涌浪（大波长缓推，CS2 开阔水面的呼吸感）
float wave_sum(vec2 p, float t) {
	return sin(p.x * 1.35 + t * 1.20) * 0.45
			+ sin(p.y * 1.72 + t * 0.95) * 0.30
			+ sin(dot(p, vec2(0.8, 1.15)) + t * 1.65) * 0.35
			+ sin(dot(p, vec2(2.35, -1.90)) + t * 2.40) * 0.18
			+ sin(dot(p, vec2(0.60, 0.80)) * 0.42 + t * 0.72) * 0.34;
}

// 多波干涉程序焦散：经典图案，输出尖锐晃动的光丝（0..1+）
float caustic_pattern(vec2 coord, float t) {
	vec2 p = mod(coord * 6.28318, 6.28318) - 250.0;
	vec2 i = p;
	float c = 1.0;
	float inten = 0.005;
	for (int n = 0; n < 4; n++) {
		float tt = t * (1.0 - (3.5 / float(n + 1)));
		i = p + vec2(cos(tt - i.x) + sin(tt + i.y), sin(tt - i.y) + cos(tt + i.x));
		c += 1.0 / length(vec2(p.x / (sin(i.x + tt) / inten), p.y / (cos(i.y + tt) / inten)));
	}
	c /= 4.0;
	c = 1.17 - pow(c, 1.4);
	return pow(abs(c), 7.0);
}

// 雨点涟漪：哈希网格上随机落点的外扩圆环（0..1，随生命期衰减）。
// 每个格子一枚雨滴，相位 = 时间 + 格子哈希 → 落点错开、连绵不断。
float rain_ring(vec2 uv, float t) {
	vec2 i = floor(uv);
	vec2 f = fract(uv) - 0.5;
	float rnd = hash21(i);
	float phase = fract(t * 1.4 + rnd * 7.0); // 0..1 = 一圈涟漪的生命期
	vec2 off = vec2(hash21(i + vec2(13.7, 5.1)), hash21(i + vec2(3.9, 9.2))) - 0.5;
	float r = length(f - off * 0.4);
	float ring = 1.0 - smoothstep(0.0, 0.055, abs(r - phase * 0.42));
	return ring * (1.0 - phase) * (1.0 - phase);
}

// 冰面裂纹：噪声"脊线"（vnoise 接近 0.5 的等值线处最亮），两层错尺度叠加，
// 随机方向分叉的细亮纹——冻结湖面的标志性观感。返回 0..1（1=裂缝）。
float ice_cracks(vec2 p) {
	float c1 = 1.0 - abs(vnoise(p * 1.7) * 2.0 - 1.0);
	float c2 = 1.0 - abs(vnoise(p * 3.9 + 17.3) * 2.0 - 1.0);
	c1 = pow(c1, 22.0); // 尖锐化：只有脊线核心处 > 0
	c2 = pow(c2, 26.0);
	return clamp(c1 + c2 * 0.6, 0.0, 1.0);
}

// 完整水面高度场 = 双层滚动噪声 + 沿流向拉长的顺流层 + 雨点涟漪。
// 所有层共用这一个函数，有限差分求梯度时才能取到每一层真实的斜率。
float water_height(vec2 a, vec2 b, vec2 c, vec2 pw, float amt) {
	float base = vnoise(a) * 0.62 + vnoise(b) * 0.38 + (vnoise(c) - 0.5) * 0.25;
	float rings = (rain_ring(pw * 2.2, TIME) + rain_ring(pw * 4.7 + 31.7, TIME * 1.23) * 0.6)
			* amt * 0.5;
	return base + rings;
}

void vertex() {
	vec3 wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_pos = wp;
	// 基础开阔水面起伏 + 宏观自然动态呼吸起伏
	float w = wave_sum(wp.xz, TIME * wave_speed) * wave_height;
	w += sin(wp.x * 0.45 + wp.z * 0.35 + TIME * 1.6) * 0.022;
	// 结冰压平波浪：冰面不起伏（freeze=1 时波高归零）
	VERTEX.y += w * (1.0 + rain_amount * 0.8) * (1.0 - freeze_amount);
}

void fragment() {
	vec2 p = world_pos.xz * noise_scale;
	float t = TIME * 0.9;

	// 三层噪声高度场：两层反向滚动 + 一层沿流向拉长的顺流层（水流方向感）；
	// 有限差分求梯度 → 法线（偏移量同步加到所有层，含雨环）
	vec2 uv1 = p * 0.55 + vec2(t * 0.060, t * 0.045);
	vec2 uv2 = p * 1.45 + vec2(-t * 0.085, t * 0.060);
	vec2 across = vec2(flow_dir.y, -flow_dir.x); // 垂直于流向
	// 沿流向坐标压缩（0.55）→ 纹理沿流向被拉长；顺流缓慢平移
	vec2 uv3 = vec2(dot(p, across) * 1.9, dot(p, flow_dir) * 0.55) - vec2(0.0, t * 0.11);
	float e = 0.022; // 差分步长收窄 → 微法线细节更密（碎金高光的闪点来源）
	float h0 = water_height(uv1, uv2, uv3, p, rain_amount);
	float hx = water_height(uv1 + vec2(e, 0.0), uv2 + vec2(e, 0.0), uv3 + vec2(e, 0.0),
			p + vec2(e, 0.0), rain_amount) - h0;
	float hz = water_height(uv1 + vec2(0.0, e), uv2 + vec2(0.0, e), uv3 + vec2(0.0, e),
			p + vec2(0.0, e), rain_amount) - h0;
	vec3 n = normalize(vec3(-hx * 7.0 * normal_strength, 1.0, -hz * 7.0 * normal_strength));
	// 结冰衰减涟漪：冰面法线只剩极轻微的凹凸噪声（保留 8% 一点点质感）
	n = normalize(mix(n, vec3(0.0, 1.0, 0.0), freeze_amount * 0.92));
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);

	// 从深度缓冲还原"该像素背后场景"的线性深度，与水面深度求差 → 水深
	float depth_raw = texture(depth_tex, SCREEN_UV).r;
	float water_depth;
	if (depth_raw > 0.9999) {
		water_depth = depth_reach; // 背后是天空（保险分支）
	} else {
		vec3 ndc = vec3(SCREEN_UV * 2.0 - 1.0, depth_raw);
		vec4 view_pos = INV_PROJECTION_MATRIX * vec4(ndc, 1.0);
		view_pos.xyz /= view_pos.w;
		water_depth = max(-view_pos.z - (-VERTEX.z), 0.0);
	}
	float depth_fade = clamp(water_depth / depth_reach, 0.0, 1.0);

	// 结冰覆盖度：浅水先冻（岸边→湖心推进）+ 低频噪声让冻结前沿参差不成圆圈。
	// freeze=1 时湖心也达到 ~0.93（全封湖），freeze=0 时全湖无冰。
	float shore = 1.0 - smoothstep(0.0, depth_reach * 0.55, water_depth); // 1=岸浅 0=湖心
	float patch = 0.72 + 0.28 * vnoise(p * 0.9);
	float cover = clamp((freeze_amount * patch - (1.0 - shore) * 0.62) * 2.4, 0.0, 1.0);

	// 折射：把屏幕画面按法线方向偏移一点再取回来
	vec2 refr_uv = clamp(SCREEN_UV + n.xz * refraction_distort * clamp(water_depth, 0.0, 1.0),
			vec2(0.001), vec2(0.999));
	vec3 refr = texture(screen_tex, refr_uv).rgb;

	// 水下焦散：光丝投在浅水看得见的湖底上，越深越弱；冰层挡光，结冰淡出
	float ca = caustic_pattern(world_pos.xz * 0.6 + vec2(t * 0.02, t * 0.013), TIME * 0.5);
	refr += vec3(0.75, 0.92, 0.88) * ca * (1.0 - depth_fade) * caustics_strength
			* (1.0 - freeze_amount);

	// 物理吸收（Beer-Lambert）：红光穿透力最弱先衰减，深水自然偏蓝绿
	vec3 absorb = exp(-water_depth * vec3(0.45, 0.18, 0.12));
	refr *= absorb;

	// 吸收：浅水几乎透明见底，深水过渡到浓郁的水色
	vec3 water_col = mix(shallow_color.rgb, deep_color.rgb, depth_fade);
	vec3 base = mix(refr, water_col, clamp(depth_fade * 1.35, 0.0, 0.92));

	// 菲涅尔：视角越平反射越强；反射色用"近似天空"按反射方向的高度取色
	vec3 view_dir = normalize(CAMERA_POSITION_WORLD - world_pos);
	float fres = pow(1.0 - clamp(dot(view_dir, n), 0.0, 1.0), 5.0);
	fres = mix(0.03, 1.0, fres);
	vec3 refl_dir = reflect(-view_dir, n);
	vec3 sky_col = mix(vec3(0.80, 0.87, 0.94), vec3(0.32, 0.54, 0.88),
			clamp(refl_dir.y * 1.7, 0.0, 1.0));
	vec3 col = mix(base, sky_col, fres * 0.7);

	// CS2 碎金高光：紧镜面瓣随波面法线抖动成闪烁光路 + 宽光泽瓣铺底；
	// 强度由 SkyCycle 按白昼×晴天度驱动（夜里剩月光残量），结冰大幅衰减
	float spec_t = pow(max(dot(reflect(-view_dir, n), sun_dir), 0.0), 240.0);
	float spec_w = pow(max(dot(reflect(-view_dir, n), sun_dir), 0.0), 24.0);
	col += vec3(1.0, 0.96, 0.86) * (spec_t * 2.6 + spec_w * 0.10) * sun_glint
			* (1.0 - freeze_amount * 0.85);

	// ══════════════════════════════════════════════════════════════════════
	// 动态拍岸浪涌与石头激浪系统 ── 大湖最重要的生命力表达
	// 目标：岸边的石头旁边激起猛烈的白色浪花，有冲击力，有呼吸感，自然
	// ══════════════════════════════════════════════════════════════════════

	// ── A. 深度区带分级 ──────────────────────────────────────────────────
	// 极浅区 (0~0.25m)：石头就在这里，浪花最猛烈
	// 近岸区 (0~shore_reach)：主力拍岸涌浪区
	float extreme_shallow = 1.0 - smoothstep(0.0, 0.28, water_depth);   // 石头激浪区
	float shore_k = 1.0 - smoothstep(0.01, shore_wave_reach, water_depth); // 整体近岸系数

	// ── B. 三频涌浪叠加（深水→浅水推进，浪峰向岸翻卷） ─────────────────
	// 三组不同相位/频率，制造"一浪叠一浪"的自然节奏感
	float lap1 = TIME * shore_wave_speed - water_depth * shore_wave_freq;
	float lap2 = TIME * shore_wave_speed * 0.72 - water_depth * shore_wave_freq * 1.35 + 2.31;
	float lap3 = TIME * shore_wave_speed * 1.41 - water_depth * shore_wave_freq * 0.85 + 4.77;

	// 浪峰破碎（幂次尖锐化 → 陡峭向岸翻卷白沫）
	float crest1 = pow(max(sin(lap1), 0.0), 2.8) * shore_k;
	float crest2 = pow(max(sin(lap2), 0.0), 3.5) * shore_k * 0.75;
	float crest3 = pow(max(sin(lap3), 0.0), 4.2) * shore_k * 0.55;
	float wave_crest = crest1 + crest2 + crest3;

	// ── C. 石头激浪（Rock Splash）────────────────────────────────────────
	// 原理：极浅区(0~0.25m)水与石头的接触界面，用高频噪声+涌浪相位叠加
	// 产生不规则爆发性白沫团 ── 模拟浪花猛击石头激起水花的瞬间冲击感
	float foam_n_fine = vnoise(p * 6.5 + vec2(TIME * 0.24, -TIME * 0.18))
					  + vnoise(p * 14.2 - vec2(TIME * 0.16, TIME * 0.22)) * 0.55
					  + vnoise(p * 28.0 + vec2(TIME * 0.09, TIME * 0.13)) * 0.30;
	foam_n_fine = foam_n_fine / 1.85; // 归一到 0~1

	// 石头激浪：涌浪推进到极浅区撞击石头时猛烈爆发（相位叠加不规则噪声）
	float rock_burst = smoothstep(0.42, 0.82, foam_n_fine)  // 噪声高值点=石头位置
					 * crest1 * 2.8                          // 随主浪峰爆发
					 * extreme_shallow;                      // 仅在石头带触发
	// 额外：石头侧面的持续飞沫（不随相位脉动，而是持续存在的薄沫）
	float rock_mist = smoothstep(0.55, 0.78, foam_n_fine) * extreme_shallow * 0.55;

	// ── D. 潮汐冲刷水线（Swash Zone）────────────────────────────────────
	float foam_n_base = vnoise(p * 3.2 + vec2(t * 0.16, -t * 0.11)) * 0.5
					  + vnoise(p * 7.0 - vec2(t * 0.10, t * 0.07)) * 0.5;
	float swash_pulse = 0.13 * sin(TIME * shore_wave_speed)
					  + 0.055 * sin(TIME * shore_wave_speed * 0.65 + 1.1)
					  + 0.028 * sin(TIME * 4.3 + world_pos.x * 0.38 + world_pos.z * 0.22);
	float swash_line = 1.0 - smoothstep(0.0, 0.22, water_depth - swash_pulse + foam_n_base * 0.10);

	// ── E. 退浪网状泡沫花边（Backwash Foam Lace）────────────────────────
	float lace_n = vnoise(p * 9.0 + vec2(TIME * 0.14, -TIME * 0.10)) * 0.6
				 + vnoise(p * 20.0 - vec2(TIME * 0.09, TIME * 0.12)) * 0.4;
	float lace_mask = smoothstep(0.50, 0.78, lace_n) * shore_k;
	// 大浪退去后遗留的银白泡沫网（强度随浪峰脉动消退）
	float backwash = lace_mask * (crest1 * 0.65 + 0.28 * shore_k);

	// ── F. 浪花飞溅白雾（Spray Mist）────────────────────────────────────
	// 浪峰最高处破碎飞溅的细小水雾，比岸边泡沫更加发白透明
	float spray_k = pow(max(sin(lap1 + 0.7), 0.0), 5.5) * extreme_shallow;
	float spray_n = vnoise(p * 22.0 + vec2(TIME * 0.35, TIME * 0.28));
	float spray = spray_k * smoothstep(0.45, 0.70, spray_n) * 1.6;

	// ── G. 静态基底泡沫（保底，极浅处永不断层）─────────────────────────
	float foam_base = 1.0 - smoothstep(0.0, foam_depth * 0.45, water_depth + foam_n_base * 0.14 - 0.04);

	// ── H. 综合混合 ──────────────────────────────────────────────────────
	float foam = foam_base * 0.60              // 静态基底
			   + wave_crest * 1.55             // 三频涌浪主浪花
			   + rock_burst * 1.80             // 石头爆发激浪（最猛）
			   + rock_mist * 0.65              // 石头飞沫
			   + swash_line * 1.20             // 水线涨落
			   + backwash * 0.70               // 退浪花边
			   + spray * 0.90;                 // 飞溅白雾
	foam = clamp(foam, 0.0, 1.0) * (1.0 - freeze_amount);

	// ── I. 浪峰法线隆起（立体破浪感）────────────────────────────────────
	float norm_surge = (crest1 * 0.55 + extreme_shallow * rock_burst * 0.30) * (1.0 - freeze_amount);
	vec3 wave_norm_offset = vec3(cos(lap1) * norm_surge, 0.0, sin(lap1) * norm_surge) * shore_k;
	n = normalize(n + wave_norm_offset);

	// ── J. 浪花颜色：纯白中带极轻微的青绿色（湖水携带）─────────────────
	vec3 splash_col = mix(foam_color.rgb, vec3(0.95, 0.99, 1.0), 0.25);
	// 石头激浪处更纯白（大量气泡与水花混合）
	splash_col = mix(splash_col, vec3(1.0, 1.0, 1.0), rock_burst * 0.60);
	col = mix(col, splash_col, foam);

	// —— 冰面观感：底冰蓝×明暗噪声 + 脊线裂缝亮纹 + 保留菲涅尔天光 ——
	if (cover > 0.001) {
		float shade = 0.90 + 0.10 * vnoise(p * 2.6 + 5.0);
		vec3 ice = ice_color * shade;
		ice = mix(ice, vec3(0.97, 0.99, 1.0), ice_cracks(p * 0.8) * 0.65);
		ice = mix(ice, sky_col, fres * 0.45); // 冰面映天
		col = mix(col, ice, cover);
	}

	// 雨天氛围：整体压暗偏冷 + 更高的雾感（与 SkyCycle 的 gloom 呼应）
	col = mix(col, col * vec3(0.80, 0.84, 0.90), rain_amount * 0.55);

	ALBEDO = col;
	ROUGHNESS = mix(mix(0.02, 0.35, foam) + rain_amount * 0.10, 0.06, cover); // 冰面比静水更镜面
	SPECULAR = 0.6;                    // 阳光碎金高光
	METALLIC = 0.0;
	ALPHA = 1.0;                       // 手动合成折射，实际不透明
}
