#include "save_system.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <vector>

#include "game_director.h"
#include "sky_cycle.h"
#include "chest.h"
#include "door.h"
#include "window.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

using namespace godot;

const char *SaveSystem::SAVE_PATH = "user://savegame.cfg";

namespace {

// 存档分区/键名（v1 格式）
const char *SEC = "game";

// v5：百叶窗固定路径（与 world.tscn 场景节点一一对应，顺序即 windows_open 定位序）
const char *WINDOW_PATHS[4] = {
	"Homestead/ShutterFL",
	"Homestead/ShutterFR",
	"Homestead/ShutterL",
	"Homestead/ShutterB",
};

} // namespace

void SaveSystem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("save_now"), &SaveSystem::save_now);
	ClassDB::bind_method(D_METHOD("load_now"), &SaveSystem::load_now);
	ClassDB::bind_method(D_METHOD("get_has_save"), &SaveSystem::get_has_save);
	ClassDB::bind_method(D_METHOD("trim_logs"), &SaveSystem::trim_logs);
}

void SaveSystem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不读不存（GDExtension 类在编辑器里也走 READY）
			}
			load_now(); // 读进缓存并尝试应用；节点没就绪由 _process 重试
			trim_logs(); // 顺带做一次日志体积兜底（幂等，几 KB 时零动作）
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			// —— 恢复收尾：SkyCycle/GameDirector 可能晚于本节点 READY，逐帧重试 ——
			if (!restored) {
				restore_targets();
			}
			// —— 每 30 秒自动存档 ——
			autosave_t += (float)get_process_delta_time();
			if (autosave_t >= AUTOSAVE_SECONDS) {
				autosave_t = 0.0f;
				save_now();
			}
		} break;
		case NOTIFICATION_EXIT_TREE:
		case NOTIFICATION_WM_CLOSE_REQUEST: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			save_now(); // 退出前兜底存一次（与自动存同格式，重复存无害）
		} break;
		default:
			break;
	}
}

SkyCycle *SaveSystem::find_sky() const {
	if (get_parent() == nullptr) {
		return nullptr;
	}
	return Object::cast_to<SkyCycle>(get_parent()->get_node_or_null(NodePath("SkyCycle")));
}

GameDirector *SaveSystem::find_director() const {
	if (get_parent() == nullptr) {
		return nullptr;
	}
	return Object::cast_to<GameDirector>(get_parent()->get_node_or_null(NodePath("GameDirector")));
}

Chest *SaveSystem::find_chest() const {
	if (get_parent() == nullptr) {
		return nullptr;
	}
	return Object::cast_to<Chest>(get_parent()->get_node_or_null(NodePath("Homestead/Chest")));
}

CabinDoor *SaveSystem::find_door() const {
	if (get_parent() == nullptr) {
		return nullptr;
	}
	return Object::cast_to<CabinDoor>(get_parent()->get_node_or_null(NodePath("Homestead/CabinDoor")));
}

ShutterWindow *SaveSystem::find_window(int p_index) const {
	if (get_parent() == nullptr || p_index < 0 || p_index >= 4) {
		return nullptr;
	}
	return Object::cast_to<ShutterWindow>(
			get_parent()->get_node_or_null(NodePath(WINDOW_PATHS[p_index])));
}

bool SaveSystem::get_has_save() const {
	return FileAccess::file_exists(SAVE_PATH);
}

bool SaveSystem::save_now() {
	SkyCycle *sky = find_sky();
	GameDirector *dir = find_director();
	if (sky == nullptr) {
		return false; // 世界还没就绪，绝不拿默认值覆盖好存档
	}

	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->set_value(SEC, "time_of_day", sky->get_time_of_day());
	cf->set_value(SEC, "day_length", sky->get_day_length());
	cf->set_value(SEC, "day_count", sky->get_day_count());
	cf->set_value(SEC, "weather", sky->get_weather());
	// *_target 必须一起存：漏存会让读档后 cloud/rain/snow 向 target 自衰减
	cf->set_value(SEC, "cloud", sky->get_cloud());
	cf->set_value(SEC, "cloud_target", sky->get_cloud_target());
	cf->set_value(SEC, "rain", sky->get_rain_amount());
	cf->set_value(SEC, "rain_target", sky->get_rain_target());
	cf->set_value(SEC, "snow", sky->get_snow_amount());
	cf->set_value(SEC, "snow_target", sky->get_snow_target());
	if (dir != nullptr) {
		cf->set_value(SEC, "director_count", dir->get_count());
		cf->set_value(SEC, "director_done", dir->is_done());
		cf->set_value(SEC, "director_daily", dir->get_daily_serial()); // v2：每日待办全量
		cf->set_value(SEC, "farm_plots", dir->get_farm_serial());      // v3：菜畦全量
	}
	// v4：木箱"发现野餐毯"标志——存前从 Chest 拉最新值；Chest 已拆树时保留缓存
	// （与 get_farm_serial 快照兜底同一铁律：缺源不清值，防退出存档覆盖好数据）
	Chest *chest = find_chest();
	if (chest != nullptr) {
		chest_looted = chest->is_looted();
	}
	cf->set_value(SEC, "chest_looted", chest_looted);

	// v5：门窗开合状态——存前拉最新值；缺源保留缓存（同 chest_looted 快照铁律：
	// 退出拆树期找不到节点时不得拿默认值覆盖好数据）。整组窗齐了才刷新序号，
	// 拆树期漏一个窗也整组保留，避免写半个序串。
	CabinDoor *door = find_door();
	if (door != nullptr) {
		if (door->is_open()) {
			door_state = door->get_open_sign() > 0 ? "1" : "-1"; // 连推/拉开向一起存
		} else {
			door_state = "0";
		}
	}
	cf->set_value(SEC, "door_state", door_state);
	bool all_windows = true;
	String win;
	for (int i = 0; i < 4; i++) {
		ShutterWindow *w = find_window(i);
		if (w == nullptr) {
			all_windows = false;
			break;
		}
		if (i > 0) {
			win += String(",");
		}
		win += String(w->is_open() ? "1" : "0");
	}
	if (all_windows) {
		windows_open = win;
	}
	cf->set_value(SEC, "windows_open", windows_open);

	return cf->save(SAVE_PATH) == Error::OK;
}

bool SaveSystem::load_now() {
	if (!get_has_save()) {
		restored = true; // 没有存档：走默认开局，恢复流程直接收尾
		return false;
	}
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load(SAVE_PATH) != Error::OK || !cf->has_section(SEC)) {
		// 读档失败（文件损坏等）：容错走默认值，不崩溃
		// （ConfigFile 对部分垃圾内容会宽松解析成功，用分区缺失兜底判废档）
		restored = true;
		return false;
	}

	// 字段缺失时 get_value 返回默认值：与 SkyCycle 初始值一致的安全快照
	time_of_day = (float)cf->get_value(SEC, "time_of_day", time_of_day);
	day_length = (float)cf->get_value(SEC, "day_length", day_length);
	day_count = (int)cf->get_value(SEC, "day_count", day_count);
	weather = (int)cf->get_value(SEC, "weather", weather);
	cloud = (float)cf->get_value(SEC, "cloud", cloud);
	cloud_target = (float)cf->get_value(SEC, "cloud_target", cloud_target);
	rain = (float)cf->get_value(SEC, "rain", rain);
	rain_target = (float)cf->get_value(SEC, "rain_target", rain_target);
	snow = (float)cf->get_value(SEC, "snow", snow);
	snow_target = (float)cf->get_value(SEC, "snow_target", snow_target);
	director_count = (int)cf->get_value(SEC, "director_count", director_count);
	director_done = (bool)cf->get_value(SEC, "director_done", director_done);
	// 字符串键缺失必须落回空串（不能用旧缓存兜底）：同进程先后读两份档时，
	// 新档缺键=没有该数据，拿上一份档的缓存回放会串状态。
	director_daily = cf->get_value(SEC, "director_daily", String());
	farm_serial = cf->get_value(SEC, "farm_plots", String()); // 旧档缺键=空串 → 全空畦
	chest_looted = (bool)cf->get_value(SEC, "chest_looted", false); // 旧档缺键=未发现
	door_state = cf->get_value(SEC, "door_state", String());       // 旧档缺键=空串 → 关门
	windows_open = cf->get_value(SEC, "windows_open", String());   // 旧档缺键=空串 → 全关

	loaded_data = true;
	restored = false;      // 有数据待应用，交给 _process 的重试通道
	restore_targets();     // 节点已就绪就立即应用（-s 脚本测试路径）
	return true;
}

void SaveSystem::restore_targets() {
	if (!loaded_data) {
		restored = true;
		return;
	}
	SkyCycle *sky = find_sky();
	GameDirector *dir = find_director();
	if (sky == nullptr) {
		return; // 下帧再试
	}
	sky->set_time_of_day(time_of_day);
	sky->set_day_length(day_length);
	sky->set_day_count(day_count);
	sky->set_weather(weather);
	sky->set_cloud(cloud);
	sky->set_cloud_target(cloud_target);
	sky->set_rain(rain);
	sky->set_rain_target(rain_target);
	sky->set_snow(snow);
	sky->set_snow_target(snow_target);
	// 月相：day_count 已恢复，get_moon_phase() 自动跟进，无需单独写
	if (dir != nullptr) {
		if (!director_daily.is_empty()) {
			// v2 存档：直接恢复整张当日清单（含天数，恢复后不弹"新的一天"）
			dir->apply_daily_serial(director_daily);
		} else if (director_count > 0) {
			// v1 旧档只有浆果单目标：折算成第 1 天清单走同一恢复路径
			dir->apply_daily_serial(String("1;0,") + String::num_int64(director_count) + String(",6"));
		}
		// v3：菜畦状态（菜畦生成晚于恢复时由 GameDirector 挂起回放，见 apply_farm_serial）
		if (!farm_serial.is_empty()) {
			dir->apply_farm_serial(farm_serial);
		}
	}
	// v4：木箱"发现野餐毯"标志（Chest 缺席只丢毯子回放，不阻塞恢复收尾）
	Chest *chest = find_chest();
	if (chest != nullptr && chest_looted) {
		chest->set_looted(true); // 回放只补摆毯子，不发公告不播音效
	}
	// v5：门窗快照就位（照 set_looted 套路：不补间不配音；缺节点只丢该项不阻塞）
	CabinDoor *door = find_door();
	if (door != nullptr) {
		int sign = 0;
		if (door_state == "1") {
			sign = 1;
		} else if (door_state == "-1") {
			sign = -1;
		}
		door->set_open_state(sign != 0, sign); // 非法/缺失序列一律落回关门
	}
	PackedStringArray win_parts = windows_open.split(",");
	for (int i = 0; i < 4; i++) {
		ShutterWindow *w = find_window(i);
		if (w == nullptr) {
			continue;
		}
		// 缺段/非法段一律落回关窗
		w->set_open_state(i < win_parts.size() && win_parts[i] == "1");
	}
	restored = true; // SkyCycle 是必恢复项；GameDirector 缺席只丢进度，不阻塞
}

void SaveSystem::trim_logs() {
	// Godot 只按份数轮转 user://logs/ 下的运行日志、不限制总体积，长期游玩/反复
	// 测试会让目录无限膨胀。启动时兜底一次：总量超过 10MB 就按修改时间从旧到新
	// 删除，回到 8MB 以内（留余量避免每次启动都触发）。godot.log 是本次运行正
	// 打开写的活跃文件，跳过不动。
	constexpr int64_t CAP_BYTES = 10LL * 1024 * 1024;
	constexpr int64_t GOAL_BYTES = 8LL * 1024 * 1024;

	const String dir_path = "user://logs";
	PackedStringArray names = DirAccess::get_files_at(dir_path);
	if (names.is_empty()) {
		return; // 目录不存在或为空（从未产生日志），静默跳过
	}
	struct LogEntry {
		String name;
		int64_t size = 0;
		uint64_t mtime = 0;
	};
	std::vector<LogEntry> entries;
	int64_t total = 0;
	for (int i = 0; i < names.size(); i++) {
		const String n = names[i];
		if (n == "godot.log") {
			continue; // 活跃日志不删
		}
		const String path = dir_path.path_join(n);
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
		if (!f.is_valid()) {
			continue; // 打不开（权限/竞态）就跳过，不让清理失败
		}
		LogEntry e;
		e.name = n;
		e.size = (int64_t)f->get_length();
		e.mtime = FileAccess::get_modified_time(path);
		total += e.size;
		entries.push_back(e);
	}
	if (total <= CAP_BYTES) {
		return;
	}
	std::sort(entries.begin(), entries.end(),
			[](const LogEntry &a, const LogEntry &b) { return a.mtime < b.mtime; }); // 最旧在前
	Ref<DirAccess> dir = DirAccess::open(dir_path);
	if (!dir.is_valid()) {
		return;
	}
	int64_t freed = 0;
	int removed = 0;
	for (const LogEntry &e : entries) {
		if (total - freed <= GOAL_BYTES) {
			break;
		}
		if (dir->remove(e.name) == Error::OK) {
			freed += e.size;
			removed++;
		}
	}
	if (removed > 0) {
		UtilityFunctions::print(String::utf8("[SaveSystem] 日志目录超过 10MB，已清理最旧日志 ")
				+ String::num_int64(removed) + String::utf8(" 个，释放 ")
				+ String::num_int64(freed / 1024) + String::utf8(" KB"));
	}
}
