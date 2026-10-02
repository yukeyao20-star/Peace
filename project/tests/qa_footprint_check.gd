# 临时验证：脚印系统是否真的生效（5 种足迹各生成一枚，俯视截图）
# 用法（非 headless）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_footprint_check.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var fm: Node = null
var done := false

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")
	fm = world.get_node_or_null("FootprintManager")

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 2:
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
			player.set("global_position", Vector3(0.0, 0.2, -28.0))
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.45)
		print("FM_NODE=", fm != null)

	if frames == 25:
		cam = Camera3D.new()
		cam.set("fov", 60.0)
		root.add_child(cam)
		cam.set("current", true)

	if frames == 30:
		# 在玩家前方一字排开生成 5 种足迹（type 0..4）
		var base := Vector3(0.0, 0.06, -26.0)
		if fm != null:
			for i in range(5):
				var p := Vector3(base.x - 1.2 + float(i) * 0.6, base.y, base.z)
				fm.call("spawn_footprint", i, p, 0.0, 1.0)
				print("SPAWN_", i, "_AT_", p)
			print("FM_POOL_SIZE=", fm.get_child_count())

	if frames > 30:
		cam.set("global_position", Vector3(0.0, 3.4, -23.6))
		cam.look_at(Vector3(0.0, 0.0, -26.0), Vector3(0.0, 1.0, 0.0))
		if frames == 55:
			var img := root.get_viewport().get_texture().get_image()
			img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/footprint_check.png")
			print("FOOTPRINT_SHOT_SAVED")
			done = true
	return done
