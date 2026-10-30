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
// cull_disabled：湖底仰视时水面不能消失（2026-10-31 用户反馈"从湖底向上看是空的"）。
// 背面在 fragment 里走独立的"水下水面穹顶"分支（见文件末尾），不吃正面那套折射/浪花。
render_mode blend_mix, depth_draw_always, cull_disabled;

uniform vec4 shallow_color : source_color = vec4(0.27, 0.62, 0.80, 1.0);
uniform bool is_stream = false;                         // 是否为高山小溪（绕开大湖 shore_map 的 discard 裁切与拍岸浪）
uniform bool is_waterfall = false;                      // 是否为悬崖飞瀑专属垂直水帘
uniform vec4 deep_color : source_color = vec4(0.06, 0.31, 0.54, 1.0);
uniform vec4 foam_color : source_color = vec4(0.93, 0.97, 1.0, 1.0);
uniform float wave_height : hint_range(0.0, 0.5) = 0.075;
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
uniform float shore_wave_reach : hint_range(0.2, 6.0) = 2.6;  // 拍岸浪作用水深范围（米，越大影响越深处）
// 拍岸涌浪的"真实几何"参数：按水深把水面顶起一道随时间向岸推进的浪脊。
// 水深图由 TerrainBuilder 建图时烘好（顶点着色器拿不到深度缓冲）。
uniform float swell_height : hint_range(0.0, 1.0) = 0.22;
uniform float swell_reach_mul = 1.8; // 几何浪脊比白沫作用范围再宽一些（先看见浪滚来，近了才碎成白沫）

// 水深图：R = 水深 / shore_map_depth。顶点（浪脊几何）与片元（白沫相位）共用同一份
// 水深值 —— 两边相位才不会错开，白沫才骑在抬起来的那道水脊上。
uniform sampler2D shore_map : filter_linear, repeat_disable;
uniform vec2 shore_map_origin = vec2(-81.0, -34.5);
uniform vec2 shore_map_size = vec2(150.0, 165.0);
uniform float shore_map_depth = 6.0;

// 湖岸礁石（由 TerrainBuilder 生成地图后写入）：xz = 世界坐标，w = 视觉半径。
// 拍岸浪在每块石头周围爆起白沫——撞击点来自真实几何，不再是噪声"假装"的石头。
uniform vec4 shore_rocks[40]; // 长度须与 TerrainBuilder::apply_shore_rocks 的 MAX_ROCKS 一致
uniform int shore_rock_count = 0;

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

// 从水深图取该点的水深（米）。超出范围或在陆地严格返回 0.0
float map_depth(vec2 wp_xz) {
	vec2 uv = (wp_xz - shore_map_origin) / shore_map_size;
	if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0) {
		return 0.0;
	}
	return texture(shore_map, uv).r * shore_map_depth;
}

// 连续平滑干涉焦散：输出自然流动晃动的光丝，彻底避免 mod() 产生规律性方格网格！
float caustic_pattern(vec2 p, float t) {
	vec2 p1 = p * 1.6 + vec2(t * 0.08, t * 0.05);
	vec2 p2 = p * 2.3 - vec2(t * 0.06, -t * 0.09);
	vec2 p3 = p * 3.1 + vec2(-t * 0.05, t * 0.07);
	float c1 = sin(p1.x + sin(p1.y * 1.3 + t * 0.7));
	float c2 = sin(p2.y + sin(p2.x * 1.2 - t * 0.6));
	float c3 = sin(dot(p3, vec2(0.85, 0.52)) + sin(p3.y * 1.1 + t * 0.5));
	float c = (c1 + c2 + c3) * 0.33333;
	return pow(max(c, 0.0), 3.2);
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
	float ring = 1.0 - smoothstep(0.0, 0.040, abs(r - phase * 0.42));
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

// 完整水面高度场 = 双层滚动噪声 + 沿流向拉长的顺流层 + 高频微细节层 + 雨点涟漪。
// 所有层共用这一个函数，有限差分求梯度时才能取到每一层真实的斜率。
// 微细节层(d)：频率是主层的 ~3.4 倍 —— 法线碎纹的来源，SSR 反射随它细碎闪烁
// （实机对比：没有这层时倒影"太整"，像贴图不像真水）。
float water_height(vec2 a, vec2 b, vec2 c, vec2 d, vec2 pw, float amt) {
	float base = vnoise(a) * 0.60 + vnoise(b) * 0.34 + (vnoise(c) - 0.5) * 0.22
			+ (vnoise(d) - 0.5) * 0.15;
	// 雨点涟漪：格子加密（涟漪更小更密）、幅度压低 —— 之前的"大圆圈"像气泡膜，
	// 真实的雨打湖面是细密的小坑。
	float rings = (rain_ring(pw * 3.4, TIME) + rain_ring(pw * 7.1 + 31.7, TIME * 1.23) * 0.55)
			* amt * 0.26;
	return base + rings;
}

void vertex() {
	vec3 wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_pos = wp;
	// 基础开阔水面起伏 + 宏观自然动态呼吸起伏
	float w = wave_sum(wp.xz, TIME * wave_speed) * wave_height;
	w += sin(wp.x * 0.45 + wp.z * 0.35 + TIME * 1.6) * 0.022;

	if (!is_stream) {
		// 水深图判定：陆地 (md_v <= 0) 绝对没有水体，波浪高度归 0，绝不在陆地凸出地面！
		float md_v = map_depth(wp.xz);
		float water_body_v = smoothstep(0.01, 0.20, md_v);

		// 小溪入湖口平滑阻尼：(38, 64) 附近为小溪水体汇入大湖的交界区，
		// 渐变衰减大湖拍岸涌浪，消除湖面与小溪 Mesh 之间的起伏穿插层！
		float d_mouth = distance(wp.xz, vec2(37.0, 63.5));
		float mouth_calm = smoothstep(2.5, 12.0, d_mouth);

		// 拍岸涌浪的真实几何：仅在湖岸浅水带顶起浪脊，陆地完全平滑归 0
		// 引入横向微相移，打破规则直圆弧
		float swell_phase_noise = (vnoise(wp.xz * 0.18) - 0.5) * 1.4;
		float swell_env = (1.0 - smoothstep(0.12, shore_wave_reach * swell_reach_mul, md_v)) * water_body_v * mouth_calm;
		float swell = pow(max(sin(TIME * shore_wave_speed + md_v * shore_wave_freq + swell_phase_noise), 0.0), 2.2)
				* swell_env;
		w += swell * swell_height;

		// 陆地完全压平波高：水面网格在陆地维持深埋 -0.55m，绝不穿透草地
		w *= water_body_v;
	}

	// 结冰压平波浪：冰面不起伏（freeze=1 时波高归零）
	VERTEX.y += w * (1.0 + rain_amount * 0.8) * (1.0 - freeze_amount);
}

void fragment() {
	// ── 0. 陆地彻底剔除 ──────────────────────────────────────────────────
	// 水面网格是 150m×165m 的大平面，大部分面积深埋在陆地下。
	// 若该像素所在位置在水深图上无水 (md <= 0.005m，即陆地)，直接丢弃该片元！
	// 彻底杜绝任何水面、波浪、白沫在远离湖泊的草地/泥土上渲染，杜绝大片发白！
	// 小溪有专门定制的河道 Mesh，不走大湖水深图剔除！
	float md = is_stream ? 1.0 : map_depth(world_pos.xz);
	if (!is_stream && md <= 0.005) {
		discard;
	}

	vec2 p = world_pos.xz * noise_scale;
	float t = TIME * 0.9;

	// 三层噪声高度场：两层反向滚动 + 一层沿流向拉长的顺流层（水流方向感）；
	// 有限差分求梯度 → 法线（偏移量同步加到所有层，含雨环）
	vec2 uv1 = p * 0.55 + vec2(t * 0.060, t * 0.045);
	vec2 uv2 = p * 1.45 + vec2(-t * 0.085, t * 0.060);
	vec2 across = vec2(flow_dir.y, -flow_dir.x); // 垂直于流向
	// 沿流向坐标压缩（0.55）→ 纹理沿流向被拉长；顺流缓慢平移
	vec2 uv3 = vec2(dot(p, across) * 1.9, dot(p, flow_dir) * 0.55) - vec2(0.0, t * 0.11);
	// 高频微细节层：反向斜滚，把法线打碎（反射闪烁的"闪点"密度由它决定）
	vec2 uv4 = p * 3.60 + vec2(-t * 0.150, -t * 0.115);
	float e = 0.016; // 差分步长收窄 → 微法线细节更密（碎金高光的闪点来源）
	float h0 = water_height(uv1, uv2, uv3, uv4, p, rain_amount);
	float hx = water_height(uv1 + vec2(e, 0.0), uv2 + vec2(e, 0.0), uv3 + vec2(e, 0.0),
			uv4 + vec2(e, 0.0), p + vec2(e, 0.0), rain_amount) - h0;
	float hz = water_height(uv1 + vec2(0.0, e), uv2 + vec2(0.0, e), uv3 + vec2(0.0, e),
			uv4 + vec2(0.0, e), p + vec2(0.0, e), rain_amount) - h0;
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

	// 折射：把屏幕画面按法线方向偏移一点再取回来（带前景物体深度校验，防止折射漏到水面上空物体）
	vec2 refr_offset = n.xz * refraction_distort * clamp(water_depth, 0.0, 1.0);
	vec2 refr_uv = clamp(SCREEN_UV + refr_offset, vec2(0.001), vec2(0.999));
	float refr_depth_raw = texture(depth_tex, refr_uv).r;
	if (refr_depth_raw < 0.9999) {
		vec3 r_ndc = vec3(refr_uv * 2.0 - 1.0, refr_depth_raw);
		vec4 r_vpos = INV_PROJECTION_MATRIX * vec4(r_ndc, 1.0);
		r_vpos.xyz /= r_vpos.w;
		if (-r_vpos.z < -VERTEX.z - 0.01) {
			refr_uv = SCREEN_UV; // 偏移后采样到了水面前方/水面之上的物体，回退不偏移
		}
	}
	vec3 refr = texture(screen_tex, refr_uv).rgb;

	// 水下焦散：光丝投在浅水看得见的湖底上，越深越弱；冰层挡光，结冰淡出
	float ca = caustic_pattern(world_pos.xz * 0.6 + vec2(t * 0.02, t * 0.013), TIME * 0.5);
	refr += vec3(0.75, 0.92, 0.88) * ca * (1.0 - depth_fade) * caustics_strength
			* (1.0 - freeze_amount);

	// 物理吸收（Beer-Lambert）：红绿衰减，保留清亮天蓝色水底
	vec3 absorb = exp(-water_depth * vec3(0.25, 0.10, 0.04));
	refr *= absorb;

	// 吸收：浅水清澈见底，深水自然过渡到浓郁深邃的碧蓝/宝石蓝（明丽晶莹，告别死黑发暗）
	vec3 water_col = mix(shallow_color.rgb, deep_color.rgb, depth_fade);
	float mix_w = clamp(depth_fade * 1.15, 0.15, 0.88);
	vec3 base = mix(refr, water_col, mix_w);

	// 菲涅尔：视角越平反射越强；反射色用"近似天空"按反射方向的高度取色
	vec3 view_dir = normalize(CAMERA_POSITION_WORLD - world_pos);
	float fres = pow(1.0 - clamp(dot(view_dir, n), 0.0, 1.0), 5.0);
	fres = mix(0.03, 1.0, fres);
	vec3 refl_dir = reflect(-view_dir, n);
	vec3 sky_col = mix(vec3(0.80, 0.87, 0.94), vec3(0.32, 0.54, 0.88),
			clamp(refl_dir.y * 1.7, 0.0, 1.0));
	// 反射分两层：引擎的环境反射 + 屏幕空间反射（SSR，需要不透明材质，见文件末尾注释）
	// 负责"真实映出天空和岸"，这里的 sky_col 只作一层薄薄的兜底 —— 保证任何角度
	// 水面都不发死，同时不把引擎那层真实反射盖掉。
	vec3 col = mix(base, sky_col, fres * 0.32);

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
	// 极浅区 (0~0.42m)：石头就在这里，浪花最猛烈
	// 近岸区 (0~shore_reach)：主力拍岸涌浪区
	// 结合真实水深图掩码，防止在水线外产生误判浪花
	float water_mask = smoothstep(0.005, 0.15, md);
	float d_inlet = distance(world_pos.xz, vec2(37.0, 63.5));
	float inlet_calm = is_stream ? 1.0 : smoothstep(3.0, 11.0, d_inlet); // 入湖口平稳过渡，不泛起大湖破碎浪花
	float extreme_shallow = (1.0 - smoothstep(0.0, 0.42, water_depth)) * water_mask * inlet_calm;   // 石头激浪区
	float shore_k = (1.0 - smoothstep(0.01, shore_wave_reach, water_depth)) * water_mask * inlet_calm; // 整体近岸系数

	// ── B. 三频涌浪叠加（深水→浅水推进，浪峰向岸翻卷） ─────────────────
	// 三组不同相位/频率，制造"一浪叠一浪"的自然节奏感。
	// 相位里的水深项必须是 **加号**：水越深相位越超前，浪峰才会随时间
	// 从深处滚向岸边。原来写成减号，浪是往外跑的 —— 这就是"浪花凭空出现"的根因。
	// 水深用**水深图**而不是深度缓冲算出来的 water_depth：顶点位移也用水深图，
	// 两边同源，白沫才骑在抬起来的那道水脊上（差一点就错开半条浪）。
	// 引入横向不规则相移（彻底打破平行等深线，消除湖面规则同心圆/平行白色硬线）
	float phase_jitter = (vnoise(world_pos.xz * 0.22 + vec2(t * 0.04, -t * 0.03)) - 0.5) * 1.6
	                   + (vnoise(world_pos.xz * 0.65 - vec2(t * 0.02, t * 0.05)) - 0.5) * 0.8;
	float lap1 = TIME * shore_wave_speed + md * shore_wave_freq + phase_jitter;
	float lap2 = TIME * shore_wave_speed * 0.72 + md * shore_wave_freq * 1.35 + 2.31 + phase_jitter * 1.15;
	float lap3 = TIME * shore_wave_speed * 1.41 + md * shore_wave_freq * 0.85 + 4.77 + phase_jitter * 0.85;

	// 白沫破碎噪声：打散浪脊使其呈现自然的浪沫簇，而不是一整条白色的硬线条！
	float foam_breakup = smoothstep(0.32, 0.72, vnoise(world_pos.xz * 1.45 + vec2(t * 0.14, -t * 0.10)));

	// 浪峰破碎（幂次尖锐化 + 有机白沫打散）
	float crest1 = pow(max(sin(lap1), 0.0), 2.8) * shore_k * foam_breakup;
	float crest2 = pow(max(sin(lap2), 0.0), 3.2) * shore_k * 0.60 * foam_breakup;
	float crest3 = pow(max(sin(lap3), 0.0), 3.8) * shore_k * 0.40 * foam_breakup;
	float wave_crest = crest1 + crest2 + crest3;

	// ── C. 石头激浪（Rock Splash）────────────────────────────────────────
	// 真实礁石坐标来自 TerrainBuilder 的 shore_rocks（xz = 世界位置，w = 半径）。
	// 浪脊（crest1）推到石头那一刻才炸 —— 白沫浪线、浪脊几何、水花粒子
	// 三者同相位，所以是"看见浪打上去 → 炸开"，不是凭空冒出来。
	float rock_ring = 0.0;
	float rock_core = 0.0;
	for (int i = 0; i < 40; i++) {
		if (i >= shore_rock_count) {
			break;
		}
		vec2 rp = shore_rocks[i].xz;
		float rr = max(shore_rocks[i].w, 0.05);
		float d = distance(world_pos.xz, rp);
		float infl = 1.0 - smoothstep(rr * 0.70, rr * 2.45, d);
		rock_ring = max(rock_ring, infl);
		rock_core = max(rock_core, infl * infl * infl);
	}

	// 石头处的不规则破碎噪声（让爆发团不成规整圆环）
	float foam_n_fine = vnoise(p * 6.5 + vec2(TIME * 0.24, -TIME * 0.18))
					  + vnoise(p * 14.2 - vec2(TIME * 0.16, TIME * 0.22)) * 0.55
					  + vnoise(p * 28.0 + vec2(TIME * 0.09, TIME * 0.13)) * 0.30;
	foam_n_fine = foam_n_fine / 1.85; // 归一到 0~1
	float rock_grain = 0.55 + 0.45 * smoothstep(0.30, 0.80, foam_n_fine);

	// 正面撞击：浪脊推到石头时猛烈炸开（最高最白）
	float rock_burst = rock_core * rock_grain * (crest1 + 0.15) * 3.6 * (1.0 - freeze_amount);
	// 侧面挂沫：石头周围一圈持续的白色碎沫（随浪脊起伏）
	float rock_mist = rock_ring * rock_grain * (0.16 + crest1 * 0.95) * shore_k * (1.0 - freeze_amount);
	// 浪脊自身的白沫线：比 crest1 更锐，带破碎噪声，不再呈现生硬长条白线
	float swell_foam = pow(max(sin(lap1), 0.0), 5.5) * shore_k * rock_grain * foam_breakup
			* 0.75 * (1.0 - freeze_amount);

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
	float foam;
	if (is_waterfall) {
		// 飞流直下高山瀑布水帘（Cascading Vertical Chute & Heavy Spray Sheets）：
		// 沿世界 Y 轴垂直高速倾泻的拉长浪涛纹理，厚重白水水柱交织碧蓝清澈水体
		float fall_speed = TIME * 4.2;
		vec2 wf_uv = vec2(world_pos.x * 1.8 + world_pos.z * 1.8, world_pos.y * 0.95);
		float wf_flow1 = vnoise(wf_uv * 1.2 - vec2(0.0, fall_speed * 1.5));
		float wf_flow2 = vnoise(vec2(wf_uv.x * 2.8, wf_uv.y * 1.8 - fall_speed * 2.2));
		float wf_sheer = vnoise(vec2(wf_uv.x * 0.6, wf_uv.y * 0.4 - fall_speed * 0.8));
		// 瀑布主体水帘：奔腾白浪与透明激流交织，水帘边缘自然镂空
		float wf_rush = smoothstep(0.38, 0.72, wf_flow1 * 0.6 + wf_flow2 * 0.4);
		foam = clamp(wf_rush * 0.85 + wf_sheer * 0.25, 0.15, 0.95);
	} else if (is_stream) {
		// 高山溪流自然急湍（Whitewater Rapids & Mountain Stream）：
		// 主体呈现清澈明丽的碧绿/青蓝色，仅在急流险滩与河心凸起处点缀晶莹灵动的白浪花
		float stream_rush = vnoise(p * 3.2 + vec2(t * 1.2, -t * 0.9)) * 0.6
						  + vnoise(p * 8.5 - vec2(t * 1.6, t * 1.3)) * 0.4;
		float stream_edge = 1.0 - smoothstep(0.01, foam_depth * 0.45, water_depth);
		// 激流白花：阈值调高，只在 15%~25% 浪尖产生灵动碎白沫，不再大面积涂白
		float rapids_foam = smoothstep(0.68, 0.88, stream_rush) * 0.70;
		foam = clamp(stream_edge * 0.35 + rapids_foam, 0.0, 0.65);
	} else {
		foam = foam_base * 0.34              // 静态基底（压低：静浪时岸边干净，浪来时对比更强）
				   + wave_crest * 1.05             // 三频涌浪主浪花（自然柔和破碎）
				   + swell_foam * 0.80             // 向岸推进的白沫浪线
				   + rock_burst * 2.30             // 石头正面爆发激浪（最猛）
				   + rock_mist * 1.05              // 石头侧面挂沫
				   + swash_line * 1.15             // 水线涨落
				   + backwash * 0.70               // 退浪花边
				   + spray * 0.95;                 // 飞溅白雾
		foam *= water_mask; // 浪花与水深严格绑定，边缘平滑收缩归零
	}
	foam = clamp(foam, 0.0, 1.0) * (1.0 - freeze_amount);

	// ── I. 浪峰法线隆起（立体破浪感）────────────────────────────────────
	float norm_surge = (crest1 * 0.70 + rock_core * crest1 * 0.60) * (1.0 - freeze_amount);
	vec3 wave_norm_offset = vec3(cos(lap1) * norm_surge, 0.0, sin(lap1) * norm_surge) * shore_k;
	n = normalize(n + wave_norm_offset);

	// ── J. 浪花颜色：纯白中带极轻微的青绿色（湖水携带）─────────────────
	vec3 splash_col = mix(foam_color.rgb, vec3(0.96, 0.99, 1.0), 0.25);
	// 石头激浪处更纯白（大量气泡与水花混合）
	splash_col = mix(splash_col, vec3(1.0, 1.0, 1.0), clamp(rock_burst + rock_mist, 0.0, 1.0) * 0.72);
	col = mix(col, splash_col, foam);

	// ── K. 岸线接触带（Meniscus / Wet Contact）────────────────────────────
	// 水陆交界：白沫之下先压一道暗色"湿痕"（水真的贴在岸上），再在最外缘描
	// 一条极细的亮水线（湿沙反光）。没有这两笔，水面像"浮"在岸上（实机对比结论）。
	if (!is_waterfall) {
		float contact_dark = (1.0 - smoothstep(0.04, 0.24, water_depth)) * water_mask;
		col = mix(col, col * vec3(0.56, 0.60, 0.64), contact_dark * 0.40 * (1.0 - foam * 0.8));
		float rim_line = smoothstep(0.10, 0.05, water_depth) * smoothstep(0.012, 0.04, water_depth);
		col += vec3(0.09, 0.10, 0.10) * rim_line * water_mask * (1.0 - foam) * (1.0 - freeze_amount);
	}

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

	// —— 水下仰视：背面走"水面穹顶"分支（2026-10-31）——
	// 相机在水下时，水面从下面看是一整片发亮的蓝白穹顶（斯涅尔窗）：
	// 越接近正上方越亮（能透见水面外的天光），视线越斜越深蓝。
	if (!FRONT_FACING) {
		vec3 vd = normalize(CAMERA_POSITION_WORLD - world_pos);
		float up_dot = clamp(-vd.y, 0.0, 1.0);
		vec3 under_col = mix(deep_color.rgb * 0.50,
				shallow_color.rgb * 1.25 + vec3(0.06, 0.10, 0.12), pow(up_dot, 1.5));
		// 从水下看，水面上的波光变成缓慢流动的亮纹（复用焦散图案）
		under_col += vec3(0.55, 0.75, 0.80) * ca * 0.22 * up_dot;
		// 结冰时从水下看是一层灰白冰盖
		under_col = mix(under_col, ice_color * 0.85, cover * 0.85);
		// 用自发光保证"水顶"不受光照方向影响（背面法线与阳光相反，
		// 走正常光照会被压暗成黑盖子）
		ALBEDO = under_col * 0.25;
		EMISSION = under_col * 0.75;
		ROUGHNESS = 0.55;
		SPECULAR = 0.05;
		METALLIC = 0.0;
	} else {
		ALBEDO = col;
		// 粗糙度压到很低 → 反射接近镜面（CS2 那种"液面镜子"的观感）；
		// 泡沫处抬高粗糙度，浪花是哑光的白。
		ROUGHNESS = mix(mix(0.015, 0.30, foam) + rain_amount * 0.08, 0.05, cover);
		SPECULAR = 0.9;                    // 反射强度（阳光碎金高光 + 环境反射）
		METALLIC = 0.0;
	}
	// 注意：这里绝不能写 ALPHA。一旦写 ALPHA，Godot 会把水面扔进透明通道，
	// 而屏幕空间反射（SSR）只作用于不透明物体 —— 水面就再也映不出桥和岸了。
	// 折射是手动合成到 ALBEDO 里的，本来也不需要 alpha 混合。
}
