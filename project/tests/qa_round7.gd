# 临时验证（本轮）：
#   1) 树：枝干能不能看见、叶子颜色有没有深有浅、树冠边缘碎不碎
#   2) 拍岸浪：浪脊是不是真的从湖心滚向石头（连拍）
#   3) 荷叶随波 / 荷花数量
#   4) 鱼竿 + 鱼线
#   5) 水深图 shore_map 有没有真的注入
# 用法：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_round7.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var done := false

var shots: Array = []
var idx := 0
var wait := 0

var rod_node: Node = null
var line_node: Node = null
var f_pressed := false
var f_frames := 0

const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

func _mm_origin(terrain: Node, nm: String) -> Variant:
	var n = terrain.get_node_or_null(nm)
	if n == null:
		return null
	var mm = n.get("multimesh")
	if mm == null:
		return null
	if int(mm.get("instance_count")) <= 0:
		return null
	return mm.get_instance_transform(0).origin

func _mm_count(terrain: Node, nm: String) -> int:
	var n = terrain.get_node_or_null(nm)
	if n == null:
		return -1
	var mm = n.get("multimesh")
	return int(mm.get("instance_count")) if mm != null else -2

func _arch_origins(terrain: Node, nm_fmt: String) -> Array:
	# 把 TreeTrunks_A0..A5 各自第一个实例的落点收集起来（用来分别拍不同树形）
	var out: Array = []
	for i in range(6):
		var o = _mm_origin(terrain, nm_fmt % i)
		if o != null:
			out.append(o)
	return out

func _shore_k(reach: float) -> void:
	var terrain = world.get_node_or_null("Terrain")
	if terrain == null:
		return
	var lake = terrain.get_node_or_null("Lake")
	if lake == null:
		return
	var m = lake.get("mesh").surface_get_material(0)
	if m != null:
		m.set_shader_parameter("shore_wave_reach", reach)

func _canopy_shadow(on: bool) -> void:
	var terrain = world.get_node_or_null("Terrain")
	if terrain == null:
		return
	for c in terrain.get_children():
		if String(c.name).begins_with("TreeCanopy"):
			c.set("cast_shadows_setting", 1 if on else 0)

func _key(kc: int, pressed: bool) -> void:
	var ev := InputEventKey.new()
	ev.keycode = kc
	ev.physical_keycode = kc
	ev.pressed = pressed
	Input.parse_input_event(ev)

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
		player = world.get_node_or_null("Player")
		if player != null:
			player.set_process(false)
			player.set("global_position", Vector3(-14.0, 4.0, 112.0))
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.34)
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
			sky.call("set_snow", 0.0)
			sky.call("set_snow_target", 0.0)

	if frames == 120:
		if player != null:
			player.set_physics_process(false)

	# —— 诊断 + 排镜头 ——
	if frames == 121:
		var terrain = world.get_node_or_null("Terrain")
		if terrain == null:
			push_error("no Terrain")
			done = true
			return true

		var tot := 0
		for i in range(6):
			var c = _mm_count(terrain, "TreeTrunks_A%d" % i)
			tot += max(c, 0)
			print("  ARCH%d trunks=%d canopy=%d" % [i, c,
					_mm_count(terrain, "TreeCanopyT1LowSnow_A%d" % i)
					+ _mm_count(terrain, "TreeCanopyT2MidSnow_A%d" % i)
					+ _mm_count(terrain, "TreeCanopyT3HighSnow_A%d" % i)])
		print("TREES total_trunks=", tot)
		print("POND lily=", _mm_count(terrain, "LilyPads"),
				" lotus=", _mm_count(terrain, "LotusFlowers"),
				" shoreReef=", _mm_count(terrain, "ShoreReefs"),
				" deepReef=", _mm_count(terrain, "DeepReef"))

		var lake = terrain.get_node_or_null("Lake")
		if lake != null:
			var lm = lake.get("mesh").surface_get_material(0)
			if lm != null:
				var smap = lm.get_shader_parameter("shore_map")
				print("SHORE_MAP=", (smap != null),
						" origin=", lm.get_shader_parameter("shore_map_origin"),
						" size=", lm.get_shader_parameter("shore_map_size"))
				print("SHORE_ROCK_COUNT=", lm.get_shader_parameter("shore_rock_count"))

		var fish = world.get_node_or_null("FishingSystem")
		if fish != null:
			rod_node = fish.get_node_or_null("FishingRod")
			line_node = fish.get_node_or_null("FishingLine")
			print("FISH rod=", rod_node != null, " line=", line_node != null,
					" children=", fish.get_child_count())

		# —— 树：每种树形各拍一张近景（看枝干/叶色），再拍一张林相看整体变化 ——
		var archs = _arch_origins(terrain, "TreeTrunks_A%d")
		print("ARCH_ORIGINS=", archs.size())
		for i in range(archs.size()):
			var t = archs[i]
			shots.append({
				"name": "tree_arch%d" % i,
				"pos": Vector3(t.x + 5.0, t.y + 2.9, t.z + 5.0),
				"look": Vector3(t.x, t.y + 2.5, t.z),
				"fov": 38.0, "wait": 10,
			})
		if archs.size() > 0:
			var t0 = archs[0]
			shots.append({
				"name": "tree_under",
				"pos": Vector3(t0.x + 1.5, t0.y + 0.8, t0.z + 1.5),
				"look": Vector3(t0.x, t0.y + 3.4, t0.z),
				"fov": 66.0, "wait": 12,
			})
			shots.append({
				"name": "tree_forest",
				"pos": Vector3(t0.x + 22.0, t0.y + 11.0, t0.z + 22.0),
				"look": Vector3(t0.x + 6.0, t0.y + 1.5, t0.z + 6.0),
				"fov": 48.0, "wait": 12,
			})
			# 对照：关掉树冠自身投影，看树冠会不会明显变亮
			# （用来判断"树偏暗"有多少来自叶团互相投影）
			shots.append({
				"name": "tree_forest_noshadow",
				"pos": Vector3(t0.x + 22.0, t0.y + 11.0, t0.z + 22.0),
				"look": Vector3(t0.x + 6.0, t0.y + 1.5, t0.z + 6.0),
				"fov": 48.0, "wait": 14, "noshadow": true,
			})

		# —— 拍岸浪：连拍 6 张看浪脊推进（机位抬高一点，能同时看到岸、石头和浪线）——
		var reef = _mm_origin(terrain, "ShoreReefs")
		if reef != null:
			print("REEF0=", reef)
			for k in range(6):
				shots.append({
					"name": "wave_seq_%02d" % k,
					"pos": Vector3(reef.x + 6.5, 3.6, reef.z + 6.5),
					"look": Vector3(reef.x, -0.35, reef.z),
					"fov": 52.0, "wait": 26,
				})
			# 对照：把近岸系数关掉（shore_wave_reach→0），岸上那层白如果还在，
			# 说明它是浅水底色（折射沙底 + 焦散），不是浪花。
			shots.append({
				"name": "wave_diag_nofoam",
				"pos": Vector3(reef.x + 6.5, 3.6, reef.z + 6.5),
				"look": Vector3(reef.x, -0.35, reef.z),
				"fov": 52.0, "wait": 14, "nofoam": true,
			})

		var lily = _mm_origin(terrain, "LilyPads")
		if lily != null:
			shots.append({
				"name": "lily_close",
				"pos": Vector3(lily.x + 2.6, 0.75, lily.z + 2.6),
				"look": Vector3(lily.x, -0.50, lily.z),
				"fov": 48.0, "wait": 12,
			})
		shots.append({
			"name": "lotus_view",
			"pos": Vector3(-14.0, 1.8, 86.0),
			"look": Vector3(-15.0, -0.5, 64.0),
			"fov": 55.0, "wait": 12,
		})

		var names := []
		for s in shots:
			names.append(s["name"])
		print("SHOT_PLAN=", names)

	if frames == 122:
		cam = Camera3D.new()
		root.add_child(cam)
		cam.set("current", true)
		cam.set("near", 0.05)
		cam.set("far", 1200.0)

	if frames < 122:
		return done

	# —— 顺序拍静态镜头 ——
	if idx < shots.size() or wait > 0:
		if wait <= 0:
			var s = shots[idx]
			cam.set("fov", s["fov"])
			cam.look_at_from_position(s["pos"], s["look"], Vector3(0.0, 1.0, 0.0))
			if s.get("nofoam", false):
				_shore_k(0.001)
			if s.get("noshadow", false):
				_canopy_shadow(false)
			wait = int(s["wait"])
			return false
		wait -= 1
		if wait == 0:
			var s = shots[idx]
			root.get_viewport().get_texture().get_image().save_png(OUT + s["name"] + ".png")
			print("SAVE ", s["name"])
			if s.get("nofoam", false):
				_shore_k(2.6)
			idx += 1
		return false

	# —— 钓鱼：按住 F 充能、松手抛竿，然后密拍一段 ——
	# 鱼线只在"已抛竿、还没收竿"那几秒里可见，窗口很窄，
	# 所以不挑单帧，改成一个俯瞰机位连续拍，再从中挑线出现的那张。
	if not f_pressed:
		_key(KEY_F, true)
		f_pressed = true
		f_frames = 0
		var pp0 = player.get("global_position")
		cam.set("fov", 55.0)
		cam.look_at_from_position(Vector3(pp0.x + 4.2, pp0.y + 5.6, pp0.z + 6.0),
				Vector3(pp0.x, pp0.y + 1.2, pp0.z - 1.8), Vector3(0.0, 1.0, 0.0))
		return false
	f_frames += 1
	if f_frames == 72:
		_key(KEY_F, false)
		return false
	if f_frames >= 76 and (f_frames - 76) % 8 == 0:
		var nm := "fish_%03d" % f_frames
		root.get_viewport().get_texture().get_image().save_png(OUT + nm + ".png")
		print("SAVE ", nm, " rod_vis=", (rod_node.is_visible() if rod_node != null else "?"),
				" line_vis=", (line_node.is_visible() if line_node != null else "?"))
		return false
	if f_frames < 260:
		return false

	done = true
	return done
