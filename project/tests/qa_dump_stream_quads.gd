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
		# 打印前 6 个三角形的 3 个顶点坐标与表面法线
		for f in range(12):
			var v0 = mdt.get_vertex(mdt.get_face_vertex(f, 0))
			var v1 = mdt.get_vertex(mdt.get_face_vertex(f, 1))
			var v2 = mdt.get_vertex(mdt.get_face_vertex(f, 2))
			var fn = mdt.get_face_normal(f)
			print("Face %2d normal=(%5.2f, %5.2f, %5.2f) | v0=(%.1f,%.1f,%.1f) v1=(%.1f,%.1f,%.1f) v2=(%.1f,%.1f,%.1f)" % [f, fn.x, fn.y, fn.z, v0.x, v0.y, v0.z, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z])
		quit(0)
		return true
	return false
