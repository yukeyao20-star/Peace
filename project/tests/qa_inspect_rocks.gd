# 检查岩石网格与材质属性，并在不同角度下观察
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
		var terrain = world.get_node_or_null("Terrain")
		if terrain == null:
			printerr("Terrain is null!")
			quit(1)
			return true

		var node = terrain.get_node_or_null("Rocks")
		var mm = node.get("multimesh")
		var mesh: ArrayMesh = mm.get("mesh")
		var arrays = mesh.surface_get_arrays(0)
		var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var norms: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		var indices_var = arrays[Mesh.ARRAY_INDEX]
		var indices: PackedInt32Array = indices_var if indices_var != null else PackedInt32Array()
		print("VERTS COUNT: ", verts.size(), " NORMS COUNT: ", norms.size(), " INDICES COUNT: ", indices.size())

		# 检查每个三角形的几何法线是否朝外（与顶点到原点向量点积 > 0）
		var inward_count := 0
		var total_tris := int(verts.size() / 3) if indices.is_empty() else int(indices.size() / 3)
		for t in range(total_tris):
			var i0 = t * 3 if indices.is_empty() else indices[t * 3]
			var i1 = t * 3 + 1 if indices.is_empty() else indices[t * 3 + 1]
			var i2 = t * 3 + 2 if indices.is_empty() else indices[t * 3 + 2]
			var v0 = verts[i0]
			var v1 = verts[i1]
			var v2 = verts[i2]
			var center = (v0 + v1 + v2) / 3.0
			var geom_norm = (v1 - v0).cross(v2 - v0).normalized()
			if geom_norm.dot(center) < 0.0:
				inward_count += 1
		print("TOTAL TRIS: ", total_tris, ", INWARD (BACKWARDS) FACES: ", inward_count)

		quit(0)
		return true

	return false
