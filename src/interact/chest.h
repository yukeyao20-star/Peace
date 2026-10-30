#pragma once

// Chest —— 可以打开/关闭的木箱（MC 储物箱风味）。
//
// 结构（局部坐标：原点在箱底中心，+Z 朝屋内一侧即"正面"）：
//   Chest
//   ├─ Body          StaticBody3D 箱体（有碰撞）
//   │   ├─ 箱体木板 / 顶部内衬（打开后能看到深色内腔）/ 黄铜锁扣座 / 四角包边
//   └─ LidPivot      盖板合页枢轴（在后上边缘，绕 X 轴向后旋开）
//      └─ Lid        StaticBody3D 盖板（有碰撞，开关时跟着转）
//         ├─ 盖板 / 黄铜锁鼻 / 两道横向加固条
//
// 开关动画与 CabinDoor 相同：Hermite 缓动约 0.33 秒；盖板向后旋开约 109°。
//
// 一次性彩蛋（ROADMAP #11）：首次打开翻出"野餐毯"——发 chest_looted 信号
// （GameDirector 中央公告），毯子+藤篮小道具摆在箱旁，本地合成"叮咚"提示音；
// looted 标志经 SaveSystem 的 chest_looted 键持久化（读档回放只补摆毯子）。

#include "interactable.h"

#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Chest : public Interactable {
	GDCLASS(Chest, Interactable)

private:
	Node3D *lid_pivot = nullptr; // 盖板合页枢轴
	float open_t = 0.0f;         // 当前开合进度 0(关)..1(开)
	float open_target = 0.0f;    // 目标进度

	class AudioStreamPlayer3D *audio_player = nullptr;
	Ref<class AudioStream> snd_open;
	Ref<class AudioStream> snd_close;

	bool looted = false;          // "发现野餐毯"已触发（SaveSystem 持久化）
	float chime_t = -1.0f;        // 发现叮咚延时计时（>0 = 等开箱吱呀收尾）
	Node3D *blanket = nullptr;    // 野餐毯小道具（发现后摆在箱旁）
	class AudioStreamPlayer3D *chime_player = nullptr;
	Ref<class AudioStream> snd_discover; // 程序合成两音上行"叮咚"

	void build(Ref<Shader> p_shader);
	void spawn_blanket(Ref<Shader> p_shader);
	Ref<class AudioStreamWAV> make_discover_chime(); // 22050Hz 16-bit，纯代码合成

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
	void _process(double p_delta) override;

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual float get_interact_distance() const override { return 1.8f; }
	virtual void interact(const Vector3 &p_player_pos) override;

	bool is_open() const { return open_target > 0.5f; }
	bool is_looted() const { return looted; }
	void set_looted(bool p_looted); // SaveSystem 读档回放用（true 时补摆毯子）
};

} // namespace godot
