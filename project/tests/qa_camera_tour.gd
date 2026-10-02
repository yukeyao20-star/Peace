# 临时验证：新相机四机位实机截图——先传送、等相机平滑到位、再拍（用完即删）
extends SceneTree

var frames := 0
var spot_idx := -1
var world: Node = null
var player: Node = null
var done := false

const SPOTS := [
	["/Users/lei/Downloads/3Dgame/docs/qa_cam_spawn.png", Vector3(0.0, 0.2, -28.0)],
	["/Users/lei/Downloads/3Dgame/docs/qa_cam_lake.png", Vector3(-6.0, 0.4, 42.0)],
	["/Users/lei/Downloads/3Dgame/docs/qa_cam_bridge.png", Vector3(-21.0, 0.4, 33.0)],
	["/Users/lei/Downloads/3Dgame/docs/qa_cam_home.png", Vector3(12.0, 0.6, 18.0)],
	["/Users/lei/Downloads/3Dgame/docs/qa_cam_tallgrass.png", Vector3(-20.0, 0.3, -1.0)],
]

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")

func _process(_delta: float) -> bool:
	frames += 1
	if player == null:
		return false
	if frames == 2:
		# 固定在上午 11 点拍摄（存档可能把游戏时间留在夜里）
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.45)
	if frames == 15:
		spot_idx = 0
		player.set("global_position", SPOTS[0][1])
	elif spot_idx >= 0 and frames == 15 + spot_idx * 60 + 45:
		var img := root.get_viewport().get_texture().get_image()
		img.save_png(SPOTS[spot_idx][0])
		print("SHOT_", spot_idx, "_SAVED")
		if spot_idx + 1 < SPOTS.size():
			player.set("global_position", SPOTS[spot_idx + 1][1])
		spot_idx += 1
	if spot_idx >= SPOTS.size():
		done = true
	return done
