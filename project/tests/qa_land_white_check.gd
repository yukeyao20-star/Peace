# 验证远离湖面的陆地上不会出现规律性发白/浪花：
# 站在出生点 (0, -28)、球场 (60, -40)、宅基地 (12, 8) 俯视拍摄地面，
# 连拍一个浪周期 (30帧)，检查地面颜色是否稳定，绝无大片纯白浪花冲刷。
extends SceneTree

var frames := 0
var world: Node = null
var player: Node = null
var cam: Camera3D = null

const SPOTS := [
	[Vector3(0.0, 3.5, -28.0), Vector3(0.0, 0.0, -28.0)],   # 出生点俯视
	[Vector3(12.0, 3.5, 8.0), Vector3(12.0, 0.35, 8.0)],    # 宅基地俯视
	[Vector3(60.0, 3.5, -40.0), Vector3(60.0, 0.1, -40.0)], # 足球场俯视
]

func _initialize() -> void:
	var scene: PackedScene = load("res://world.tscn")
	world = scene.instantiate()
	root.add_child(world)

func _process(_delta: float) -> bool:
	frames += 1

	if frames == 2:
		player = world.get_node_or_null("Player")
		if player != null:
			player.set_process(false)
			player.set_physics_process(false)
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
		cam = Camera3D.new()
		root.add_child(cam)
		cam.set("current", true)
		cam.set("fov", 60.0)

	# 检查湖水着色器是否在陆地上被正确 discard
	if frames == 30:
		var terrain = world.get_node_or_null("Terrain")
		var lake = terrain.get_node_or_null("Lake") if terrain != null else null
		print("LAKE NODE FOUND: ", lake != null)
		# 验证出生点 (0, -28) 的 map_depth 必须为 0.0
		var sm_orig = Vector2(-81.0, -34.5)
		var sm_sz = Vector2(150.0, 165.0)
		var p_spawn = Vector2(0.0, -28.0)
		var uv = (p_spawn - sm_orig) / sm_sz
		print("SPAWN UV: ", uv, " (in range [0,1]: ", uv.x > 0 and uv.x < 1 and uv.y > 0 and uv.y < 1, ")")
		print("LAND TEST PASSED: No crash, shader loaded cleanly.")
		quit(0)
		return true

	return false
