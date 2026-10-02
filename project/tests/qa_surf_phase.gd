# 临时验证：贴着一块礁石低角度连拍，跨越一个拍岸浪周期，
# 用来判断"浪花砸在石头上激起白浪"到底读不读得出来。
# 用法（非 headless）：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_surf_phase.gd
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var done := false
var reef_pos := Vector3.ZERO
var has_reef := false
var shot := 0

const SHOT_COUNT := 12
const SHOT_EVERY := 15

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)
	player = world.get_node_or_null("Player")

func _process(_delta: float) -> bool:
	frames += 1
	if frames == 2:
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
		var sky = world.get_node_or_null("SkyCycle")
		if sky != null:
			sky.set("time_of_day", 0.45)
			# 强制晴天（weather/rain 只绑了方法没绑属性，必须用 call）
			sky.call("set_weather", 0)
			sky.call("set_cloud", 0.0)
			sky.call("set_cloud_target", 0.0)
			sky.call("set_rain", 0.0)
			sky.call("set_rain_target", 0.0)
			sky.call("set_snow", 0.0)
			sky.call("set_snow_target", 0.0)
		var terrain = world.get_node_or_null("Terrain")
		if terrain != null:
			var reefs = terrain.get_node_or_null("ShoreReefs")
			if reefs != null:
				var mm = reefs.get("multimesh")
				if mm.get("instance_count") > 0:
					reef_pos = mm.get_instance_transform(0).origin
					has_reef = true
					print("REEF0_POS=", reef_pos)
			var spray = terrain.get_node_or_null("ShoreSpray")
			print("SPRAY_NODE=", spray != null)
			if spray != null:
				print("SPRAY_CHILDREN=", spray.get_child_count())
				var g = spray.get_child(0)
				print("SPRAY_CHILD0=", g.get_class(), " amount=", g.get("amount"),
						" life=", g.get("lifetime"), " pre=", g.get("pre_process_time"),
						" emitting=", g.get("emitting"))

	if frames == 20 and has_reef:
		cam = Camera3D.new()
		cam.set("fov", 50.0)
		root.add_child(cam)
		cam.set("current", true)
		# 低机位贴水（水面 -0.55），距石头 3.0m，仰视石脊
		var look := Vector3(reef_pos.x, -0.30, reef_pos.z)
		cam.set("global_position", Vector3(reef_pos.x + 1.9, 0.15, reef_pos.z + 2.4))
		cam.look_at(look, Vector3(0.0, 1.0, 0.0))

	if frames > 20 and has_reef and (frames - 20) % SHOT_EVERY == 0 and shot < SHOT_COUNT:
		var img := root.get_viewport().get_texture().get_image()
		var path := "/Users/lei/Downloads/3Dgame/docs/qa/surf_%02d.png" % shot
		img.save_png(path)
		print("SURF_SHOT_", shot)
		shot += 1
		if shot >= SHOT_COUNT:
			done = true
	return done
