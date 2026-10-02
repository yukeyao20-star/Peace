#pragma once

#include <godot_cpp/classes/decal.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

// FootprintManager —— 实时地面积雪/泥土痕迹与脚印管理器
//
// 1. 基于 Godot 4 高性能 Decal 投影贴花池，贴合任意起伏缓坡、山石与桥梁，无穿模或悬空；
// 2. 独占 Cull Mask Layer 1（仅投射于场景地面），绝不污染角色或动物模型本体；
// 3. 内置 5 种精细纯代码抗锯齿程序化足迹（人鞋左/右、牛羊偶蹄、猫狗爪印、野兔蹦跳爪迹）；
// 4. 感知当前天气与地面：雪天为冷灰冰晶阴影凹陷，雨天为湿润深褐泥水印，晴天为干燥泥砂微印；
// 5. 环形缓冲池（180 槽位），超期（20~35s）平滑余弦渐隐消散，性能极高无卡顿。
class FootprintManager : public Node3D {
	GDCLASS(FootprintManager, Node3D)

public:
	enum FootprintType {
		TYPE_HUMAN_LEFT = 0,
		TYPE_HUMAN_RIGHT,
		TYPE_HOOF,
		TYPE_PAW,
		TYPE_RABBIT,
		TYPE_MAX
	};

private:
	static FootprintManager *singleton;
	static const int POOL_SIZE = 180;

	struct Slot {
		Decal *decal = nullptr;
		float age = 0.0f;
		float max_age = 28.0f;
		Color base_color;
		bool active = false;
	};

	Slot pool[POOL_SIZE];
	int next_slot = 0;

	Ref<ImageTexture> textures[TYPE_MAX];

	void build_textures();
	void build_pool();

protected:
	static void _bind_methods();

public:
	static FootprintManager *get_singleton() { return singleton; }

	FootprintManager();
	~FootprintManager();

	void _notification(int p_what);
	void _process(double delta) override;

	// 产出一个脚印/痕迹
	// type: 足印种类
	// pos: 落地世界坐标（贴近地面高度）
	// yaw_rad: 移动朝向（弧度，对齐足印尖端）
	// scale: 缩放比率（幼年/体型差异）
	// surface: 脚下地面类型（沿用 Player::_step_kind 的编码：
	//          0 荒草/泥土、1 雪地、2 雨天湿泥、3 石桥石板、4 木地板、5 球场草坪、
	//          6 泥土小径）；传 -1 = 调用方不指定，纯按当前天气决定（动物走这条路）。
	//          硬质地面踩不出泥印，只留浅灰擦痕——不区分的话石桥上会印一串泥脚印。
	void spawn_footprint(FootprintType type, const Vector3 &pos, float yaw_rad, float scale = 1.0f, int surface = -1);
};

} // namespace godot

VARIANT_ENUM_CAST(godot::FootprintManager::FootprintType);

