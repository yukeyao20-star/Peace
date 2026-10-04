extends SceneTree

var frames := 0
var world: Node = null
var cam: Camera3D = null

const OUT_DIR := "/Users/lei/Downloads/3Dgame/docs/qa/"

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 2:
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.38) # 上午晴好光照
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
			sky.call("set_snow", 0.0)
			sky.call("set_snow_target", 0.0)

		# 创建独立测试相机
		cam = Camera3D.new()
		cam.name = "QACam"
		root.add_child(cam)
		cam.make_current()

	if frames == 8:
		# 视角 1: 站在小河中游 (58, 2.5, 76) 俯瞰碧蓝小溪与平缓水流
		cam.global_position = Vector3(64.0, 3.2, 78.0)
		cam.look_at(Vector3(56.0, 0.8, 72.0), Vector3.UP)
		print("Framing shot 1: Stream view...")

	if frames == 12:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png(OUT_DIR + "qa_stream_new.png")
		print("Saved: qa_stream_new.png")

		# 视角 2: 仰望壮丽大瀑布 (82, 4.0, 82) -> (96, 12.0, 96)
		cam.global_position = Vector3(82.0, 4.8, 82.0)
		cam.look_at(Vector3(95.0, 11.0, 95.0), Vector3.UP)
		print("Framing shot 2: Waterfall view...")

	if frames == 16:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png(OUT_DIR + "qa_waterfall_new.png")
		print("Saved: qa_waterfall_new.png")
		quit(0)
		return true

	return false
