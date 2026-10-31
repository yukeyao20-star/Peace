#pragma once

// PhotoFrame —— 门廊墙上的木相框，显示相册里最新的一张截图。
//
// 拍照模式 F12 保存到 user://screenshots 后，相框会自动换成最新那张；
// 还没有照片时显示一张程序化的湖畔小画。

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class MeshInstance3D;
class StandardMaterial3D;

class PhotoFrame : public Node3D {
	GDCLASS(PhotoFrame, Node3D)

private:
	MeshInstance3D *canvas = nullptr;
	Ref<StandardMaterial3D> canvas_mat;

	void build();
	Ref<class Texture2D> load_latest();
	Ref<class Texture2D> make_placeholder();

protected:
	static void _bind_methods();

public:
	void _notification(int p_what);

	void refresh();
	bool get_has_photo() const;
	int get_texture_width() const;
};

} // namespace godot
