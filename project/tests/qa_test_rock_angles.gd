# 360 度多角度环绕拍摄水边礁石和陆地石头，检查在哪些角度下会出现透明/穿透/破面
extends SceneTree

var frames := 0
var world: Node = null
var cam: Camera3D = null

const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

# 检查目标：
# 1. 湖中巨礁 DeepReef
# 2. 湖岸礁石 ShoreReefs[0]
# 3. 陆地岩石 Rocks
var targets: Array = []
var target_idx := 0
var angle_step := 0

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

var wait_frames := 0

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
		var player = world.get_node_or_null("Player")
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.40)
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)

		var terrain = world.get_node_or_null("Terrain")
		if terrain != null:
			# 1. 收集 DeepReef
			var dr = terrain.get_node_or_null("DeepReef")
			if dr != null:
				var mm = dr.get("multimesh")
				if mm != null and mm.get("instance_count") > 0:
					targets.append({"name": "deep_reef", "pos": mm.get_instance_transform(0).origin, "radius": 2.5})
			# 2. 收集 ShoreReefs
			var sr = terrain.get_node_or_null("ShoreReefs")
			if sr != null:
				var mm = sr.get("multimesh")
				if mm != null and mm.get("instance_count") > 0:
					targets.append({"name": "shore_reef", "pos": mm.get_instance_transform(0).origin, "radius": 1.2})
			# 3. 收集陆地岩石 Rocks
			var rk = terrain.get_node_or_null("Rocks")
			if rk != null:
				var mm = rk.get("multimesh")
				if mm != null and mm.get("instance_count") > 0:
					for i in range(min(50, mm.get("instance_count"))):
						var p = mm.get_instance_transform(i).origin
						if p.y > 0.2:
							targets.append({"name": "land_rock", "pos": p, "radius": 1.0})
							break

		print("TARGETS FOUND: ", targets.size())
		cam = Camera3D.new()
		root.add_child(cam)
		cam.set("current", true)
		cam.set("near", 0.05)
		cam.set("far", 500.0)

	if frames >= 20:
		if target_idx < targets.size():
			if wait_frames == 0:
				var tgt = targets[target_idx]
				var tpos: Vector3 = tgt["pos"]
				var trad: float = tgt["radius"]
				var a = float(angle_step) * (PI / 4.0)
				var cam_dist = trad * 2.8
				var cam_h = trad * 0.8
				var cpos = tpos + Vector3(cos(a) * cam_dist, cam_h, sin(a) * cam_dist)
				cam.look_at_from_position(cpos, tpos + Vector3(0.0, trad * 0.2, 0.0), Vector3(0.0, 1.0, 0.0))
				wait_frames = 5
				return false

			wait_frames -= 1
			if wait_frames == 0:
				var tgt = targets[target_idx]
				var tex = root.get_viewport().get_texture()
				if tex != null:
					var img = tex.get_image()
					if img != null:
						var fn = "%srock_angle_%s_%02d.png" % [OUT, tgt["name"], angle_step * 45]
						img.save_png(fn)
						print("SAVED: ", fn)

				angle_step += 1
				if angle_step >= 8:
					angle_step = 0
					target_idx += 1
		else:
			print("ALL ANGLES COMPLETED!")
			quit(0)
			return true

	return false
