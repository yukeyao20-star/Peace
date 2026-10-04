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
		# 俯视小溪流与大湖交汇口
		cam.look_at_from_position(Vector3(42.0, 3.8, 72.0), Vector3(32.0, -0.4, 60.0), Vector3.UP)
		return false

	if frames == 12:
		var img = root.get_viewport().get_texture().get_image()
		var err = img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_lake_junction_real.png")
		print("Save lake junction result: ", err)
		
		# 换机位拍木屋室内衣柜、木箱与窗户
		var homestead = world.get_node_or_null("Homestead")
		if homestead != null:
			var w_trans = homestead.global_transform
			var c_local = Vector3(0.0, 1.2, 0.5)
			var t_local = Vector3(0.5, 0.8, -2.6)
			cam.look_at_from_position(w_trans * c_local, w_trans * t_local, Vector3.UP)
		return false

	if frames == 20:
		var img = root.get_viewport().get_texture().get_image()
		var err = img.save_png("/Users/lei/Downloads/3Dgame/docs/qa/qa_cabin_interior_real.png")
		print("Save cabin interior result: ", err)
		quit(0)
		return true

	return false
