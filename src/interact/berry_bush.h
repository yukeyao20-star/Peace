#pragma once

// BerryBush —— 可采摘浆果丛（接入 Interactable 交互框架）。
//
// 结构（局部坐标：原点在地面中心，y=0 贴地）：
//   BerryBush
//   ├─ Foliage      3 团压扁球体灌丛（卡通深绿，错落堆叠）
//   ├─ Berries      浆果组（红果小球撒在灌丛表面，采摘时整组隐藏）
//   └─ SnowCaps     3 片压扁白球雪毯搭在各团灌丛顶（雪量驱动淡入，无雪隐形）
//
// 交互：靠近按 E 采浆果 → 果组隐藏 → REGROW_SECONDS 秒后重新长出
// （0.5 秒 Hermite 缓动弹出）。可无限重复采摘，纯收集乐趣无背包。
// 浆果落点用世界坐标哈希播种（同 Bird 套路）：各丛形态可复现又不雷同。

#include "interactable.h"

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class SkyCycle;
class StandardMaterial3D;

class BerryBush : public Interactable {
	GDCLASS(BerryBush, Interactable)

private:
	Node3D *berries = nullptr;            // 浆果组
	Node3D *snow_caps = nullptr;          // 雪毯组（雪量驱动显隐/透明度）
	Ref<StandardMaterial3D> snow_mat;     // 雪毯共用材质（每帧只调 albedo 透明度）
	SkyCycle *cycle = nullptr;            // 懒查找的 SkyCycle（读雪量因子）
	AudioStreamPlayer3D *voice = nullptr; // 采摘音效（3D 声源）
	Ref<AudioStreamWAV> snd_pop;          // 程序化合成的"啵"声
	float regrow_t = 0.0f;                // 重长倒计时（秒）；<=0 表示果满
	float pop_t = 1.0f;                   // 长出弹出动画进度 0..1

	void build();
	void update_snow();               // 雪毯显隐/透明度随雪量（找不到 SkyCycle 视为无雪）
	Ref<AudioStreamWAV> make_pop(); // 22050Hz 16-bit，纯代码无外部资源

protected:
	static void _bind_methods(); // 绑定 interact/get_prompt，便于脚本测试与扩展

public:
	void _notification(int p_what);
	void _process(double p_delta) override;

	virtual String get_prompt() const override;
	virtual Vector3 get_interact_pos() const override;
	virtual void interact(const Vector3 &p_player_pos) override;
};

} // namespace godot
