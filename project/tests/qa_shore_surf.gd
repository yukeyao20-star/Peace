# 临时验证：湖岸礁石是否生成 + 水面着色器是否收到 shore_rocks + 拍岸浪特写
# 用法（非 headless）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_shore_surf.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var done := false
var reef_pos := Vector3.ZERO
var has_reef := false

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 2:
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.45)

		# —— 查礁石 ——
		var terrain = world.get_node_or_null("Terrain")
		if terrain != null:
			var reefs = terrain.get_node_or_null("ShoreReefs")
			print("REEF_NODE=", reefs != null)
			if reefs != null:
				var mm = reefs.get("multimesh")
				print("REEF_COUNT=", mm.get("instance_count"))
				if mm.get("instance_count") > 0:
					var t: Transform3D = mm.get_instance_transform(0)
					reef_pos = t.origin
					has_reef = true
					print("REEF0_POS=", reef_pos)
			var lake = terrain.get_node_or_null("Lake")
			if lake != null:
				var mat: Material = lake.get("mesh").surface_get_material(0)
				if mat != null and mat is ShaderMaterial:
					print("SHADER_ROCK_COUNT=", mat.get_shader_parameter("shore_rock_count"))
					var arr = mat.get_shader_parameter("shore_rocks")
					if arr != null and arr.size() > 0:
						print("SHADER_ROCK0=", arr[0])

	if frames == 25:
		cam = Camera3D.new()
		cam.set("fov", 55.0)
		root.add_child(cam)
		cam.set("current", true)

	if frames > 25 and has_reef:
		# 相机悬在礁石斜上方看它（水面在 y=-0.55）
		var look := Vector3(reef_pos.x, -0.45, reef_pos.z)
		cam.set("global_position", Vector3(reef_pos.x + 4.2, 1.5, reef_pos.z + 4.2))
		cam.look_at(look, Vector3(0.0, 1.0, 0.0))
		if frames == 70:
			var img := root.get_viewport().get_texture().get_image()
			img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/shore_reef.png")
			print("REEF_SHOT_SAVED")
			done = true
	elif frames > 90:
		done = true
	return done
