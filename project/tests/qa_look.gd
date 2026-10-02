# 临时验证：这一轮改动的目视检查（石头质感 / 树的层次 / 深水巨礁大浪花）。
# 用法（非 headless，要真渲染）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_look.gd
extends SceneTree

var frames := 0
var world: Node = null
var cam: Camera3D = null
var done := false

var shots: Array = []   # [{name, pos, look, fov}]
var idx := -1
var wait := 0

var burst_pos := Vector3.ZERO
var burst_n := 0
var burst_cool := 0

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

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
		var player = world.get_node_or_null("Player")
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.42)          # 上午斜阳，树冠层次看得出来
			sky.call("set_weather", 0)            # 强制晴天
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
			sky.call("set_snow", 0.0)
			sky.call("set_snow_target", 0.0)

		var terrain = world.get_node_or_null("Terrain")
		if terrain == null:
			push_error("no Terrain")
			done = true
			return true

		# ---- 诊断打印 ----
		for nm in ["TreeTrunks", "T1LowSnow", "T2MidSnow", "T3HighSnow", "ShoreReefs", "DeepReef"]:
			var n = terrain.get_node_or_null(nm)
			if n == null:
				continue
			var mm = n.get("multimesh")
			if mm != null:
				print("COUNT ", nm, " = ", mm.get("instance_count"))
		for nm in ["TreeCanopyLowT1LowSnow", "TreeCanopyLowT2MidSnow", "TreeCanopyLowT3HighSnow"]:
			var n = terrain.get_node_or_null(nm)
			if n != null:
				print("CANOPY ", nm, " = ", n.get("multimesh").get("instance_count"))
		var spray = terrain.get_node_or_null("ShoreSpray")
		print("SPRAY_CHILDREN=", spray.get_child_count() if spray != null else -1)
		var lake = world.get_node_or_null("Terrain/Lake")
		if lake != null:
			var lmesh = lake.get("mesh")
			if lmesh != null:
				var lm = lmesh.surface_get_material(0)
				if lm != null:
					print("SHORE_ROCK_COUNT=", lm.get_shader_parameter("shore_rock_count"))

		# ---- 找一棵"林子里"的树（邻居多 = 成片）----
		var trunks = terrain.get_node_or_null("TreeTrunks")
		var tree0 := Vector3.ZERO
		var tree1 := Vector3.ZERO
		var have_tree := false
		if trunks != null:
			var mm = trunks.get("multimesh")
			var cnt = int(mm.get("instance_count"))
			var lim = min(cnt, 300)
			var best_n := -1
			for i in range(lim):
				var p = mm.get_instance_transform(i).origin
				var nb := 0
				for j in range(lim):
					if i == j:
						continue
					if p.distance_to(mm.get_instance_transform(j).origin) < 7.0:
						nb += 1
				if nb > best_n:
					best_n = nb
					tree0 = p
					have_tree = true
			if have_tree:
				var d_best := 1e9
				for j in range(lim):
					var q = mm.get_instance_transform(j).origin
					var d = tree0.distance_to(q)
					if d > 1.0 and d < d_best:
						d_best = d
						tree1 = q
			print("TREE0=", tree0, " neigh=", best_n, " TREE1=", tree1)

		# ---- 岸边礁石 / 巨礁 ----
		var reef := Vector3.ZERO
		var have_reef := false
		var rv = _mm_origin(terrain, "ShoreReefs")
		if rv != null:
			reef = rv
			have_reef = true
		var dv = _mm_origin(terrain, "DeepReef")
		if dv != null:
			burst_pos = dv
			print("DEEP_REEF=", burst_pos)
		print("HAVE_TREE=", have_tree, " HAVE_REEF=", have_reef)
		# ---- 排镜头 ----
		if have_tree:
			var mid = (tree0 + tree1) * 0.5 if tree1 != Vector3.ZERO else tree0
			shots.append({
				"name": "tree_wide",
				"pos": Vector3(mid.x + 10.0, tree0.y + 3.6, mid.z + 10.0),
				"look": Vector3(mid.x, tree0.y + 0.9, mid.z),
				"fov": 55.0,
			})
			shots.append({
				"name": "tree_close",
				"pos": Vector3(tree0.x + 3.6, tree0.y + 0.4, tree0.z + 3.6),
				"look": Vector3(tree0.x, tree0.y + 1.3, tree0.z),
				"fov": 48.0,
			})
			shots.append({
				"name": "tree_mid",
				"pos": Vector3(mid.x + 6.5, tree0.y + 1.4, mid.z + 6.5),
				"look": Vector3(mid.x, tree0.y + 1.4, mid.z),
				"fov": 50.0,
			})
		if have_reef:
			shots.append({
				"name": "rock_close",
				"pos": Vector3(reef.x + 2.6, reef.y + 1.9, reef.z + 2.6),
				"look": Vector3(reef.x, reef.y + 0.15, reef.z),
				"fov": 45.0,
			})
		# 陆地巨石（Rocks 散落）：找最大的一颗贴近看质感
		var bnode = terrain.get_node_or_null("Rocks")
		if bnode != null:
			var bmm = bnode.get("multimesh")
			if bmm != null and int(bmm.get("instance_count")) > 0:
				var best := 0
				var best_s := -1.0
				for i in range(min(int(bmm.get("instance_count")), 400)):
					var sc = bmm.get_instance_transform(i).basis.get_scale().x
					if sc > best_s:
						best_s = sc
						best = i
				var bp = bmm.get_instance_transform(best).origin
				print("ROCK_LAND=", bp, " scale=", best_s)
				shots.append({
					"name": "rock_land",
					"pos": Vector3(bp.x + 1.5, bp.y + 1.1, bp.z + 1.5),
					"look": Vector3(bp.x, bp.y + 0.1, bp.z),
					"fov": 42.0,
				})

	if frames == 20:
		cam = Camera3D.new()
		root.add_child(cam)
		cam.set("current", true)
		cam.set("near", 0.05)
		cam.set("far", 1200.0)
		var names := []
		for s in shots:
			names.append(s["name"])
		print("SHOT_PLAN=", names, " size=", shots.size())

	if frames < 20:
		return done

	# ---- 顺序拍静态镜头：摆机位 → 等 10 帧让画面稳定 → 存图 ----
	# 注意收尾条件要写成 (idx < size or wait > 0)：最后一张摆完机位后 idx 已经
	# 等于 size，只用 idx < size 会把最后一张吞掉。
	if idx < shots.size() or wait > 0:
		if wait <= 0:
			var s = shots[idx]
			cam.set("fov", s["fov"])
			cam.look_at_from_position(s["pos"], s["look"], Vector3(0.0, 1.0, 0.0))
			wait = 10
			return false
		wait -= 1
		if wait == 0:
			var s = shots[idx]
			root.get_viewport().get_texture().get_image().save_png(OUT + s["name"] + ".png")
			print("SAVE ", s["name"], " at ", s["pos"])
			idx += 1
		return false

	# ---- 巨礁水花：跨一个浪周期连拍 10 张（周期 2π/2.8 ≈ 2.24s ≈ 134 帧）----
	if burst_pos != Vector3.ZERO and burst_n < 10:
		if burst_n == 0 and burst_cool == 0 and idx >= shots.size() and wait != -99:
			# 摆好机位，等 40 帧让粒子进入稳定相位
			cam.set("fov", 50.0)
			cam.look_at_from_position(Vector3(burst_pos.x + 8.0, 0.30, burst_pos.z + 8.0),
					Vector3(burst_pos.x, 1.40, burst_pos.z), Vector3(0.0, 1.0, 0.0))
			wait = -99
			burst_cool = 40
			return false
		if burst_cool > 0:
			burst_cool -= 1
			return false
		root.get_viewport().get_texture().get_image().save_png(OUT + "reef_burst_%02d.png" % burst_n)
		print("SHOT reef_burst_", burst_n)
		burst_n += 1
		burst_cool = 13
		if burst_n >= 10:
			done = true
		return false

	return done
