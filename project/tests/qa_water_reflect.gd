# 临时验证：水面反射到底吃不吃环境（SSR / 天空）。
# 机位贴水、横看石桥 —— 如果水面能映出桥和岸，说明 SSR 生效；
# 若水面只剩一片"渐变天空色"，说明它还在透明通道里，SSR 没作用到。
# 用法（非 headless）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_water_reflect.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var done := false
var shot := 0

# [输出名, 相机位置, 看向点, 说明]
const SHOTS := [
	["reflect_bridge_low.png", Vector3(6.0, 0.35, 46.0), Vector3(-6.0, -0.20, 26.0)],
	["reflect_bridge_side.png", Vector3(26.0, 1.1, 40.0), Vector3(-8.0, -0.15, 26.0)],
	["reflect_open.png", Vector3(4.0, 0.30, 62.0), Vector3(4.0, -0.30, 96.0)],
]

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
			sky.set("time_of_day", 0.38)
			# 强制晴天：雨点涟漪会盖住水本身的观感，先看"默认水色"。
			# 注意：weather/rain/cloud 只绑了方法没绑属性，必须用 call()。
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
			sky.call("set_snow", 0.0)
			sky.call("set_snow_target", 0.0)
			print("WEATHER_NOW=", sky.call("get_weather"))
	if frames == 20:
		cam = Camera3D.new()
		cam.set("fov", 60.0)
		root.add_child(cam)
		cam.set("current", true)
	if frames > 20:
		var idx := int((frames - 20) / 40)
		if idx < SHOTS.size():
			var s = SHOTS[idx]
			cam.set("global_position", s[1])
			cam.look_at(s[2], Vector3(0.0, 1.0, 0.0))
			if (frames - 20) % 40 == 36:
				var img := root.get_viewport().get_texture().get_image()
				img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/" + s[0])
				print("SHOT_SAVED_", s[0])
		else:
			done = true
	return done
