// 船帆：布料随风的顶点起伏 + 帆面上的 Peace 字样（文字跟随 UV，和帆布一起动）
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform sampler2D sail_tex : source_color, filter_linear, repeat_disable;
uniform vec4 base_color : source_color = vec4(0.94, 0.90, 0.78, 1.0);
uniform float wind : hint_range(0.0, 2.0) = 1.0;
uniform float sail_w = 5.0;
uniform float sail_h = 3.75;
uniform float sail_top = 6.45;
varying vec2 cloth_uv;

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
	// 用顶点自身的局部坐标推 UV（不依赖 SurfaceTool 的 UV 通道），
	// 再把同一份 UV 交给布料位移后的片元，文字就跟着布褶一起动。
	vec3 orig = VERTEX;
	cloth_uv = vec2((orig.x + sail_w * 0.5) / sail_w, (sail_top - orig.y) / sail_h);
	float u = clamp(cloth_uv.x, 0.0, 1.0);
	float v = clamp(cloth_uv.y, 0.0, 1.0);
	float edge = sin(3.1415926 * u) * sin(3.1415926 * v);
	// 有机布褶：两层慢噪声 + 一层顺风起伏，避免整齐的竖条
	float n1 = vnoise(vec2(u * 5.0 + TIME * 0.22, v * 3.5 - TIME * 0.16)) - 0.5;
	float n2 = vnoise(vec2(u * 12.0 - TIME * 0.35, v * 8.0 + TIME * 0.28)) - 0.5;
	float gust = sin(TIME * 1.1 + u * 4.0 + v * 2.5) * 0.035;
	VERTEX.z += (n1 * 0.14 + n2 * 0.05 + gust) * edge * wind;
	VERTEX.x += 0.03 * sin(TIME * 0.75 + v * 4.0 + u * 2.0) * edge * wind;
	VERTEX.y += (n2 * 0.03) * edge * wind;
}

void fragment() {
	vec2 uv = clamp(cloth_uv, vec2(0.0), vec2(1.0));
	// 网格正面朝船头，背面朝船尾；两面都要读到正的 Peace，所以按视角翻转 U
	if (FRONT_FACING) {
		uv.x = 1.0 - uv.x;
	}
	vec3 cloth = texture(sail_tex, uv).rgb;
	// 布褶明暗：与顶点位移同源的慢噪声。unshaded 的帆如果没有这层明暗，
	// 看起来就是一块平色板——褶皱要能"被看见"才算布料。
	float n1 = vnoise(vec2(uv.x * 5.0 + TIME * 0.22, uv.y * 3.5 - TIME * 0.16)) - 0.5;
	float n2 = vnoise(vec2(uv.x * 12.0 - TIME * 0.35, uv.y * 8.0 + TIME * 0.28)) - 0.5;
	float fold = 0.88 + 0.24 * n1 + 0.10 * n2;
	// 帆腹受光：弧面中央比两侧亮，静止时也能看出"被风撑满"的鼓形
	fold *= 0.93 + 0.09 * sin(3.1415926 * uv.x) * sin(3.1415926 * clamp(uv.y * 1.15, 0.0, 1.0));
	// 四边收紧处略暗，帆布像被绳边勒住
	float edge = smoothstep(0.0, 0.07, uv.x) * smoothstep(1.0, 0.93, uv.x)
			* smoothstep(0.0, 0.06, uv.y) * smoothstep(1.0, 0.94, uv.y);
	fold *= mix(0.86, 1.0, edge);
	ALBEDO = cloth * fold;
}
