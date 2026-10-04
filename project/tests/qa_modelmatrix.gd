# 探针：CPUParticles3D 的顶点着色器里，MODEL_MATRIX 给的是"世界坐标"还是"节点局部坐标"？
#
# 做法：把一个 CPUParticles3D 挂到位于世界 (1000,0,0) 的父节点下，
# 粒子固定在发射点不动（速度 0、重力 0、lifetime 很长）。
# 着色器按 (MODEL_MATRIX * vec4(0,0,0,1)).x 判定：
#   > 500  → 红色（说明含父节点变换 = 世界坐标）
#   <= 500 → 绿色（说明是相对父节点的局部坐标）
# 相机对准它截图，取像素看颜色即可。
#
# 用法：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project -s tests/qa_modelmatrix.gd
extends SceneTree

const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

var frames := 0
var cam: Camera3D = null

const SHADER := """
shader_type spatial;
render_mode unshaded;
varying float v_flag;
void vertex() {
	v_flag = step(500.0, (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).x);
}
void fragment() {
	ALBEDO = v_flag > 0.5 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
}
"""

func _initialize() -> void:
	var holder := Node3D.new()
	holder.name = "Holder"
	holder.position = Vector3(1000.0, 0.0, 0.0)   # 父节点搬得很远
	root.add_child(holder)

	var mat := ShaderMaterial.new()
	var sh := Shader.new()
	sh.code = SHADER
	mat.shader = sh

	var quad := QuadMesh.new()
	quad.size = Vector2(3.0, 3.0)
	quad.material = mat

	var p := CPUParticles3D.new()
	p.name = "Probe"
	p.amount = 1
	p.lifetime = 100.0
	p.mesh = quad
	p.direction = Vector3(0.0, -1.0, 0.0)
	p.spread = 0.0
	p.gravity = Vector3.ZERO
	p.initial_velocity_min = 0.0
	p.initial_velocity_max = 0.0
	p.emission_shape = CPUParticles3D.EMISSION_SHAPE_POINT
	p.emitting = true
	holder.add_child(p)

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 20:
		cam = Camera3D.new()
		cam.fov = 60.0
		root.add_child(cam)
		cam.current = true
		cam.global_position = Vector3(1000.0, 0.0, 6.0)
		cam.look_at(Vector3(1000.0, 0.0, 0.0), Vector3.UP)
	if frames == 60:
		var img := root.get_viewport().get_texture().get_image()
		img.save_png(OUT + "diag3_modelmatrix.png")
		# 直接读中心像素
		var w := img.get_width()
		var h := img.get_height()
		var c := img.get_pixel(w / 2, h / 2)
		print("CENTER_PIXEL=", c)
		print("判定：红=世界坐标(含父节点变换)  绿=节点局部坐标")
		return true
	return false
