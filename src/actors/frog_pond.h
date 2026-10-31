#pragma once

// FrogPond —— 荷叶上的青蛙（交互式氛围小生物）。
//
// 3 只低模青蛙蹲在湖面固定的荷叶/荷花点位上：
//   * 平时偶尔"呱"一声（程序化合成），玩家靠近 4.5m 内会蹬腿跳进水里；
//   * 入水溅起一圈扩散的涟漪 + 落水声，随后躲 10 秒再回到原位；
//   * 全部程序化，无外部资源。
// 水面海拔 -0.55 与 terrain_builder.h water_level 联动。

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace godot {

class AudioStreamPlayer3D;
class AudioStreamWAV;
class MeshInstance3D;
class Material;

class FrogPond : public Node3D {
	GDCLASS(FrogPond, Node3D)

private:
	struct Frog {
		Node3D *root = nullptr;
		Vector3 home;
		bool hidden = false;
		bool jumping = false;
		float jump_t = 0.0f;
		float jump_dur = 0.55f;
		Vector3 jump_start;
		Vector3 jump_end;
		float respawn_t = 0.0f;
		float croak_phase = 0.0f;
	};

	std::vector<Frog> frogs;
	Vector2 lake_center = Vector2(-6.0f, 26.0f);
	float water_level = -0.55f;
	float anim_t = 0.0f;

	Node3D *player = nullptr;
	AudioStreamPlayer3D *voice = nullptr;
	Ref<AudioStreamWAV> snd_croak;
	Ref<AudioStreamWAV> snd_plop;

	MeshInstance3D *ring = nullptr;
	Ref<Material> ring_mat;
	float ring_t = 0.0f;
	float ring_dur = 0.6f;
	bool ring_active = false;

	float croak_timer = 3.0f;

	void build();
	void spawn_frog(int p_index, const Vector3 &p_home, unsigned p_seed);
	void jump(Frog &p_frog);
	void start_ring(const Vector3 &p_pos);
	Ref<AudioStreamWAV> make_croak();
	Ref<AudioStreamWAV> make_plop();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);
};

} // namespace godot
