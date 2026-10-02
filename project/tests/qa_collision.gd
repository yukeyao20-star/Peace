# 临时验证：散布物碰撞体实测（StaticBody 计数 + 逐类射线打样）
extends SceneTree

var frames := 0
var done := false

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	root.add_child(scene.instantiate())

func _process(_delta: float) -> bool:
	frames += 1
	if frames < 10 or done:
		return done
	done = true
	var world = root.get_child(root.get_child_count() - 1)
	var terrain = world.get_node_or_null("Terrain")
	var space = world.get_world_3d().direct_space_state
	var sb := 0
	for c in terrain.get_children():
		if c is StaticBody3D:
			sb += 1
	print("STATIC_BODIES=", sb)
	for name in ["Boulders", "Rocks", "TreeTrunks", "TreeStumps", "FallenLogs"]:
		var mmi = terrain.get_node_or_null(name)
		if mmi == null:
			print(name, "=MISSING")
			continue
		var mm = mmi.multimesh
		var hits := 0
		var checks = min(mm.instance_count, 6)
		for i in range(checks):
			var xf = mm.get_instance_transform((i * 37) % mm.instance_count)
			var pos = mmi.global_transform * xf.origin
			var q = PhysicsRayQueryParameters3D.create(pos + Vector3(0, 4, 0), pos + Vector3(0, -8, 0))
			var hit = space.intersect_ray(q)
			if not hit.is_empty() and hit.position.y > pos.y - 1.5:
				hits += 1
		print(name, "_RAY_HITS=", hits, "/", checks)
	return true
