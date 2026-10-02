// 荷叶 / 荷花共用材质：让它们跟着湖面一起起伏、倾斜。
//
// 荷叶是平贴水面的薄片，荷花是浮在水上的花。如果水在动、它们不动，一眼就露馅 ——
// 像贴在玻璃上。做法：顶点里在**各自的世界坐标**处采一次水面波高，再把这一位移加回本地空间。
// 因为一片荷叶的各个顶点世界坐标不同，荷叶会跟着波面自然倾斜，而不只是整体上下平移。
//
// 注意：wave_sum 必须与 water.gdshader 里的那份逐字一致，否则荷叶和水面会各晃各的。
// 水面波高变了（wave_height / wave_speed）也要两边一起改。

shader_type spatial;
render_mode cull_disabled; // 薄片，正反两面都要看得见

uniform float wave_height : hint_range(0.0, 0.5) = 0.075; // == water.gdshader wave_height
uniform float wave_speed : hint_range(0.0, 4.0) = 1.1;    // == water.gdshader wave_speed
uniform float bob_extra = 0.045;  // 比水面多晃一点（本身轻，随波起伏更明显）
uniform vec3 tint = vec3(1.0, 1.0, 1.0);
uniform vec3 emission : source_color = vec3(0.0); // 荷花用：花瓣自发光（荷叶留 0）

// —— 与 water.gdshader 的 wave_sum 完全一致 ——
float wave_sum(vec2 p, float t) {
	return sin(p.x * 1.35 + t * 1.20) * 0.45
			+ sin(p.y * 1.72 + t * 0.95) * 0.30
			+ sin(dot(p, vec2(0.8, 1.15)) + t * 1.65) * 0.35
			+ sin(dot(p, vec2(2.35, -1.90)) + t * 2.40) * 0.18
			+ sin(dot(p, vec2(0.60, 0.80)) * 0.42 + t * 0.72) * 0.34;
}

void vertex() {
	// Godot 4 渲染 MultiMesh 时 MODEL_MATRIX 已含实例变换（yaw / 倾斜 / 缩放）
	mat4 inst = MODEL_MATRIX;
	vec3 wp = (inst * vec4(VERTEX, 1.0)).xyz;

	float dy = wave_sum(wp.xz, TIME * wave_speed) * (wave_height + bob_extra);

	// 把"世界 Y 方向抬高 dy"换算回本地空间：
	//   inst = R·S（旋转 × 等比缩放）→ 本地位移 = transpose(basis)·(0,dy,0) / scale
	// 荷叶实例带倾斜，不能简单地 VERTEX.y += dy（那会沿倾斜后的本地 Y 走，抬不够）
	mat3 basis = mat3(inst);
	float sc = max(length(basis[1]), 0.0001);
	VERTEX += transpose(basis) * vec3(0.0, dy, 0.0) / sc;
}

void fragment() {
	// 实例色（MultiMesh 的 COLOR 已含实例色）决定荷叶深浅
	ALBEDO = COLOR.rgb * tint;
	ROUGHNESS = 0.62; // 荷叶表面有一点蜡质光泽，但比水面哑得多
	METALLIC = 0.0;
	EMISSION = emission;
}
