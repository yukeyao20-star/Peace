#pragma once

// Bookshelf —— 木屋后墙的书架。
//
// 三层旧木头书架，书脊颜色深浅不一、有的歪着靠在一起。按 E 抽一本读，
// 会显示一段关于湖畔小日子的短文，伴随纸页翻动声；书架边有细小的尘光飘着。
//
// 参考：
//   * godot-tutorial.net 逃脱教程 Part 3 —— 把书架/门/物品统一成
//     LOOKABLE / OPENABLE / PICKABLE 交互类型；
//   * mbiggeri/Aura_Interaction-Godot3D —— RayCast + Interactable 基类的
//     聚焦与检视交互；
//   * miskatonicstudio/interactive-book-godot —— 可翻阅书籍的结构参考。

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "../interact/interactable.h"

namespace godot {

class AudioStreamPlayer;
class AudioStreamWAV;
class CPUParticles3D;

class Bookshelf : public Interactable {
	GDCLASS(Bookshelf, Interactable)

static const int BOOK_COUNT = 27;

	CPUParticles3D *motes = nullptr;
	AudioStreamPlayer *page_voice = nullptr;
	Ref<AudioStreamWAV> snd_page;
	int read_count = 0;
	int last_index = -1;

	void build();
	void build_page_sound();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	String get_prompt() const override;
	Vector3 get_interact_pos() const override;
	float get_interact_distance() const override { return 2.4f; }
	bool requires_line_of_sight() const override { return false; }
	void interact(const Vector3 &p_player_pos) override;

	int get_book_count() const { return BOOK_COUNT; }
	int get_read_count() const { return read_count; }
	int get_last_index() const { return last_index; }
};

} // namespace godot
