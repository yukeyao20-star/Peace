// 植被风摆着色器（草 / 灌木 / 树冠 / 芦苇共用）
//
// 原理：在顶点阶段按"离根的高度"加权做水平偏移——根部不动、梢部摆幅最大，
// 摆动相位取自世界坐标（不同位置的草不同步），再叠一个倍频让风有"阵性"。
// 每个实例本身带随机朝向（yaw），所以摆的方向天然错开，不需要每实例数据。
//
// 颜色仍走 MultiMesh 实例色（COLOR），与原 StandardMaterial 外观一致。
//
// 雪盖：snow_amount 由 TerrainBuilder 每帧按天气机雪强因子写入——
//   * 覆盖噪声决定"哪些株/哪一片"被积雪覆盖（低频成片 + 高频碎雪）；
//   * 被覆盖处混入雪色（保留一点实例色底味，不是死白）；
//   * 梢部按雪量微微下坠，雪压弯草的观感。

shader_type spatial;
render_mode cull_disabled; // 草叶是双面薄片（草丛三叶网格），关闭背面剔除

uniform vec4 albedo : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform float sway_strength : hint_range(0.0, 0.3) = 0.05; // 摆幅（米，实例本地空间）
uniform float sway_base_y = -0.2; // 从这个本地高度开始算"离根距离"
uniform float sway_span = 0.4;    // 本地高度跨度（到梢部）
uniform float sway_speed : hint_range(0.0, 6.0) = 1.6;
uniform float flutter_strength : hint_range(0.0, 0.3) = 0.0; // 叶片碎抖幅度（树冠开；草/灌木保持 0 不受影响）
uniform float flutter_scale = 5.5; // 碎抖的空间频率（越大越碎、相邻叶团越不同步）
uniform float bottom_shade : hint_range(0.3, 1.2) = 0.78; // 根部/背光面的压暗系数（树冠调更低 → 上浅下深更明显）
// —— 叶片透光（backlight）——
// 叶子是薄的、半透的：太阳在叶子背面时，光会透过来把那一片照得发亮。
// 这件事对树冠特别重要 —— 树冠朝相机的那半边往往是背光面，只靠天光（偏蓝）
// 照亮就会糊成一片墨绿。加一层透光暖光，背光面立刻活过来、也回到草木的绿。
// sun_dir 由 SkyCycle 每帧写入（TerrainBuilder::update_sky_driven）。
uniform vec3 sun_dir = vec3(0.3, 0.8, 0.5);
uniform float backlight : hint_range(0.0, 1.5) = 0.0;   // 透光强度（只有树冠开）
uniform vec3 backlight_color : source_color = vec3(1.0, 0.93, 0.72); // 透出来的光偏暖（阳光穿过叶绿素）
uniform float snow_amount : hint_range(0.0, 1.0) = 0.0; // 雪盖强度（天气机驱动）
uniform vec3 snow_color : source_color = vec3(0.93, 0.95, 0.99);
uniform float alpha : hint_range(0.0, 1.0) = 1.0; // 整体不透明度（树枝雪毯用：雪量驱动淡入；其余植被默认 1 不受影响）
uniform int trample_count = 0;        // 踩草者数量（TerrainBuilder 每帧写入：玩家+农场动物）
uniform vec3 trample_pos[8];          // 踩草者世界坐标（半径 1m 内草叶被压倒）
uniform float trample_energy[8];      // 每个踩草源的移动能量（0 站立 → 1 跑动；站立时无涟漪）
uniform float trample_weight[8];      // 踩草源的着地权重（1 站在地面压实 → 0 跳起腾空完全不压）
uniform vec2 gust_dir = vec2(0.7071, 0.7071); // 风向（旷野之息式移动风带的方向）
uniform float gust_strength : hint_range(0.0, 1.0) = 0.6; // 风滚草浪强度
uniform sampler2D blade_tex : filter_linear, repeat_disable; // 叶片纹理（运行期程序化生成：叶形轮廓+渐变+中脉）
uniform bool use_blade_tex = false;   // 草毯/簇草/高草开纹理；灌木/树冠不用
uniform bool normal_up = false;       // 草叶法线强制朝上（BOTW 经典：按地面光照着色，消除黑色背光面）

varying vec3 wpos;   // 世界坐标（覆盖噪声用——必须含实例变换，否则全图共享同一张噪声图）
varying float h_rel; // 0 根 → 1 梢（草叶根深→梢浅渐变用）
varying float gust_v; // 当前风带强度（fragment 里做光泽波动）

float vhash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(vhash(i), vhash(i + vec2(1.0, 0.0)), u.x),
			mix(vhash(i + vec2(0.0, 1.0)), vhash(i + vec2(1.0, 1.0)), u.x), u.y);
}

void vertex() {
	// Godot 4 渲染 MultiMesh 时 MODEL_MATRIX 已含实例变换（yaw/缩放）——
	// 没有 INSTANCE_TRANSFORM 这个标识符（旧文档写法会编译失败）
	mat4 inst = MODEL_MATRIX;
	if (normal_up) {
		NORMAL = vec3(0.0, 1.0, 0.0); // 草叶按地面光照：背光面不再发黑
	}
	vec3 wp = (inst * vec4(VERTEX, 1.0)).xyz;
	wpos = wp;
	float w = clamp((VERTEX.y - sway_base_y) / max(sway_span, 0.001), 0.0, 1.0);
	h_rel = w;
	float phase = dot(wp.xz, vec2(0.55, 0.42));
	float sway = sin(TIME * sway_speed + phase)
			+ 0.55 * sin(TIME * sway_speed * 2.17 + phase * 1.9);
	// 旷野之息风滚草浪：大尺度移动风带（波长 ~22m 缓慢扫过全图），
	// 风带经过处摆动增强 + 草叶向风向整体倾倒 + 明暗色浪（fragment）
	float gust_phase = dot(wp.xz, gust_dir) * 0.28 - TIME * 0.8;
	float gust = pow(sin(gust_phase) * 0.5 + 0.5, 2.0);
	gust_v = gust * gust_strength;
	float sway_total = sway_strength * (0.6 + gust_v * 1.6);
	VERTEX.x += sway * sway_total * w;
	VERTEX.z += sway * sway_total * 0.55 * w;
	vec2 lean = gust_dir * gust_v * 0.14 * w * w;
	VERTEX.x += lean.x;
	VERTEX.z += lean.y;
	// 叶片碎抖：相位按世界坐标高频取（flutter_scale ≈ 5.5/m）。
	// 同一棵树的两个叶团相距不到 1m，相位差就有好几弧度 → 各自抖各自的，
	// 而不是整坨树冠一起平移。频率也给得比主摆动快（2.7×/4.3×），像叶子在风里抖。
	// 默认 0：草、灌木、芦苇完全不受影响，只有树冠材质把它打开。
	if (flutter_strength > 0.0) {
		float f1 = sin(TIME * sway_speed * 2.7
				+ dot(wp, vec3(flutter_scale, flutter_scale * 0.63, flutter_scale * 1.21)));
		float f2 = sin(TIME * sway_speed * 4.3
				+ dot(wp, vec3(-flutter_scale * 0.87, flutter_scale * 1.13, flutter_scale * 0.79)));
		float fl = (f1 * 0.62 + f2 * 0.38) * flutter_strength * (0.45 + 0.55 * w);
		VERTEX.x += fl;
		VERTEX.z += fl * 0.72;
		VERTEX.y += fl * 0.30;
	}
	// 雪压：积雪越厚梢部越往下坠（w² 让只有梢部明显下沉）
	VERTEX.y -= snow_amount * w * w * 0.06;
	// 踩草波纹：草叶主要是"向下压伏"（折叠伏倒），外推只做一点点——
	// 水平推太多会把四边形面片扯长（没有弯折关节）
	for (int i = 0; i < 8; i++) {
		if (i >= trample_count) {
			break;
		}
		float weight = trample_weight[i];
		if (weight <= 0.001) {
			continue; // 空中腾空状态：完全不压草
		}
		vec2 dxz = wp.xz - trample_pos[i].xz;
		float d = length(dxz);
		if (d < 1.4) {
			// 垂直高度衰减：
			// trample_pos[i].y 是角色脚底高度，wp.y 是草叶顶点世界高度。
			// 1. 脚底高于草叶（跳起在空中、站在桥上/岩石上）：脚底离草叶高度差超过 5cm 开始衰减，超过 22cm 完全脱离
			float dy_above = trample_pos[i].y - wp.y;
			float vert_att = 1.0 - smoothstep(0.05, 0.22, dy_above);
			// 2. 草叶远高于角色（草在上方峭壁崖顶，人在崖底）：不被踩
			float dy_below = wp.y - trample_pos[i].y;
			vert_att *= 1.0 - smoothstep(1.8, 2.4, dy_below);

			float eff_w = weight * vert_att;
			if (eff_w <= 0.001) {
				continue;
			}

			float f = (1.0 - d / 1.4) * eff_w;
			float ring = sin(f * 3.14159); // 环形包络（边缘与中心弱，中环强）
			float moving = trample_energy[i]; // 0 站立 → 1 跑动
			float ripple = (0.75 + 0.25 * sin(TIME * 7.0 - d * 9.0)) * moving;
			vec3 push_w = vec3(dxz.x / max(d, 0.001), 0.0, dxz.y / max(d, 0.001))
					* (f * 0.06 + ring * 0.08) * ripple * w * w;
			push_w.y = -(f * 0.34 + ring * 0.10 * moving * eff_w) * w * w; // 压伏常驻；空中平滑归零
			mat3 im = mat3(inst);
			float s2 = max(dot(im[0], im[0]), 0.0001);
			VERTEX += transpose(im) * push_w / s2;
		}
	}
}

void fragment() {
	// 根深梢浅渐变（根部 bottom_shade → 梢部 1.04）
	// 树冠把 bottom_shade 调到 0.74：每个叶团的底面略压暗，
	// 叠上"越高的团越亮"就出来一整棵树上浅下深的层次。
	// （别压太狠 —— 这一项要再乘实例色和高度项，三处下限叠起来会把树压成墨绿疙瘩。）
	// + 旷野之息明暗色浪（幅度 0.14，不让亮波把深绿洗浅）
	vec3 base = albedo.rgb * COLOR.rgb * mix(bottom_shade, 1.04, h_rel)
			* (1.0 + (gust_v - 0.3) * 0.14);
	// 叶片纹理：叶形轮廓 alpha + 明暗细节（草的"贴图感"，边缘 scissor 裁出叶形）
	float shape_a = 1.0;
	if (use_blade_tex) {
		vec4 t = texture(blade_tex, UV);
		base *= t.rgb;
		shape_a = t.a;
	}
	// 覆盖噪声：低频决定成片、高频加碎雪颗粒。
	// 无雪时整段跳过（snow_amount=0 → cov 恒为 0，数学上与旧路径逐像素一致）：
	// 每片草叶像素省两次 value-noise（~32 次 hash/sin），是植被片元成本的大头；
	// 雪天仍走原路径，观感不变。
	ALBEDO = base;
	ROUGHNESS = 1.0;
	float cov = 0.0;
	if (snow_amount > 0.001) {
		float cov_n = vnoise(wpos.xz * 0.8 + vec2(3.7, 1.3)) * 0.7
				+ vnoise(wpos.xz * 3.1) * 0.3;
		cov = smoothstep(1.0 - snow_amount - 0.08, 1.0 - snow_amount + 0.08, cov_n)
				* snow_amount;
		// 雪色保留 15% 实例色底味（草雪偏绿、冠雪偏深，MC 雪层覆在方块上的层次）
		ALBEDO = mix(base, snow_color * (0.85 + 0.15 * COLOR.rgb), cov);
		ROUGHNESS = mix(1.0, 0.5, cov);
	}
	// 叶片透光：法线背对太阳时，光从叶子背面透过来（越"侧对"太阳越明显）。
	// NORMAL 在片元里是视图空间，先用 INV_VIEW_MATRIX 转回世界空间再和太阳方向比。
	// 这一项加在 ALBEDO 上而不是自发光：它是"被照亮"，要跟着叶色走、也要吃遮挡。
	if (backlight > 0.0) {
		vec3 wn = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
		float trans = pow(max(-dot(wn, normalize(sun_dir)), 0.0), 1.5);
		ALBEDO += backlight_color * (trans * backlight * (1.0 - cov));
	}
	METALLIC = 0.0;
	ALPHA = alpha * shape_a;
	// 关键：写了 ALPHA 会被踢进透明管线（树冠互相穿透的元凶）——
	// 加裁剪阈值回到不透明管线：普通植被 alpha=1 全通过；
	// 雪毯淡入退化为"过半显现"的像素裁切（观感可接受）
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
