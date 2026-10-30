#include "ice_surface.h"

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/world_boundary_shape3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "sky_cycle.h"

using namespace godot;

namespace {

// 站立高度：湖面海拔 water_level=-0.55（terrain_builder.h，改水位需同步此值）
// 上方留 1cm 余量，让玩家脚底恰好"踩在冰皮上"而不是陷进水皮。
constexpr float ICE_Y = -0.54f;
constexpr float FREEZE_ON = 0.85f;  // 雪量升到这里 → 冰面可行走（湖心冰面已大面积成形）
constexpr float FREEZE_OFF = 0.70f; // 回落到这里 → 解除（滞回带宽 0.15，防临界抖动）

} // namespace

void IceSurface::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ice_active"), &IceSurface::is_ice_active);
}

void IceSurface::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break; // 编辑器里不生成，保持场景干净
			}
			// 无限大水平面：一次建好、默认禁用，之后只在物理帧切 disabled——
			// 不增删物理体，天然满足"碰撞状态改动须在物理帧同步"的约束。
			Ref<WorldBoundaryShape3D> plane;
			plane.instantiate();
			plane->set_plane(Plane(Vector3(0.0f, 1.0f, 0.0f), 0.0f)); // 过本体原点，法线 +Y

			col = memnew(CollisionShape3D);
			col->set_name("IcePlane");
			col->set_shape(plane);
			col->set_disabled(true);
			add_child(col);

			// 摆平摆正：无视父节点（SkyCycle）可能带的变换，保证世界空间里
			// 是一张水平的冰面。
			Transform3D t;
			t.set_origin(Vector3(0.0f, ICE_Y, 0.0f));
			set_global_transform(t);

			add_to_group("ice_surface");
			set_physics_process(true);
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			if (col == nullptr) {
				return;
			}
			if (sky == nullptr) {
				// 沿祖先链向上找 SkyCycle（BerryBush 第 34 轮同款套路——
				// 挂在非 World 直接子级时 get_parent() 一把摸不到）。
				for (Node *n = get_parent(); n != nullptr; n = n->get_parent()) {
					if (SkyCycle *s = Object::cast_to<SkyCycle>(n)) {
						sky = s;
						break;
					}
				}
				if (sky == nullptr) {
					return; // 找不到天气源就保持静默禁用，不猜雪量
				}
			}
			float snow = sky->get_snow_amount();
			if (!active && snow >= FREEZE_ON) {
				set_active(true);
			} else if (active && snow <= FREEZE_OFF) {
				set_active(false);
			}
		} break;
		default:
			break;
	}
}

void IceSurface::set_active(bool p) {
	active = p;
	if (col != nullptr) {
		col->set_disabled(!p);
	}
}
