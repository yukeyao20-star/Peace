# 鼠标视角手感验证
#
#   1) 合成 200 像素的鼠标位移，看 yaw 变化是不是 = 位移 × 灵敏度
#      （走"位移路线"就精确等于；走老的"速度×帧时间"积分就不是这个数）
#   2) 转完之后隔两帧再看 yaw —— 相机位置平滑已经挪到"跟随点"上了，
#      环绕应该立刻到位，两帧之间不该再有残余变化。
#
# 用法：
#   /Applications/Godot.app/Contents/MacOS/Godot --path project --resolution 1600x1000 \
#       -s tests/qa_look_sens.gd
extends SceneTree

const SENS := 0.0032     # 与 Player::mouse_sens 一致
const CAM_HEIGHT := 1.55 # 与 Player::camera_height 一致
const OUT := "/Users/lei/Downloads/3Dgame/docs/qa/"

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null
var yaw0 := 0.0
var yaw_a := 0.0

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

# 从相机位置反推环绕偏航角（cam_yaw 没暴露给脚本，用几何反算）
func _cam_yaw() -> float:
	var head: Vector3 = player.get("global_position") + Vector3(0.0, CAM_HEIGHT, 0.0)
	var off: Vector3 = cam.get("global_position") - head
	return atan2(off.x, off.z)

func _lmb(pressed: bool) -> void:
	var ev := InputEventMouseButton.new()
	ev.button_index = MOUSE_BUTTON_LEFT
	ev.pressed = pressed
	Input.parse_input_event(ev)

func _motion(dx: float, dy: float) -> void:
	var ev := InputEventMouseMotion.new()
	ev.relative = Vector2(dx, dy)
	Input.parse_input_event(ev)

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
		player = world.get_node_or_null("Player")
		if player != null:
			# 注意：**不要 set_process(false)** —— 视角与相机都在 NOTIFICATION_PROCESS 里跑。
			# 只关物理，让角色定在原地不掉下去。
			player.set_physics_process(false)
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

	if frames == 60:
		cam = player.get_node_or_null("Camera3D") if player != null else null
		print("CAM found=", cam != null,
				"  sens=", (player.get("mouse_sensitivity") if player != null else "?"))

	# 点一下左键捕获鼠标（Player 靠这个进 MOUSE_MODE_CAPTURED）
	if frames == 70:
		_lmb(true)
		return false
	if frames == 72:
		_lmb(false)
		return false

	# 灌 200 像素的右移
	if frames == 80:
		yaw0 = _cam_yaw()
		print("yaw0 = %.5f" % yaw0)
		for i in range(20):
			_motion(10.0, 0.0)
		return false

	if frames == 84:
		yaw_a = _cam_yaw()
		var d := yaw_a - yaw0
		print("--- 200px 右移之后 ---")
		print("实测 delta_yaw = %.5f rad" % d)
		print("位移路线应为   = %.5f rad" % (-200.0 * SENS))
		print("偏差           = %.5f rad" % (d - (-200.0 * SENS)))
		return false

	# 再等两帧：如果相机位置还在平滑追赶，yaw 会继续变
	if frames == 86:
		var yaw_b := _cam_yaw()
		print("--- 隔两帧后 ---")
		print("残余变化 = %.6f rad（应接近 0，说明环绕是立刻到位的）" % (yaw_b - yaw_a))
		root.get_viewport().get_texture().get_image().save_png(OUT + "look_after.png")
		print("SAVE look_after")
		return true

	return false
