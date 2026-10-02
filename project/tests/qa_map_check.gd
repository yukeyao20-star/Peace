# 临时验证：构建落盘后截图确认地形/植被恢复（用完即删）
extends SceneTree

var frames := 0
var cam: Camera3D = null
var done := false

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	var world := scene.instantiate()
	root.add_child(world)

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 5:
		var world = root.get_child(root.get_child_count() - 1)
		var player = world.get_node_or_null("Player")
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
		# 体检：关键节点是否为真身（占位符会报 class 名不符）
		var terrain = world.get_node_or_null("Terrain")
		print("TERRAIN_CLASS=", terrain.get_class() if terrain != null else "MISSING")
		var mmi_count := 0
		if terrain != null:
			for child in terrain.get_children():
				if child is MultiMeshInstance3D:
					mmi_count += 1
		print("MULTIMESH_COUNT=", mmi_count)
		cam = Camera3D.new()
		world.add_child(cam)
		cam.global_position = Vector3(4.0, 7.0, -12.0)
		cam.look_at(Vector3(-16.0, 2.0, 8.0), Vector3(0, 1, 0))
		cam.make_current()
	if frames == 25 and cam != null and not done:
		done = true
		var img := root.get_viewport().get_texture().get_image()
		img.save_png("/Users/lei/Downloads/3Dgame/docs/shot_map_recovered.png")
		print("SHOT_SAVED")
	return done
