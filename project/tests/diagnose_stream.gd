extends SceneTree

func _init():
	var world_scene = load("res://world.tscn")
	var world = world_scene.instantiate()
	root.add_child(world)
	await process_frame
	
	var terrain = world.get_node_or_null("Terrain")
	var stream_mi = terrain.get_node_or_null("MountainStreamWater")
	print("MountainStreamWater node: ", stream_mi)
	if stream_mi:
		print("  visible: ", stream_mi.is_visible())
		print("  mesh: ", stream_mi.mesh)
		if stream_mi.mesh:
			print("  aabb: ", stream_mi.mesh.get_aabb())
		print("  mat override: ", stream_mi.material_override)
	
	# 测试溪流关键位置的高度与地形高度
	var pts = [
		Vector2(96, 96),
		Vector2(80, 86),
		Vector2(62, 76),
		Vector2(45, 68),
		Vector2(28, 60)
	]
	for p in pts:
		var th = terrain.get_terrain_height(p.x, p.y)
		print("Point (", p.x, ", ", p.y, "): terrain_height = ", th)
		
	# 测试桥下 (62, 76) 的断面高度
	print("\nCross section at bridge (62, 76):")
	for off_z in [-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0]:
		for off_x in [-2.0, -1.0, 0.0, 1.0, 2.0]:
			var x = 62.0 + off_x
			var z = 76.0 + off_z
			var th = terrain.get_terrain_height(x, z)
			print("x=%4.1f, z=%4.1f -> terrain_h=%4.2f" % [x, z, th])
	quit()
