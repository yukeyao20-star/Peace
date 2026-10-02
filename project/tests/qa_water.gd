# 临时验证：本轮四项改动的目视检查
#   1) 拍岸浪：浪脊是不是真的从远处滚向石头，撞上才炸水花
#   2) 荷叶：是不是跟着水面起伏、有没有一部分翘出水面
#   3) 荷花：数量
#   4) 钓鱼：鱼竿有没有握在手上、鱼线有没有从竿梢连到浮漂
# 用法（非 headless，要真渲染）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_water.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var done := false

var shots: Array = []
var idx := 0
var wait := 0

var burst_pos := Vector3.ZERO   # 巨礁
var burst_n := 0
var burst_cool := 0

var rod_node: Node = null
var line_node: Node = null
var fishing_node: Node = null
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
			player.set_process(false) # 只关相机/交互，留着物理让它落地
			player.set("global_position", Vector3(-14.0, 4.0, 112.0))
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.40)
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
			sky.call("set_snow", 0.0)
			sky.call("set_snow_target", 0.0)

	# —— 玩家落地 ——
	if frames == 120:
		if player != null:
			player.set_physics_process(false)
			print("PLAYER_AT=", player.get("global_position"))

	# —— 诊断 + 排镜头 ——
	if frames == 121:
		var terrain = world.get_node_or_null("Terrain")
		if terrain == null:
			push_error("no Terrain")
			done = true
			return true

		for nm in ["LilyPads", "LotusFlowers", "ShoreReefs", "DeepReef"]:
			var n = terrain.get_node_or_null(nm)
			if n == null:
				print("MISSING ", nm)
				continue
			var mm = n.get("multimesh")
			print("COUNT ", nm, " = ", (mm.get("instance_count") if mm != null else -1))
		var spray = terrain.get_node_or_null("ShoreSpray")
		print("SPRAY_CHILDREN=", (spray.get_child_count() if spray != null else -1))
		var lake = terrain.get_node_or_null("Lake")
		if lake != null:
			var lm = lake.get("mesh").surface_get_material(0)
			if lm != null:
				print("SHORE_ROCK_COUNT=", lm.get_shader_parameter("shore_rock_count"))

		fishing_node = world.get_node_or_null("FishingSystem")
		if fishing_node != null:
			rod_node = fishing_node.get_node_or_null("FishingRod")
			line_node = fishing_node.get_node_or_null("FishingLine")
			print("FISH rod=", rod_node != null, " line=", line_node != null)

		var reef = _mm_origin(terrain, "ShoreReefs")
		var deep = _mm_origin(terrain, "DeepReef")
		if deep != null:
			burst_pos = deep
			print("DEEP_REEF=", burst_pos)

		if reef != null:
			shots.append({
				"name": "wave_rock_low",
				"pos": Vector3(reef.x + 3.4, 0.45, reef.z + 3.4),
				"look": Vector3(reef.x, -0.15, reef.z),
				"fov": 58.0,
			})
			shots.append({
				"name": "wave_rock_side",
				"pos": Vector3(reef.x + 5.4, 1.2, reef.z + 1.4),
				"look": Vector3(reef.x, -0.35, reef.z),
				"fov": 52.0,
			})
		var lily = _mm_origin(terrain, "LilyPads")
		if lily != null:
			print("LILY0=", lily)
			shots.append({
				"name": "lily_close",
				"pos": Vector3(lily.x + 2.8, 0.85, lily.z + 2.8),
				"look": Vector3(lily.x, -0.45, lily.z),
				"fov": 48.0,
			})
		shots.append({
			"name": "lotus_view",
			"pos": Vector3(-14.0, 1.8, 86.0),
			"look": Vector3(-15.0, -0.5, 64.0),
			"fov": 55.0,
		})
		if player != null:
			var pp = player.get("global_position")
			shots.append({
				"name": "fishing_player",
				"pos": Vector3(pp.x + 2.6, pp.y + 1.7, pp.z + 3.2),
				"look": Vector3(pp.x, pp.y + 1.0, pp.z - 1.2),
				"fov": 42.0,
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
			wait = 12
			return false
		wait -= 1
		if wait == 0:
			var s = shots[idx]
			root.get_viewport().get_texture().get_image().save_png(OUT + s["name"] + ".png")
			print("SAVE ", s["name"])
			idx += 1
		return false

	# —— 钓鱼：按住 F 抛竿，等线拉起来再拍两张 ——
	if not f_pressed:
		_key(KEY_F, true)
		f_pressed = true
		f_frames = 0
		return false
	f_frames += 1
	if f_frames == 4:
		_key(KEY_F, false)
		print("F_RELEASED")
		return false
	if f_frames == 40 or f_frames == 90:
		var pp2 = player.get("global_position")
		cam.set("fov", 40.0)
		cam.look_at_from_position(Vector3(pp2.x + 3.0, pp2.y + 2.0, pp2.z + 3.4),
				Vector3(pp2.x, pp2.y + 1.0, pp2.z - 1.5), Vector3(0.0, 1.0, 0.0))
		return false
	if f_frames == 46 or f_frames == 96:
		var nm := "fishing_cast_%d" % f_frames
		root.get_viewport().get_texture().get_image().save_png(OUT + nm + ".png")
		print("SAVE ", nm, " rod_vis=", (rod_node.is_visible() if rod_node != null else "?"),
				" line_vis=", (line_node.is_visible() if line_node != null else "?"))
		return false
	if f_frames < 100:
		return false

	# —— 巨礁浪花连拍 ——
	if burst_pos != Vector3.ZERO and burst_n < 8:
		if burst_n == 0 and burst_cool == 0 and wait != -99:
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
		print("SAVE reef_burst_", burst_n)
		burst_n += 1
		burst_cool = 14
		if burst_n >= 8:
			done = true
		return false

	return done
