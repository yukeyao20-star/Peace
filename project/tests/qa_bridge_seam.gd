# 临时验证：石桥两端引道接缝特写（排查渲染频闪 / 图层互相覆盖）
# 用法（非 headless，否则截不了图）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1920x1080 \
#       -s tests/qa_bridge_seam.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var done := false

# [输出路径, 相机位置, 看向点]
const SHOTS := [
	["/Users/lei/Downloads/3Dgame/docs/qa/seam_west_close.png", Vector3(-25.5, 1.2, 26.0), Vector3(-21.0, -0.25, 26.0)],
	["/Users/lei/Downloads/3Dgame/docs/qa/seam_west_side.png", Vector3(-23.0, 1.0, 32.5), Vector3(-21.5, -0.24, 26.0)],
	["/Users/lei/Downloads/3Dgame/docs/qa/seam_east_close.png", Vector3(13.5, 1.2, 26.0), Vector3(9.0, -0.25, 26.0)],
	["/Users/lei/Downloads/3Dgame/docs/qa/seam_west_far.png", Vector3(-52.0, 3.0, 26.0), Vector3(-21.0, -0.24, 26.0)],
]

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 2:
		# 冻结玩家（它的相机会每帧 look_at 主角，抢走镜头）
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.42) # 上午侧光，接缝阴影最明显
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
	if frames == 20:
		cam = Camera3D.new()
		cam.set("fov", 55.0)
		root.add_child(cam)
		cam.set("current", true)
	if frames > 20:
		var idx := int((frames - 20) / 45)
		if idx < SHOTS.size():
			var s = SHOTS[idx]
			cam.set("global_position", s[1])
			cam.look_at(s[2], Vector3(0.0, 1.0, 0.0))
			if (frames - 20) % 45 == 40:
				var img := root.get_viewport().get_texture().get_image()
				img.save_png(s[0])
				print("SEAM_SHOT_SAVED_", idx)
		else:
			done = true
	return done
