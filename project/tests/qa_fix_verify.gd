# 本轮修复的现场验收（2026-10-04）
#   ① 雨雪不再落进屋里 —— 屋檐遮蔽体（SkyCycle::is_sheltered）
#   ② 壁炉燃烧声听得见 + 室内雨声压暗 —— Ambience 各层实际 volume_db
#   ③ 高山小溪看得见 —— 溪床地面高度必须低于水面（射线实测）
#   ④ 睡觉 —— 平躺姿态 + 一觉到清晨
#
# 用法（必须非 headless：headless 是 dummy 渲染器，截不出图）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project -s tests/qa_fix_verify.gd
# 截图落在 docs/qa/fix_*.png
#
# 注意：节点引用一律用**无类型 var**。GDScript 4 的静态分析器不允许在
# Node 类型变量上访问 global_position / set_rain 这类子类属性，会直接报错。
extends SceneTree

const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

var t := 0.0
var stage := 0
var world = null
var player = null
var sky = null
var bed = null
var cam: Camera3D = null


func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")
	sky = world.get_node_or_null("SkyCycle")
	bed = world.get_node_or_null("Homestead/Cabin/Bed")
	cam = Camera3D.new()
	cam.fov = 60.0
	root.add_child(cam)


func _ray_y(x: float, z: float) -> float:
	var space: PhysicsDirectSpaceState3D = player.get_world_3d().direct_space_state
	var q := PhysicsRayQueryParameters3D.create(Vector3(x, 200.0, z), Vector3(x, -50.0, z))
	var hit: Dictionary = space.intersect_ray(q)
	if hit.is_empty():
		return 999.0
	var pos: Vector3 = hit["position"]
	return pos.y


# 溪流水面标高（与 TerrainBuilder::get_stream_spline 同一公式）
func _stream_water_y(tt: float) -> float:
	var k: float = pow(1.0 - tt, 1.35)
	return 15.2 * k + (-0.55) * (1.0 - k)


func _dump(label: String, p: Vector3, expect: bool) -> void:
	var got = false
	if sky != null and sky.has_method("is_sheltered"):
		got = sky.is_sheltered(p)
	var mark := "OK " if got == expect else "!! "
	print("%s%-16s 期望 sheltered=%-5s 实际=%s" % [mark, label, str(expect), str(got)])


func _shoot(name: String, at: Vector3, look: Vector3) -> void:
	cam.global_position = at
	cam.look_at(look, Vector3.UP)
	cam.current = true
	var img: Image = root.get_texture().get_image()
	img.save_png(OUT + name + ".png")
	print(">> 截图 %s.png" % name)


func _layer(nm: String) -> String:
	var amb = world.get_node_or_null("Ambience")
	if amb == null:
		return "!! 找不到 Ambience"
	var p = amb.get_node_or_null(nm)
	if p == null:
		return "   %-10s 缺失" % nm
	return "   %-10s vol=%7.2f dB  playing=%-5s paused=%-5s stream=%s" % [
		nm, p.volume_db, str(p.playing), str(p.stream_paused), str(p.stream)]


func _process(delta: float) -> bool:
	t += delta

	# —— 阶段 0：数值体检 ——
	if stage == 0:
		stage = 1
		print("\n============ ① 屋檐遮蔽（雨雪不该落进屋里）============")
		var cabin = world.get_node_or_null("Homestead/Cabin")
		if cabin == null:
			print("!! 找不到 Homestead/Cabin")
		else:
			var gt: Transform3D = cabin.get_global_transform()
			_dump("主屋正中", gt * Vector3(0, 1.0, -2.1), true)
			_dump("壁炉前", gt * Vector3(-2.5, 1.0, -0.6), true)
			_dump("柴棚内", gt * Vector3(5.6, 1.0, -1.45), true)
			_dump("门廊下", gt * Vector3(0, 1.5, 4.35), true)
			_dump("屋脊上方", gt * Vector3(0, 6.5, 0), false)
			_dump("院子南侧", gt * Vector3(0, 1.0, 9.0), false)
			_dump("柴棚外东侧", gt * Vector3(10.0, 1.0, -1.45), false)

		print("\n============ ③ 高山小溪（地面必须低于水面）============")
		var pts := [Vector2(96, 96), Vector2(80, 86), Vector2(62, 76), Vector2(45, 68), Vector2(28, 60)]
		var ts := [0.0, 0.2488, 0.5051, 0.7526, 1.0]
		var bad := 0
		for i in pts.size():
			var gy := _ray_y(pts[i].x, pts[i].y)
			var wy := _stream_water_y(ts[i])
			var ok: bool = gy < wy - 0.10
			if not ok:
				bad += 1
			print("%s (%5.0f,%5.0f) 水面 y=%6.2f 地面 y=%6.2f 水深 %+5.2f m" % [
				"OK " if ok else "!! ", pts[i].x, pts[i].y, wy, gy, wy - gy])
		print(">> 埋水点数量 = %d（应为 0）" % bad)
		var sw = world.find_child("MountainStreamWater", true, false)
		print(">> MountainStreamWater = %s" % str(sw))
		if sw is MeshInstance3D:
			print(">> 水面 AABB = %s" % str(sw.get_aabb()))
		var sv = world.find_child("StreamAudioBridge", true, false)
		if sv != null:
			print(">> StreamAudioBridge vol=%.2f dB playing=%s pos=%s" % [
				sv.volume_db, str(sv.playing), str(sv.global_position)])
		else:
			print("!! 找不到 StreamAudioBridge")

		# 下雨，供后面测室内雨声衰减
		if sky != null:
			sky.set_rain(1.0)
			sky.set_rain_target(1.0)
			sky.set_cloud(1.0)
			sky.set_cloud_target(1.0)
		return false

	# —— 阶段 1/2：溪流侧拍 + 源头 ——
	if stage == 1 and t > 1.2:
		_shoot("fix_stream_side", Vector3(50.0, 9.5, 64.0), Vector3(66.0, 5.0, 79.0))
		stage = 2
		return false
	if stage == 2 and t > 1.8:
		_shoot("fix_stream_source", Vector3(86.0, 24.0, 88.0), Vector3(93.0, 14.0, 94.0))
		stage = 3
		return false

	# —— 阶段 3/4：玩家挪到院子南侧（室外）→ 读雨声 ——
	if stage == 3 and t > 2.2:
		if player != null:
			player.global_position = Vector3(12.0, 1.2, 22.0)
		stage = 4
		return false
	if stage == 4 and t > 4.6:
		print("\n============ ② 壁炉声 + 雨声室内外对比 ============")
		print("   [室外 院子南侧 (12, 22)]")
		print(_layer("RainLayer"))
		print(_layer("Fireplace"))
		if player != null:
			player.global_position = Vector3(10.5, 1.2, 9.5)
		stage = 5
		return false
	if stage == 5 and t > 7.4:
		print("   [室内 木屋 (10.5, 9.5)：雨声应明显变小、壁炉声应满响]")
		print(_layer("RainLayer"))
		print(_layer("Fireplace"))
		print(_layer("Wind"))
		print(_layer("WindSnow"))
		print(_layer("WaterLap"))
		print(_layer("Birds"))
		if sky != null:
			sky.set_time_of_day(0.72) # 傍晚入睡
			print(">> 入睡前 time_of_day=%.2f day=%d" % [sky.get_time_of_day(), sky.get_day_count()])
		if bed != null and player != null:
			player.start_sleep(bed.global_position, bed.global_rotation.y)
			var m = player.get_node("Model")
			print(">> is_sleeping=%s" % str(player.get_is_sleeping()))
			print(">> Model basis = %s" % str(m.basis))
			print(">> Model pos   = %s" % str(m.position))
			print(">> Player 原点 = %s（床 %s）" % [str(player.global_position), str(bed.global_position)])
		stage = 6
		return false

	# —— 阶段 6：睡姿截图（与游戏内睡觉机位一致）——
	if stage == 6 and t > 8.0:
		if bed != null:
			var bz := Vector3(sin(bed.global_rotation.y), 0.0, cos(bed.global_rotation.y))
			var bx := Vector3(cos(bed.global_rotation.y), 0.0, -sin(bed.global_rotation.y))
			_shoot("fix_sleep_lie",
					bed.global_position + bx * 2.0 + bz * 0.55 + Vector3(0, 1.85, 0),
					bed.global_position + Vector3(0, 0.5, 0))
		stage = 7
		return false

	# —— 阶段 7：等睡觉过场（3.8s）走完 ——
	if stage == 7 and t > 13.5:
		if sky != null:
			print(">> 睡醒后 time_of_day=%.2f day=%d（应为 0.28，day 已 +1）" % [
				sky.get_time_of_day(), sky.get_day_count()])
		if player != null:
			print(">> 醒来位置 = %s（is_sleeping=%s）" % [
				str(player.global_position), str(player.get_is_sleeping())])
		print("\n============ 验收结束 ============")
		return true

	return false
