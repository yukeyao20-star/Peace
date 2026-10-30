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
// v1 未覆盖（后续项）：玩家位置、weather_timer（读档后按默认
// 剩余时长继续，到点正常换天）、浆果丛当前重长进度。
// v3 增量：菜畦状态（farm_plots 键，经 GameDirector 聚合/回放；旧档缺键=全空畦）。
// v4 增量：木箱"发现野餐毯"一次性标志（chest_looted 键；旧档缺键=未发现）。
// v5 增量：门窗开合状态（door_state "0/1/-1" 含推拉向 + windows_open "1,0,0,1"
// 定位序 FL,FR,L,B；旧档缺键=全关）。

#include <godot_cpp/classes/node.hpp>

namespace godot {

class GameDirector;
class SkyCycle;
class Chest;
class CabinDoor;
class ShutterWindow;

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
	String director_daily; // 每日待办序列 "day:type,prog,target;..."（v2 存档格式）
	String farm_serial;    // 菜畦序列 "state,%,浇灌;..."×4（v3 增量键，旧档缺省为空=全空畦）
	bool chest_looted = false; // 木箱"发现野餐毯"标志（v4 增量键，旧档缺省=未发现）

	String door_state;   // 木门 "0"关 / "1"推 / "-1"拉（v5 增量键，旧档缺省=关）
	String windows_open; // 百叶窗定位序 "1,0,0,1"×4 FL,FR,L,B（v5 增量键，旧档缺省=全关）

	SkyCycle *find_sky() const;
	GameDirector *find_director() const;
	Chest *find_chest() const;
	CabinDoor *find_door() const;
	ShutterWindow *find_window(int p_index) const; // 0..3 → ShutterFL/FR/L/B 固定路径
	void restore_targets(); // 把缓存值写回 SkyCycle/GameDirector（缺谁下帧再试）
	void trim_logs();       // user://logs/ 超过 10MB 时按最旧优先删除（引擎只按份数轮转不限体积）

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	bool save_now();    // 把当前世界状态写入存档，成功返回 true
	bool load_now();    // 读档并立即尝试应用（场景就绪时），返回是否读到数据
	bool get_has_save() const; // 存档文件是否存在
};

} // namespace godot
