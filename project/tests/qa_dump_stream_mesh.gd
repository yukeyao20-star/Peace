extends SceneTree

var frames := 0
var world = null

func _initialize():
	world = load("res://world.tscn").instantiate()
	root.add_child(world)

func _process(delta):
	frames += 1
	if frames == 5:
		var terrain = world.get_node_or_null("Terrain")
		var stream_mi = terrain.get_node_or_null("MountainStreamWater")
		var mesh = stream_mi.mesh as ArrayMesh
		var mdt = MeshDataTool.new()
		mdt.create_from_surface(mesh, 0)
		print("Stream Vertex Count: ", mdt.get_vertex_count())
		for i in range(0, mdt.get_vertex_count(), 30):
			var v = mdt.get_vertex(i)
			var th = terrain.get_terrain_height(v.x, v.z)
			print("Vert %3d: pos=(%5.1f, %5.2f, %5.1f) | terrain_h=%5.2f | diff(water-terrain)=%5.2f" % [i, v.x, v.y, v.z, th, v.y - th])
		quit(0)
		return true
	return false
