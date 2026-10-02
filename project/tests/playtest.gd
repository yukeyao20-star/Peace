extends SceneTree
# Automated playtest smoke for world.tscn (Godot 4.7 + GDExtension).
# Run:
#   Godot --headless --path /Users/lei/Downloads/3Dgame/project -s res://tests/playtest.gd
#
# Phase machine driven from MainLoop._process; returns true (quit) when done.
# Output lines: "PASS/WARN/SKIP/FAIL: message" + final summary. English only
# (godot-cpp String(const char*) is latin-1; keep stdout ASCII-safe).

const START_FRAME := 5
const MOVE_FRAMES := 60
const INTERACT_WAIT_FRAMES := 30
const MAX_FRAMES := 600

var frame := 0
var phase_frame := 0
var phase := 0
var done := false

var world: Node = null
var player: Node3D = null

var move_from := Vector3.ZERO
var move_dist := -1.0

var bush: Node3D = null
var berries: Node3D = null
var interact_called := false

var n_pass := 0
var n_warn := 0
var n_fail := 0
var n_skip := 0
var quit_countdown := -1 # frames to idle after cleanup so audio playbacks free


func _initialize() -> void:
	print("PLAYTEST: loading res://world.tscn ...")
	var ps: PackedScene = load("res://world.tscn")
	if ps == null:
		_rec("FAIL", "world.tscn could not be loaded")
		done = true
		return
	world = ps.instantiate()
	if world == null:
		_rec("FAIL", "world.tscn instantiate() returned null")
		done = true
		return
	root.add_child(world)
	print("PLAYTEST: world instantiated, phase machine starting")


func _rec(status: String, msg: String) -> void:
	print(status + ": " + msg)
	if status == "PASS":
		n_pass += 1
	elif status == "WARN":
		n_warn += 1
	elif status == "SKIP":
		n_skip += 1
	elif status == "FAIL":
		n_fail += 1


func _key(code: int, pressed: bool) -> void:
	# player.cpp reads Input::is_key_pressed(KEY_*) directly (no actions),
	# so inject raw InputEventKey with both keycode and physical_keycode.
	var ev := InputEventKey.new()
	ev.keycode = code as Key
	ev.physical_keycode = code as Key
	ev.pressed = pressed
	Input.parse_input_event(ev)


func _find_player() -> Node3D:
	if world == null:
		return null
	var direct := world.get_node_or_null("Player")
	if direct != null and direct is Node3D:
		return direct as Node3D
	# Fallback: class-name scan under world root.
	var stack: Array = [world]
	while not stack.is_empty():
		var n: Node = stack.pop_back()
		if n.get_class() == "Player" and n is Node3D:
			return n as Node3D
		stack.append_array(n.get_children())
	return null


func _find_bush() -> Node3D:
	# Prefer the dedicated group, fall back to interactables + class filter.
	var candidates: Array = get_nodes_in_group("berry_bushes")
	if candidates.is_empty():
		for n in get_nodes_in_group("interactables"):
			if n.get_class() == "BerryBush":
				candidates.append(n)
	var best: Node3D = null
	var best_d := INF
	var origin := player.global_position if player != null else Vector3.ZERO
	for n in candidates:
		var b := n as Node3D
		if b == null:
			continue
		var d: float = origin.distance_squared_to(b.global_position)
		if d < best_d:
			best_d = d
			best = b
	return best


func _find_label(from: Node) -> Label:
	if from is Label:
		return from as Label
	for c in from.get_children():
		var l := _find_label(c)
		if l != null:
			return l
	return null


func _process(_delta: float) -> bool:
	if done:
		# Give the audio server a few live frames to release playbacks
		# (streams were stopped at summary time), then free the world,
		# idle once more and quit.
		if quit_countdown == 2:
			_stop_audio(root)
			if world != null and world.is_inside_tree():
				root.remove_child(world)
				world.free()
				world = null
		if quit_countdown > 0:
			quit_countdown -= 1
			return false
		return true
	frame += 1
	phase_frame += 1
	if done:
		return true
	if frame > MAX_FRAMES:
		_rec("FAIL", "global frame budget (%d) exceeded, aborting" % MAX_FRAMES)
		return _finish()
	var keep_going := true
	match phase:
		0:
			keep_going = _phase_scene()
		1:
			keep_going = _phase_move()
		2:
			keep_going = _phase_interact()
		3:
			keep_going = _phase_hud()
		_:
			return _finish()
	if not keep_going:
		return _finish()
	return false


# ---- Phase 0: scene sanity ------------------------------------------------
func _phase_scene() -> bool:
	if frame < START_FRAME:
		return true
	var ok := true
	if world == null or not world.is_inside_tree():
		_rec("FAIL", "world node missing or not in tree")
		ok = false
	else:
		_rec("PASS", "world scene loaded and inside tree")

	player = _find_player()
	if player != null:
		_rec("PASS", "Player found at %s (pos %s)" % [player.get_path(), player.global_position])
	else:
		_rec("FAIL", "Player node not found (group/class scan)")
		ok = false

	var inter := get_nodes_in_group("interactables")
	if inter.size() > 0:
		_rec("PASS", "interactables group has %d instances" % inter.size())
	else:
		_rec("FAIL", "interactables group is empty")
		ok = false

	var hud := world.get_node_or_null("HUDClock") if world != null else null
	if hud != null:
		_rec("PASS", "HUDClock found at %s" % hud.get_path())
	else:
		var any_label := _find_label(root) if root != null else null
		if any_label != null:
			_rec("WARN", "HUDClock node not found by name, but a Label exists")
		else:
			_rec("WARN", "HUDClock/Label not found (non-fatal)")

	var sky := world.get_node_or_null("SkyCycle") if world != null else null
	if sky != null:
		_rec("PASS", "SkyCycle found at %s" % sky.get_path())
	else:
		_rec("WARN", "SkyCycle not found (non-fatal)")

	# Informational only (not judged): GameDirector node from the other task.
	var gd := world.get_node_or_null("GameDirector") if world != null else null
	if gd != null:
		print("INFO: GameDirector node present at %s (not judged)" % gd.get_path())
	else:
		print("INFO: GameDirector node absent (not judged)")

	if player == null or bush == null:
		bush = _find_bush()
	if not ok:
		return false
	_next_phase()
	return true


# ---- Phase 1: simulated movement ------------------------------------------
func _phase_move() -> bool:
	if player == null:
		_rec("SKIP", "movement test skipped: no Player node")
		_next_phase()
		return true
	if phase_frame == 1:
		move_from = player.global_position
		_key(KEY_W, true)
		return true
	if phase_frame <= MOVE_FRAMES:
		return true
	# One extra frame so the last physics tick sees the released key.
	if phase_frame == MOVE_FRAMES + 1:
		_key(KEY_W, false)
		return true
	move_dist = player.global_position.distance_to(move_from)
	if move_dist > 0.5:
		_rec("PASS", "player moved %.2f m after %d frames of W input (> 0.5 m)" % [move_dist, MOVE_FRAMES])
	else:
		_rec("FAIL", "player displacement %.2f m after %d frames of W input (need > 0.5 m)" % [move_dist, MOVE_FRAMES])
	_next_phase()
	return true


# ---- Phase 2: berry bush interaction ---------------------------------------
func _phase_interact() -> bool:
	if phase_frame == 1:
		if bush == null:
			bush = _find_bush()
		if bush == null:
			_rec("SKIP", "no BerryBush in scene, interaction test skipped")
			_next_phase()
			return true
		if player != null:
			# Teleport within 2 m of the bush (bush interact pos is +0.5 y).
			player.global_position = bush.global_position + Vector3(1.2, 0.8, 0.0)
		berries = bush.get_node_or_null("Berries") as Node3D
		if berries == null:
			_rec("SKIP", "BerryBush has no 'Berries' child to observe")
			_next_phase()
			return true
		if not berries.visible:
			_rec("SKIP", "berries already hidden (regrowing), cannot test pick")
			_next_phase()
			return true
		return true
	if phase_frame == 2:
		return true # frame for physics to settle after teleport
	if phase_frame == 3 and not interact_called:
		# BerryBush::interact is ClassDB-bound, call it directly.
		var ppos := player.global_position if player != null else bush.global_position
		bush.interact(ppos)
		interact_called = true
		return true
	if interact_called:
		var hidden := not berries.visible
		var prompt_empty := true
		if bush.has_method("get_prompt"):
			prompt_empty = String(bush.get_prompt()).is_empty()
		if hidden and prompt_empty:
			_rec("PASS", "berries hidden and prompt empty within %d frames after interact()" % INTERACT_WAIT_FRAMES)
			_next_phase()
		elif phase_frame >= 3 + INTERACT_WAIT_FRAMES:
			_rec("FAIL", "berries.visible=%s prompt_empty=%s after %d frames" % [str(berries.visible), str(prompt_empty), INTERACT_WAIT_FRAMES])
			_next_phase()
		return true
	_next_phase()
	return true


# ---- Phase 3: HUD clock ----------------------------------------------------
func _phase_hud() -> bool:
	var hud := world.get_node_or_null("HUDClock") if world != null else null
	if hud == null:
		_rec("SKIP", "HUD test skipped: no HUDClock node")
		_next_phase()
		return true
	# Give the per-frame refresh() a moment, then assert.
	if phase_frame < 5:
		return true
	var label := _find_label(hud)
	if label == null:
		_rec("WARN", "no Label found under HUDClock")
		_next_phase()
		return true
	var text := label.text
	if text.is_empty():
		_rec("WARN", "HUDClock Label text is empty (SkyCycle not resolved?)")
	elif text.find("·") == -1:
		_rec("WARN", "HUDClock Label text lacks '·' separator: len=%d" % text.length())
	else:
		_rec("PASS", "HUDClock Label refreshed, text length %d with '·' separators" % text.length())
	_next_phase()
	return true


func _next_phase() -> void:
	phase += 1
	phase_frame = 0


func _finish() -> bool:
	if not done:
		done = true
		# Stop any still-playing streams (footsteps / pick pop) so the audio
		# server can release its playbacks during the idle frames that follow.
		_stop_audio(root)
		print("PLAYTEST SUMMARY: %d pass, %d warn, %d skip, %d fail" % [n_pass, n_warn, n_skip, n_fail])
		quit_countdown = 3
	return true


func _stop_audio(from: Node) -> void:
	if from is AudioStreamPlayer or from is AudioStreamPlayer3D:
		from.stop()
	for c in from.get_children():
		_stop_audio(c)
