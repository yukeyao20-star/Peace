# 临时验证：桥/地面交接处的频闪是不是阴影 acne（而不是 z-fighting）。
# 同一机位拍两张：Sun 阴影开 / 关。若两张只差在"影子"上 → 是阴影问题；
# 若关掉阴影后交接处那种"碎纹/闪烁"仍在 → 是几何共面问题。
# 用法（非 headless）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_shadow_acne.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var sun: Node = null
var done := false

# [输出后缀, 相机位置, 看向点]
const SPOTS := [
	["bridge_end", Vector3(-19.0, 1.55, 26.0), Vector3(-24.0, -0.30, 26.0)],
	["ground_look", Vector3(-26.5, 1.60, 26.0), Vector3(-21.0, -0.25, 26.0)],
]

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")
	sun = world.get_node_or_null("Sun")

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
		cam.set("near", 0.1)
		cam.set("far", 800.0)
		root.add_child(cam)
		cam.set("current", true)

	# 每个 spot 用 60 帧：0-29 阴影开，30-59 阴影关
	if frames > 15:
		var idx := int((frames - 15) / 60)
		if idx < SPOTS.size():
			var s = SPOTS[idx]
			var local := (frames - 15) % 60
			cam.set("global_position", s[1])
			cam.look_at(s[2], Vector3(0.0, 1.0, 0.0))
			if local == 0:
				if sun != null:
					sun.set("shadow_enabled", true)
			elif local == 30:
				if sun != null:
					sun.set("shadow_enabled", false)
			elif local == 25:
				root.get_viewport().get_texture().get_image().save_png(
						"/Users/lei/Downloads/3Dgame/docs/qa/acne_%s_on.png" % s[0])
				print("ACNE_ON_", s[0])
			elif local == 55:
				root.get_viewport().get_texture().get_image().save_png(
						"/Users/lei/Downloads/3Dgame/docs/qa/acne_%s_off.png" % s[0])
				print("ACNE_OFF_", s[0])
		else:
			done = true
	return done
