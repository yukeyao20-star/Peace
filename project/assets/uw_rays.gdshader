shader_type canvas_item;
render_mode blend_add, unshaded;

// 水下光柱 + 焦散网纹（屏幕空间叠加层，由 SkyCycle 在水下时调 uw_amount 淡入）
uniform float uw_amount : hint_range(0.0, 1.0) = 0.0;

void fragment() {
	vec2 uv = UV;
	float t = TIME;

	// 光柱：从右上方向下透进来的柔和斜条，随时间轻微摆动
	float slant = uv.x + uv.y * 0.55;
	float beam = 0.0;
	beam += pow(max(cos(slant * 13.0 + sin(uv.y * 2.2 + t * 0.25) * 0.5 + t * 0.18), 0.0), 12.0);
	beam += pow(max(cos(slant * 21.0 - t * 0.13 + 1.7), 0.0), 16.0) * 0.7;
	beam *= smoothstep(0.0, 0.45, uv.y) * (1.0 - uv.y * 0.55);

	// 焦散：三组正弦叠加出的缓慢流动网纹
	float c = sin(uv.x * 26.0 + t * 1.1) + sin(uv.y * 21.0 - t * 0.8)
			+ sin((uv.x * 0.7 + uv.y) * 30.0 + t * 0.6);
	float caustic = pow(max(c / 3.0, 0.0), 4.0);

	float a = (beam * 0.16 + caustic * 0.035) * uw_amount;
	COLOR = vec4(vec3(0.55, 0.92, 0.95), a);
}
