shader_type canvas_item;

// 拍照模式滤镜：读取当前屏幕并做暖调 / 冷调 / 黑白处理。
uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;
uniform int mode = 0; // 0 原色 / 1 暖调 / 2 冷调 / 3 黑白
uniform float strength = 0.85;

void fragment() {
	vec3 c = texture(screen_tex, SCREEN_UV).rgb;
	if (mode == 1) {
		vec3 warm = c * vec3(1.10, 0.99, 0.80) + vec3(0.030, 0.012, 0.0);
		c = mix(c, warm, strength);
	} else if (mode == 2) {
		vec3 cool = c * vec3(0.84, 0.97, 1.14) + vec3(0.0, 0.010, 0.030);
		c = mix(c, cool, strength);
	} else if (mode == 3) {
		float g = dot(c, vec3(0.299, 0.587, 0.114));
		c = mix(c, vec3(g), strength);
	}
	COLOR = vec4(c, 1.0);
}
