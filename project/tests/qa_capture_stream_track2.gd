extends SceneTree

var frames := 0
var world: Node = null
var cam: Camera3D = null

func _initialize() -> void:
	world = load("res://world.tscn").instantiate()
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

		var um = world.get_node_or_null("UpdateManager")
		if um != null:
			um.queue_free()

		cam = Camera3D.new()
		cam.name = "QACam"
		root.add_child(cam)
		cam.make_current()
		cam.set("fov", 65.0)
		return false

	# 视角 1: 特写峡谷小溪上的小木桥 (44.0, 3.2, 62.0) 远眺架在小溪上的木桥 (50.0, 1.2, 69.4)
	if frames == 8:
		cam.look_at_from_position(Vector3(43.5, 3.0, 62.5), Vector3(50.0, 1.2, 69.4), Vector3.UP)
		return false

	if frames == 12:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/shot_stream_bridge_land.png")
		print("Saved: shot_stream_bridge_land.png")
		
		# 视角 2: 仰望壮丽悬崖瀑布 (82, 4.5, 82) -> (96, 12.0, 96)
		cam.look_at_from_position(Vector3(82.0, 4.5, 82.0), Vector3(96.0, 11.5, 96.0), Vector3.UP)
		return false

	if frames == 18:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/shot_waterfall_cs2.png")
		print("Saved: shot_waterfall_cs2.png")

		# 视角 3: 走上桥头视角 (53.0, 2.2, 64.5) -> (47.0, 1.0, 74.0)
		cam.look_at_from_position(Vector3(53.2, 2.2, 64.0), Vector3(48.5, 1.2, 71.5), Vector3.UP)
		return false

	if frames == 24:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/shot_bridge_crossing.png")
		print("Saved: shot_bridge_crossing.png")
		quit(0)
		return true

	return false
