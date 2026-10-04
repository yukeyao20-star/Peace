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

		cam = Camera3D.new()
		cam.name = "QACam"
		root.add_child(cam)
		cam.make_current()
		cam.set("fov", 60.0)
		cam.look_at_from_position(Vector3(64.0, 3.2, 78.0), Vector3(56.0, 0.8, 72.0), Vector3.UP)
		return false

	if frames == 12:
		var img = root.get_viewport().get_texture().get_image()
		var err = img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_stream_new.png")
		print("Save stream result: ", err)
		
		# 换机位拍瀑布
		cam.look_at_from_position(Vector3(82.0, 4.8, 82.0), Vector3(95.0, 11.0, 95.0), Vector3.UP)
		return false

	if frames == 20:
		var img = root.get_viewport().get_texture().get_image()
		var err = img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_waterfall_new.png")
		print("Save waterfall result: ", err)
		quit(0)
		return true

	return false
