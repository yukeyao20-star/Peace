#pragma once

// SaveSystem —— 存档系统（任务 #28：解决"退出即清零"）。
//
// 用 ConfigFile 把世界状态写到 user://savegame.cfg：
//   * SkyCycle 全状态：time_of_day / day_length / day_count / weather /
//     cloud / cloud_target / rain / rain_target / snow / snow_target
//     （*_target 必须一起存，否则读档后 cloud/rain/snow 会向 target 自衰减清零）
//   * GameDirector 进度：count / done
//   * 月相由 day_count 推导（get_moon_phase 唯一来源），无需单独存。
//
// 触发：READY 读档一次；READY 后每 30 秒自动存；EXIT_TREE / WM_CLOSE_REQUEST 存一次
// （全部非编辑器守卫）。恢复走懒查找（SkyCycle/GameDirector 可能晚于本节点 READY），
// 用 _process 里 restored 门控逐帧重试，同 GameDirector::connect_bushes 套路。
//
// v1 未覆盖（后续项）：门窗/木箱开关状态、玩家位置、weather_timer（读档后按默认
// 剩余时长继续，到点正常换天）、浆果丛当前重长进度。

#include <godot_cpp/classes/node.hpp>

namespace godot {

class GameDirector;
class SkyCycle;

class SaveSystem : public Node {
	GDCLASS(SaveSystem, Node)

private:
	static const char *SAVE_PATH; // "user://savegame.cfg"

	static constexpr float AUTOSAVE_SECONDS = 30.0f; // 自动存档间隔

	bool restored = false;     // 读档应用流程已收尾（无论有无存档）
	bool loaded_data = false;  // 存档文件存在且成功读入
	float autosave_t = 0.0f;   // 自动存档计时（秒）

	// 读档缓存（字段缺失时保持这里的安全默认值，与 SkyCycle 初始值一致）
	float time_of_day = 0.32f;
	float day_length = 240.0f;
	int day_count = 1;
	int weather = 0;
	float cloud = 0.0f, cloud_target = 0.0f;
	float rain = 0.0f, rain_target = 0.0f;
	float snow = 0.0f, snow_target = 0.0f;
	int director_count = 0;
	bool director_done = false;

	SkyCycle *find_sky() const;
	GameDirector *find_director() const;
	void restore_targets(); // 把缓存值写回 SkyCycle/GameDirector（缺谁下帧再试）

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool save_now();    // 把当前世界状态写入存档，成功返回 true
	bool load_now();    // 读档并立即尝试应用（场景就绪时），返回是否读到数据
	bool get_has_save() const; // 存档文件是否存在
};

} // namespace godot
