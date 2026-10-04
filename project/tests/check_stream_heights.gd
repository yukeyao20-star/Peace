extends SceneTree

var frames := 0
var world = null

func _initialize():
	var world_scene = load("res://world.tscn")
	world = world_scene.instantiate()
	root.add_child(world)

func _process(delta):
	frames += 1
	if frames == 5:
		var terrain = world.get_node_or_null("Terrain")
		var stream_mi = terrain.get_node_or_null("MountainStreamWater")
		print("Stream MI: ", stream_mi)
		if stream_mi:
			print("Stream AABB: ", stream_mi.mesh.get_aabb())
		
		var pts = [
			Vector2(92.0, 92.0), # P2 底潭 (水面 3.0)
			Vector2(85.0, 87.0),
			Vector2(78.0, 82.0), # P3 泄流 (水面 2.0)
			Vector2(67.0, 77.0),
			Vector2(56.0, 72.0), # P4 中游 (水面 0.8)
			Vector2(45.0, 67.25),
			Vector2(34.0, 62.5), # P5 木桥 (水面 -0.55)
			Vector2(26.0, 59.25),
			Vector2(18.0, 56.0)  # P6 湖口 (水面 -0.55)
		]
		for p in pts:
			var th = terrain.get_terrain_height(p.x, p.y)
			print("P(%.1f, %.1f): terrain_h=%.2f" % [p.x, p.y, th])
		quit(0)
		return true
	return false
