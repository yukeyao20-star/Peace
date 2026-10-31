#include "photo_frame.h"

#include <algorithm>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include "../systems/mesh_util.h"

using namespace godot;

namespace {
const char *SCREEN_DIR = "user://screenshots";
}

bool PhotoFrame::get_has_photo() const {
	if (!canvas_mat.is_valid()) {
		return false;
	}
	Ref<Texture2D> tex = canvas_mat->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
	return tex.is_valid() && tex->get_width() != 128; // 128 宽是占位画
}

int PhotoFrame::get_texture_width() const {
	if (!canvas_mat.is_valid()) {
		return 0;
	}
	Ref<Texture2D> tex = canvas_mat->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
	return tex.is_valid() ? tex->get_width() : 0;
}

void PhotoFrame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &PhotoFrame::refresh);
	ClassDB::bind_method(D_METHOD("get_has_photo"), &PhotoFrame::get_has_photo);
	ClassDB::bind_method(D_METHOD("get_texture_width"), &PhotoFrame::get_texture_width);
}

void PhotoFrame::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
		build();
		refresh();
	}
}

Ref<Texture2D> PhotoFrame::load_latest() {
	PackedStringArray names = DirAccess::get_files_at(SCREEN_DIR);
	String best;
	uint64_t best_t = 0;
	for (int i = 0; i < names.size(); i++) {
		const String n = names[i];
		if (!n.to_lower().ends_with(".png")) {
			continue;
		}
		String path = String(SCREEN_DIR) + String("/") + n;
		uint64_t mt = FileAccess::get_modified_time(path);
		if (best.is_empty() || mt > best_t || (mt == best_t && path > best)) {
			best = path;
			best_t = mt;
		}
	}
	if (best.is_empty()) {
		return Ref<Texture2D>();
	}
	Ref<Image> img;
	img.instantiate();
	if (img->load(best) != Error::OK) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(img);
}

Ref<Texture2D> PhotoFrame::make_placeholder() {
	Ref<Gradient> g;
	g.instantiate();
	PackedFloat32Array offsets;
	offsets.push_back(0.0f);
	offsets.push_back(0.55f);
	offsets.push_back(1.0f);
	PackedColorArray colors;
	colors.push_back(Color(0.66f, 0.84f, 0.95f));
	colors.push_back(Color(0.52f, 0.74f, 0.78f));
	colors.push_back(Color(0.24f, 0.46f, 0.34f));
	g->set_offsets(offsets);
	g->set_colors(colors);
	Ref<GradientTexture2D> tex;
	tex.instantiate();
	tex->set_gradient(g);
	tex->set_width(128);
	tex->set_height(96);
	return tex;
}

void PhotoFrame::refresh() {
	if (!canvas_mat.is_valid()) {
		return;
	}
	Ref<Texture2D> tex = load_latest();
	if (tex.is_null()) {
		tex = make_placeholder();
	}
	canvas_mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
}

void PhotoFrame::build() {
	if (get_parent() == nullptr) {
		return;
	}
	Node3D *host = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Homestead"));
	if (host == nullptr) {
		return;
	}
	Ref<Shader> toon = meshutil::load_toon_shader();
	const Color wood(0.38f, 0.25f, 0.13f);
	const Vector3 center(-1.35f, 2.45f, 3.55f);

	// 木框
	meshutil::add_box(host, toon, Vector3(0.92f, 0.06f, 0.06f), center + Vector3(0, 0.30f, 0), wood);
	meshutil::add_box(host, toon, Vector3(0.92f, 0.06f, 0.06f), center + Vector3(0, -0.30f, 0), wood);
	meshutil::add_box(host, toon, Vector3(0.06f, 0.66f, 0.06f), center + Vector3(-0.43f, 0, 0), wood);
	meshutil::add_box(host, toon, Vector3(0.06f, 0.66f, 0.06f), center + Vector3(0.43f, 0, 0), wood);

	// 画布
	Ref<BoxMesh> canvas_mesh;
	canvas_mesh.instantiate();
	canvas_mesh->set_size(Vector3(0.80f, 0.54f, 0.02f));
	canvas_mat.instantiate();
	canvas_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	canvas_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	canvas_mesh->set_material(Ref<Material>(canvas_mat));
	canvas = memnew(MeshInstance3D);
	canvas->set_name("Canvas");
	canvas->set_mesh(canvas_mesh);
	canvas->set_position(center + Vector3(0, 0, 0.06f));
	host->add_child(canvas);
}
