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
		cam.set("fov", 65.0)
		# 1. 瀑布特写
		cam.look_at_from_position(Vector3(82.0, 4.8, 82.0), Vector3(95.0, 11.0, 95.0), Vector3.UP)
		return false

	if frames == 8:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_waterfall_v2.png")
		print("Saved waterfall v2")
		
		# 2. 小木桥与小溪
		cam.look_at_from_position(Vector3(56.0, 2.2, 74.0), Vector3(50.0, 1.2, 69.4), Vector3.UP)
		return false

	if frames == 14:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_bridge_v2.png")
		print("Saved bridge v2")
		
		# 3. 小溪入湖交界口
		cam.look_at_from_position(Vector3(45.0, 2.5, 70.0), Vector3(32.0, -0.55, 60.0), Vector3.UP)
		return false

	if frames == 20:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_lake_junction_v2.png")
		print("Saved lake junction v2")
		
		# 4. 木屋室内（看衣柜与木箱、开窗）
		# Homestead 是 (12, 0.35, 8) 绕 Y 45度
		var homestead = world.get_node_or_null("Homestead")
		if homestead != null:
			var w_trans = homestead.global_transform
			# 室内衣柜在 (1.8, 0.1, -2.6)，木箱在 (-1.4, 0.2, -2.5)
			# 相机放门内看向北侧墙角
			var c_local = Vector3(0.0, 1.4, 1.0)
			var t_local = Vector3(0.5, 1.0, -2.5)
			cam.look_at_from_position(w_trans * c_local, w_trans * t_local, Vector3.UP)
		return false

	if frames == 26:
		var img = root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_cabin_interior_v2.png")
		print("Saved cabin interior v2")
		quit(0)
		return true

	return false
