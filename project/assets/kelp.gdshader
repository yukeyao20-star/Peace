shader_type spatial;
// 湖底水草：窄长叶片随水慢慢摆动，底部深绿、顶端浅绿。
render_mode cull_disabled, specular_schlick_ggx;

uniform vec3 base_color : source_color = vec3(0.06, 0.19, 0.13);
uniform vec3 tip_color : source_color = vec3(0.24, 0.47, 0.29);
uniform float sway_amount = 0.14;

void vertex() {
	// 每株水草以自身世界坐标为相位，摆向由实例旋转决定，整体不会同步摆动
	vec3 origin = MODEL_MATRIX[3].xyz;
	float ph = origin.x * 0.42 + origin.z * 0.31;
	float h = UV.y;
	VERTEX.x += sin(TIME * 1.15 + ph + h * 2.4) * sway_amount * h;
	VERTEX.z += cos(TIME * 0.87 + ph * 1.37 + h * 1.9) * sway_amount * 0.55 * h;
}

void fragment() {
	float h = clamp(UV.y, 0.0, 1.0);
	ALBEDO = mix(base_color, tip_color, h);
	ROUGHNESS = 0.9;
	METALLIC = 0.0;
	EMISSION = tip_color * 0.10 * h;
	BACKLIGHT = tip_color * 0.25 * h;
}
