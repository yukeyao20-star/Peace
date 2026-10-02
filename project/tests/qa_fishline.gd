# 鱼线专项（正式验证版）：抛竿后按住空格不让鱼跑掉，拍若干帧看线在不在、够不够显眼。
extends SceneTree

var frames := 0
var world: Node = null
var cam: Camera3D = null
var fish: Node = null
var line: Node = null
var rod: Node = null

const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

func _key(kc: int, pressed: bool) -> void:
	var ev := InputEventKey.new()
	ev.keycode = kc
	ev.physical_keycode = kc
	ev.pressed = pressed
	Input.parse_input_event(ev)

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
		var player = world.get_node_or_null("Player")
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
		var player = world.get_node_or_null("Player")
		if player != null:
			player.set_physics_process(false)
		fish = world.get_node_or_null("FishingSystem")
		if fish != null:
			line = fish.get_node_or_null("FishingLine")
			rod = fish.get_node_or_null("FishingRod")
		cam = Camera3D.new()
		root.add_child(cam)
		cam.set("current", true)
		cam.set("near", 0.05)
		cam.set("far", 1200.0)
		cam.set("fov", 52.0)
		var pp = player.get("global_position") if player != null else Vector3.ZERO
		cam.look_at_from_position(Vector3(pp.x + 4.6, pp.y + 3.4, pp.z + 5.2),
				Vector3(pp.x, pp.y + 1.6, pp.z - 2.6), Vector3(0.0, 1.0, 0.0))
		_key(KEY_F, true)
		return false

	if frames > 122 and frames < 500:
		if frames % 24 == 0:
			_key(KEY_SPACE, true)
		if frames % 24 == 12:
			_key(KEY_SPACE, false)

	if frames == 170 or frames == 240 or frames == 330 or frames == 430:
		var t = rod.get_global_transform() * Vector3(0.0, 0.0, -2.45)
		print("F%d line_vis=%s rod_vis=%s tip=(%.2f,%.2f,%.2f)"
				% [frames, line.is_visible(), rod.is_visible(), t.x, t.y, t.z])
		root.get_viewport().get_texture().get_image().save_png(OUT + "fline_%03d.png" % frames)
		print("SAVE fline_%03d" % frames)
		return false

	if frames > 440:
		return true
	return false
