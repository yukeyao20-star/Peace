# 临时诊断（2026-10-04）：一次跑完三件事的现场取证
#   1. 高山小溪到底在不在、水面在不在河槽里（俯拍 + 侧拍 + 射线测地形高度）
#   2. 木屋里的雨雪（室内粒子）—— 拍一张屋内的
#   3. 壁炉声：把玩家挪到木屋正中，dump Ambience 各层的 volume_db / 流 / 播放状态
#
# 用法（必须非 headless，headless 是 dummy 渲染器截不出图）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project -s tests/qa_diag3.gd
extends SceneTree

const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var shots: Array = []      # [名字, 机位, 看向]
var shot_i := 0
var stage := 0

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")

func _ray_y(x: float, z: float) -> float:
	var space: PhysicsDirectSpaceState3D = player.get_world_3d().direct_space_state
	var q := PhysicsRayQueryParameters3D.create(
			Vector3(x, 200.0, z), Vector3(x, -50.0, z))
	var hit: Dictionary = space.intersect_ray(q)
	if hit.is_empty():
		return 999.0
	var pos: Vector3 = hit["position"]
	return pos.y

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.42)

	# ---------- 第 1 段：溪流取证 ----------
	if frames == 20:
		var terrain = world.get_node_or_null("Terrain")
		print("=== 溪流 ===")
		print("Terrain 节点: ", terrain != null)
		if terrain != null:
			for n in terrain.get_children():
				if str(n.name).to_lower().contains("stream") or str(n.name).to_lower().contains("bridge"):
					print("  子节点 ", n.name, "  type=", n.get_class(), "  visible=", n.visible if n is Node3D else "?")
			var sw = terrain.get_node_or_null("MountainStreamWater")
			print("MountainStreamWater: ", sw)
			if sw != null:
				print("  visible=", sw.visible)
				var m: Mesh = sw.mesh
				if m != null:
					print("  mesh aabb=", m.get_aabb())
				print("  material_override=", sw.material_override)
				var mat = sw.material_override
				if mat is ShaderMaterial:
					print("  is_stream=", mat.get_shader_parameter("is_stream"))
					print("  wave_height=", mat.get_shader_parameter("wave_height"))
			var sv = terrain.get_node_or_null("StreamAudioBridge")
			print("StreamAudioBridge: ", sv)
			if sv != null:
				print("  playing=", sv.playing, "  volume_db=", sv.volume_db,
						"  stream=", sv.stream, "  pos=", sv.global_position)
		# 沿溪流样条量地形高度（射线），看水面和地面的相对关系
		var pts := [Vector2(96, 96), Vector2(80, 86), Vector2(62, 76), Vector2(45, 68), Vector2(28, 60)]
		print("  样条点地面高度（射线）:")
		for p in pts:
			print("    (%.0f, %.0f) -> 地面 y = %.2f" % [p.x, p.y, _ray_y(p.x, p.y)])

	# ---------- 第 2 段：拍溪流 ----------
	if frames == 40:
		cam = Camera3D.new()
		cam.fov = 60.0
		root.add_child(cam)
		cam.current = true
		shots = [
			["diag3_stream_aerial", Vector3(70.0, 46.0, 84.0), Vector3(62.0, 5.0, 76.0)],
			["diag3_stream_bridge", Vector3(68.0, 9.0, 84.0), Vector3(62.0, 5.4, 76.0)],
			["diag3_stream_source", Vector3(96.0, 22.0, 106.0), Vector3(96.0, 14.0, 96.0)],
			["diag3_stream_low", Vector3(45.0, 10.0, 74.0), Vector3(45.0, 4.0, 68.0)],
		]

	if frames > 40 and shot_i < shots.size():
		cam.global_position = shots[shot_i][1]
		cam.look_at(shots[shot_i][2], Vector3.UP)
		if frames % 12 == 0:
			var img := root.get_viewport().get_texture().get_image()
			img.save_png(OUT + shots[shot_i][0] + ".png")
			print("SHOT ", shots[shot_i][0])
			shot_i += 1

	# ---------- 第 3 段：玩家挪进木屋，dump 环境声 ----------
	if frames == 110:
		if player != null:
			player.set_process(true)
			player.set_physics_process(true)
			player.global_position = Vector3(12.0, 0.9, 8.0)   # 木屋正中
			player.set("cam_yaw", 2.2)

	if frames == 115:
		# 下雨，把降水粒子打开
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("rain", 1.0)
			sky.set("rain_target", 1.0)
			sky.set("snow", 0.0)
			sky.set("snow_target", 0.0)
			sky.call("apply")
			print("=== 天气已设为大雨 ===")
		var rr = world.get_node_or_null("SkyCycle/RainRoot")
		print("RainRoot: ", rr, "  pos=", rr.global_position if rr else "?")
		if rr != null:
			for c in rr.get_children():
				print("   ", c.name, "  emitting=", c.emitting, "  amount=", c.amount,
						"  visible=", c.visible)

	if frames == 160:
		var amb = world.get_node_or_null("Ambience")
		print("=== Ambience ===")
		print("Ambience 节点: ", amb)
		if amb != null:
			print("  player_cache 位置 = ", player.global_position if player else "?")
			for c in amb.get_children():
				if c is AudioStreamPlayer:
					print("  %-12s vol=%7.2f dB  stream=%s  playing=%s  paused=%s" % [
							c.name, c.volume_db, str(c.stream != null), str(c.playing), str(c.stream_paused)])

	# 拍一张屋内的
	if frames > 40 and shot_i >= shots.size() and frames == 175:
		cam.global_position = Vector3(12.0, 2.4, 8.0)
		cam.look_at(Vector3(8.5, 0.8, 6.0), Vector3.UP)

	if frames == 190:
		var img2 := root.get_viewport().get_texture().get_image()
		img2.save_png(OUT + "diag3_indoors_rain.png")
		print("SHOT diag3_indoors_rain")

	if frames == 215:
		# 屋外对照：站在院子里看屋子，雨应该正常下
		cam.global_position = Vector3(12.0, 6.0, 20.0)
		cam.look_at(Vector3(12.0, 3.0, 8.0), Vector3.UP)

	if frames == 230:
		var img3 := root.get_viewport().get_texture().get_image()
		img3.save_png(OUT + "diag3_outdoor_rain.png")
		print("SHOT diag3_outdoor_rain")
		return true

	return false
