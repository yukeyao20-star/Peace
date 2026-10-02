# 诊断：树冠"墨绿"到底来自哪一层
#   v0 原样 / v1 关 SSAO / v2 环境光去蓝 / v3 去蓝+关SSAO / v4 环境光纯暖绿
# 同一机位各拍一张，回来看像素统计。
extends SceneTree

var frames := 0
var world: Node = null
var cam: Camera3D = null
var env: Environment = null

var shots: Array = []
var idx := 0
var wait := 0
var done := false

const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

func _apply(v: Dictionary) -> void:
	if env == null:
		return
	if v.has("ssao"):
		env.set("ssao_enabled", v["ssao"])
	if v.has("skyc"):
		env.set("ambient_light_sky_contribution", v["skyc"])
	if v.has("ambc"):
		env.set("ambient_light_color", v["ambc"])

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
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
		var we = world.get_node_or_null("WorldEnvironment")
		if we != null:
			env = we.get("environment")
			print("ENV ok=", env != null,
					" ssao=", env.get("ssao_enabled"),
					" src=", env.get("ambient_light_source"),
					" skyc=", env.get("ambient_light_sky_contribution"),
					" ambc=", env.get("ambient_light_color"))

	if frames == 60:
		var terrain = world.get_node_or_null("Terrain")
		var t = null
		if terrain != null:
			var n = terrain.get_node_or_null("TreeTrunks_A0")
			if n != null:
				t = n.get("multimesh").get_instance_transform(0).origin
		if t == null:
			t = Vector3(0, 0, 0)
		var pos := Vector3(t.x + 5.0, t.y + 2.9, t.z + 5.0)
		var look := Vector3(t.x, t.y + 2.5, t.z)
		var NEU := Color(0.74, 0.80, 0.68)
		var variants := [
			{ "name": "tc_tscn" },
			{ "name": "tc_b_skyamb", "ssao": 0.8, "skyc": 1.0, "ambc": Color(0, 0, 0) },
			{ "name": "tc_c_ssao00", "ssao": 0.0, "skyc": 0.45, "ambc": Color(0.7, 0.76, 0.72) },
			{ "name": "tc_d_ssao08_grey", "ssao": 0.8, "skyc": 0.45, "ambc": Color(0.70, 0.74, 0.74) },
			{ "name": "tc_e_ssao16_neu", "ssao": 1.6, "skyc": 0.45, "ambc": Color(0.7, 0.76, 0.72) },
		]
		for v in variants:
			shots.append({ "v": v, "pos": pos, "look": look })
		print("TC_PLAN=", shots.size())

	if frames == 61:
		cam = Camera3D.new()
		root.add_child(cam)
		cam.set("current", true)
		cam.set("near", 0.05)
		cam.set("far", 1200.0)
		cam.set("fov", 38.0)

	if frames < 62:
		return false

	if idx < shots.size() or wait > 0:
		if wait <= 0:
			var s = shots[idx]
			_apply(s["v"])
			cam.look_at_from_position(s["pos"], s["look"], Vector3(0.0, 1.0, 0.0))
			wait = 12
			return false
		wait -= 1
		if wait == 0:
			var s = shots[idx]
			root.get_viewport().get_texture().get_image().save_png(OUT + s["v"]["name"] + ".png")
			print("SAVE ", s["v"]["name"])
			idx += 1
		return false

	done = true
	return done
