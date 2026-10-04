extends SceneTree

var frames := 0
var world: Node = null
var cam: Camera3D = null

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 2:
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.38)
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
			sky.call("set_snow", 0.0)
			sky.call("set_snow_target", 0.0)

		# 关闭玩家与更新弹窗干扰
		var um = world.get_node_or_null("UpdateManager")
		if um != null:
			um.queue_free()

		cam = Camera3D.new()
		cam.name = "QACam"
		root.add_child(cam)
		cam.make_current()
		cam.set("fov", 65.0)
		return false

	# 拍中游俯瞰 (60, 4.0, 75) 看 (56, 0.8, 72)
	if frames == 10:
		cam.look_at_from_position(Vector3(62.0, 3.8, 77.0), Vector3(56.0, 0.8, 72.0), Vector3.UP)
		return false

	if frames == 14:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/shot_stream_mid.png")
		print("Saved: shot_stream_mid.png")
		
		# 拍近距离低角度贴水面 (57, 1.8, 74) 看向河道顺流 (45, 0.0, 68)
		cam.look_at_from_position(Vector3(58.0, 1.8, 74.0), Vector3(48.0, 0.2, 69.0), Vector3.UP)
		return false

	if frames == 18:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/shot_stream_close.png")
		print("Saved: shot_stream_close.png")

		# 拍瀑布近景 (86, 5.0, 86) 仰拍飞瀑水帘与底潭 (94, 12.0, 94)
		cam.look_at_from_position(Vector3(86.0, 5.0, 86.0), Vector3(95.0, 10.0, 95.0), Vector3.UP)
		return false

	if frames == 24:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/shot_waterfall_close.png")
		print("Saved: shot_waterfall_close.png")
		quit(0)
		return true

	return false
