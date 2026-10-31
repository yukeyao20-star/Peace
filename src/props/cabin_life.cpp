#include "cabin_life.h"

#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "../systems/mesh_util.h"
#include "../world/sky_cycle.h"

using namespace godot;

void CabinLife::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_window_glow_energy", "index"), &CabinLife::get_window_glow_energy);
	ClassDB::bind_method(D_METHOD("get_porch_energy"), &CabinLife::get_porch_energy);
	ClassDB::bind_method(D_METHOD("get_smoke_enabled"), &CabinLife::get_smoke_enabled);
}

float CabinLife::get_window_glow_energy(int p_index) const {
	if (p_index < 0 || p_index >= 4 || !window_glow[p_index].is_valid()) {
		return 0.0f;
	}
	return (float)window_glow[p_index]->get("emission_energy_multiplier");
}

float CabinLife::get_porch_energy() const {
	if (porch_light == nullptr) {
		return 0.0f;
	}
	return porch_light->get_param(Light3D::PARAM_ENERGY);
}

bool CabinLife::get_smoke_enabled() const {
	return smoke != nullptr && smoke->is_emitting();
}

void CabinLife::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			if (get_parent() != nullptr) {
				sky = Object::cast_to<SkyCycle>(get_parent()->get_node_or_null("SkyCycle"));
			}
			build();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			const float delta = (float)get_process_delta_time();
			t += delta;
			const float rain = (sky != nullptr) ? sky->get_rain_amount() : 0.0f;
			const float snow = (sky != nullptr) ? sky->get_snow_amount() : 0.0f;
			wet = Math::lerp(wet, Math::clamp(rain + snow * 0.7f, 0.0f, 1.0f), 1.0f - std::exp(-1.5f * delta));
			const float night = (sky != nullptr) ? sky->get_night_factor() : 0.0f;
			const float on = Math::clamp((night - 0.22f) / 0.40f, 0.0f, 1.0f);
			const float flicker = 0.94f + 0.04f * std::sin(t * 6.1f) + 0.02f * std::sin(t * 13.7f + 1.2f);
			for (int i = 0; i < 4; i++) {
				if (window_glow[i].is_valid()) {
					window_glow[i]->set("emission_energy_multiplier", (0.20f + 2.6f * on) * flicker * (1.0f - 0.25f * wet));
				}
			}
			if (window_light != nullptr) {
				window_light->set_param(Light3D::PARAM_ENERGY, 0.75f * on * flicker);
				window_light->set_visible(on > 0.02f);
			}
			if (porch_light != nullptr) {
				porch_light->set_param(Light3D::PARAM_ENERGY, 0.95f * on * flicker);
				porch_light->set_visible(on > 0.02f);
			}
			if (porch_glass.is_valid()) {
				porch_glass->set("emission_energy_multiplier", 0.25f + 1.8f * on);
			}
			if (smoke != nullptr) {
				// 风把炊烟慢慢往东南带；雨雪天压低一点
				smoke->set_gravity(Vector3(0.10f + 0.05f * std::sin(t * 0.21f), 0.10f, 0.04f + 0.03f * std::sin(t * 0.13f + 1.0f)));
				smoke->set_emitting(wet < 0.55f);
			}
		} break;
		default:
			break;
	}
}

void CabinLife::build() {
	if (get_parent() == nullptr) {
		return;
	}
	Node3D *host = Object::cast_to<Node3D>(get_parent()->get_node_or_null("Homestead"));
	if (host == nullptr) {
		return;
	}
	Ref<Shader> toon = meshutil::load_toon_shader();

	// —— 屋顶石烟囱 ——
	const Vector3 chimney_base(1.7f, 3.55f, -1.2f);
	meshutil::add_box(host, toon, Vector3(0.72f, 1.55f, 0.72f), chimney_base, Color(0.40f, 0.39f, 0.37f));
	meshutil::add_box(host, toon, Vector3(0.84f, 0.16f, 0.84f), chimney_base + Vector3(0, 0.80f, 0), Color(0.30f, 0.29f, 0.28f));

	// 青烟：一团团慢慢升上去、散开
	Ref<SphereMesh> puff;
	puff.instantiate();
	puff->set_radius(0.16f);
	puff->set_height(0.32f);
	puff->set_radial_segments(8);
	puff->set_rings(5);
	Ref<StandardMaterial3D> smoke_mat;
	smoke_mat.instantiate();
	smoke_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	smoke_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	smoke_mat->set_albedo(Color(0.62f, 0.65f, 0.64f, 0.10f));
	smoke_mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	puff->set_material(smoke_mat);
	smoke = memnew(CPUParticles3D);
	smoke->set_name("ChimneySmoke");
	smoke->set_amount(22);
	smoke->set_lifetime(5.5f);
	smoke->set_mesh(puff);
	smoke->set_direction(Vector3(0, 1, 0));
	smoke->set_spread(10.0f);
	smoke->set_gravity(Vector3(0.10f, 0.10f, 0.04f));
	smoke->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.28f);
	smoke->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.55f);
	smoke->set_param_min(CPUParticles3D::PARAM_SCALE, 0.7f);
	smoke->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
	smoke->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	smoke->set_emission_sphere_radius(0.12f);
	smoke->set_position(chimney_base + Vector3(0, 0.95f, 0));
	smoke->set_emitting(true);
	host->add_child(smoke);

	// —— 四扇窗的暖光 ——
	const Vector3 panes[4] = {
		Vector3(-2.4f, 2.0f, 3.60f),   // 前左
		Vector3(2.6f, 2.0f, 3.60f),    // 前右
		Vector3(-4.20f, 2.0f, 0.3f),   // 左侧（贴侧面墙）
		Vector3(1.6f, 2.0f, -3.62f),   // 后墙
	};
	const Vector3 pane_size[4] = {
		Vector3(1.05f, 0.92f, 0.03f),
		Vector3(1.05f, 0.92f, 0.03f),
		Vector3(0.03f, 0.92f, 1.05f),
		Vector3(1.05f, 0.92f, 0.03f),
	};
	for (int i = 0; i < 4; i++) {
		Ref<BoxMesh> pane;
		pane.instantiate();
		pane->set_size(pane_size[i]);
		window_glow[i].instantiate();
		window_glow[i]->set_albedo(Color(1.0f, 0.80f, 0.48f));
		window_glow[i]->set("emission_enabled", true);
		window_glow[i]->set("emission", Color(1.0f, 0.67f, 0.30f));
		window_glow[i]->set("emission_energy_multiplier", 0.20f);
		pane->set_material(Ref<Material>(window_glow[i]));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name(String("WindowGlow") + String::num_int64(i));
		mi->set_mesh(pane);
		mi->set_position(panes[i]);
		host->add_child(mi);
	}

	// 前窗十字窗棂：把整块暖光分成四格，更像一扇真的窗
	for (int i = 0; i < 2; i++) {
		meshutil::add_box(host, toon, Vector3(1.10f, 0.045f, 0.025f), panes[i] + Vector3(0.0f, 0.0f, 0.02f), Color(0.30f, 0.19f, 0.10f));
		meshutil::add_box(host, toon, Vector3(0.045f, 0.96f, 0.025f), panes[i] + Vector3(0.0f, 0.0f, 0.02f), Color(0.30f, 0.19f, 0.10f));
	}

	window_light = memnew(OmniLight3D);
	window_light->set_name("WindowLight");
	window_light->set_position(Vector3(0.0f, 2.1f, 4.3f));
	window_light->set_color(Color(1.0f, 0.68f, 0.34f));
	window_light->set_param(Light3D::PARAM_RANGE, 7.5f);
	window_light->set_param(Light3D::PARAM_ENERGY, 0.0f);
	window_light->set_visible(false);
	host->add_child(window_light);

	// —— 门口小灯 ——
	const Color bark(0.34f, 0.23f, 0.13f);
	const Vector3 post(1.55f, 0.0f, 3.95f);
	meshutil::add_box(host, toon, Vector3(0.09f, 1.55f, 0.09f), post + Vector3(0, 0.77f, 0), bark);
	meshutil::add_box(host, toon, Vector3(0.36f, 0.05f, 0.05f), post + Vector3(-0.13f, 1.50f, 0), bark);
	meshutil::add_box(host, toon, Vector3(0.20f, 0.04f, 0.20f), post + Vector3(-0.28f, 1.53f, 0), bark);
	meshutil::add_box(host, toon, Vector3(0.20f, 0.04f, 0.20f), post + Vector3(-0.28f, 1.26f, 0), bark);

	Ref<BoxMesh> pg;
	pg.instantiate();
	pg->set_size(Vector3(0.16f, 0.22f, 0.16f));
	porch_glass.instantiate();
	porch_glass->set_albedo(Color(1.0f, 0.80f, 0.48f));
	porch_glass->set("emission_enabled", true);
	porch_glass->set("emission", Color(1.0f, 0.68f, 0.32f));
	porch_glass->set("emission_energy_multiplier", 0.25f);
	pg->set_material(Ref<Material>(porch_glass));
	MeshInstance3D *pmi = memnew(MeshInstance3D);
	pmi->set_name("PorchGlass");
	pmi->set_mesh(pg);
	pmi->set_position(post + Vector3(-0.28f, 1.40f, 0));
	host->add_child(pmi);

	porch_light = memnew(OmniLight3D);
	porch_light->set_name("PorchLight");
	porch_light->set_position(post + Vector3(-0.30f, 1.37f, 0.16f));
	porch_light->set_color(Color(1.0f, 0.70f, 0.36f));
	porch_light->set_param(Light3D::PARAM_RANGE, 6.0f);
	porch_light->set_param(Light3D::PARAM_ENERGY, 0.0f);
	porch_light->set_visible(false);
	host->add_child(porch_light);
}
