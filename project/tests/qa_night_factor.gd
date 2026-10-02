# 临时 QA：验证 SkyCycle::get_night_factor() 入夜带映射。
# 用法：Godot --headless --path project -s res://tests/qa_night_factor.gd
extends SceneTree

func _initialize() -> void:
	var sky := SkyCycle.new()
	root.add_child(sky)
	var f := FileAccess.open("/tmp/td19_night2.txt", FileAccess.WRITE)
	for tod in [0.762, 0.764, 0.768, 0.772]:
		sky.set_time_of_day(tod)
		f.store_line("tod=%.3f night=%.4f" % [tod, sky.get_night_factor()])
	f.close()
	root.remove_child(sky)
	sky.free()
	quit(0)
