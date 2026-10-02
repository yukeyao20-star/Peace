#include "save_system.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>

#include "game_director.h"
#include "sky_cycle.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

using namespace godot;

const char *SaveSystem::SAVE_PATH = "user://savegame.cfg";

namespace {

// 存档分区/键名（v1 格式）
const char *SEC = "game";

} // namespace

void SaveSystem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("save_now"), &SaveSystem::save_now);
	ClassDB::bind_method(D_METHOD("load_now"), &SaveSystem::load_now);
	ClassDB::bind_method(D_METHOD("get_has_save"), &SaveSystem::get_has_save);
}

void SaveSystem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不读不存（GDExtension 类在编辑器里也走 READY）
			}
			load_now(); // 读进缓存并尝试应用；节点没就绪由 _process 重试
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
	}

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
		dir->set_count(director_count);
		dir->set_done(director_done);
	}
	restored = true; // SkyCycle 是必恢复项；GameDirector 缺席只丢进度，不阻塞
}
