shader_type canvas_item;

// 拍照模式暗角：只在画面边缘压暗，中心保持清澈。
uniform float strength = 0.55;
uniform float inner = 0.34;
uniform vec3 tint : source_color = vec3(0.02, 0.025, 0.03);

void fragment() {
	vec2 uv = UV - 0.5;
	float d = length(uv * vec2(1.15, 1.0)) * 1.42;
	float v = smoothstep(inner, 1.0, d) * strength;
	COLOR = vec4(tint, v);
}
