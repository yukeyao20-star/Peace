// 低模岩石材质：配合 make_rock_mesh 的棱面网格（逐面法线）使用。
//
// 石头显假的两个原因：① 光滑球体 —— 永远是"蛋"；② 单色平涂 —— 一块死灰。
// ① 靠几何（噪声位移 + flat shading）解决，② 靠这里的**世界坐标噪声斑驳**：
// 不用任何贴图，直接在世界空间采两个尺度的值噪声，叠出大块斑纹 + 细碎颗粒，
// 让每一块石头表面都不一样。再加一条"水线以下压暗"——被水泡透的湿石，
// 这是礁石和岸边干石头的关键区别（参考实拍：水下的石头是暗的）。

shader_type spatial;

uniform float mottle : hint_range(0.0, 1.0) = 0.46;       // 斑驳强度
uniform float mottle_scale : hint_range(0.1, 12.0) = 3.6; // 斑驳密度（一块石头跨 3~5 个噪声格才看得见）
uniform float water_line = -0.55;                         // 静水线（湿石分界）
uniform float wet_darken : hint_range(0.0, 1.0) = 0.50;   // 水线以下压暗程度
uniform float wet_gloss : hint_range(0.0, 1.0) = 0.55;    // 湿石更滑一点

varying vec3 v_world;

float hash31(vec3 p) {
	p = fract(p * vec3(127.1, 311.7, 74.7));
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float vnoise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float n000 = hash31(i);
	float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
	float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
	float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
	float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
	float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
	float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
	float n111 = hash31(i + vec3(1.0, 1.0, 1.0));
	return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
			mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

void vertex() {
	v_world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	// 三个尺度叠加：低频大斑块 + 中频石纹 + 高频细颗粒。
	// 只用一两个尺度时，小石头（半径半米）整个表面落在同一个噪声格里 → 又变平涂。
	float n = vnoise3(v_world * mottle_scale) * 0.50
			+ vnoise3(v_world * mottle_scale * 2.7 + 17.0) * 0.30
			+ vnoise3(v_world * mottle_scale * 7.3 + 41.0) * 0.20;
	vec3 base = COLOR.rgb * (1.0 - mottle + mottle * (0.35 + 1.30 * n));

	// 水线以下 = 湿石：压暗、偏冷、更滑（在水面处平滑过渡，不留硬边）
	float wet = 1.0 - smoothstep(water_line - 0.06, water_line + 0.24, v_world.y);
	base = mix(base, base * vec3(0.50, 0.57, 0.66), wet * wet_darken);

	ALBEDO = base;
	ROUGHNESS = mix(0.96 - 0.12 * n, 0.52, wet * wet_gloss);
	SPECULAR = 0.30;
	METALLIC = 0.0;
}
