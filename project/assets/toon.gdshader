// 卡通渲染（toon shading）着色器
//
// 原理：让引擎用"阶跃"而非"平滑"的方式处理光照，得到无渐变的色块感，
// 这是《双人成行》/ 任天堂一类风格化画面的基础手段之一。
// RIM 额外加一层边缘光，让物体从背景中"跳"出来。

shader_type spatial;
render_mode diffuse_toon, specular_toon, cull_back;

uniform vec4 albedo : source_color = vec4(0.92, 0.55, 0.35, 1.0);
uniform float rim : hint_range(0.0, 1.0) = 0.5;
uniform float roughness_value : hint_range(0.0, 1.0) = 0.85;

void fragment() {
	ALBEDO = albedo.rgb;
	METALLIC = 0.0;
	ROUGHNESS = roughness_value;
	RIM = rim;
}
