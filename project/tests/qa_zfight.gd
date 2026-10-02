# 临时验证：抓 z-fighting（渲染频闪）。
# 同一机位把相机横移 2cm 各拍一帧 —— 共面重叠处两帧的像素会明显不同，
# 正常几何只会整体平移一点点。相机参数按玩家相机（near 0.1 / far 800）设置，
# 否则测出来的不是玩家看到的东西。
# 用法（非 headless）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_zfight.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var done := false
var shot := 0

# [相机基准位置, 看向点]
const SPOTS := [
	[Vector3(-19.5, 0.60, 26.0), Vector3(-23.0, -0.30, 26.0)], # 站在桥西端看桥台/地面交接
	[Vector3(-25.5, 0.70, 26.0), Vector3(-21.5, -0.25, 26.0)], # 站在西岸地面看桥台
	[Vector3(-16.0, 0.55, 30.5), Vector3(-22.0, -0.40, 26.0)], # 斜看引道下坡入水处
]

const FRAMES_PER_SHOT := 30
const MOVE := 0.02 # 横移量（米）

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
			sky.set("time_of_day", 0.42)
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
	if frames == 15:
		cam = Camera3D.new()
		cam.set("fov", 60.0)
		cam.set("near", 0.1)   # 与玩家相机一致
		cam.set("far", 800.0)
		root.add_child(cam)
		cam.set("current", true)
	if frames > 15:
		var idx := int((frames - 15) / FRAMES_PER_SHOT)
		if idx < SPOTS.size():
			var s = SPOTS[idx]
			var local := (frames - 15) % FRAMES_PER_SHOT
			# 第 0 帧基准，第 15 帧横移 MOVE
			var offset := 0.0 if local < 15 else MOVE
			var pos: Vector3 = s[0] + Vector3(0.0, 0.0, offset)
			cam.set("global_position", pos)
			cam.look_at(s[1], Vector3(0.0, 1.0, 0.0))
			if local == 10:
				root.get_viewport().get_texture().get_image().save_png(
						"/Users/lei/Downloads/3Dgame/docs/qa/zf_%d_a.png" % idx)
				print("ZF_A_", idx)
			elif local == 25:
				root.get_viewport().get_texture().get_image().save_png(
						"/Users/lei/Downloads/3Dgame/docs/qa/zf_%d_b.png" % idx)
				print("ZF_B_", idx)
		else:
			done = true
	return done
