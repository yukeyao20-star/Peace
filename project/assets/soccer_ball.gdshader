// 经典黑白 32 面缝线足球着色器 (截角二十面体 Voronoi)
// 12 颗正五边形黑色皮革面板 + 20 颗正六边形白色皮革面板 + 凹槽立体缝线
shader_type spatial;
render_mode cull_back;

uniform vec4 color_white : source_color = vec4(0.96, 0.96, 0.95, 1.0);
uniform vec4 color_black : source_color = vec4(0.12, 0.12, 0.13, 1.0);
uniform vec4 color_seam  : source_color = vec4(0.04, 0.04, 0.05, 1.0);

varying vec3 local_dir;

const vec3 PENT_CENTERS[12] = vec3[12](
    vec3(0.000000, 0.525731, 0.850651),
    vec3(0.000000, -0.525731, 0.850651),
    vec3(0.000000, 0.525731, -0.850651),
    vec3(0.000000, -0.525731, -0.850651),
    vec3(0.525731, 0.850651, 0.000000),
    vec3(-0.525731, 0.850651, 0.000000),
    vec3(0.525731, -0.850651, 0.000000),
    vec3(-0.525731, -0.850651, 0.000000),
    vec3(0.850651, 0.000000, 0.525731),
    vec3(-0.850651, 0.000000, 0.525731),
    vec3(0.850651, 0.000000, -0.525731),
    vec3(-0.850651, 0.000000, -0.525731)
);

const vec3 HEX_CENTERS[20] = vec3[20](
    vec3(0.356822, 0.000000, 0.934172),
    vec3(-0.356822, 0.000000, 0.934172),
    vec3(0.000000, 0.934172, 0.356822),
    vec3(0.577350, 0.577350, 0.577350),
    vec3(-0.577350, 0.577350, 0.577350),
    vec3(0.000000, -0.934172, 0.356822),
    vec3(0.577350, -0.577350, 0.577350),
    vec3(-0.577350, -0.577350, 0.577350),
    vec3(0.356822, 0.000000, -0.934172),
    vec3(-0.356822, 0.000000, -0.934172),
    vec3(0.000000, 0.934172, -0.356822),
    vec3(0.577350, 0.577350, -0.577350),
    vec3(-0.577350, 0.577350, -0.577350),
    vec3(0.000000, -0.934172, -0.356822),
    vec3(0.577350, -0.577350, -0.577350),
    vec3(-0.577350, -0.577350, -0.577350),
    vec3(0.934172, 0.356822, 0.000000),
    vec3(-0.934172, 0.356822, 0.000000),
    vec3(0.934172, -0.356822, 0.000000),
    vec3(-0.934172, -0.356822, 0.000000)
);

void vertex() {
	local_dir = normalize(VERTEX);
}

void fragment() {
	vec3 p = normalize(local_dir);

	// 找出最近的五边形中心 (dot 最大)
	float max_p_dot = -2.0;
	vec3 best_p = vec3(0.0);
	for (int i = 0; i < 12; i++) {
		float d = dot(p, PENT_CENTERS[i]);
		if (d > max_p_dot) {
			max_p_dot = d;
			best_p = PENT_CENTERS[i];
		}
	}

	// 找出最近与次近的六边形中心
	float max_h1_dot = -2.0;
	float max_h2_dot = -2.0;
	vec3 best_h1 = vec3(0.0);
	for (int i = 0; i < 20; i++) {
		float d = dot(p, HEX_CENTERS[i]);
		if (d > max_h1_dot) {
			max_h2_dot = max_h1_dot;
			max_h1_dot = d;
			best_h1 = HEX_CENTERS[i];
		} else if (d > max_h2_dot) {
			max_h2_dot = d;
		}
	}

	// 判定归属面板类型：
	// 若到五边形中心距离更近（max_p_dot > max_h1_dot），则为五边形（黑色面板）
	// 否则为六边形（白色面板）
	bool is_pentagon = (max_p_dot > max_h1_dot);

	// 计算到缝线的角距离边缘：
	// 缝线即 Voronoi 胞体边缘（两个相邻中心到当前点的距离相等之处）
	float seam_dist = 0.0;
	if (is_pentagon) {
		// 五边形胞体与最近六边形的边界线：max_p_dot 与 max_h1_dot 的差值
		seam_dist = max_p_dot - max_h1_dot;
	} else {
		// 六边形可能与相邻五边形接壤，也可能与相邻六边形接壤
		float diff_p = max_h1_dot - max_p_dot;
		float diff_h = max_h1_dot - max_h2_dot;
		seam_dist = min(diff_p, diff_h);
	}

	// 皮革凸起质感（中央微隆，向缝线处微陷）
	float bulge = smoothstep(0.0, 0.06, seam_dist);
	// 缝线凹槽（seam_dist < 0.016 为缝线深沟）
	float seam_line = smoothstep(0.006, 0.016, seam_dist);

	vec3 panel_color = is_pentagon ? color_black.rgb : color_white.rgb;
	// 皮革边缘柔和阴影，增加立体饱满感
	panel_color *= (0.86 + 0.14 * bulge);

	// 混合缝线颜色与面板颜色
	vec3 final_color = mix(color_seam.rgb, panel_color, seam_line);

	ALBEDO = final_color;
	ROUGHNESS = is_pentagon ? 0.38 : 0.32;
	SPECULAR = 0.55;
	METALLIC = 0.0;
}
