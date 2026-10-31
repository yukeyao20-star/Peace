shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never, fog_disabled;

uniform vec4 color_a : source_color = vec4(0.2, 1.0, 0.6, 1.0);
uniform vec4 color_b : source_color = vec4(0.4, 0.4, 1.0, 1.0);
uniform float strength : hint_range(0.0, 1.0) = 0.0;
uniform float phase = 0.0;

varying vec2 v_uv;

void vertex() {
	v_uv = UV;
	vec3 v = VERTEX;
	v.y += sin(UV.x * 7.0 + TIME * 0.15 + phase) * 3.0;
	v.z += cos(UV.x * 5.0 + TIME * 0.10 + phase) * 2.0;
	VERTEX = v;
}

void fragment() {
	float band = smoothstep(0.0, 0.25, v_uv.y) * (1.0 - smoothstep(0.55, 1.0, v_uv.y));
	float side = smoothstep(0.0, 0.15, v_uv.x) * (1.0 - smoothstep(0.85, 1.0, v_uv.x));
	float streak = 0.55 + 0.45 * sin(v_uv.x * 23.0 + TIME * 0.6 + phase);
	float fine = 0.70 + 0.30 * sin(v_uv.x * 61.0 - TIME * 0.9 + phase * 2.0);
	vec3 col = mix(color_a.rgb, color_b.rgb, smoothstep(0.10, 0.90, v_uv.y));
	ALBEDO = col;
	ALPHA = band * side * strength * (0.20 + 0.26 * streak) * fine;
}
