# 验证收竿不自动重抛：
# 1. 玩家站在水边抛竿 -> state 1 (等待咬钩)
# 2. 按 F 收竿 -> state 变成 0 (idle)
# 3. 接下来几帧内保持按住/松开 F，验证 state 始终保持为 0，不误触发二次蓄力/抛竿
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var fish: Node = null
var test_passed := false

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
		player = world.get_node_or_null("Player")
		if player != null:
			player.set_process(false)
			# 站在石桥正中 (-6.0, 1.5, 26.0)，必在垂钓区
			player.set("global_position", Vector3(-6.0, 1.5, 26.0))
		fish = world.get_node_or_null("FishingSystem")

	if frames == 120:
		if player != null:
			player.set_physics_process(false)
			print("PLAYER POS = ", player.get("global_position"))
		print("FRAME 120: Player ready, pressing F to start charge")
		_key(KEY_F, true)

	if frames == 125:
		print("FRAME 125: is_key_pressed(F) = ", Input.is_key_pressed(KEY_F), " charging = ", fish.call("is_charging"))

	if frames == 140:
		print("FRAME 140: Releasing F to cast")
		_key(KEY_F, false)

	if frames == 145:
		var state = fish.call("get_state") if fish != null else -1
		print("FRAME 145: State after cast = ", state)
		if state != 1:
			printerr("FAILED: state should be 1 after cast, got ", state)
			quit(1)
			return true

	# 在第 160 帧按 F 收竿，并持续按住 10 帧（模拟人类正常按键时长）
	if frames == 160:
		print("FRAME 160: Pressing F to reel in (cancel fishing)")
		_key(KEY_F, true)

	if frames == 162:
		var state = fish.call("get_state") if fish != null else -1
		print("FRAME 162: State immediately after pressing F = ", state)
		if state != 0:
			printerr("FAILED: state should be 0 immediately after pressing F, got ", state)
			quit(1)
			return true

	if frames == 170:
		print("FRAME 170: Releasing F after reel in")
		_key(KEY_F, false)

	if frames > 170 and frames <= 200:
		var state = fish.call("get_state") if fish != null else -1
		var charging = fish.call("is_charging") if fish != null else false
		if state != 0 or charging:
			printerr("FAILED: state erroneously changed to ", state, ", charging=", charging, " at frame ", frames)
			quit(1)
			return true

	if frames == 205:
		print("SUCCESS: State remained 0 after reeling in! No accidental recast occurred.")
		test_passed = true
		quit(0)
		return true

	return false
