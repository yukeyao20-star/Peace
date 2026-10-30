extends SceneTree
# perf_probe.gd —— 性能基线探针（ROADMAP #9，docs/PERF.md 配套工具）
#
# 方法：RenderingServer.force_draw(false) 离屏渲染 + 视口回读强制 GPU 同步。
#       单帧墙钟成本 = 真实 GPU 帧成本，不经窗口呈现 → 免疫 macOS 非前台窗口节流
#       （PERF.md 实锤：脚本窗口自然帧率被系统压到 ~10fps，不可信）。
#       回读本身有固定开销（~9ms @3840×1944），先测「空场景基线」再从各机位毛值中扣除，
#       报告同时给毛值 frame_us_avg 与净值 frame_us_net。
#
# 采样协议：
#   * SkyCycle 锁定正午 + 晴天（每机位采样前重申），保证光照/粒子负载可比；
#   * 每机位先自然跑 SETTLE 帧完成落地/相机跟随/着色器变体编译，再离屏采样 SAMPLE_N 次；
#   * 需要看水的机位先按住 camera_right 30 帧转向（InputMap 动作模拟）；
#   * 画质三档阶梯：high(特效开+缩放1.0) → low(特效关+缩放0.7) → ultra(特效关+缩放0.5)，
#     low/ultra 只复测两个最重机位（石桥湖畔=SSR 大头、足球场=10 万草几何大头）。
#
# 运行（约 2 分钟，窗口会短暂打开：已静音、鼠标不捕获、HOME 隔离不碰真实存档）：
#   cd <repo根> && HOME=$PWD/tmp/test_home /Applications/Godot.app/Contents/MacOS/Godot \
#     --path project -s "$PWD/tools/perf_probe.gd" --quit-after 4000 \
#     > tmp/cache/perf_probe.log 2>&1
# 输出：逐行 "PERF {json}" + 末行 "PERF_SUMMARY {json}"（grep '^PERF' 提取）。
# 环境变量：ZCODE_PERF_QUICK=1 只测出生点高画质（改脚本后先跑它验链路）。

const WARMUP := 150        # 开局预热帧
const SETTLE := 90         # 传送后稳定帧
const YAW_HOLD := 30       # 转向帧数（camera_right 2.4rad/s → ~72°）
const SAMPLE_N := 30       # 每机位离屏采样次数

# 画质阶梯：[档名, 是否开 SSR/SSAO/Glow, scaling_3d_scale, 是否开 MSAA 4x]
# 低/极低档与游戏内「画质：低（流畅）」一致：2026-10-30 起该档同时关闭 MSAA。
const PASSES := [
	["high", true, 1.0, true],
	["low", false, 0.7, false],
	["ultra", false, 0.5, false],
]

# [机位名, x, y, z, 是否转向]（坐标出处：terrain_builder.h 湖心/球场 + 石桥端头 + 瀑布喷口）
const SPOTS := [
	["spawn_cabin", 0.0, 1.2, -28.0, false],
	["lake_south_shore", -6.0, 1.6, 9.5, true],
	["stone_bridge_west", -20.0, 1.4, 26.0, false],
	["soccer_field", 60.0, 1.2, -32.0, false],
	["waterfall_pool", 86.0, 5.0, 86.0, true],
]
const HEAVY_SPOTS := ["stone_bridge_west", "soccer_field"]

var world: Node = null
var env_res: Environment = null
var sky: Node = null
var state := "boot"
var frame_ct := 0
var spot_i := 0
var pass_i := 0
var yaw_hold := 0
var base_acc := 0
var base_n := 0
var base_us := 0
var results: Array = []
var img_size := ""

func _initialize() -> void:
	AudioServer.set_bus_mute(0, true)          # 采样不打扰使用者
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE # 防游戏抢捕获鼠标
	process_frame.connect(_tick)

func _apply_pass(p: int) -> void:
	if env_res != null:
		env_res.set_ssr_enabled(PASSES[p][1])
		env_res.set_ssao_enabled(PASSES[p][1])
		env_res.set_glow_enabled(PASSES[p][1])
	# 像素缩放是像素填充型负载的主兜底（Retina hidpi 下 backbuffer 为逻辑 2x）
	root.scaling_3d_scale = PASSES[p][2]
	# MSAA 4x 是 2026-10-30 实测的第二大杠杆（高→关约 −20%），低档随之关闭
	root.msaa_3d = Viewport.MSAA_4X if PASSES[p][3] else Viewport.MSAA_DISABLED

func _tick() -> void:
	frame_ct += 1
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	match state:
		"boot":
			state = "baseline"
		"baseline":
			# 第 4~13 帧测空场景回读基线（前几帧有驱动初始化噪声）
			if frame_ct >= 4 and frame_ct <= 13:
				base_acc += _readback_cost()
				base_n += 1
			elif frame_ct > 13:
				base_us = int(round(float(base_acc) / base_n))
				print("PERF baseline_readback_us=", base_us)
				world = load("res://world.tscn").instantiate()
				root.add_child(world)
				state = "wait_ready"
		"wait_ready":
			var we: Node = world.get_node_or_null("WorldEnvironment")
			var pl: Node = world.get_node_or_null("Player")
			var sc: Node = world.get_node_or_null("SkyCycle")
			if we != null and pl != null and sc != null:
				env_res = we.environment
				sky = sc
				_apply_pass(0)
				sky.set_time_of_day(0.5)  # 锁正午
				sky.set_weather(0)        # 锁晴天
				state = "warmup"
		"warmup":
			if frame_ct >= WARMUP:
				if OS.get_environment("ZCODE_PERF_QUICK") == "1":
					results.append(_measure(0, 0))
					print("PERF_SUMMARY ", JSON.stringify({"baseline_readback_us": base_us, "results": results}))
					print("PERF DONE (quick)")
					quit(0)
				else:
					_start_spot(0)
		"settle":
			if yaw_hold > 0:
				yaw_hold -= 1
				if yaw_hold == 0:
					Input.action_release("camera_right")
			_phase_frame += 1
			if _phase_frame >= SETTLE:
				_phase_frame = 0
				_after_settle()
		_:
			pass

var _phase_frame := 0

func _start_spot(i: int) -> void:
	spot_i = i
	if sky != null:
		sky.set_weather(0)  # 重申晴天，防 40s+ 天气轮换混入粒子负载
	var pl: Node3D = world.get_node("Player")
	pl.global_position = Vector3(SPOTS[i][1], SPOTS[i][2], SPOTS[i][3])
	pl.velocity = Vector3.ZERO
	yaw_hold = YAW_HOLD if SPOTS[i][4] else 0
	if yaw_hold > 0:
		Input.action_press("camera_right")
	_phase_frame = 0
	state = "settle"

func _after_settle() -> void:
	var rec: Dictionary = _measure(spot_i, pass_i)
	results.append(rec)
	if pass_i == 0 and spot_i + 1 < SPOTS.size():
		_start_spot(spot_i + 1)
	elif pass_i == 0:
		# 高画质全机位完成 → 进入 low 档，从最重机位复测
		pass_i = 1
		_apply_pass(pass_i)
		_start_spot(_heavy_index(0))
	elif spot_i + 1 < SPOTS.size() and SPOTS[spot_i + 1][0] in HEAVY_SPOTS:
		_start_spot(spot_i + 1)
	elif pass_i < PASSES.size() - 1:
		pass_i += 1
		_apply_pass(pass_i)
		_start_spot(_heavy_index(0))
	else:
		var summary := {"baseline_readback_us": base_us, "results": results}
		if img_size != "":
			summary["readback_size"] = img_size
		print("PERF_SUMMARY ", JSON.stringify(summary))
		print("PERF DONE")
		quit(0)

func _heavy_index(k: int) -> int:
	for i in SPOTS.size():
		if SPOTS[i][0] == HEAVY_SPOTS[k]:
			return i
	return 0

func _measure(i: int, p: int) -> Dictionary:
	var costs: Array = []
	var dc: Array = []
	var pr: Array = []
	var acc := 0
	for j in SAMPLE_N:
		var c := _readback_cost()
		costs.append(c)
		acc += c
		dc.append(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME))
		pr.append(Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME))
	costs.sort()
	dc.sort()
	pr.sort()
	if img_size == "":
		var im: Image = root.get_texture().get_image()
		if im != null:
			img_size = "%dx%d" % [im.get_width(), im.get_height()]
			print("PERF readback_size=", img_size, " window=", DisplayServer.window_get_size())
	var avg := int(round(float(acc) / SAMPLE_N))
	return {
		"spot": SPOTS[i][0], "quality": PASSES[p][0],
		"frame_us_avg": avg, "frame_us_p95": costs[int(SAMPLE_N * 0.95) - 1],
		"frame_us_min": costs[0], "frame_us_net": avg - base_us,
		"draw_calls": dc[int(SAMPLE_N / 2)],
		"primitives_k": int(round(float(pr[int(SAMPLE_N / 2)]) / 1000.0)),
		"scaling": PASSES[p][2],
	}

func _readback_cost() -> int:
	var t0 := Time.get_ticks_usec()
	RenderingServer.force_draw(false)
	var img: Image = root.get_texture().get_image()
	if img != null and img.get_width() > 8:
		# 触碰像素数据，确保回读真正完成（取中心 4x4 区域）
		img.get_region(Rect2i(img.get_width() / 2, img.get_height() / 2, 4, 4))
	return int(Time.get_ticks_usec() - t0)
