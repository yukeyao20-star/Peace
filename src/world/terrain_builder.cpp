// TerrainBuilder 实现 —— 见 terrain_builder.h 顶部说明。
//
// 关键思路：
//   * 高度场 terrain_height() 是"唯一事实来源"：地形网格用它取高度，
//     所有植被也用它贴地，并用 terrain_slope() 拒绝长在陡崖/水里的候选点。
//   * 植被用 MultiMesh：几千个实例只占十来个 draw call，草再多也不心疼。
//   * 随机数用固定种子 std::mt19937(1337)，地图可复现。

#include "terrain_builder.h"

#include "player.h"
#include "sky_cycle.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/particle_process_material.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// 程序化草丛纹理：64×128 RGBA——一片 quad 画一丛细草
// （根部分散、各自微弯不同高度/明度、中脉折痕）→ 视觉草量 ×N 而不加实例。
// p_roots：每丛画几根。6 = 野草默认；12 = 球场羊毛级细密（根数翻倍叶片自动变细）。
Ref<ImageTexture> make_blade_texture(int p_roots = 6) {
	const int W = 64, H = 128;
	Ref<Image> img = Image::create_empty(W, H, false, Image::FORMAT_RGBA8);
	img->fill(Color(0.0f, 0.0f, 0.0f, 0.0f));
	struct B { float cx0, lean, h, hw, bright; };
	std::vector<B> blades;
	const float hw0 = 0.30f / (float)p_roots; // 根数翻倍 → 叶片减半，根部覆盖面积守恒
	for (int i = 0; i < p_roots; i++) {
		float t = (p_roots > 1) ? (float)i / (float)(p_roots - 1) : 0.5f; // 0..1
		float j = std::fmod(std::sin((float)i * 7.13f) * 0.5f + 0.5f, 1.0f); // 确定性抖动 0..1
		B b;
		b.cx0 = lerpf(0.08f, 0.92f, t) + (j - 0.5f) * 0.06f;
		b.lean = (i % 2 == 0 ? 1.0f : -1.0f) * (0.14f + j * 0.12f);
		b.h = lerpf(0.80f, 1.00f, j);
		b.hw = hw0 * (0.85f + j * 0.30f);
		b.bright = 0.86f + j * 0.24f;
		blades.push_back(b);
	}
	for (int y = 0; y < H; y++) {
		float v = 1.0f - (float)y / (float)(H - 1); // 0 根 → 1 梢
		for (int x = 0; x < W; x++) {
			float u = (float)x / (float)(W - 1);
			float best = -1.0f;
			for (const B &b : blades) {
				if (v > b.h) {
					continue;
				}
				float hv = v / b.h;
				float cx = b.cx0 + b.lean * hv * hv;
				float hw = b.hw * (1.0f - hv) + 0.004f;
				float d = Math::abs(u - cx);
				if (d < hw) {
					float midrib = 1.0f - (1.0f - Math::clamp(d / (hw * 0.35f), 0.0f, 1.0f)) * 0.16f;
					float bright = (0.72f + hv * 0.36f) * b.bright * midrib;
					best = Math::max(best, bright);
				}
			}
			if (best >= 0.0f) {
				img->set_pixel(x, y, Color(best, best, best, 1.0f));
			}
		}
	}
	img->generate_mipmaps(); // 关键：远处自动糊成柔软草绒（否则 2px 笔画走样成"文字墙"）
	return ImageTexture::create_from_image(img);
}

// 一批实例的收集器：变换 + 每实例颜色（材质本身用白色，颜色全部来自实例）
struct Scatter {
	std::vector<Transform3D> xforms;
	std::vector<Color> colors;

	void add(const Vector3 &p_pos, float p_yaw, float p_tilt, float p_scale, const Color &p_color) {
		Transform3D t;
		Basis b;
		b.set_euler_scale(Vector3(p_tilt, p_yaw, p_tilt), Vector3(p_scale, p_scale, p_scale));
		t.set_basis(b);
		t.set_origin(p_pos);
		xforms.push_back(t);
		colors.push_back(p_color);
	}
	int count() const { return (int)xforms.size(); }
};

// 把收集好的实例打包成一个 MultiMeshInstance3D 挂到父节点下
void commit_scatter(Node3D *p_parent, const char *p_name, Ref<Mesh> p_mesh, const Scatter &p_s) {
	if (p_s.count() == 0) {
		return;
	}
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_use_colors(true);
	mm->set_mesh(p_mesh);
	mm->set_instance_count(p_s.count());
	for (int i = 0; i < p_s.count(); i++) {
		mm->set_instance_transform(i, p_s.xforms[i]);
		mm->set_instance_color(i, p_s.colors[i]);
	}
	MultiMeshInstance3D *mi = memnew(MultiMeshInstance3D);
	mi->set_name(p_name);
	mi->set_multimesh(mm);
	p_parent->add_child(mi);
}

// 拍岸浪参数：必须与 water.gdshader 的 shore_wave_speed / shore_wave_freq / swell_freq
// 默认值一致（水面着色器用它们算浪脊相位，浪花粒子要跟它同相）。改一处就要改两处。
constexpr float REEF_WAVE_SPEED = 2.8f; // == water.gdshader shore_wave_speed
constexpr float REEF_WAVE_FREQ = 7.5f;  // == water.gdshader shore_wave_freq
constexpr float SWELL_FREQ = 1.7f;      // == water.gdshader swell_freq（浪脊向石头收拢的相位频率）

// 程序化岩石网格：球面按 3D 噪声做径向位移（不规则的棱面轮廓），
// 再生成逐面法线（flat shading）—— 低模岩石硬朗的切面感就来自这里。
// 光滑球体当石头永远是"蛋"：棱面 + 噪声才是石头的关键。
//   p_radius：基础半径（与 SphereMesh::set_radius 同义，方便沿用原有缩放参数）
//   p_squash：压扁 Y（<1 更扁，像卧石/被水磨过的顽石；竖向半高 = p_radius·p_squash）
//   p_amp   ：位移幅度，0.15~0.35 比较自然（再大就成麻花了）
//   p_freq  ：噪声频率，越大碎面越密
Ref<ArrayMesh> make_rock_mesh(int p_seg, int p_rings, float p_radius, float p_squash,
		float p_amp, float p_freq, int p_seed) {
	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	noise->set_seed(p_seed);
	noise->set_frequency(p_freq);
	noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	noise->set_fractal_octaves(3);
	noise->set_fractal_gain(0.5f);
	noise->set_fractal_lacunarity(2.1f);

	// 经纬球顶点：j = 0 顶极点 → j = p_rings 底极点
	std::vector<std::vector<Vector3>> grid(p_rings + 1);
	for (int j = 0; j <= p_rings; j++) {
		float phi = (float)j / (float)p_rings * Math::PI;
		float sy = Math::cos(phi);
		float sr = Math::sin(phi);
		grid[j].resize(p_seg + 1);
		for (int i = 0; i <= p_seg; i++) {
			float th = (float)i / (float)p_seg * Math::TAU;
			Vector3 dir(sr * Math::cos(th), sy, sr * Math::sin(th));
			float n = noise->get_noise_3d(dir.x, dir.y, dir.z);
			Vector3 p = dir * (p_radius * (1.0f + p_amp * n));
			p.y *= p_squash;
			grid[j][i] = p;
		}
	}

	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);
	st->set_smooth_group(-1); // 逐面法线 → flat shading（石头要的就是硬切面）
	for (int j = 0; j < p_rings; j++) {
		for (int i = 0; i < p_seg; i++) {
			const Vector3 &a = grid[j][i];
			const Vector3 &b = grid[j][i + 1];
			const Vector3 &c = grid[j + 1][i + 1];
			const Vector3 &d = grid[j + 1][i];
			if (j >= 1) { // 顶极点处 a==b，退化三角形跳过
				st->add_vertex(a);
				st->add_vertex(b);
				st->add_vertex(c);
			}
			if (j <= p_rings - 2) { // 底极点处 c==d
				st->add_vertex(a);
				st->add_vertex(c);
				st->add_vertex(d);
			}
		}
	}
	st->generate_normals();
	return st->commit();
}

// 叶团网格：球面按低频噪声径向位移 —— 蓬松、不规则的叶簇体积。
// 和石头用的是同一套思路，区别在两点：
//   1) 平滑法线（叶簇是软团，不是石头那种硬切面）；
//   2) 位移幅度小、频率低（叶团轮廓要"鼓"，不能碎成麻花）。
// 一颗树冠由 7 个这样的小团错落叠出来 —— 单球当树冠就是"一坨"的根源。
//   p_radius：基础半径（实例缩放直接乘这个，所以取 1.0 最好算）
//   p_squash：压扁 Y（叶团略扁，像被自身重量压下来的冠形）
Ref<ArrayMesh> make_leaf_blob_mesh(float p_radius, float p_squash, float p_amp,
		float p_freq, int p_seed, int p_seg, int p_rings) {
	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	noise->set_seed(p_seed);
	noise->set_frequency(p_freq);
	noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	// 3 层 FBM：低频定叶团的鼓包轮廓，中频给表面加起伏。
	// 为什么是 3 层而不是 4 层：4 层时最高一层频率到 1.30×2.5³≈20，波长只有 0.3，
	// 而网格顶点间距约 0.33 —— 噪声比顶点还密，等于给每个顶点随机抖动，
	// 表面被抖成一片硬邦邦的碎面（看着像绿色碎石堆，而不是叶子）。
	// 降到 3 层、lacunarity 2.0，最高频 5.2（波长 1.2，约 3.6 个顶点/波）才采得动。
	noise->set_fractal_octaves(3);
	noise->set_fractal_gain(0.52f);
	noise->set_fractal_lacunarity(2.0f);

	// 叶面明暗：再采一层更高频的噪声当顶点色（乘在实例色上）。
	// 树冠从此不是一块平涂的绿 —— 同一团叶子里有的在光里、有的在阴影里，
	// 这是"看起来像叶子"最便宜也最有效的一招（不用任何贴图）。
	Ref<FastNoiseLite> tint;
	tint.instantiate();
	tint->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	tint->set_seed(p_seed + 977);
	tint->set_frequency(p_freq * 2.8f);
	tint->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	// 2 层 + lacunarity 2.0。叶色的高频"噪"不是缺点 —— 插值后就是一层细碎的
	// 叶面明暗斑驳，正是"叶簇"该有的质感；反倒是大块平涂看着像塑料。
	tint->set_fractal_octaves(2);
	tint->set_fractal_lacunarity(2.0f);

	std::vector<std::vector<Vector3>> grid(p_rings + 1);
	std::vector<std::vector<Vector3>> leafc(p_rings + 1); // 顶点色：叶片明暗 + 色相偏移
	for (int j = 0; j <= p_rings; j++) {
		float phi = (float)j / (float)p_rings * Math::PI;
		float sy = Math::cos(phi);
		float sr = Math::sin(phi);
		grid[j].resize(p_seg + 1);
		leafc[j].resize(p_seg + 1);
		for (int i = 0; i <= p_seg; i++) {
			float th = (float)i / (float)p_seg * Math::TAU;
			Vector3 dir(sr * Math::cos(th), sy, sr * Math::sin(th));
			// 乘 1.6 把噪声坐标推离原点：球心附近噪声梯度小，直接采样会让
			// 相邻叶团长得几乎一样（同一棵树像复制粘贴）
			float n = noise->get_noise_3d(dir.x * 1.6f, dir.y * 1.6f, dir.z * 1.6f);
			Vector3 p = dir * (p_radius * (1.0f + p_amp * n));
			p.y *= p_squash;
			grid[j][i] = p;
			float d = tint->get_noise_3d(dir.x * 2.4f, dir.y * 2.4f, dir.z * 2.4f);
			// 叶片不是"同一块绿打上灰度明暗"——真实树叶暗部偏冷（深青绿）、亮部偏暖（黄绿）。
			// 所以顶点色给的是 RGB 而不是灰度：红通道拉得最开，绿次之，蓝在暗部反而抬高。
			// 这层色相分离乘在实例色（树本身的绿）上，才有"叶子有深有浅"的层次，
			// 否则不管怎么调明暗，看着都还是同一块绿布。
			float t = clampf(0.5f + 0.78f * d, 0.0f, 1.0f);
			// 暗端抬到 0.86：暗部是要"暗绿"，不是"发黑"。
			// 蓝端 0.70~0.74：这一层是"别往墨绿走"的最后一道闸 —— 树冠大面积
			// 只吃蓝色天光，蓝通道给多少就原样冒出来多少，这里必须压住。
			// 亮端红 1.14（原来是 1.22）：红给太高亮部会往黄/橄榄偏，
			// 出来的不是"亮绿"而是"灰橄榄"，反而更不像草木。
			leafc[j][i] = Vector3(0.86f, 1.01f, 0.70f).lerp(Vector3(1.14f, 1.16f, 0.74f), t);
		}
	}

	// 法线自己算，不走 SurfaceTool::generate_normals()。
	// 原因：那条路要靠"同位置顶点合并"来平滑，叶团每个顶点都带不同的顶点色，
	// 合并条件很脆弱 —— 实测出来是一张张硬平面（树冠像绿色碎石堆）。
	// 这里直接对参数曲面求切向量再叉乘，逐顶点法线天然连续，跟顶点色无关。
	std::vector<std::vector<Vector3>> nrm(p_rings + 1);
	for (int j = 0; j <= p_rings; j++) {
		nrm[j].resize(p_seg + 1);
		for (int i = 0; i <= p_seg; i++) {
			int jp = j > 0 ? j - 1 : 0;
			int jn = j < p_rings ? j + 1 : p_rings;
			// 纬向环绕：第 p_seg 列是第 0 列的副本，所以 i=0 的"前一列"是 p_seg-1，
			// i=p_seg 的"后一列"是 1（不是 0，否则差分为 0）
			int im = (i == 0) ? (p_seg - 1) : (i - 1);
			int ip = (i == p_seg) ? 1 : (i + 1);
			Vector3 du = grid[jn][i] - grid[jp][i];   // 沿经线
			Vector3 dv = grid[j][ip] - grid[j][im];   // 沿纬线
			Vector3 n = dv.cross(du);
			if (n.length_squared() < 1e-12f) {
				// 极点 / 退化处：退回径向（球面外法线）
				Vector3 rad = grid[j][i];
				n = rad.length_squared() > 1e-9f ? rad : Vector3(0.0f, 1.0f, 0.0f);
			}
			nrm[j][i] = n.normalized();
		}
	}

	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);
	for (int j = 0; j < p_rings; j++) {
		for (int i = 0; i < p_seg; i++) {
			const Vector3 &a = grid[j][i];
			const Vector3 &b = grid[j][i + 1];
			const Vector3 &c = grid[j + 1][i + 1];
			const Vector3 &d = grid[j + 1][i];
			if (j >= 1) {
				st->set_color(Color(leafc[j][i].x, leafc[j][i].y, leafc[j][i].z));
				st->set_normal(nrm[j][i]);
				st->add_vertex(a);
				st->set_color(Color(leafc[j][i + 1].x, leafc[j][i + 1].y, leafc[j][i + 1].z));
				st->set_normal(nrm[j][i + 1]);
				st->add_vertex(b);
				st->set_color(Color(leafc[j + 1][i + 1].x, leafc[j + 1][i + 1].y, leafc[j + 1][i + 1].z));
				st->set_normal(nrm[j + 1][i + 1]);
				st->add_vertex(c);
			}
			if (j <= p_rings - 2) {
				st->set_color(Color(leafc[j][i].x, leafc[j][i].y, leafc[j][i].z));
				st->set_normal(nrm[j][i]);
				st->add_vertex(a);
				st->set_color(Color(leafc[j + 1][i + 1].x, leafc[j + 1][i + 1].y, leafc[j + 1][i + 1].z));
				st->set_normal(nrm[j + 1][i + 1]);
				st->add_vertex(c);
				st->set_color(Color(leafc[j + 1][i].x, leafc[j + 1][i].y, leafc[j + 1][i].z));
				st->set_normal(nrm[j + 1][i]);
				st->add_vertex(d);
			}
		}
	}
	return st->commit();
}

// 浪花强度预设：粒子数 / 初速倍率 / 粒径倍率 / 淡出位置（占生命期比例）/ 扩散角 / 阻尼。
// 岸边小石头用小档（水花收敛），深水巨礁用大档（参考实拍那种炸起来的水柱）。
struct SplashStyle {
	int amount;
	float vel;
	float size;
	float fade;
	float spread;
	float damp; // 空气阻力：小 → 飞得高、散得开（大浪）；大 → 短促收口（岸边小水花）
};

// 水花软圆点贴图（程序生成）：中心实、边缘柔和衰减的白点。
// 拿它做粒子的 billboard 面片，比实心小球更像飞溅的水滴/水雾。
Ref<Texture2D> make_splash_dot() {
	const int S = 32;
	Ref<Image> img = Image::create_empty(S, S, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			float dx = ((float)x + 0.5f) / (float)S * 2.0f - 1.0f;
			float dy = ((float)y + 0.5f) / (float)S * 2.0f - 1.0f;
			float d = Math::sqrt(dx * dx + dy * dy);
			// 软到没有硬核：单颗像一粒水珠，几十颗叠起来才成"白雾"。
			// 峰值只给 0.9 —— 留一点余量给重叠累积，密处自然糊成一片白。
			float a = clampf(1.0f - d, 0.0f, 1.0f);
			a = Math::pow(a, 1.7f) * 0.9f;
			img->set_pixel(x, y, Color(1.0f, 1.0f, 1.0f, a));
		}
	}
	return ImageTexture::create_from_image(img);
}

// 礁石激浪水花：一撮从石头处向上炸开、受重力回落的水滴粒子（真实几何，不是贴片）。
// 与水面拍岸浪同周期（周期 = 2π / 浪速），并用 pre_process_time 把发射时刻推到浪峰 ——
// 所以水花和石头根部那圈白沫是同一道浪打出来的，不会各响各的。
// p_depth 是石头处的水深：水越深，浪到得越晚（相位推迟）。
GPUParticles3D *make_reef_splash(const Vector3 &p_pos, float p_radius, float p_depth,
		const Ref<Texture2D> &p_dot, const Ref<Mesh> &p_quad, const SplashStyle &p_style) {
	GPUParticles3D *p = memnew(GPUParticles3D);
	p->set_position(p_pos);
	p->set_amount(p_style.amount);
	const float period = Math::TAU / REEF_WAVE_SPEED;
	p->set_lifetime(period); // 生命周期 = 浪周期 → 每道浪喷一次
	p->set_one_shot(false);
	p->set_explosiveness_ratio(1.0f); // 一次性喷完，不是细水长流
	p->set_randomness_ratio(0.06f);   // 一点点错开，别像机关枪
	p->set_draw_pass_mesh(0, p_quad);
	// 包围盒按浪花高度给足，避免被视锥剔除掉
	float reach = 3.0f + 3.0f * p_style.vel;
	p->set_visibility_aabb(AABB(Vector3(-reach, -4.0f, -reach),
			Vector3(reach * 2.0f, 8.0f + 2.0f * p_style.vel, reach * 2.0f)));

	Ref<ParticleProcessMaterial> pm;
	pm.instantiate();
	pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_SPHERE);
	// 发射球半径取石头半径的一半：水花是从石头根部那一圈炸开的，
	// 球给太大粒子会从石头肚子里面冒出来。
	pm->set_emission_sphere_radius(Math::max(p_radius * 0.55f, 0.12f));
	pm->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	pm->set_spread(p_style.spread);
	pm->set_gravity(Vector3(0.0f, -9.5f, 0.0f));
	pm->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 2.2f * p_style.vel);
	pm->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 4.8f * p_style.vel);
	pm->set_param_min(ParticleProcessMaterial::PARAM_DAMPING, p_style.damp * 0.75f);
	pm->set_param_max(ParticleProcessMaterial::PARAM_DAMPING, p_style.damp * 1.6f);
	pm->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.05f * p_style.size);
	pm->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 0.15f * p_style.size);

	// 生命期透明度：一出现就亮，随后淡出（水花落下即散，不留"白球飘着"）。
	// p_style.fade 决定"完全消失"落在生命期的哪个位置 —— 大浪花飞得高、要看完整条弧线，
	// 淡出点就往后推；岸边小水花短促，淡出点靠前。
	Ref<Gradient> ramp;
	ramp.instantiate();
	float f = p_style.fade;
	ramp->add_point(0.00f, Color(1.0f, 1.0f, 1.0f, 0.0f));
	ramp->add_point(f * 0.07f, Color(1.0f, 1.0f, 1.0f, 0.95f));
	ramp->add_point(f * 0.34f, Color(1.0f, 1.0f, 1.0f, 0.74f));
	ramp->add_point(f * 0.67f, Color(1.0f, 1.0f, 1.0f, 0.32f));
	ramp->add_point(f, Color(1.0f, 1.0f, 1.0f, 0.0f));
	ramp->add_point(1.00f, Color(1.0f, 1.0f, 1.0f, 0.0f));
	Ref<GradientTexture1D> gt;
	gt.instantiate();
	gt->set_gradient(ramp);
	pm->set_color_ramp(gt);
	p->set_process_material(pm);

	// 相位对齐：涌浪的浪脊推到石头时才喷水花。
	// 水面着色器里浪脊相位 = TIME·speed + d·swell_freq（d = 到石头的距离），
	// 石头表面处 d = p_radius → 浪脊到达石头的时刻 = (π/2 - p_radius·swell_freq) / speed。
	// pre_process_time 把模拟时间提前 (周期 - 相位)，发射时刻就正好落在浪脊撞石头上。
	// 注：p_depth 不再参与相位（浪脊是按到石头的距离推进的，不是按水深），
	// 保留在签名里是因为调用方按水深做了别的筛选，将来也可能再用来调参。
	(void)p_depth;
	double raw_delay = ((double)Math::PI * 0.5 - (double)p_radius * (double)SWELL_FREQ)
			/ (double)REEF_WAVE_SPEED;
	float delay = (float)std::fmod(raw_delay, (double)period);
	if (delay < 0.0f) {
		delay += period;
	}
	p->set_pre_process_time(period - delay);
	return p;
}

// 水花粒子的面片：软圆点 + 始终面向相机的 billboard（所有石头共用一份）
Ref<Mesh> make_splash_quad(const Ref<Texture2D> &p_dot) {
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(1.0f, 1.0f));
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED); // 水花是自亮的白，不吃光照
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	m->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, p_dot);
	m->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true); // 让粒子渐隐生效
	m->set_flag(BaseMaterial3D::FLAG_DONT_RECEIVE_SHADOWS, true);
	quad->set_material(m);
	return quad;
}

// 植被通用材质：白色底色 + 顶点色（实例色）决定实际颜色；果子/花可带一点点自发光
Ref<StandardMaterial3D> veg_material(const Color &p_emission = Color()) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(Color(1.0f, 1.0f, 1.0f));
	m->set_roughness(1.0f);
	m->set_metallic(0.0f);
	m->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	if (p_emission.r > 0.0f || p_emission.g > 0.0f || p_emission.b > 0.0f) {
		m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		m->set_emission(p_emission);
	}
	return m;
}

// 岩石材质：rock.gdshader（世界坐标噪声斑驳 + 水线以下湿石压暗）。
// 加载失败退回 veg_material（纯实例色），不至于石头消失。
// 注意：必须定义在 veg_material 之后（失败路径要用它）。
Ref<Material> rock_material() {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/rock.gdshader");
	Ref<Shader> shader = Ref<Shader>(Object::cast_to<Shader>(res.ptr()));
	if (shader.is_null()) {
		return veg_material();
	}
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	m->set_shader_parameter("water_line", -0.55f); // 与 water_level 一致
	return m;
}

// 风摆材质：加载 veg_wind.gdshader，按网格尺寸给摆动参数（根部不动、梢部摆）
// 注意：不要用 static Ref 缓存 Shader——静态析构发生在引擎关闭之后，会段错误+资源泄漏；
// ResourceLoader 自带缓存，每次加载的开销可忽略。
// 返回 Ref<ShaderMaterial> 便于调用方登记进 snow_bands（积雪分级驱动）。
Ref<ShaderMaterial> wind_material(float p_base_y, float p_span, float p_strength) {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/veg_wind.gdshader");
	Ref<Shader> shader = Ref<Shader>(Object::cast_to<Shader>(res.ptr()));
	if (shader.is_null()) {
		return Ref<ShaderMaterial>(); // 兜底：调用方拿到空引用时退回静态材质
	}
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	m->set_shader_parameter("sway_base_y", p_base_y);
	m->set_shader_parameter("sway_span", p_span);
	m->set_shader_parameter("sway_strength", p_strength);
	return m;
}

// 风摆材质的静态兜底（wind_material 加载失败时用）
Ref<Material> fallback_wind_material() {
	return veg_material();
}

// 随波材质（荷叶 / 荷花共用）：加载 lily.gdshader，顶点按世界坐标采水面波高。
// p_bob_extra：比水面多晃多少 —— 荷叶贴水、荷花轻，都比水面晃得明显一点。
// p_emission：自发光（荷花花瓣要；荷叶留 0）。
// 返回空引用表示着色器没加载上，调用方自行退回静态材质。
Ref<ShaderMaterial> water_bob_material(float p_bob_extra, const Vector3 &p_emission) {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/lily.gdshader");
	Ref<Shader> shader = Ref<Shader>(Object::cast_to<Shader>(res.ptr()));
	if (shader.is_null()) {
		return Ref<ShaderMaterial>();
	}
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	m->set_shader_parameter("bob_extra", p_bob_extra);
	m->set_shader_parameter("emission", p_emission);
	return m;
}

// 往 SurfaceTool 里塞一段锥形圆管（从 a 到 b，起端半径 ra、末端 rb，seg 边）。
// 为什么自己拼管而不用 CylinderMesh：树枝要指向任意方向，而 CylinderMesh 只能沿本地 Y；
// 而且主干和树枝必须落在同一个网格里，才能共用同一份 MultiMesh 实例变换（一棵树一次 draw）。
static void add_tube(SurfaceTool *st, const Vector3 &a, const Vector3 &b,
		float ra, float rb, int seg, const Color &col) {
	Vector3 axis = b - a;
	float len = axis.length();
	if (len < 0.0001f) {
		return;
	}
	axis /= len;
	// 与轴垂直的正交基；ref 按轴方向挑，避免叉乘退化
	Vector3 ref = Math::abs(axis.y) < 0.95f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
	Vector3 u = axis.cross(ref).normalized();
	Vector3 v = axis.cross(u).normalized();
	for (int i = 0; i < seg; i++) {
		float t0 = (float)i / (float)seg * Math::TAU;
		float t1 = (float)(i + 1) / (float)seg * Math::TAU;
		Vector3 d0 = u * Math::cos(t0) + v * Math::sin(t0);
		Vector3 d1 = u * Math::cos(t1) + v * Math::sin(t1);
		Vector3 a0 = a + d0 * ra, a1 = a + d1 * ra;
		Vector3 b0 = b + d0 * rb, b1 = b + d1 * rb;
		// 绕序朝外：u×v = axis（因为 v = axis×u），所以按 (a0,b1,b0) / (a0,a1,b1)
		// 展开时法线才指向径向外侧；反过来会得到一圈朝内的面，正面剔除后整根管子消失。
		st->set_color(col);
		st->add_vertex(a0);
		st->set_color(col);
		st->add_vertex(b1);
		st->set_color(col);
		st->add_vertex(b0);
		st->set_color(col);
		st->add_vertex(a0);
		st->set_color(col);
		st->add_vertex(a1);
		st->set_color(col);
		st->add_vertex(b1);
	}
}

// —— 树原型（archetype）——
// 起因：如果所有树共用一份骨架网格 + 一套固定叶团坐标，整片林子就是同一棵树复制几百遍，
// 远看是一排排一模一样的绿蘑菇（用户一眼就指出来了）。
// 所以建图时先预生成 K 套"树形"：每套自带自己的树冠形状（宽窄/高低/叶团个数）
// 和**与之配套**的枝条布局。种树时随机挑一套，再叠上各自的 yaw / 缩放 / 叶色。
//
// 为什么枝条必须和叶团同源生成：上一版枝条是写死的固定表、叶团是另一张固定表，
// 结果枝梢经常戳到树冠外面，变成一根光秃秃的树枝挂在天上，像 bug。
// 现在每根枝条的梢部坐标就是对应叶团的中心坐标 —— 枝梢永远埋在叶团里。
//
// 关键比例：主干到 y≈2.3 细成"主梢"钻进树冠，侧枝在 y≈1.1~1.5 从主干分叉出去，
// 而树冠底面抬到 y≈1.7 —— 中间那半米枝条是露在外面的。树干分叉看得见，
// 树才不是"一根棍顶着一坨绿"。
struct TreeBlob {
	float ang;   // 方位角
	float rad;   // 离主干轴的水平距离
	float y;     // 离地高度
	float scale; // 叶团半径（叶团网格基准半径 1.0）
};
struct TreeLimb {
	float ang; // 方位角
	float rad; // 梢部离主干轴的水平距离
	float y0;  // 起点高度（在主干上）
	float y1;  // 梢部高度
	float r0;  // 起点半径
	float r1;  // 梢部半径
};
struct TreeArchetype {
	std::vector<TreeBlob> blobs;
	std::vector<TreeLimb> limbs;
	Ref<ArrayMesh> skeleton;
	float top_y = 4.0f;    // 冠顶高度（积雪分档 / 雪毯用）
	float shoulder_y = 3.3f; // 冠肩高度
	float crown_r = 1.7f;  // 树冠横向半径
};

// 生成一套树形。坐标系是"树本地空间"：y=0 地面、y 向上，尺寸按未缩放的树。
TreeArchetype make_tree_archetype(std::mt19937 &rng) {
	auto rr = [&rng](float lo, float hi) -> float {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};
	TreeArchetype a;
	float crown_w = rr(0.84f, 1.14f); // 横向铺开倍率
	float crown_h = rr(0.94f, 1.08f); // 纵向抬高倍率
	float blob_r = rr(0.72f, 0.92f);  // 叶团基准半径
	float base_ang = rr(0.0f, Math::TAU);

	// 叶团个数逐套变化（3~5 / 2~4 / 1~3）：这是"每棵树长得不一样"最直接的来源，
	// 比换 yaw、换缩放管用得多。团数一多一少，树冠的疏密和轮廓就完全不同了。
	//
	// 底圈：铺得开、团之间留缝 —— 缝里能看见枝干，层次感就是从这来的
	int n1 = 3 + (int)rr(0.0f, 3.0f);
	for (int i = 0; i < n1; i++) {
		float ang = base_ang + (float)i / (float)n1 * Math::TAU + rr(-0.26f, 0.26f);
		float rad = rr(0.56f, 0.86f) * crown_w;
		float y = rr(2.38f, 2.78f) * crown_h;
		float sc = blob_r * rr(0.66f, 0.84f);
		a.blobs.push_back({ ang, rad, y, sc });
		a.limbs.push_back({ ang, rad * 0.96f, rr(1.10f, 1.48f), y - sc * 0.10f,
				rr(0.075f, 0.092f), rr(0.030f, 0.040f) });
	}
	// 中圈：收一点、抬一点
	int n2 = 2 + (int)rr(0.0f, 2.99f);
	for (int i = 0; i < n2; i++) {
		float ang = base_ang + 0.9f + (float)i / (float)n2 * Math::TAU + rr(-0.34f, 0.34f);
		float rad = rr(0.32f, 0.64f) * crown_w;
		float y = rr(2.90f, 3.28f) * crown_h;
		float sc = blob_r * rr(0.58f, 0.76f);
		a.blobs.push_back({ ang, rad, y, sc });
		a.limbs.push_back({ ang, rad, rr(1.55f, 1.95f), y - sc * 0.10f,
				rr(0.055f, 0.070f), rr(0.024f, 0.034f) });
	}
	// 顶团：收成冠尖
	int n3 = 1 + (int)rr(0.0f, 2.99f);
	for (int i = 0; i < n3; i++) {
		float ang = base_ang + rr(0.0f, Math::TAU);
		float rad = rr(0.0f, 0.28f) * crown_w;
		float y = rr(3.26f, 3.60f) * crown_h;
		float sc = blob_r * rr(0.50f, 0.70f);
		a.blobs.push_back({ ang, rad, y, sc });
		a.limbs.push_back({ ang, rad, rr(2.05f, 2.35f), y - sc * 0.10f,
				rr(0.045f, 0.058f), rr(0.020f, 0.028f) });
	}

	// 碎叶团 2~3 个：贴在大叶团的表面上，专门用来打碎树冠轮廓。
	// 树冠"像不像叶子"主要看边缘碎不碎 —— 边缘光滑的一坨永远是气球；
	// 边缘被小团啃出锯齿，才像一簇簇叶子堆出来的。
	// （这些团不配枝条：它们本来就趴在大团表面，配枝条反而会戳出来。）
	int base_n = (int)a.blobs.size();
	int n4 = 2 + (int)rr(0.0f, 1.99f);
	for (int i = 0; i < n4; i++) {
		TreeBlob host = a.blobs[(int)rr(0.0f, (float)base_n - 0.001f)]; // 按值拷贝：下面要 push_back
		float hx = host.rad * Math::cos(host.ang);
		float hz = host.rad * Math::sin(host.ang);
		float oa = rr(0.0f, Math::TAU);
		float orr = host.scale * rr(0.55f, 0.85f);
		float nx = hx + Math::cos(oa) * orr;
		float nz = hz + Math::sin(oa) * orr;
		float ny = host.y + rr(-0.18f, 0.48f) * host.scale;
		a.blobs.push_back({ Math::atan2(nz, nx), Math::sqrt(nx * nx + nz * nz), ny,
				host.scale * rr(0.36f, 0.54f) });
	}

	// 冠顶 / 冠肩 / 横向半径由叶团反推 —— 不再写死常数，
	// 这样树冠形状一变，雪毯和积雪分档自动跟着走。
	float top = 0.0f;
	float rad_max = 0.0f;
	for (const TreeBlob &b : a.blobs) {
		top = Math::max(top, b.y + b.scale * 0.86f); // 叶团网格 squash=0.86
		rad_max = Math::max(rad_max, b.rad + b.scale);
	}
	a.top_y = top;
	a.shoulder_y = Math::max(top - 0.62f, 2.0f);
	a.crown_r = rad_max;
	return a;
}

// 由原型生成骨架网格：主干 + 所有侧枝，一体成型。
// 主干和树枝必须在同一个网格里，才能共用同一份 MultiMesh 实例变换（一棵树一次 draw）。
Ref<ArrayMesh> make_tree_skeleton_mesh(const TreeArchetype &p_a) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);
	// 圆管要平滑着色（8 边形截面若走逐面法线，看着像根铅笔）；整棵树共用一个平滑组，
	// 各段顶点位置不同，不会互相污染法线。
	st->set_smooth_group(0);
	const Color bark(1.0f, 1.0f, 1.0f); // 顶点色留白，颜色全走 MultiMesh 实例色（树皮棕）

	// 主干：底端埋进土 0.10，往上细成"主梢"钻进树冠（顶端藏在叶团里，不用封口）
	add_tube(st.ptr(), Vector3(0.0f, -0.10f, 0.0f), Vector3(0.0f, 2.30f, 0.0f), 0.30f, 0.085f, 8, bark);
	for (const TreeLimb &l : p_a.limbs) {
		Vector3 a(0.055f * Math::cos(l.ang), l.y0, 0.055f * Math::sin(l.ang));
		Vector3 b(l.rad * Math::cos(l.ang), l.y1, l.rad * Math::sin(l.ang));
		add_tube(st.ptr(), a, b, l.r0, l.r1, 6, bark);
	}

	st->generate_normals();
	Ref<ArrayMesh> m = st->commit();
	m->surface_set_material(0, veg_material());
	return m;
}

// 夜光孢子光尘：一撮淡绿微光粒子，缓慢上浮、生命周期首尾渐隐。
// 生成时 emitting=false，白天不冒；夜里由 update_glow 打开。
// 调用方负责 add_child（在 build_vegetation 的收尾统一挂到 TerrainBuilder 下）。
CPUParticles3D *make_spore_puff(const Vector3 &p_pos) {
	Ref<SphereMesh> mote;
	mote.instantiate();
	mote->set_radius(0.012f);
	mote->set_height(0.024f);
	mote->set_radial_segments(5);
	mote->set_rings(2);
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_albedo(Color(0.52f, 0.92f, 0.62f, 1.0f));
	mote->set_material(m);

	// 透明度渐变：出现 → 淡入 → 缓慢淡出（颜色乘 alpha，unshaded 下直接生效）
	Ref<Gradient> ramp;
	ramp.instantiate();
	ramp->add_point(0.0f, Color(0.52f, 0.92f, 0.62f, 0.0f));
	ramp->add_point(0.25f, Color(0.52f, 0.92f, 0.62f, 1.0f));
	ramp->add_point(1.0f, Color(0.52f, 0.92f, 0.62f, 0.0f));

	CPUParticles3D *p = memnew(CPUParticles3D);
	p->set_amount(8);
	p->set_lifetime(5.0f);
	p->set_lifetime_randomness(0.6f);
	p->set_randomness_ratio(0.4f);
	p->set_mesh(mote);
	p->set_color_ramp(ramp);
	p->set_direction(Vector3(0.0f, 1.0f, 0.0f));
	p->set_spread(40.0f);
	p->set_gravity(Vector3(0.0f, 0.02f, 0.0f)); // 近似失重的缓缓上飘
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.05f);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 0.18f);
	p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.6f);
	p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.4f);
	p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	p->set_emission_sphere_radius(0.55f); // 从整簇蘑菇的体积里冒出来
	p->set_emitting(false);
	p->set_position(p_pos);
	return p;
}

} // namespace

// ---------------------------------------------------------------- 基础

void TerrainBuilder::_bind_methods() {
	// 纯运行时生成器，暂不暴露 Inspector 属性；想调地图改 terrain_builder.h 里的参数
}

void TerrainBuilder::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// GDExtension 的 C++ 类在编辑器里也会收到 READY（相当于永远 @tool）。
			// 只在真正运行游戏时生成世界，避免把运行时节点塞进编辑器场景。
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			build_terrain();
			build_water();
			build_vegetation();
			// 世界生成完之后还有一件轻量的常驻工作：按昼夜驱动蘑菇夜光
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			update_sky_driven();
		} break;
		default:
			break;
	}
}

float TerrainBuilder::sstepf(float a, float b, float x) {
	float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

// 天气/昼夜驱动的运行时材质：每帧从 SkyCycle 读因子，写进各专用材质。
//   * 昼夜映射与 Bird/Rabbit/Butterfly 一致：夜幕因子统一取
//     SkyCycle::get_night_factor()（sun_h 从 -0.02 降到 -0.12 → 0→1 的唯一来源）；
//     再平方一下让黄昏过渡更含蓄。
//     驱动蘑菇伞盖/白点自发光能量 + 孢子光尘开关；天气联动：雨雪淋湿菌盖
//     荧光减弱（wet 因子），雨雪天孢子光尘被冲散不发射。
//   * 雪强 snow → 地形雪盖（terrain_mat）与分级积雪（snow_bands）的
//     snow_amount：地形缓坡铺雪、草/灌木/树冠覆雪且梢部下坠。
// 找不到 SkyCycle（夜幕缺省 0、雨雪缺省 0）视为晴天白天 → 能量 0、无雪。
void TerrainBuilder::update_sky_driven() {
	// SkyCycle 是兄弟节点，懒查找一次后缓存（READY 顺序无关，同 bird/rabbit 套路）
	if (sky_cache == nullptr) {
		Node *parent = get_parent();
		if (parent) {
			sky_cache = Object::cast_to<SkyCycle>(parent->get_node_or_null(NodePath("SkyCycle")));
		}
	}
	float rain = 0.0f, snow = 0.0f;
	if (sky_cache) {
		rain = sky_cache->get_rain_amount();
		snow = sky_cache->get_snow_amount();
		// 太阳方向 → 树冠叶片透光（背光面透出的暖光）。方向每帧都在变，
		// 所以放在这里逐帧写；材质数量少（3 档树冠），代价可忽略。
		if (!leaf_mats.empty()) {
			Vector3 sd = sky_cache->get_sun_dir();
			for (auto &m : leaf_mats) {
				m->set_shader_parameter("sun_dir", sd);
			}
		}
	}

	// —— 踩草倒伏：收集玩家 + 农场动物的位置与"移动能量"写进草丛着色器 ——
	// 能量 0 = 站着不动（草只保持压倒的凹陷，无涟漪）；1 = 跑动（涟漪全开）。
	if (!trample_mats.empty()) {
		std::vector<std::pair<Node *, Vector3>> sources;
		Node *parent = get_parent();
		if (parent) {
			Node3D *pl = Object::cast_to<Node3D>(parent->get_node_or_null(NodePath("Player")));
			if (pl != nullptr) {
				sources.push_back({ pl, pl->get_global_position() });
			}
			SceneTree *tree = get_tree();
			if (tree != nullptr) {
				Array animals = tree->get_nodes_in_group("farm_animals");
				for (int i = 0; i < animals.size() && (int)sources.size() < 8; i++) {
					Node3D *a = Object::cast_to<Node3D>(animals[i]);
					if (a != nullptr) {
						sources.push_back({ a, a->get_global_position() });
					}
				}
			}
		}
		float delta = get_process_delta_time();
		PackedVector3Array tp;
		PackedFloat32Array te;
		PackedFloat32Array tw;
		for (auto &s : sources) {
			Player *player = Object::cast_to<Player>(s.first);
			CharacterBody3D *cb = Object::cast_to<CharacterBody3D>(s.first);
			bool on_floor = (cb == nullptr) || cb->is_on_floor();

			Vector3 s_pos = s.second;
			float target_e = 0.0f;
			float prev_e = 0.0f;
			float prev_w = 1.0f;
			auto it = trample_track.find(s.first);
			if (it != trample_track.end()) {
				prev_e = it->second.energy;
				prev_w = it->second.weight;
				if (delta > 0.0f && on_floor) {
					// 仅水平位移计入跑步移动能量，防止起跳垂直速度误触发奔跑水波
					Vector2 d_xz(s.second.x - it->second.prev_pos.x, s.second.z - it->second.prev_pos.z);
					float speed = d_xz.length() / delta; // m/s
					target_e = clampf(speed / 2.5f, 0.0f, 1.0f); // 2.5 m/s 跑满
				}
			}

			float w = 0.0f;
			float e = 0.0f;
			if (on_floor) {
				// 踩在地面：权重拉满 1.0，平滑速度 20/s
				w = lerpf(prev_w, 1.0f, clampf(delta * 20.0f, 0.0f, 1.0f));
				e = lerpf(prev_e, target_e, clampf(delta * 10.0f, 0.0f, 1.0f));
				s_pos = s.second;
			} else if (player != nullptr && player->get_land_timer() > 0.0) {
				// 连跳腾空阶段：刚踩过地面接地点，接地点小草保持倒伏并平滑回弹（0.4s 回弹曲线）
				float ratio = clampf((float)(player->get_land_timer() / 0.40), 0.0f, 1.0f);
				w = ratio;
				e = ratio * 0.5f; // 落地轻微冲击波随回弹渐隐
				s_pos = player->get_last_land_pos(); // 接地点保持在地面，不随身体拔高而被垂直裁剪
			} else {
				// 普通空中腾空阶段（纯起跳且无落地缓冲）：迅速回弹归 0，完全不压草
				w = lerpf(prev_w, 0.0f, clampf(delta * 14.0f, 0.0f, 1.0f));
				if (w < 0.005f) {
					w = 0.0f;
				}
				e = lerpf(prev_e, 0.0f, clampf(delta * 10.0f, 0.0f, 1.0f));
				s_pos = s.second;
			}

			trample_track[s.first] = { s.second, e, w };
			tp.push_back(s_pos);
			te.push_back(e);
			tw.push_back(w);
		}
		for (Ref<ShaderMaterial> &tm : trample_mats) {
			if (tm.is_valid()) {
				tm->set_shader_parameter("trample_count", (int)tp.size());
				tm->set_shader_parameter("trample_pos", tp);
				tm->set_shader_parameter("trample_energy", te);
				tm->set_shader_parameter("trample_weight", tw);
			}
		}
	}

	// —— 蘑菇夜光 ——
	if (shroom_cap_mat.is_valid() && shroom_spot_mat.is_valid()) {
		// 夜幕因子唯一来源 SkyCycle::get_night_factor()；缺省 0 = 正午白天
		float night = (sky_cache != nullptr) ? sky_cache->get_night_factor() : 0.0f;
		float glow = night * night;
		float wet = Math::clamp(rain + snow, 0.0f, 1.0f);
		glow *= 1.0f - 0.75f * wet;

		// 柔和微光：控制在泛光阈值以下，夜里清晰辨识红伞白点与轮廓，杜绝白茫茫光斑
		shroom_cap_mat->set_emission_energy_multiplier(glow * 0.25f);
		shroom_spot_mat->set_emission_energy_multiplier(glow * 0.18f);
		bool em = glow > 0.05f && rain < 0.3f && snow < 0.3f;
		for (CPUParticles3D *p : spore_puffs) {
			p->set_emitting(em);
		}
	}

	// —— 积雪视觉：地形雪盖用原始雪量（坡度/水线门控在着色器里）；
	// 植被与雪毯走分级条目——每档按 [lo,hi] 映射雪量，低处先积雪 ——
	if (terrain_mat.is_valid()) {
		terrain_mat->set_shader_parameter("snow_amount", snow);
	}
	for (SnowBand &b : snow_bands) {
		if (!b.mat.is_valid()) {
			continue;
		}
		float v = sstepf(b.lo, b.hi, snow);
		b.mat->set_shader_parameter("snow_amount", v);
		if (b.drive_alpha) {
			b.mat->set_shader_parameter("alpha", v); // 雪毯：随所在档位淡入，无雪隐形
		}
	}
}

// ------------------------------------------------------------- 高度场

float TerrainBuilder::value_noise(float x, float z) const {
	auto hash2 = [](float px, float pz) -> float {
		float s = std::sin(px * 127.1f + pz * 311.7f) * 43758.5453f;
		return s - std::floor(s); // [0, 1)
	};
	float xi = std::floor(x);
	float zi = std::floor(z);
	float xf = x - xi;
	float zf = z - zi;
	float u = xf * xf * (3.0f - 2.0f * xf); // 平滑插值权重
	float v = zf * zf * (3.0f - 2.0f * zf);
	float a = hash2(xi, zi);
	float b = hash2(xi + 1.0f, zi);
	float c = hash2(xi, zi + 1.0f);
	float d = hash2(xi + 1.0f, zi + 1.0f);
	return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}

float TerrainBuilder::fbm(float x, float z) const {
	// 三个倍频程叠加后归一，输出约 [-1, 1]
	float f = (value_noise(x, z) - 0.5f) * 2.0f
			+ (value_noise(x * 2.13f + 5.2f, z * 2.13f + 1.3f) - 0.5f) * 0.5f
			+ (value_noise(x * 4.31f + 9.1f, z * 4.31f + 7.7f) - 0.5f) * 0.25f;
	return f / 1.75f;
}

float TerrainBuilder::terrain_height(float x, float z) const {
	// 山丘：大起伏 + 中起伏 + 丘陵带 + 细节
	float h = fbm(x * 0.016f, z * 0.016f) * 3.4f;
	h += fbm(x * 0.055f + 40.0f, z * 0.055f - 17.0f) * 1.1f;
	// 丘陵带：波长约 36m、±1.75m 的滚坡（用户要的"小山坡"层次；
	// 加在洼地压平之前 → 谷底自动压平防干坑，湖盆/引道/平台在其后覆盖）
	h += fbm(x * 0.028f - 77.0f, z * 0.028f + 63.0f) * 1.35f;
	h += (value_noise(x * 0.21f, z * 0.21f) - 0.5f) * 0.36f;

	// 洼地压平：湖外不允许出现低于水面的坑（防止"断水"的裸坑）
	if (h < 0.0f) {
		h *= 0.18f;
	}

	// 地图边缘抬升成环山，天然围住玩家
	float half = map_size * 0.5f;
	float edge = Math::max(std::abs(x), std::abs(z)) / half;
	h += sstepf(0.74f, 1.0f, edge) * (5.5f + 3.0f * value_noise(x * 0.028f, z * 0.028f));

	// 壮阔复合大湖盆（全面扩建成大湖：西北浩瀚主湖 44m + 东北阳光水域 32m + 石桥水峡 13x18m + 南侧湖湾 22m）
	// 引入双频海岸线 FBM 域扭曲（Domain Warping），使岸线自然蜿蜒，彻底消除几何直圆弧与呆板感
	float c_wx = fbm(x * 0.055f + 17.3f, z * 0.055f - 29.1f) * 6.5f;
	float c_wz = fbm(x * 0.055f - 43.7f, z * 0.055f + 11.5f) * 6.5f;
	float wx = x + c_wx;
	float wz = z + c_wz;

	// 1. 西北大湖主水域（深水开阔湖区）
	float b_nw = 1.0f - sstepf(44.0f * 0.50f, 44.0f * 1.02f, (Vector2(wx, wz) - Vector2(-14.0f, 66.0f)).length());
	// 2. 东北阳光水域（浩渺碧波，与西北主湖相连）
	float b_ne = 1.0f - sstepf(32.0f * 0.50f, 32.0f * 1.02f, (Vector2(wx, wz) - Vector2(16.0f, 56.0f)).length());
	// 3. 石桥水峡（平缓穿过石桥下方，水流南北贯通）
	float b_dx = (wx - (-6.0f)) / 13.0f;
	float b_dz = (wz - 26.0f) / 18.0f;
	float b_bridge = 1.0f - sstepf(0.50f, 1.02f, std::sqrt(b_dx * b_dx + b_dz * b_dz));
	// 4. 南侧湖湾（靠近小径与木屋南方的风景湾）
	float b_south = 1.0f - sstepf(22.0f * 0.50f, 22.0f * 1.02f, (Vector2(wx, wz) - Vector2(-15.0f, 8.0f)).length());

	float basin = Math::max(Math::max(b_nw, b_ne), Math::max(b_bridge, b_south));
	h = lerpf(h, -2.85f, basin);

	// 木桥两端的引道平台（与桥面齐平，方便上岸；丘陵带叠加后过渡带放宽到 13m 防陡坡顶脚）
	Vector2 b1 = Vector2(x, z) - Vector2(bridge_end_west, lake_center.y);
	Vector2 b2 = Vector2(x, z) - Vector2(bridge_end_east, lake_center.y);
	float ramp = 1.0f - sstepf(5.0f, 13.0f, Math::min(b1.length(), b2.length()));
	h = lerpf(h, bridge_deck_end_y, ramp);

	// 出生点小平台
	Vector2 to_spawn = Vector2(x, z) - spawn_point;
	float flat = 1.0f - sstepf(6.0f, 12.0f, to_spawn.length());
	h = lerpf(h, 0.06f, flat);

	// 宅基地平台（木屋 + 篱笆院子）：全平 11m，向外 5m 过渡回自然地形。
	// 彻底修复"湖水和土地交接处有一小块切割得整整齐齐"的异常：
	// 根因：过去 home_flat 在所有压平之后无条件覆盖，把已陷入湖盆 (x=1, z=5..13) 的水下地形
	// 强行平整拔高至 homestead_height(+0.35m)，在水线上形成了一条 9 米长的刀削平截断崖！
	// 修复方案：引入 basin 衰减门控 —— 陆地上完整保持院落平整，进入湖水凹陷处时平滑避让，
	// 让岸坡自然顺滑地滑入湖底，完全恢复自然岸线！
	Vector2 to_home = Vector2(x, z) - homestead_center;
	float home_flat = 1.0f - sstepf(11.0f, 16.0f, to_home.length());
	float home_weight = home_flat * Math::max(0.0f, 1.0f - basin * 3.5f);
	h = lerpf(h, homestead_height, home_weight);

	// 足球场平台：矩形平地 + 8m 过渡带（放在最后 → 优先级最高）
	Vector2 to_pitch = Vector2(x, z) - pitch_center;
	float rx = Math::abs(to_pitch.x) / (pitch_half_w + 8.0f);
	float rz = Math::abs(to_pitch.y) / (pitch_half_d + 8.0f);
	float pr = 1.0f - sstepf(0.72f, 1.0f, Math::max(rx, rz));
	h = lerpf(h, pitch_height, pr);

	return h;
}

bool TerrainBuilder::is_near_lake(float x, float z, float margin) const {
	Vector2 p(x, z);
	// 1. 西北大湖浩瀚主水域 (44m)
	if ((p - Vector2(-14.0f, 66.0f)).length() < (44.0f + margin)) {
		return true;
	}
	// 2. 东北阳光水域 (32m)
	if ((p - Vector2(16.0f, 56.0f)).length() < (32.0f + margin)) {
		return true;
	}
	// 3. 石桥水峡 (13m x 18m)
	float b_dx = (x - (-6.0f)) / (13.0f + margin);
	float b_dz = (z - 26.0f) / (18.0f + margin);
	if (b_dx * b_dx + b_dz * b_dz < 1.0f) {
		return true;
	}
	// 4. 南侧秀丽湖湾 (22m)
	if ((p - Vector2(-15.0f, 8.0f)).length() < (22.0f + margin)) {
		return true;
	}
	return false;
}

bool TerrainBuilder::is_in_bush(float x, float z, float y) const {
	for (const BushObstacle &b : bush_obstacles) {
		float dx = x - b.pos.x;
		float dz = z - b.pos.z;
		float r = b.radius;
		if (dx * dx + dz * dz <= r * r) {
			if (y >= b.pos.y - 0.25f && y <= b.pos.y + b.height + 0.35f) {
				return true;
			}
		}
	}
	return false;
}

// 判断某点是否在球场内
bool TerrainBuilder::in_pitch(float x, float z) const {
	return Math::abs(x - pitch_center.x) < pitch_half_w
			&& Math::abs(z - pitch_center.y) < pitch_half_d;
}

// 泥土小径：出生点 → 宅基地院门、出生点 → 石桥西岸。返回点到折线的平面距离。
float TerrainBuilder::dist_to_path(float x, float z) const {
	auto seg_dist = [](const Vector2 &p, const Vector2 &a, const Vector2 &b) -> float {
		Vector2 ab = b - a;
		float denom = ab.length_squared();
		float t = denom > 0.0001f ? clampf((p - a).dot(ab) / denom, 0.0f, 1.0f) : 0.0f;
		return (p - (a + ab * t)).length();
	};
	Vector2 p(x, z);
	// 院门在世界坐标 (12,8) + 45° 朝向的局部 +Z*5 ≈ (15.5, 11.5)
	Vector2 gate(15.5f, 11.5f);
	Vector2 bridge_west(bridge_end_west, lake_center.y);
	float d1 = seg_dist(p, spawn_point, gate);
	float d2 = seg_dist(p, spawn_point, bridge_west);
	return Math::min(d1, d2);
}

float TerrainBuilder::terrain_slope(float x, float z) const {
	const float e = 0.55f;
	float dx = (terrain_height(x + e, z) - terrain_height(x - e, z)) / (2.0f * e);
	float dz = (terrain_height(x, z + e) - terrain_height(x, z - e)) / (2.0f * e);
	return std::sqrt(dx * dx + dz * dz);
}

// ------------------------------------------------------------- 地形网格

void TerrainBuilder::build_terrain() {
	const int n = grid_steps;
	const float half = map_size * 0.5f;
	const float step = map_size / (float)n;

	// 顶点颜色即"材质"：草地 / 干草 / 沙滩 / 湖床 / 岩壁 按高度与坡度混合。
	// 优先用雪盖着色器（snow_amount=0 时外观与纯顶点色一致，雪天由
	// update_sky_driven 拉起来）；着色器加载失败则退回标准顶点色材质。
	terrain_mat = Ref<ShaderMaterial>();
	Ref<Resource> snow_res = ResourceLoader::get_singleton()->load("res://assets/snow_ground.gdshader");
	Ref<Shader> snow_shader = Ref<Shader>(Object::cast_to<Shader>(snow_res.ptr()));
	if (snow_shader.is_valid()) {
		terrain_mat.instantiate();
		terrain_mat->set_shader(snow_shader);
		terrain_mat->set_shader_parameter("snow_amount", 0.0f);
		terrain_mat->set_shader_parameter("snow_water_line", water_level);
	} else {
		WARN_PRINT("TerrainBuilder: snow_ground.gdshader 加载失败，地形无雪盖（用标准材质兜底）");
	}
	Ref<StandardMaterial3D> mat;
	if (terrain_mat.is_null()) {
		mat.instantiate();
		mat->set_albedo(Color(1.0f, 1.0f, 1.0f));
		mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		mat->set_roughness(1.0f);
	}

	// SurfaceTool 是 RefCounted（Object 派生），必须用 Ref 创建，不能栈上构造
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	auto vert_color = [&](float x, float z, float h) -> Color {
		float gmix = value_noise(x * 0.09f, z * 0.09f);
		if (in_pitch(x, z)) {
			return Color(0.09f, 0.26f, 0.07f); // 球场底色：草叶很密，地面只当陪衬
		}
		Color grass = Color(lerpf(0.20f, 0.14f, gmix), lerpf(0.44f, 0.36f, gmix), lerpf(0.13f, 0.09f, gmix));
		float dry = sstepf(2.2f, 4.5f, h); // 高处偏干黄
		grass = grass.lerp(Color(0.45f, 0.48f, 0.25f), dry * 0.6f);

		// 草地成片：与草丛聚簇密度图同源的噪声，稀草区露出偏干的浅土
		// （MC 草方块之间裸土斑块的观感；参数须与 build_vegetation 草丛循环一致）
		float clump = value_noise(x * 0.14f + 21.0f, z * 0.14f + 11.0f);
		float bare = 1.0f - sstepf(0.26f, 0.40f, clump);
		if (bare > 0.0f) {
			grass = grass.lerp(Color(0.52f, 0.45f, 0.31f), bare * 0.5f);
		}

		float sand_t = 1.0f - sstepf(water_level + 0.08f, water_level + 0.30f, h); // 水线窄窄一圈沙滩
		float bed_t = sstepf(water_level, water_level - 0.9f, h);                  // 越深越湿
		Color c = grass.lerp(Color(0.72f, 0.65f, 0.46f), sand_t);
		c = c.lerp(Color(0.50f, 0.47f, 0.36f), bed_t);

		// 湖岸湿沙带：水线上方一小圈被湖水浸润的深色沙（涨落水痕迹，
		// 让沙滩 → 草地的过渡多一层层次）
		float wet_t = 1.0f - sstepf(water_level + 0.02f, water_level + 0.24f, h);
		if (wet_t > 0.0f) {
			c = c.lerp(Color(0.44f, 0.40f, 0.28f), wet_t * 0.55f);
		}

		float rock_t = sstepf(0.55f, 0.95f, terrain_slope(x, z)); // 陡坡露岩
		rock_t = Math::max(rock_t, sstepf(4.0f, 6.5f, h));        // 高山也露岩
		if (rock_t > 0.0f) {
			// MC 石壁层理：按海拔的水平岩层条带，条带边界用低频噪声扰动，
			// 再叠一层高频碎石颗粒纹，避免大面积纯色
			float warp = (value_noise(x * 0.35f, z * 0.35f) - 0.5f) * 1.4f;
			float band = sstepf(0.10f, 0.60f, std::sin((h + warp) * 2.4f));
			float grain = value_noise(x * 1.9f, z * 1.9f) - 0.5f;
			Color rock(lerpf(0.40f, 0.54f, band) + grain * 0.10f,
					lerpf(0.37f, 0.50f, band) + grain * 0.09f,
					lerpf(0.33f, 0.43f, band) + grain * 0.08f);
			c = c.lerp(rock, rock_t);
		}

		// 泥土小径（出生点 → 院门 / 石桥西岸），边缘略带磨损感
		float path_t = 1.0f - sstepf(0.8f, 1.8f, dist_to_path(x, z));
		if (path_t > 0.0f) {
			float wear = value_noise(x * 1.7f, z * 1.7f); // 路面斑驳
			Color dirt(lerpf(0.50f, 0.56f, wear), lerpf(0.40f, 0.45f, wear), lerpf(0.26f, 0.30f, wear));
			c = c.lerp(dirt, path_t);
		}
		return c;
	};

	auto add_v = [&](float x, float z) {
		float h = terrain_height(x, z);
		st->set_color(vert_color(x, z, h));
		st->add_vertex(Vector3(x, h, z));
	};

	for (int j = 0; j < n; j++) {
		for (int i = 0; i < n; i++) {
			float x0 = -half + i * step;
			float x1 = x0 + step;
			float z0 = -half + j * step;
			float z1 = z0 + step;
			// Godot 顺时针缠绕为正面：俯视时以下两组都是顺时针 → 法线朝上
			add_v(x0, z0);
			add_v(x1, z0);
			add_v(x1, z1);
			add_v(x0, z0);
			add_v(x1, z1);
			add_v(x0, z1);
		}
	}

	st->generate_normals();
	if (terrain_mat.is_valid()) {
		st->set_material(terrain_mat);
	} else {
		st->set_material(mat);
	}

	Ref<ArrayMesh> mesh = st->commit();
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name("TerrainMesh");
	mi->set_mesh(mesh);
	add_child(mi);
	mi->create_trimesh_collision(); // 自动加 StaticBody3D + 三角网格碰撞
}

// ------------------------------------------------------------- 水面

void TerrainBuilder::build_water() {
	Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/water.gdshader");
	Ref<Shader> shader = Ref<Shader>(Object::cast_to<Shader>(res.ptr()));
	if (shader.is_null()) {
		return; // 找不到着色器就不放水（正常不会发生）
	}

	Ref<ShaderMaterial> mat;
	mat.instantiate();
	mat->set_shader(shader);
	water_mat = mat; // 存成员：build_vegetation 撒完湖岸礁石后要把坐标写进来

	Ref<PlaneMesh> plane;
	plane.instantiate();
	plane->set_size(Vector2(150.0f, 165.0f)); // 边缘深埋进沿岸丘陵，水面完整覆盖壮阔大湖 (X: -81~69, Z: -34.5~130.5)
	plane->set_subdivide_width(176);          // 顶点波浪细分（大湖水面波浪精细起伏）
	// 细分从 104 提到 192：顶点间距约 0.85m —— 拍岸涌浪的浪脊宽约 1.5m，
	// 网格太粗的话浪脊比一个格子还窄，直接采样走样（水面一格一格地闪）。
	plane->set_subdivide_depth(192);
	plane->set_material(mat);

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name("Lake");
	mi->set_mesh(plane);
	mi->set_position(Vector3(-6.0f, water_level, 48.0f));
	add_child(mi);

	// ---- 水深图（shore_map）----
	// 为什么需要它：拍岸浪的相位是"离岸多远"的函数，但顶点着色器拿不到深度缓冲
	// （hint_depth_texture / SCREEN_UV 只在 fragment 有效），所以顶点里用的水深
	// 必须由 CPU 提前烘成一张纹理。片元侧也读同一张图 —— 两边相位同源，
	// 白沫才会骑在抬起来的水脊上，而不是各算各的。
	//
	// 覆盖范围必须严格等于上面的 PlaneMesh：中心 (-6, 48) + size(150,165)
	//   → X: -81 ~ 69，Z: -34.5 ~ 130.5
	const Vector2 sm_origin(-81.0f, -34.5f);
	const Vector2 sm_size(150.0f, 165.0f);
	const float SM_DEPTH = 6.0f; // 与 water.gdshader 的 shore_map_depth 一致
	const int SMW = 384;
	const int SMH = 422; // 约 0.39 m/像素；线性过滤把像素台阶抹平，浪脊轮廓够细
	Ref<Image> sm = Image::create_empty(SMW, SMH, false, Image::FORMAT_R8);
	for (int y = 0; y < SMH; y++) {
		for (int x = 0; x < SMW; x++) {
			Vector2 w = sm_origin + sm_size * Vector2(((float)x + 0.5f) / (float)SMW,
													  ((float)y + 0.5f) / (float)SMH);
			float d = clampf((water_level - terrain_height(w.x, w.y)) / SM_DEPTH, 0.0f, 1.0f);
			sm->set_pixel(x, y, Color(d, 0.0f, 0.0f));
		}
	}
	mat->set_shader_parameter("shore_map", ImageTexture::create_from_image(sm));
	mat->set_shader_parameter("shore_map_origin", sm_origin);
	mat->set_shader_parameter("shore_map_size", sm_size);
	mat->set_shader_parameter("shore_map_depth", SM_DEPTH);
}

// 把湖岸礁石坐标写进水面着色器（shore_rocks 数组 + 数量）。
// 着色器据此在每块石头周围爆起白沫 —— 拍岸浪的"撞击点"来自真实几何。
void TerrainBuilder::apply_shore_rocks() {
	if (water_mat.is_null()) {
		return;
	}
	const int MAX_ROCKS = 40; // 湖岸 32 + 深水巨礁 1 + 伴礁 2 + 余量；须与 water.gdshader 数组长度一致
	PackedVector4Array arr;
	arr.resize(MAX_ROCKS);
	int n = 0;
	for (const Vector4 &r : shore_rocks) {
		if (n >= MAX_ROCKS) {
			break;
		}
		arr[n] = r;
		++n;
	}
	for (int i = n; i < MAX_ROCKS; ++i) {
		arr[i] = Vector4(0.0f, -999.0f, 0.0f, 0.0f); // 空槽扔到天边，着色器里距离永远拉满
	}
	water_mat->set_shader_parameter("shore_rocks", arr);
	water_mat->set_shader_parameter("shore_rock_count", n);
}

// ------------------------------------------------------------- 植被

void TerrainBuilder::build_vegetation() {
	const float half = map_size * 0.5f;
	const float inner = half * 0.86f; // 环山内可用区域
	bush_obstacles.clear();

	std::mt19937 rng(1337); // 固定种子 → 地图可复现
	auto rnd = [&rng](float lo, float hi) -> float {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(rng);
	};
	// 叶团抖动专用随机源：独立种子、独立序列。
	// 树冠从"1 个球"变成"7 个球"要多抽一批随机数，若混用上面的 rng 会把
	// 全局序列整体推后 → 灌木/草/芦苇/石头全部重排一遍（整图变样）。
	// 单开一条序列，树的形状变了，地图其余部分原封不动。
	std::mt19937 blob_rng(20250810);
	auto blob_rnd = [&blob_rng](float lo, float hi) -> float {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(blob_rng);
	};
	// 荷叶倾斜 / 荷花撒点用的独立随机源，同样是为了不动全局序列。
	std::mt19937 pond_rng(552211);
	auto pond_rnd = [&pond_rng](float lo, float hi) -> float {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(pond_rng);
	};

	// 给共用网格挂风摆材质（积雪走 snow_bands 分级，登记见各网格创建处；
	// 着色器加载失败时退回静态材质，行为与旧版一致）
	// 泛型参数：PrimitiveMesh 用 set_material；ArrayMesh（草丛三叶簇）走 surface_set_material
	auto apply_wind = [](auto p_mesh, float p_base_y, float p_span, float p_strength) -> Ref<ShaderMaterial> {
		Ref<ShaderMaterial> wm = wind_material(p_base_y, p_span, p_strength);
		Ref<Material> mat = wm.is_valid() ? Ref<Material>(wm) : fallback_wind_material();
		Ref<PrimitiveMesh> prim = Ref<PrimitiveMesh>(Object::cast_to<PrimitiveMesh>(p_mesh.ptr()));
		if (prim.is_valid()) {
			prim->set_material(mat);
		} else {
			Ref<ArrayMesh> am = Ref<ArrayMesh>(Object::cast_to<ArrayMesh>(p_mesh.ptr()));
			if (am.is_valid()) {
				am->surface_set_material(0, mat);
			}
		}
		return wm;
	};

	// —— 共用网格（低模 + 实例色，一种植物一个 MultiMesh） ——
	// 树形原型：预生成 ARCH_N 套树（各自的树冠形状 + 配套枝条），种树时随机挑一套。
	// 这是"树不要全都长的一模一样"的落点 —— 光靠 yaw 和缩放区分不出来，
	// 必须让树冠本身的形状（宽窄 / 高低 / 叶团个数）就不一样。
	const int ARCH_N = 6;
	std::mt19937 arch_rng(90210); // 独立随机源：不干扰全局序列，地图其余部分不动
	std::vector<TreeArchetype> archetypes;
	archetypes.reserve(ARCH_N);
	for (int i = 0; i < ARCH_N; i++) {
		TreeArchetype a = make_tree_archetype(arch_rng);
		a.skeleton = make_tree_skeleton_mesh(a);
		archetypes.push_back(a);
	}
	// 树干按原型分桶：每个原型一份 MultiMesh（共用的 mesh 做不到逐实例换形状）
	std::vector<Scatter> trunks_by_arch(ARCH_N);

	// 树冠 + 树枝雪毯：按树顶高度分三档（T1 低 / T2 中 / T3 高），每档一套
	// 材质（veg_wind）。积雪分级：低档先覆雪先现雪毯、高档后积雪——映射区间
	// 逐档上移。树冠与雪毯同档同区间，覆雪与雪毯同步出现。
	// 雪毯网格比树冠更扁，叠在冠顶/冠肩上；材质与树冠同摆动参数（摆动相位
	// 取自世界坐标，天然同步），alpha 由所在档位驱动淡入，无雪完全隐形
	// （fallback 同理不可见，属降级路径）。
	const float band_rng[3][2] = { { 0.15f, 0.45f }, { 0.35f, 0.65f }, { 0.55f, 0.85f } };
	// 树冠改用"叶团"网格（噪声位移的球），一颗树由 7 个错落的叶团拼成。
	// 原来那颗压扁的光滑球就是用户说的"一坨"—— 一个球没有任何层次，
	// 加上噪声位移和多团叠放才有叶簇的体积感。
	Ref<Mesh> canopy_mesh_bands[3];
	Ref<SphereMesh> snowcap_mesh_bands[3];
	for (int b = 0; b < 3; b++) {
		float lo = band_rng[b][0];
		float hi = band_rng[b][1];

		// 三档用不同噪声种子：低树和高树的叶簇长相不同，成片时不至于像复制
		canopy_mesh_bands[b] = make_leaf_blob_mesh(1.0f, 0.86f, 0.34f, 1.30f, 6100 + b * 37, 19, 12);
		Ref<ShaderMaterial> cwm = wind_material(-0.80f, 1.60f, 0.05f); // 树冠随风轻摆
		if (cwm.is_valid()) {
			cwm->set_shader_parameter("snow_amount", 0.0f);
			// 叶团额外开"碎抖"：相位按世界坐标高频取，相邻叶团差好几弧度 →
			// 同一棵树上有的叶子在动、有的没动，而不是整坨树冠一起平移。
			cwm->set_shader_parameter("flutter_strength", 0.042f);
			cwm->set_shader_parameter("flutter_scale", 5.5f);
			// 叶团底面压暗一点 → 配合"越高的团越亮"做出整棵树上浅下深的层次。
			// 别压太狠：0.62 那版三处暗系数叠起来把树压成了墨绿疙瘩，0.74 刚好有层次又不发黑。
			cwm->set_shader_parameter("bottom_shade", 0.78f);
			// 叶片透光：太阳在叶子背面时透出来的暖光（背光面靠它提亮、脱离墨绿）
			cwm->set_shader_parameter("backlight", 0.50f);
			leaf_mats.push_back(cwm);
			canopy_mesh_bands[b]->surface_set_material(0, cwm);
			snow_bands.push_back({ cwm, lo, hi, false });
		} else {
			canopy_mesh_bands[b]->surface_set_material(0, fallback_wind_material());
		}

		snowcap_mesh_bands[b].instantiate();
		snowcap_mesh_bands[b]->set_radius(1.05f);
		snowcap_mesh_bands[b]->set_height(0.45f); // 压得更扁，当"雪毯"
		snowcap_mesh_bands[b]->set_radial_segments(10);
		snowcap_mesh_bands[b]->set_rings(4);
		Ref<ShaderMaterial> swm = wind_material(-0.75f, 1.5f, 0.05f);
		if (swm.is_valid()) {
			swm->set_shader_parameter("snow_amount", 0.0f);
			swm->set_shader_parameter("alpha", 0.0f);
			snowcap_mesh_bands[b]->set_material(swm);
			snow_bands.push_back({ swm, lo, hi, true });
		} else {
			Ref<StandardMaterial3D> fb;
			fb.instantiate();
			fb->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			fb->set_albedo(Color(0.93f, 0.95f, 0.99f, 0.0f)); // 加载失败：保持隐形
			fb->set_roughness(0.55f);
			snowcap_mesh_bands[b]->set_material(fb);
		}
	}

	Ref<SphereMesh> bush_mesh;
	bush_mesh.instantiate();
	bush_mesh->set_radius(0.5f);
	bush_mesh->set_height(0.72f);
	bush_mesh->set_radial_segments(8);
	bush_mesh->set_rings(4);
	Ref<ShaderMaterial> bush_wind = apply_wind(bush_mesh, -0.36f, 0.72f, 0.04f); // 灌木摆幅小一些
	if (bush_wind.is_valid()) {
		snow_bands.push_back({ bush_wind, 0.02f, 0.30f, false }); // 贴地，最先积雪
	}

	// 草丛网格：一丛 3 片叶（各自的 yaw/外倾/高不同），四边形双面叶片。
	// 基部在本地 y=0（放置时直接贴地），叶尖收成 15% 宽 —— 视觉密度×3 而实例数不变
	Ref<ArrayMesh> grass_mesh;
	{
		Ref<SurfaceTool> gst;
		gst.instantiate();
		gst->begin(Mesh::PRIMITIVE_TRIANGLES);
		struct Blade { float yaw, tilt, h, w; };
		// 柔软细密风：5 叶/簇（3 外倾 + 2 中立短叶更蓬松），叶宽 ~4.5cm、尖部 30% 宽（不扎眼）
		const Blade blades[5] = {
			{ 0.0f, 0.16f, 0.28f, 0.046f },
			{ 1.4f, 0.30f, 0.24f, 0.042f },
			{ 2.9f, -0.24f, 0.26f, 0.044f },
			{ 4.3f, 0.22f, 0.25f, 0.043f },
			{ 5.6f, 0.04f, 0.19f, 0.040f }, // 中立短叶填心
		};
		for (const Blade &b : blades) {
			float cy = std::cos(b.yaw), sy = std::sin(b.yaw);
			// 叶片局部：底边 (±w,0,0)，尖边 (±w·0.15, h, tilt·h)，绕 Y 转 yaw
			auto rot = [&](float x, float y, float z) -> Vector3 {
				return Vector3(x * cy + z * sy, y, -x * sy + z * cy);
			};
			Vector3 b0 = rot(-b.w, 0.0f, 0.0f), b1 = rot(b.w, 0.0f, 0.0f);
			Vector3 t0 = rot(-b.w * 0.30f, b.h, b.tilt * b.h), t1 = rot(b.w * 0.30f, b.h, b.tilt * b.h);
			// 正面 + 反面（双面，配合着色器 cull_disabled）；UV：根 v=1 → 梢 v=0
			const PackedVector2Array uv_b = { Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0) };
			gst->add_triangle_fan({ b0, b1, t1, t0 }, uv_b);
			gst->add_triangle_fan({ b0, t0, t1, b1 }, uv_b);
		}
		gst->generate_normals();
		grass_mesh.instantiate();
		gst->commit(grass_mesh);
	}
	Ref<ImageTexture> blade_tex = make_blade_texture(); // 草叶纹理（簇草/高草共用，6 根/张）
	Ref<ImageTexture> pitch_tex = make_blade_texture(12); // 球场专用：12 根/张（羊毛级细密）
	Ref<ShaderMaterial> grass_wind = apply_wind(grass_mesh, 0.0f, 0.28f, 0.04f); // 草梢摆动（细叶矮草，摆幅收窄）
	if (grass_wind.is_valid()) {
		trample_mats.push_back(grass_wind); // 登记：踩草倒伏的逐帧驱动目标
		snow_bands.push_back({ grass_wind, 0.02f, 0.30f, false }); // 贴地，最先积雪
		grass_wind->set_shader_parameter("blade_tex", blade_tex);
		grass_wind->set_shader_parameter("use_blade_tex", true);
		grass_wind->set_shader_parameter("normal_up", true);
	}

	// 草毯网格：单叶双面四边形（贴"一丛 6 根细草"纹理：宽 6cm 挺立少躺倒，高 0.34m）
	Ref<ArrayMesh> carpet_mesh;
	{
		Ref<SurfaceTool> cst;
		cst.instantiate();
		cst->begin(Mesh::PRIMITIVE_TRIANGLES);
		const float cw = 0.060f, ch = 0.34f;
		Vector3 b0(-cw, 0.0f, 0.0f), b1(cw, 0.0f, 0.0f);
		Vector3 t0(-cw * 0.45f, ch, 0.04f), t1(cw * 0.45f, ch, 0.04f); // 挺立微倾（原 0.09 太躺）
		const PackedVector2Array uv_f = { Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0) };
		cst->add_triangle_fan({ b0, b1, t1, t0 }, uv_f);
		cst->add_triangle_fan({ b0, t0, t1, b1 }, uv_f);
		cst->generate_normals();
		carpet_mesh.instantiate();
		cst->commit(carpet_mesh);
	}
	Ref<ShaderMaterial> carpet_wind = apply_wind(carpet_mesh, 0.0f, 0.34f, 0.035f); // 及膝草的舒展摆动
	if (carpet_wind.is_valid()) {
		trample_mats.push_back(carpet_wind);
		snow_bands.push_back({ carpet_wind, 0.02f, 0.30f, false });
		carpet_wind->set_shader_parameter("blade_tex", pitch_tex);
		carpet_wind->set_shader_parameter("use_blade_tex", true);
		carpet_wind->set_shader_parameter("normal_up", true);
	}

	Ref<SphereMesh> fruit_mesh;
	fruit_mesh.instantiate();
	fruit_mesh->set_radius(0.09f);
	fruit_mesh->set_height(0.13f);
	fruit_mesh->set_radial_segments(8);
	fruit_mesh->set_rings(4);
	fruit_mesh->set_material(veg_material(Color(0.10f, 0.03f, 0.02f))); // 微自发光，暗处也鲜亮

	Ref<SphereMesh> flower_mesh;
	flower_mesh.instantiate();
	flower_mesh->set_radius(0.05f);
	flower_mesh->set_height(0.08f);
	flower_mesh->set_radial_segments(6);
	flower_mesh->set_rings(3);
	flower_mesh->set_material(veg_material(Color(0.08f, 0.08f, 0.06f)));

	Ref<CylinderMesh> reed_mesh;
	reed_mesh.instantiate();
	reed_mesh->set_top_radius(0.012f);
	reed_mesh->set_bottom_radius(0.03f);
	reed_mesh->set_height(1.0f);
	reed_mesh->set_radial_segments(5);
	reed_mesh->set_rings(1);
	Ref<ShaderMaterial> reed_wind = apply_wind(reed_mesh, -0.5f, 1.0f, 0.06f); // 芦苇细长，摆幅最大
	if (reed_wind.is_valid()) {
		snow_bands.push_back({ reed_wind, 0.02f, 0.30f, false }); // 贴地，最先积雪
	}

	// 荷叶：带 V 形缺口的浅碗形圆盘（睡莲叶的标志性形状），双面薄片。
	// 材质走 lily.gdshader —— 顶点里按水面波高抬降，荷叶跟着湖面一起起伏、倾斜。
	Ref<ArrayMesh> lily_mesh;
	{
		Ref<SurfaceTool> lst;
		lst.instantiate();
		lst->begin(Mesh::PRIMITIVE_TRIANGLES);
		lst->set_smooth_group(-1);
		const int LSEG = 16;
		const float R_OUT = 0.55f;
		const float NOTCH = 0.62f; // 缺口张角（弧度）
		float la0 = NOTCH * 0.5f;
		float lspan = Math::TAU - NOTCH;
		Vector3 lc(0.0f, -0.022f, 0.0f); // 叶心略低 → 浅碗
		for (int i = 0; i < LSEG; i++) {
			float t0 = la0 + lspan * (float)i / (float)LSEG;
			float t1 = la0 + lspan * (float)(i + 1) / (float)LSEG;
			Vector3 p0(std::cos(t0) * R_OUT, 0.030f, std::sin(t0) * R_OUT); // 叶缘上翘
			Vector3 p1(std::cos(t1) * R_OUT, 0.030f, std::sin(t1) * R_OUT);
			lst->add_vertex(lc);
			lst->add_vertex(p0);
			lst->add_vertex(p1);
			lst->add_vertex(lc); // 反面（薄片，配合 cull_disabled）
			lst->add_vertex(p1);
			lst->add_vertex(p0);
		}
		lst->generate_normals();
		lily_mesh.instantiate();
		lst->commit(lily_mesh);
	}
	{
		Ref<Resource> res = ResourceLoader::get_singleton()->load("res://assets/lily.gdshader");
		Ref<Shader> sh = Ref<Shader>(Object::cast_to<Shader>(res.ptr()));
		if (sh.is_valid()) {
			Ref<ShaderMaterial> lm;
			lm.instantiate();
			lm->set_shader(sh);
			lily_mesh->surface_set_material(0, lm);
		} else {
			lily_mesh->surface_set_material(0, veg_material());
		}
	}

	// 荷花网格：外层粉白盛开花瓣 + 内层深粉微拢花瓣 + 金黄花蕊
	Ref<ArrayMesh> lotus_mesh;
	{
		Ref<SurfaceTool> lst;
		lst.instantiate();
		lst->begin(Mesh::PRIMITIVE_TRIANGLES);
		// 外层 8 片花瓣（微微展开，粉白晕染）
		for (int i = 0; i < 8; i++) {
			float a = (float)i * (6.283185f / 8.0f);
			float ca = std::cos(a), sa = std::sin(a);
			float pa = a + 3.14159f * 0.5f;
			float cpa = std::cos(pa), spa = std::sin(pa);
			float w = 0.08f;
			Vector3 b0(ca * 0.04f - cpa * w, 0.01f, sa * 0.04f - spa * w);
			Vector3 b1(ca * 0.04f + cpa * w, 0.01f, sa * 0.04f + spa * w);
			Vector3 tip(ca * 0.24f, 0.09f, sa * 0.24f);
			Vector3 mid(ca * 0.15f, 0.07f, sa * 0.15f);
			Color c_base(0.98f, 0.90f, 0.95f);
			Color c_mid(0.96f, 0.62f, 0.78f);
			Color c_tip(0.92f, 0.35f, 0.60f);

			lst->set_color(c_base); lst->add_vertex(b0);
			lst->set_color(c_base); lst->add_vertex(b1);
			lst->set_color(c_mid);  lst->add_vertex(mid);

			lst->set_color(c_base); lst->add_vertex(b0);
			lst->set_color(c_mid);  lst->add_vertex(mid);
			lst->set_color(c_tip);  lst->add_vertex(tip);

			lst->set_color(c_base); lst->add_vertex(b1);
			lst->set_color(c_tip);  lst->add_vertex(tip);
			lst->set_color(c_mid);  lst->add_vertex(mid);

			// 背面双面渲染
			lst->set_color(c_base); lst->add_vertex(b1);
			lst->set_color(c_base); lst->add_vertex(b0);
			lst->set_color(c_mid);  lst->add_vertex(mid);

			lst->set_color(c_tip);  lst->add_vertex(tip);
			lst->set_color(c_mid);  lst->add_vertex(mid);
			lst->set_color(c_base); lst->add_vertex(b0);

			lst->set_color(c_mid);  lst->add_vertex(mid);
			lst->set_color(c_tip);  lst->add_vertex(tip);
			lst->set_color(c_base); lst->add_vertex(b1);
		}
		// 内层 6 片花瓣（向上收拢，颜色更深粉）
		for (int i = 0; i < 6; i++) {
			float a = (float)i * (6.283185f / 6.0f) + 0.35f;
			float ca = std::cos(a), sa = std::sin(a);
			float pa = a + 3.14159f * 0.5f;
			float cpa = std::cos(pa), spa = std::sin(pa);
			float w = 0.06f;
			Vector3 b0(ca * 0.03f - cpa * w, 0.02f, sa * 0.03f - spa * w);
			Vector3 b1(ca * 0.03f + cpa * w, 0.02f, sa * 0.03f + spa * w);
			Vector3 tip(ca * 0.13f, 0.18f, sa * 0.13f);
			Color c_base(0.98f, 0.85f, 0.92f);
			Color c_tip(0.94f, 0.28f, 0.55f);

			lst->set_color(c_base); lst->add_vertex(b0);
			lst->set_color(c_base); lst->add_vertex(b1);
			lst->set_color(c_tip);  lst->add_vertex(tip);

			lst->set_color(c_base); lst->add_vertex(b1);
			lst->set_color(c_base); lst->add_vertex(b0);
			lst->set_color(c_tip);  lst->add_vertex(tip);
		}
		// 金黄莲蓬与花蕊
		for (int i = 0; i < 8; i++) {
			float a0 = (float)i * (6.283185f / 8.0f);
			float a1 = (float)(i + 1) * (6.283185f / 8.0f);
			float r = 0.05f;
			Vector3 v0(0.0f, 0.055f, 0.0f);
			Vector3 v1(std::cos(a0) * r, 0.045f, std::sin(a0) * r);
			Vector3 v2(std::cos(a1) * r, 0.045f, std::sin(a1) * r);
			Color c_gold(0.98f, 0.84f, 0.18f);
			lst->set_color(c_gold); lst->add_vertex(v0);
			lst->set_color(c_gold); lst->add_vertex(v1);
			lst->set_color(c_gold); lst->add_vertex(v2);
		}
		lst->generate_normals();
		lotus_mesh.instantiate();
		lst->commit(lotus_mesh);
		// 荷花也要随浪起伏 —— 原来用的是静态 veg_material，水在动它钉死在水面上，
		// 一眼就看出是"插"上去的。换成和荷叶同一个随波着色器：
		// 花瓣顶点各自采世界坐标处的波高，花会跟着波面轻轻点头、侧倾。
		// bob_extra 比荷叶再大一点（荷花立得高，晃起来更明显），自发光保留原来的暖粉光。
		Ref<ShaderMaterial> lot_mat = water_bob_material(0.065f, Vector3(0.12f, 0.05f, 0.08f));
		if (lot_mat.is_valid()) {
			lotus_mesh->surface_set_material(0, lot_mat);
		} else {
			lotus_mesh->surface_set_material(0, veg_material(Color(0.12f, 0.05f, 0.08f)));
		}
	}

	// 陆地巨石 / 碎石坡：换成和湖岸礁石同一套"噪声棱面网格 + 岩石材质"。
	// 原来是个光滑球 + 平涂色 —— 就是用户说的"石头质感太垃圾"。
	// 半径/压扁比例沿用旧的 SphereMesh(r0.4 / h0.5 → 竖向半高 0.25)，外观尺度不变。
	Ref<ArrayMesh> rock_mesh = make_rock_mesh(11, 6, 0.40f, 0.64f, 0.30f, 2.0f, 771);
	rock_mesh->surface_set_material(0, rock_material());

	// 鹅卵石：扁圆小石，铺在水线沙滩上
	Ref<SphereMesh> pebble_mesh;
	pebble_mesh.instantiate();
	pebble_mesh->set_radius(0.10f);
	pebble_mesh->set_height(0.07f);
	pebble_mesh->set_radial_segments(6);
	pebble_mesh->set_rings(2);
	pebble_mesh->set_material(veg_material());

	// 落叶：贴地的小圆叶
	Ref<CylinderMesh> leaf_mesh;
	leaf_mesh.instantiate();
	leaf_mesh->set_top_radius(0.055f);
	leaf_mesh->set_bottom_radius(0.055f);
	leaf_mesh->set_height(0.012f);
	leaf_mesh->set_radial_segments(5);
	leaf_mesh->set_rings(1);
	leaf_mesh->set_material(veg_material());

	// 倒木：横卧的枯树干（比树干粗些、更灰败；少量放置）
	Ref<CylinderMesh> log_mesh;
	log_mesh.instantiate();
	log_mesh->set_top_radius(0.17f);
	log_mesh->set_bottom_radius(0.22f);
	log_mesh->set_height(3.0f);
	log_mesh->set_radial_segments(7);
	log_mesh->set_rings(1);
	log_mesh->set_material(veg_material());

	// 蘑菇：柄（奶白细柱）、伞盖（压扁球）、红伞上的白点（MC 观感）
	Ref<CylinderMesh> shroom_stem_mesh;
	shroom_stem_mesh.instantiate();
	shroom_stem_mesh->set_top_radius(0.022f);
	shroom_stem_mesh->set_bottom_radius(0.032f);
	shroom_stem_mesh->set_height(0.14f);
	shroom_stem_mesh->set_radial_segments(6);
	shroom_stem_mesh->set_rings(1);
	shroom_stem_mesh->set_material(veg_material());

	Ref<SphereMesh> shroom_cap_mesh;
	shroom_cap_mesh.instantiate();
	shroom_cap_mesh->set_radius(0.10f);
	shroom_cap_mesh->set_height(0.09f); // 压扁的半球当伞盖
	shroom_cap_mesh->set_radial_segments(8);
	shroom_cap_mesh->set_rings(4);
	// 伞盖专用材质：带自发光（能量默认 0，夜里由 update_glow 拉起来泛暖橘光）。
	// 自发光颜色与实例色无关（实例色只乘 albedo），红伞/棕伞夜里泛同一种暖光。
	shroom_cap_mat.instantiate();
	shroom_cap_mat->set_albedo(Color(1.0f, 1.0f, 1.0f));
	shroom_cap_mat->set_roughness(1.0f);
	shroom_cap_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	shroom_cap_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	shroom_cap_mat->set_emission(Color(0.72f, 0.20f, 0.10f));
	shroom_cap_mat->set_emission_energy_multiplier(0.0f);
	shroom_cap_mesh->set_material(shroom_cap_mat);

	Ref<SphereMesh> shroom_spot_mesh;
	shroom_spot_mesh.instantiate();
	shroom_spot_mesh->set_radius(0.018f);
	shroom_spot_mesh->set_height(0.018f);
	shroom_spot_mesh->set_radial_segments(5);
	shroom_spot_mesh->set_rings(2);
	// 白点专用材质：夜里泛冷白微光，与伞盖的暖红微光形成层次，层次分明不刺眼
	shroom_spot_mat.instantiate();
	shroom_spot_mat->set_albedo(Color(1.0f, 1.0f, 1.0f));
	shroom_spot_mat->set_roughness(1.0f);
	shroom_spot_mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	shroom_spot_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	shroom_spot_mat->set_emission(Color(0.55f, 0.60f, 0.52f));
	shroom_spot_mat->set_emission_energy_multiplier(0.0f);
	shroom_spot_mesh->set_material(shroom_spot_mat);

	// 蘑菇伞顶雪毯：小扁白球扣在伞盖上（红伞/棕伞都盖）。蘑菇低矮贴地 →
	// 积雪档位最靠前（0.08~0.35）。材质仍走 veg_wind 但摆幅为 0（蘑菇僵硬
	// 不随风摆，只借它的 alpha 通道做雪量淡入）。
	Ref<SphereMesh> shroom_snow_mesh;
	shroom_snow_mesh.instantiate();
	shroom_snow_mesh->set_radius(0.075f); // 略小于伞盖半径 0.10，雪檐收在伞面内
	shroom_snow_mesh->set_height(0.055f);
	shroom_snow_mesh->set_radial_segments(7);
	shroom_snow_mesh->set_rings(3);
	Ref<ShaderMaterial> shroom_snow_mat = wind_material(0.0f, 0.2f, 0.0f);
	if (shroom_snow_mat.is_valid()) {
		shroom_snow_mat->set_shader_parameter("snow_amount", 0.0f);
		shroom_snow_mat->set_shader_parameter("alpha", 0.0f);
		shroom_snow_mesh->set_material(shroom_snow_mat);
		snow_bands.push_back({ shroom_snow_mat, 0.08f, 0.35f, true });
	} else {
		Ref<StandardMaterial3D> fb;
		fb.instantiate();
		fb->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		fb->set_albedo(Color(0.95f, 0.96f, 1.0f, 0.0f)); // 加载失败：保持隐形
		fb->set_roughness(0.55f);
		shroom_snow_mesh->set_material(fb);
	}

	// —— 实例收集器 ——
	Scatter fruits, bushes, grass, reeds, lilies, lotuses, flowers, rocks;
	Scatter carpet; // 草毯层：单叶小刀片百万级平铺（足球场式连续地毯）
	Scatter berry_bushes, berries; // 浆果丛（MC 甜浆果丛风格）：深绿矮丛 + 鲜红浆果
	Scatter pebbles, leaves;       // 湖岸鹅卵石带 + 树下落叶
	Scatter scree;                 // 岩壁下的碎石坡（山体裸岩地貌）
	Scatter logs;                  // 林间平缓处的横卧倒木
	Scatter shroom_stems, shroom_caps, shroom_spots; // 蘑菇：柄 + 伞盖 + 红伞白点
	Scatter shroom_snow;           // 蘑菇伞顶雪毯（低矮最先积雪）
	// 树冠与树枝雪毯按树顶高度分三档：T1 低 / T2 中 / T3 高（积雪分档，低树先积雪）。
	// 树冠再按"树形原型"分桶 —— 每个原型一份 MultiMesh，见 trunks_by_arch 的说明。
	Scatter canopy_lo[ARCH_N][3];
	Scatter snowcap_bands[3];

	// 石桥桥面矩形（含桥台外扩一点）：压平后的引道与桥面同高，
	// 植被/石头若落在这里会从石板里钻出来，一律排除
	auto on_bridge_deck = [&](const Vector2 &p) -> bool {
		return p.x > bridge_end_west - 0.6f && p.x < bridge_end_east + 0.6f
				&& Math::abs(p.y - lake_center.y) < 2.6f;
	};

	auto tree_color = [&rnd]() -> Color {
		// 逐树的底色。三个坑都踩过，记一下：
		//  1) 太暗 → 整片林子发闷（红 0.22~0.44、绿 0.42~0.66 那版）；
		//  2) **蓝通道不能给高**。树冠大面积是背光面，靠蓝色天光照亮，
		//     蓝给到 0.3 以上，出来的像素蓝通道就跟绿通道齐平 —— 那就是"墨绿"。
		//  3) 底色还得够亮。树冠是一堆球面，单位面积的受光本来就比平铺的草地少
		//     （实测大约只有草地的一半），底色不抬到接近上限就永远显得比草地暗一截。
		// 结论：绿 0.77~0.95、红 0.54~0.74（偏黄绿）、蓝 0.32~0.44（压住不偏青）。
		// 绿比红高、蓝压到红的六成左右 —— 实测这条比例出来的像素和草地同一族色相
		// （红/绿≈0.74、蓝/绿≈0.69），而不是"灰绿/鼠尾草色"。
		return Color(0.54f + rnd(0.0f, 0.20f), 0.77f + rnd(0.0f, 0.18f), 0.32f + rnd(0.0f, 0.12f));
	};

	std::vector<Vector2> tree_pos; // 已种的树，用于最小间距判定
	auto can_place_tree = [&](const Vector2 &p) -> bool {
		if (Math::abs(p.x) > inner || Math::abs(p.y) > inner) {
			return false;
		}
		if ((p - spawn_point).length() < 5.0f) {
			return false;
		}
		if (in_pitch(p.x, p.y)) { // 球场里不许种树
			return false;
		}
		if (dist_to_path(p.x, p.y) < 2.2f) { // 别把树种在小径上
			return false;
		}
		if (on_bridge_deck(p)) { // 别种在桥上
			return false;
		}
		if ((p - homestead_center).length() < 14.0f) { // 宅基地周围不留树
			return false;
		}
		if (is_near_lake(p.x, p.y, 3.5f)) { // 避开复合大湖水域与湖滩
			return false;
		}
		float h = terrain_height(p.x, p.y);
		if (h < water_level + 0.6f || h > 4.0f) { // 不种水里，也不种环山陡坡上
			return false;
		}
		if (terrain_slope(p.x, p.y) > 0.55f) {
			return false;
		}
		for (const Vector2 &t : tree_pos) {
			if ((p - t).length_squared() < 5.3f) { // 树间距 ≥ ~2.3 m
				return false;
			}
		}
		return true;
	};

	auto plant_tree = [&](const Vector2 &p) {
		float h = terrain_height(p.x, p.y);
		float s = rnd(0.8f, 1.45f);
		float yaw = rnd(0.0f, 6.2831f);
		float tilt = rnd(-0.05f, 0.05f);
		Color canopy = tree_color();

		// 随机挑一套树形（用独立的 blob_rnd，不动全局序列）。
		// 这一步是"树不要长得一模一样"的关键：叶团个数、树冠宽窄、枝条走向都由原型决定。
		int arch = (int)blob_rnd(0.0f, (float)ARCH_N - 0.001f);
		arch = Math::clamp(arch, 0, ARCH_N - 1);
		const TreeArchetype &ta = archetypes[arch];

		// 积雪分档：以这棵树的实际冠顶高（h + top_y·s）分三档 —— 低树先积雪、高树后积雪。
		// 档位由已有参数确定性导出，不调 rnd() —— 不挪动全局随机序列。
		int band = (h + ta.top_y * s) < 5.35f ? 0 : ((h + ta.top_y * s) < 7.35f ? 1 : 2);

		// 树干骨架：原点就在树根（骨架网格自带 -0.10~2.30 的竖向范围），所以 y 直接用 h。
		trunks_by_arch[arch].add(Vector3(p.x, h, p.y), yaw, tilt, s,
				Color(0.29f + rnd(0.0f, 0.05f), 0.195f, 0.13f));
		// 树干碰撞（穿树出戏；半径取平均×0.88，略细于视觉防蹭人）
		StaticBody3D *tb = memnew(StaticBody3D);
		tb->set_position(Vector3(p.x, h + 1.45f * s, p.y));
		CollisionShape3D *tcs = memnew(CollisionShape3D);
		Ref<CylinderShape3D> tcy;
		tcy.instantiate();
		tcy->set_radius(0.17f * s);
		tcy->set_height(3.0f * s);
		tcs->set_shape(tcy);
		tb->add_child(tcs);
		add_child(tb);

		// —— 树冠：按原型铺叶团 ——
		// 每团的位置/大小都带小幅抖动（同一原型的树也不完全重合），但幅度压得很小：
		// 枝条是全原型共用的固定网格，抖大了枝梢就跑到叶团外面去了。
		// 颜色有两层变化：整棵树的底色（tree_color 逐树随机）+
		// 每团自己的明暗与冷暖（shade / warm 逐团随机）——
		// 同一棵树上有深绿团也有黄绿团，这才是"叶子有深有浅"。
		for (const TreeBlob &bl : ta.blobs) {
			float a = yaw + bl.ang + blob_rnd(-0.15f, 0.15f);
			float rr = bl.rad * s * blob_rnd(0.94f, 1.06f);
			float yy = bl.y * s * blob_rnd(0.96f, 1.04f);
			float sc = bl.scale * s * blob_rnd(0.90f, 1.10f);
			float warm = blob_rnd(-0.07f, 0.07f); // 偏黄 / 偏青（小幅度，只做微调）
			// —— 上浅下深：天光自上而下，冠顶的团在光里、底圈和内侧的团压在暗处 ——
			// 这条渐变必须**压过**逐团的随机明暗。之前随机明暗给到 0.74~1.26，
			// 比高度项还强，结果一棵树上"哪团亮"是随机的，上下层次全被冲掉了
			// （用户要的正是"靠上的浅、靠下的深"）。
			// 现在：高度决定 0.80~1.22 的主基调，随机只留 ±7% 打散呆板。
			// **下限不能压太低**：这一项还要再乘顶点色和着色器的底暗系数，
			// 三个下限叠起来（0.64×0.66×0.62≈0.26）会糊出一大片发黑，
			// 树就成了墨绿疙瘩。抬到 0.80 之后最暗处也还有 0.45 的亮度。
			float t = clampf((bl.y - 2.30f) / 1.40f, 0.0f, 1.0f);
			float m = (0.86f + 0.36f * t) * blob_rnd(0.94f, 1.06f);
			// 色相也跟着高度走：越往上越偏黄绿（受光），越往下越偏青绿（背光）。
			// 只调亮度不做色相分离的话，看着还是"同一块绿调了曝光"。
			Color bc(Math::clamp(canopy.r * m + warm + 0.07f * t, 0.0f, 1.0f),
					Math::clamp(canopy.g * m, 0.0f, 1.0f),
					Math::clamp(canopy.b * m - warm * 0.7f - 0.05f * t, 0.0f, 1.0f));
			canopy_lo[arch][band].add(
					Vector3(p.x + std::cos(a) * rr, h + yy, p.y + std::sin(a) * rr),
					blob_rnd(0.0f, 6.2831f), blob_rnd(-0.22f, 0.22f), sc, bc);
		}

		// 雪毯两片：冠顶一顶（压在顶团上）+ 冠肩一圈（搭在中圈肩部）。
		// 高度与半径都从原型的冠顶/冠肩/横向半径推出来 —— 树形一变雪毯自动跟着走。
		snowcap_bands[band].add(Vector3(p.x, h + (ta.top_y - 0.30f) * s, p.y), yaw * 0.5f, tilt,
				ta.crown_r * 0.40f * s, Color(1.0f, 1.0f, 1.0f));
		snowcap_bands[band].add(Vector3(p.x, h + ta.shoulder_y * s, p.y), yaw * 0.5f, tilt,
				ta.crown_r * 0.58f * s, Color(0.96f, 0.97f, 1.0f));

		tree_pos.push_back(p);

		// 约四成的树结果子：红 / 橙 / 黄小果挂在树冠外缘。
		// 半径 / 高度都按原型反推（crown_r 的 0.72~1.00 倍、冠底到冠顶之间），
		// 这样树冠形状变了果子还在叶子上，不会飘在空中或埋进叶团里。
		if (rnd(0.0f, 1.0f) < 0.42f) {
			int n_fruit = 2 + (int)rnd(0.0f, 2.9f);
			float fr_lo = ta.crown_r * 0.72f;
			float fr_hi = ta.crown_r * 1.00f;
			float fy_lo = ta.top_y * 0.56f;
			float fy_hi = ta.top_y * 0.88f;
			for (int k = 0; k < n_fruit; k++) {
				float a = rnd(0.0f, 6.2831f);
				float rad = rnd(fr_lo, fr_hi) * s;
				float pick = rnd(0.0f, 3.0f);
				Color fc;
				if (pick < 1.0f) {
					fc = Color(0.88f, 0.22f, 0.18f); // 红
				} else if (pick < 2.0f) {
					fc = Color(0.95f, 0.55f, 0.15f); // 橙
				} else {
					fc = Color(0.95f, 0.84f, 0.25f); // 黄
				}
				fruits.add(Vector3(p.x + std::cos(a) * rad, h + rnd(fy_lo, fy_hi) * s, p.y + std::sin(a) * rad),
						0.0f, 0.0f, rnd(0.8f, 1.25f), fc);
			}
		}
	};

	// —— 树林：7×6 抖动网格的林心（成片但不呆板），加 40 棵独树 ——
	for (int gx = 0; gx < 9; gx++) {
		for (int gz = 0; gz < 8; gz++) {
			Vector2 center(-105.0f + gx * 35.0f + rnd(-8.0f, 8.0f), -92.5f + gz * 37.0f + rnd(-8.0f, 8.0f));
			if (is_near_lake(center.x, center.y, 7.0f) || (center - spawn_point).length() < 16.0f) {
				continue; // 离复合湖 / 出生点太近的林心跳过
			}
			int before = (int)tree_pos.size();
			for (int attempt = 0; attempt < 60 && (int)tree_pos.size() - before < 8; attempt++) {
				float a = rnd(0.0f, 6.2831f);
				float r = rnd(0.0f, 7.5f);
				Vector2 cand = center + Vector2(std::cos(a), std::sin(a)) * r;
				if (can_place_tree(cand)) {
					plant_tree(cand);
				}
			}
		}
	}
	int loners = 0;
	for (int attempt = 0; attempt < 2100 && loners < 70; attempt++) {
		Vector2 cand(rnd(-inner, inner), rnd(-inner, inner));
		if (can_place_tree(cand)) {
			plant_tree(cand);
			loners++;
		}
	}

	// —— 灌木：湖岸环 + 草地随机 ——
	for (int i = 0; i < 780; i++) {
		Vector2 p;
		if (i < 180) { // 前 180 次围着壮阔大湖岸放
			float a = rnd(0.0f, 6.2831f);
			float lobe_pick = rnd(0.0f, 4.0f);
			if (lobe_pick < 1.4f) {
				p = Vector2(-14.0f, 66.0f) + Vector2(std::cos(a), std::sin(a)) * 44.0f * rnd(1.03f, 1.30f);
			} else if (lobe_pick < 2.4f) {
				p = Vector2(16.0f, 56.0f) + Vector2(std::cos(a), std::sin(a)) * 32.0f * rnd(1.03f, 1.30f);
			} else if (lobe_pick < 3.2f) {
				p = Vector2(-15.0f, 8.0f) + Vector2(std::cos(a), std::sin(a)) * 22.0f * rnd(1.03f, 1.30f);
			} else {
				p = Vector2(-6.0f + std::cos(a) * 13.0f * rnd(1.03f, 1.30f), 26.0f + std::sin(a) * 18.0f * rnd(1.03f, 1.30f));
			}
		} else {
			p = Vector2(rnd(-inner, inner), rnd(-inner, inner));
		}
		if (Math::abs(p.x) > inner || Math::abs(p.y) > inner) {
			continue;
		}
		if ((p - spawn_point).length() < 3.0f) {
			continue;
		}
		if (in_pitch(p.x, p.y)) { // 球场里不长灌木
			continue;
		}
		if ((p - homestead_center).length() < 10.0f) { // 别长进院子
			continue;
		}
		if (dist_to_path(p.x, p.y) < 1.3f || on_bridge_deck(p)) { // 别挡在小径/桥上
			continue;
		}
		float h = terrain_height(p.x, p.y);
		if (h < water_level + 0.2f || terrain_slope(p.x, p.y) > 0.75f) {
			continue;
		}
		float s = rnd(0.6f, 1.5f);
		Color c(0.22f + rnd(0.0f, 0.10f), 0.36f + rnd(0.0f, 0.12f), 0.17f + rnd(0.0f, 0.07f));
		bushes.add(Vector3(p.x, h + 0.28f * s, p.y), rnd(0.0f, 6.2831f), rnd(-0.08f, 0.08f), s, c);
		bush_obstacles.push_back({ Vector3(p.x, h, p.y), 0.52f * s, 0.72f * s });
	}

	// —— 浆果丛：林缘成簇（MC 甜浆果丛风味）——
	// 深绿矮丛，浆果点缀在丛体表面偏上的位置，颜色鲜红带微自发光。
	int berry_clusters = 0;
	for (int attempt = 0; attempt < 2000 && berry_clusters < 58; attempt++) {
		if (tree_pos.empty()) {
			break;
		}
		// 从已种的树里随机挑一棵，在树冠边缘外 2~3.5m 处落一个簇心
		int ti = (int)rnd(0.0f, (float)tree_pos.size() - 0.001f);
		float ca = rnd(0.0f, 6.2831f);
		Vector2 base = tree_pos[ti] + Vector2(std::cos(ca), std::sin(ca)) * rnd(2.0f, 3.5f);
		if (in_pitch(base.x, base.y)) { // 球场里不长浆果丛
			continue;
		}

		auto berry_spot_ok = [&](const Vector2 &p) -> bool {
			if ((p - homestead_center).length() < 10.0f) {
				return false;
			}
		if (dist_to_path(p.x, p.y) < 1.4f || on_bridge_deck(p)) {
			return false;
		}
			float h = terrain_height(p.x, p.y);
			if (h < water_level + 0.3f || terrain_slope(p.x, p.y) > 0.7f) {
				return false;
			}
			return true;
		};
		if (!berry_spot_ok(base)) {
			continue;
		}

		int n_bush = 3 + (int)rnd(0.0f, 2.9f);
		for (int k = 0; k < n_bush; k++) {
			Vector2 p = base + Vector2(rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f)) * rnd(0.0f, 1.4f);
			if (!berry_spot_ok(p)) {
				continue;
			}
			float h = terrain_height(p.x, p.y);
			float s = rnd(0.75f, 1.15f);
			Color bc(0.16f + rnd(0.0f, 0.06f), 0.30f + rnd(0.0f, 0.08f), 0.14f + rnd(0.0f, 0.04f));
			berry_bushes.add(Vector3(p.x, h + 0.30f * s, p.y), rnd(0.0f, 6.2831f), rnd(-0.06f, 0.06f), s, bc);
			bush_obstacles.push_back({ Vector3(p.x, h, p.y), 0.52f * s, 0.72f * s });

			// 每丛 5~9 颗浆果，贴着丛体上半表面
			int n_berry = 5 + (int)rnd(0.0f, 4.9f);
			for (int b = 0; b < n_berry; b++) {
				float a = rnd(0.0f, 6.2831f);
				float rr = rnd(0.28f, 0.46f) * s;
				float yy = h + rnd(0.15f, 0.52f) * s;
				Color rc(0.85f + rnd(-0.07f, 0.0f), 0.13f + rnd(0.0f, 0.06f), 0.10f + rnd(0.0f, 0.03f));
				berries.add(Vector3(p.x + std::cos(a) * rr, yy, p.y + std::sin(a) * rr),
						0.0f, 0.0f, rnd(0.5f, 0.75f), rc);
			}
		}
		berry_clusters++;
	}

	// —— 足球场草坪：全图草毯已删，草只长在球场里（深色羊毛级 + 剪草条纹） ——
	// 独立随机种子：改球场不影响其他散布的随机序列
	std::mt19937 pitch_rng(771002);
	auto prnd = [&pitch_rng](float lo, float hi) {
		std::uniform_real_distribution<float> d(lo, hi);
		return d(pitch_rng);
	};
	for (int attempt = 0; attempt < 1100000 && carpet.count() < pitch_blades; attempt++) {
		float px = prnd(-pitch_half_w, pitch_half_w);
		float pz = prnd(-pitch_half_d, pitch_half_d);
		Vector2 p = pitch_center + Vector2(px, pz);
		float h = terrain_height(p.x, p.y);
		// 剪草条纹：每 3m 深浅交替
		float stripe = std::fmod(std::floor((px + pitch_half_w) / 3.0f), 2.0f);
		Color c = stripe < 1.0f ? Color(0.10f, 0.30f, 0.07f) : Color(0.14f, 0.36f, 0.09f);
		c = Color(c.r + prnd(-0.015f, 0.015f), c.g + prnd(-0.02f, 0.02f), c.b + prnd(-0.01f, 0.01f));
		carpet.add(Vector3(p.x, h - 0.01f, p.y), prnd(0.0f, 6.2831f), prnd(-0.12f, 0.12f),
				prnd(0.85f, 1.10f), c);
	}



	// —— 簇草层（点缀）：5 叶簇当野趣纹理，保留聚簇/裸土与枯草黄 ——
	// 聚簇分布：用与地形裸土同源的密度图（value_noise 0.14 频率、偏移 21/11）
	// 做门控 —— 噪声低的片区大量跳过，草丛便成片生长，与裸土斑块严格对齐。
	for (int attempt = 0; attempt < 2000000; attempt++) {
		Vector2 p(rnd(-inner, inner), rnd(-inner, inner));
		if (in_pitch(p.x, p.y)) { // 球场里不长簇草
			continue;
		}
		float h = terrain_height(p.x, p.y);
		if (h < water_level + 0.18f || terrain_slope(p.x, p.y) > 1.1f) {
			continue;
		}
		float clump = value_noise(p.x * 0.14f + 21.0f, p.y * 0.14f + 11.0f);
		if (clump + rnd(-0.14f, 0.14f) < 0.26f) { // 稀草/裸土片区（阈值放宽 → 覆盖更密）
			continue;
		}
		if ((p - homestead_center).length() < 8.5f) { // 屋内院子里不钻草
			continue;
		}
		if (dist_to_path(p.x, p.y) < 1.0f || on_bridge_deck(p)) { // 小径与桥面上不长草
			continue;
		}
		// 草甸分区：低频噪声划出"修剪草坪(矮) vs 野草疯长(高)"的片区性格
		float meadow = value_noise(p.x * 0.045f + 3.3f, p.y * 0.045f - 8.8f);
		float s = rnd(0.6f, 1.3f) * (0.75f + meadow * 0.75f);
		// MC 风格：草色按噪声"一坡一色"分片，同片区颜色接近，片间渐变
		float gm = value_noise(p.x * 0.33f + 7.7f, p.y * 0.33f + 3.1f);
		gm = clampf(gm + rnd(-0.09f, 0.09f), 0.0f, 1.0f); // 片内轻微抖动
		Color c(lerpf(0.16f, 0.26f, gm), lerpf(0.40f, 0.50f, gm), lerpf(0.10f, 0.14f, gm));
		if (rnd(0.0f, 1.0f) < 0.08f) { // 8% 枯草黄点缀（草甸野趣）
			c = Color(0.44f + rnd(0.0f, 0.05f), 0.39f, 0.17f);
		}
		grass.add(Vector3(p.x, h - 0.01f, p.y), rnd(0.0f, 6.2831f), rnd(-0.15f, 0.15f), s, c);
		if (grass.count() >= 100000) { // 点缀密度（草毯已铺满，簇草只负责野趣）
			break;
		}
	}

	// —— 芦苇：浅水带里探出头（沿大湖四段湖岸浅滩分布） ——
	for (int attempt = 0; attempt < 3000 && reeds.count() < 420; attempt++) {
		float a = rnd(0.0f, 6.2831f);
		float lobe_pick = rnd(0.0f, 4.0f);
		Vector2 p;
		if (lobe_pick < 1.4f) {
			p = Vector2(-14.0f, 66.0f) + Vector2(std::cos(a), std::sin(a)) * 44.0f * rnd(0.88f, 1.04f);
		} else if (lobe_pick < 2.4f) {
			p = Vector2(16.0f, 56.0f) + Vector2(std::cos(a), std::sin(a)) * 32.0f * rnd(0.88f, 1.04f);
		} else if (lobe_pick < 3.2f) {
			p = Vector2(-15.0f, 8.0f) + Vector2(std::cos(a), std::sin(a)) * 22.0f * rnd(0.88f, 1.04f);
		} else {
			p = Vector2(-6.0f + std::cos(a) * 13.0f * rnd(0.85f, 1.04f), 26.0f + std::sin(a) * 18.0f * rnd(0.85f, 1.04f));
		}
		if (on_bridge_deck(p)) {
			continue;
		}
		float h = terrain_height(p.x, p.y);
		if (h < -1.45f || h > water_level - 0.02f) {
			continue;
		}
		float s = rnd(0.7f, 1.3f);
		Color c(0.34f + rnd(0.0f, 0.10f), 0.42f + rnd(0.0f, 0.10f), 0.20f);
		reeds.add(Vector3(p.x, h + 0.42f * s, p.y), rnd(0.0f, 6.2831f), rnd(-0.06f, 0.06f), s, c);
	}

	// —— 高草丛：两片"快没过头顶"的野草地（1.4~1.75m，每片约两座房子占地） ——
	// 独立 rng（固定种子）——不挪动其它 Scatter 的随机序列；位置硬编码避开水/径/桥/宅。
	Ref<ArrayMesh> tall_mesh;
	{
		Ref<SurfaceTool> tst;
		tst.instantiate();
		tst->begin(Mesh::PRIMITIVE_TRIANGLES);
		struct TB { float yaw, tilt, h, w; };
		const TB tbs[4] = {
			{ 0.0f, 0.14f, 1.75f, 0.042f },
			{ 1.9f, 0.30f, 1.50f, 0.038f },
			{ 3.6f, -0.24f, 1.62f, 0.040f },
			{ 5.2f, 0.22f, 1.40f, 0.036f },
		};
		for (const TB &b : tbs) {
			float cy = std::cos(b.yaw), sy = std::sin(b.yaw);
			auto rot = [&](float x, float y, float z) -> Vector3 {
				return Vector3(x * cy + z * sy, y, -x * sy + z * cy);
			};
			Vector3 b0 = rot(-b.w, 0.0f, 0.0f), b1 = rot(b.w, 0.0f, 0.0f);
			Vector3 t0 = rot(-b.w * 0.15f, b.h, b.tilt * b.h), t1 = rot(b.w * 0.15f, b.h, b.tilt * b.h);
			const PackedVector2Array uv_t = { Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0) };
			tst->add_triangle_fan({ b0, b1, t1, t0 }, uv_t);
			tst->add_triangle_fan({ b0, t0, t1, b1 }, uv_t);
		}
		tst->generate_normals();
		tall_mesh.instantiate();
		tst->commit(tall_mesh);
	}
	Ref<ShaderMaterial> tall_wind = apply_wind(tall_mesh, 0.0f, 1.75f, 0.09f); // 高草大幅摆动
	if (tall_wind.is_valid()) {
		trample_mats.push_back(tall_wind);                        // 人/动物走过压弯
		snow_bands.push_back({ tall_wind, 0.02f, 0.30f, false }); // 与贴地植被同档积雪
		tall_wind->set_shader_parameter("blade_tex", blade_tex);
		tall_wind->set_shader_parameter("use_blade_tex", true);
		tall_wind->set_shader_parameter("normal_up", true);
	}
	Scatter tall;
	{
		std::mt19937 rng_t(770311);
		auto rnd_t = [&rng_t](float lo, float hi) -> float {
			std::uniform_real_distribution<float> d(lo, hi);
			return d(rng_t);
		};
		// 两片：出生点西侧野地 / 宅基地东侧草场（均离小径/桥/宅足够远）
		const Vector2 patches[2] = { Vector2(-20.0f, -6.0f), Vector2(36.0f, 16.0f) };
		for (const Vector2 &pc : patches) {
			int pc_n = 0;
			for (int attempt = 0; attempt < 4000 && pc_n < 300; attempt++) {
				Vector2 p = pc + Vector2(rnd_t(-4.5f, 4.5f), rnd_t(-4.5f, 4.5f));
				float h = terrain_height(p.x, p.y);
				if (h < water_level + 0.25f || terrain_slope(p.x, p.y) > 0.5f) {
					continue;
				}
				if (dist_to_path(p.x, p.y) < 2.0f || on_bridge_deck(p)) {
					continue;
				}
				if ((p - homestead_center).length() < 12.0f || (p - spawn_point).length() < 5.0f) {
					continue;
				}
				float s = rnd_t(0.85f, 1.25f);
				float gm = value_noise(p.x * 0.5f, p.y * 0.5f);
				// 深绿野草丛，少量黄绿穗色
				Color c(lerpf(0.26f, 0.38f, gm), lerpf(0.40f, 0.50f, gm), lerpf(0.15f, 0.22f, gm));
				if (rnd_t(0.0f, 1.0f) < 0.12f) {
					c = Color(0.55f, 0.50f, 0.24f); // 黄绿穗尖丛
				}
				tall.add(Vector3(p.x, h - 0.02f, p.y), rnd_t(0.0f, 6.2831f), rnd_t(-0.10f, 0.10f), s, c);
				pc_n++;
			}
		}
	}

	// —— 睡莲与荷花：大湖深水区与近岸水域 ——
	// 荷叶带物理碰撞体（StaticBody3D + CylinderShape3D），玩家可直接踏在荷叶上跳跃行走
	for (int attempt = 0; attempt < 1200 && lilies.count() < 120; attempt++) {
		float a = rnd(0.0f, 6.2831f);
		float lobe_pick = rnd(0.0f, 4.0f);
		Vector2 p;
		if (lobe_pick < 1.4f) {
			p = Vector2(-14.0f, 66.0f) + Vector2(std::cos(a), std::sin(a)) * 44.0f * rnd(0.12f, 0.82f);
		} else if (lobe_pick < 2.4f) {
			p = Vector2(16.0f, 56.0f) + Vector2(std::cos(a), std::sin(a)) * 32.0f * rnd(0.15f, 0.80f);
		} else if (lobe_pick < 3.2f) {
			p = Vector2(-15.0f, 8.0f) + Vector2(std::cos(a), std::sin(a)) * 22.0f * rnd(0.15f, 0.82f);
		} else {
			p = Vector2(-6.0f + std::cos(a) * 11.0f * rnd(0.15f, 0.85f), 26.0f + std::sin(a) * 15.0f * rnd(0.15f, 0.85f));
		}
		if (on_bridge_deck(p)) {
			continue;
		}
		if (terrain_height(p.x, p.y) > -1.20f) {
			continue;
		}
		float s = rnd(0.85f, 1.45f);
		Color c(0.20f + rnd(0.0f, 0.10f), 0.42f + rnd(0.0f, 0.12f), 0.20f);
		// 约三成荷叶翘出水面：叶缘翻起一个大角度，整片抬到水面之上。
		// 全平贴水面会很死板 —— 真实荷塘里总有几片被风掀起来、或者还没展平。
		float ltilt = 0.0f;
		float llift = 0.0f;
		if (pond_rnd(0.0f, 1.0f) < 0.34f) {
			float mag = pond_rnd(0.18f, 0.46f);
			ltilt = pond_rnd(0.0f, 1.0f) < 0.5f ? -mag : mag;
			llift = 0.09f + mag * 0.22f;
		}
		lilies.add(Vector3(p.x, water_level + 0.06f + llift, p.y), rnd(0.0f, 6.2831f), ltilt, s, c);

		// 荷叶实体碰撞：使主角能够稳稳踩在荷叶上行走和连跳
		StaticBody3D *lb = memnew(StaticBody3D);
		lb->set_position(Vector3(p.x, water_level + 0.02f, p.y));
		CollisionShape3D *lcs = memnew(CollisionShape3D);
		Ref<CylinderShape3D> lcy;
		lcy.instantiate();
		lcy->set_radius(0.52f * s);
		lcy->set_height(0.12f);
		lcs->set_shape(lcy);
		lb->add_child(lcs);
		add_child(lb);
	}

	// 荷花点缀：在石桥两侧与壮丽大湖碧波中盛开的粉白荷花（外粉内红、金黄花蕊）
	const Vector3 lotus_spots[] = {
		Vector3(-4.5f, water_level + 0.065f, 30.5f),  // 石桥北侧水面，站在桥上凭栏俯瞰清晰可见
		Vector3(-7.5f, water_level + 0.065f, 21.5f),  // 石桥南侧水面，石拱倒影旁
		Vector3(-16.0f, water_level + 0.065f, 58.0f), // 西北大湖主水域荷花群
		Vector3(-10.0f, water_level + 0.065f, 74.0f), // 西北大湖北部开阔水面
		Vector3(12.0f, water_level + 0.065f, 52.0f),  // 东北阳光水域碧波
		Vector3(20.0f, water_level + 0.065f, 64.0f),  // 东北阳光水域远眺
		Vector3(-14.0f, water_level + 0.065f, 12.0f), // 南侧风景湾水面
		Vector3(-18.0f, water_level + 0.065f, 3.0f)   // 南侧风景湾幽静处
	};
	for (const Vector3 &lpos : lotus_spots) {
		lotuses.add(lpos, rnd(0.0f, 6.2831f), 0.0f, rnd(1.05f, 1.35f), Color(1.0f, 1.0f, 1.0f));
	}
	// 程序化补种：湖面这么大，只摆 8 朵太稀。绕各湖湾浅水区撒到 46 朵左右。
	// 用独立随机源（pond_rnd），不动全局序列，其他植被的位置不会跟着变。
	for (int attempt = 0; attempt < 900 && lotuses.count() < 46; attempt++) {
		float a = pond_rnd(0.0f, 6.2831f);
		float lobe = pond_rnd(0.0f, 4.0f);
		Vector2 p;
		if (lobe < 1.4f) {
			p = Vector2(-14.0f, 66.0f) + Vector2(std::cos(a), std::sin(a)) * 44.0f * pond_rnd(0.10f, 0.72f);
		} else if (lobe < 2.4f) {
			p = Vector2(16.0f, 56.0f) + Vector2(std::cos(a), std::sin(a)) * 32.0f * pond_rnd(0.12f, 0.70f);
		} else if (lobe < 3.2f) {
			p = Vector2(-15.0f, 8.0f) + Vector2(std::cos(a), std::sin(a)) * 22.0f * pond_rnd(0.12f, 0.72f);
		} else {
			p = Vector2(-6.0f + std::cos(a) * 11.0f * pond_rnd(0.12f, 0.78f),
					26.0f + std::sin(a) * 15.0f * pond_rnd(0.12f, 0.78f));
		}
		if (on_bridge_deck(p)) {
			continue; // 别长到桥面底下
		}
		if (terrain_height(p.x, p.y) > -0.95f) {
			continue; // 太浅（快到岸边）不长荷花
		}
		lotuses.add(Vector3(p.x, water_level + 0.065f, p.y), pond_rnd(0.0f, 6.2831f), 0.0f,
				pond_rnd(0.95f, 1.40f), Color(1.0f, 1.0f, 1.0f));
	}

	// —— 小花：草地上的颜色点缀 ——
	for (int attempt = 0; attempt < 4500; attempt++) {
		Vector2 p(rnd(-inner, inner), rnd(-inner, inner));
		if (in_pitch(p.x, p.y)) { // 球场里不撒花
			continue;
		}
		float h = terrain_height(p.x, p.y);
		if (h < water_level + 0.3f || terrain_slope(p.x, p.y) > 0.7f) {
			continue;
		}
		if ((p - homestead_center).length() < 8.5f) { // 院子里不撒花
			continue;
		}
		if (dist_to_path(p.x, p.y) < 1.2f || on_bridge_deck(p)) { // 花别开在路/桥中间
			continue;
		}
		float pick = rnd(0.0f, 4.0f);
		Color c;
		if (pick < 1.0f) {
			c = Color(0.96f, 0.96f, 0.92f); // 白
		} else if (pick < 2.0f) {
			c = Color(0.98f, 0.85f, 0.30f); // 黄
		} else if (pick < 3.0f) {
			c = Color(0.95f, 0.62f, 0.70f); // 粉
		} else {
			c = Color(0.62f, 0.76f, 0.95f); // 淡蓝
		}
		flowers.add(Vector3(p.x, h + 0.09f, p.y), 0.0f, 0.0f, rnd(0.8f, 1.3f), c);
		if (flowers.count() >= 1400) {
			break;
		}
	}

	// —— 石头：湖岸环 + 坡地散布 ——
	for (int attempt = 0; attempt < 1500; attempt++) {
		Vector2 p;
		if (attempt % 3 == 0) { // 三分之一围着湖岸
			float a = rnd(0.0f, 6.2831f);
			float lobe_pick = rnd(0.0f, 4.0f);
			if (lobe_pick < 1.4f) {
				p = Vector2(-14.0f, 66.0f) + Vector2(std::cos(a), std::sin(a)) * 44.0f * rnd(1.03f, 1.30f);
			} else if (lobe_pick < 2.4f) {
				p = Vector2(16.0f, 56.0f) + Vector2(std::cos(a), std::sin(a)) * 32.0f * rnd(1.03f, 1.30f);
			} else if (lobe_pick < 3.2f) {
				p = Vector2(-15.0f, 8.0f) + Vector2(std::cos(a), std::sin(a)) * 22.0f * rnd(1.03f, 1.30f);
			} else {
				p = Vector2(-6.0f + std::cos(a) * 13.0f * rnd(1.03f, 1.30f), 26.0f + std::sin(a) * 18.0f * rnd(1.03f, 1.30f));
			}
		} else {
			p = Vector2(rnd(-inner, inner), rnd(-inner, inner));
		}
		if (Math::abs(p.x) > inner || Math::abs(p.y) > inner) {
			continue;
		}
		if ((p - spawn_point).length() < 3.0f) {
			continue;
		}
		if ((p - homestead_center).length() < 9.5f) { // 院子里不滚石头
			continue;
		}
		if (in_pitch(p.x, p.y)) { // 球场里不滚石头
			continue;
		}
		if (dist_to_path(p.x, p.y) < 1.2f || on_bridge_deck(p)) { // 小径/桥面上不绊石头
			continue;
		}
		float h = terrain_height(p.x, p.y);
		if (h < water_level + 0.12f) {
			continue;
		}
		float s = rnd(0.5f, 1.9f);
		float g = rnd(0.42f, 0.58f);
		rocks.add(Vector3(p.x, h + 0.12f * s, p.y), rnd(0.0f, 6.2831f), rnd(-0.15f, 0.15f), s,
				Color(g + 0.03f, g, g - 0.02f));
		// 碎石碰撞（半埋小圆石，能站能绕）
		StaticBody3D *rb = memnew(StaticBody3D);
		rb->set_position(Vector3(p.x, h + 0.12f * s, p.y));
		CollisionShape3D *rcs = memnew(CollisionShape3D);
		Ref<SphereShape3D> rsp;
		rsp.instantiate();
		rsp->set_radius(0.34f * s);
		rcs->set_shape(rsp);
		rb->add_child(rcs);
		add_child(rb);
		if (rocks.count() >= 320) {
			break;
		}
	}

	// —— 鹅卵石：水线沙滩带上密铺一圈（MC 海滩砾石观感） ——
	for (int attempt = 0; attempt < 3200 && pebbles.count() < 750; attempt++) {
		float a = rnd(0.0f, 6.2831f);
		float lobe_pick = rnd(0.0f, 4.0f);
		Vector2 p;
		if (lobe_pick < 1.4f) {
			p = Vector2(-14.0f, 66.0f) + Vector2(std::cos(a), std::sin(a)) * 44.0f * rnd(0.96f, 1.34f);
		} else if (lobe_pick < 2.4f) {
			p = Vector2(16.0f, 56.0f) + Vector2(std::cos(a), std::sin(a)) * 32.0f * rnd(0.96f, 1.34f);
		} else if (lobe_pick < 3.2f) {
			p = Vector2(-15.0f, 8.0f) + Vector2(std::cos(a), std::sin(a)) * 22.0f * rnd(0.96f, 1.34f);
		} else {
			p = Vector2(-6.0f + std::cos(a) * 13.0f * rnd(0.96f, 1.34f), 26.0f + std::sin(a) * 18.0f * rnd(0.96f, 1.34f));
		}
		if (Math::abs(p.x) > inner || Math::abs(p.y) > inner) {
			continue;
		}
		float h = terrain_height(p.x, p.y);
		if (h < water_level + 0.04f || h > water_level + 0.7f) { // 只落在沙滩带上
			continue;
		}
		if (dist_to_path(p.x, p.y) < 0.9f || on_bridge_deck(p)) {
			continue;
		}
		if ((p - homestead_center).length() < 9.5f || (p - spawn_point).length() < 3.0f) {
			continue;
		}
		float s = rnd(0.5f, 1.4f);
		float g = rnd(0.30f, 0.44f); // 灰石子（原 0.46~0.62 太阳下白得刺眼）
		float warm = rnd(0.0f, 1.0f);
		// 灰石子与暖褐石子混铺
		Color c = warm < 0.6f ? Color(g + 0.04f, g, g - 0.02f) : Color(0.44f, 0.37f, 0.29f);
		pebbles.add(Vector3(p.x, h + 0.02f * s, p.y), rnd(0.0f, 6.2831f), 0.0f, s, c);
	}

	// —— 落叶：每棵树下撒一小圈秋色圆叶（MC 橡树落叶观感） ——
	for (const Vector2 &t : tree_pos) {
		int n = 4 + (int)rnd(0.0f, 4.9f);
		for (int k = 0; k < n && leaves.count() < 2100; k++) {
			float a = rnd(0.0f, 6.2831f);
			float r = rnd(0.3f, 1.8f);
			Vector2 p = t + Vector2(std::cos(a), std::sin(a)) * r;
			float h = terrain_height(p.x, p.y);
			if (h < water_level + 0.2f || terrain_slope(p.x, p.y) > 0.8f) {
				continue;
			}
			float pick = rnd(0.0f, 4.0f);
			float j = rnd(-0.04f, 0.04f);
			Color c;
			if (pick < 1.0f) {
				c = Color(0.86f + j, 0.45f + j, 0.12f); // 橙
			} else if (pick < 2.0f) {
				c = Color(0.90f + j, 0.70f + j, 0.20f); // 黄
			} else if (pick < 3.0f) {
				c = Color(0.62f + j, 0.26f + j, 0.14f); // 红褐
			} else {
				c = Color(0.48f + j, 0.34f + j, 0.18f); // 枯褐
			}
			leaves.add(Vector3(p.x, h + 0.02f, p.y), rnd(0.0f, 6.2831f), 0.0f, rnd(0.7f, 1.3f), c);
		}
	}

	// —— 碎石坡：陡坡带上滚落的石屑（增强"山体岩层裸露"的地貌观感） ——
	for (int attempt = 0; attempt < 4500 && scree.count() < 660; attempt++) {
		Vector2 p(rnd(-inner, inner), rnd(-inner, inner));
		float slope = terrain_slope(p.x, p.y);
		if (slope < 0.62f || slope > 1.6f) { // 只落在"露岩过渡带"的陡坡上
			continue;
		}
		if ((p - spawn_point).length() < 4.0f || (p - homestead_center).length() < 12.0f) {
			continue;
		}
		if (dist_to_path(p.x, p.y) < 1.5f || on_bridge_deck(p)) {
			continue;
		}
		float h = terrain_height(p.x, p.y);
		if (h < water_level + 0.25f) {
			continue;
		}
		float s = rnd(0.18f, 0.55f); // rock_mesh 半径 0.4 → 实际 0.07~0.22m 的小石
		float g = rnd(0.40f, 0.55f);
		scree.add(Vector3(p.x, h + 0.08f * s, p.y), rnd(0.0f, 6.2831f), rnd(-0.2f, 0.2f), s,
				Color(g + 0.03f, g, g - 0.03f));
	}

	// —— 倒木：林间平缓处横卧的枯木（少量、彼此拉开距离，增添荒野叙事感） ——
	std::vector<Vector2> log_pos;
	for (int attempt = 0; attempt < 2400 && logs.count() < 34; attempt++) {
		Vector2 p(rnd(-inner, inner), rnd(-inner, inner));
		if (in_pitch(p.x, p.y)) { // 球场里不躺倒木
			continue;
		}
		if (terrain_slope(p.x, p.y) > 0.45f) {
			continue; // 只躺平缓草地
		}
		if (terrain_height(p.x, p.y) < water_level + 0.3f) {
			continue; // 不下水、不贴滩
		}
		if (dist_to_path(p.x, p.y) < 1.8f || on_bridge_deck(p)) {
			continue;
		}
		if ((p - homestead_center).length() < 12.0f || (p - spawn_point).length() < 5.0f) {
			continue;
		}
		float yaw = rnd(0.0f, 6.2831f);
		float s = rnd(0.75f, 1.25f);
		// 两端贴地校验：半长 1.5m，端点高差大就换地方（防一头翘到半空）
		Vector2 dir(-std::cos(yaw), std::sin(yaw)); // 欧拉 (tilt,yaw,tilt) 下圆柱轴指向的水平方向
		float half_len = 1.5f * s;
		float h0 = terrain_height(p.x - dir.x * half_len, p.y - dir.y * half_len);
		float h1 = terrain_height(p.x + dir.x * half_len, p.y + dir.y * half_len);
		if (Math::abs(h1 - h0) > 0.5f) {
			continue;
		}
		bool too_close = false;
		for (const Vector2 &q : log_pos) {
			if ((p - q).length() < 7.0f) {
				too_close = true;
				break;
			}
		}
		if (too_close) {
			continue;
		}
		log_pos.push_back(p);
		float pick = rnd(0.0f, 1.0f);
		// 枯木棕 / 灰褐风化，偶尔带点苔藓绿
		Color c = pick < 0.5f ? Color(0.46f, 0.37f, 0.26f)
				: pick < 0.85f ? Color(0.47f, 0.45f, 0.34f)
							   : Color(0.40f, 0.44f, 0.30f);
		float tilt_l = rnd(1.42f, 1.66f);
		logs.add(Vector3(p.x, (h0 + h1) * 0.5f + 0.10f * s, p.y), yaw, tilt_l, s, c);
		// 倒木碰撞（能踩上去走平衡木；与 Scatter 实例同款欧拉角 (tilt,yaw,tilt)）
		StaticBody3D *lb = memnew(StaticBody3D);
		lb->set_position(Vector3(p.x, (h0 + h1) * 0.5f + 0.10f * s, p.y));
		lb->set_rotation(Vector3(tilt_l, yaw, tilt_l));
		CollisionShape3D *lcs = memnew(CollisionShape3D);
		Ref<CylinderShape3D> lcy;
		lcy.instantiate();
		lcy->set_radius(0.20f * s);
		lcy->set_height(2.9f * s);
		lcs->set_shape(lcy);
		lb->add_child(lcs);
		add_child(lb);
	}

	// —— 蘑菇：树荫下林缘成簇（MC 红伞白点蘑菇 + 棕色蘑菇混生） ——
	// 簇心挂在某棵树干周围 0.9~2.2m（树冠正下方的"荫影区"），
	// 每簇 3~7 朵，红伞六成（带白点）/ 棕伞四成。蘑菇僵硬不随风摆。
	int shroom_clusters = 0;
	for (int attempt = 0; attempt < 1600 && shroom_clusters < 45; attempt++) {
		if (tree_pos.empty()) {
			break;
		}
		int ti = (int)rnd(0.0f, (float)tree_pos.size() - 0.001f);
		float ca = rnd(0.0f, 6.2831f);
		Vector2 base = tree_pos[ti] + Vector2(std::cos(ca), std::sin(ca)) * rnd(0.9f, 2.2f);

		if ((base - homestead_center).length() < 9.0f) {
			continue;
		}
		if ((base - spawn_point).length() < 3.0f) {
			continue;
		}
		if (dist_to_path(base.x, base.y) < 1.2f || on_bridge_deck(base)) {
			continue;
		}
		float bh = terrain_height(base.x, base.y);
		if (bh < water_level + 0.25f || terrain_slope(base.x, base.y) > 0.6f) {
			continue; // 长在荫凉平缓的草地上，不下水不爬坡
		}

		int n_shroom = 3 + (int)rnd(0.0f, 4.9f);
		for (int k = 0; k < n_shroom; k++) {
			Vector2 p = base + Vector2(rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f)) * rnd(0.0f, 0.9f);
			float h = terrain_height(p.x, p.y);
			if (h < water_level + 0.25f || terrain_slope(p.x, p.y) > 0.7f) {
				continue;
			}
			if (dist_to_path(p.x, p.y) < 0.9f) {
				continue;
			}
			float s = rnd(0.7f, 1.35f);
			float yaw = rnd(0.0f, 6.2831f);
			// 柄：奶白色，略带深浅变化；顶端在 h + 0.14·s
			shroom_stems.add(Vector3(p.x, h + 0.07f * s, p.y), yaw, rnd(-0.10f, 0.10f), s,
					Color(0.88f + rnd(-0.04f, 0.04f), 0.84f + rnd(-0.03f, 0.0f), 0.72f + rnd(-0.03f, 0.0f)));
			if (rnd(0.0f, 1.0f) < 0.6f) {
				// 红伞蘑菇（MC 红色蘑菇）：伞盖中心在 h + 0.15·s
				shroom_caps.add(Vector3(p.x, h + 0.15f * s, p.y), yaw, 0.0f, s,
						Color(0.78f + rnd(-0.06f, 0.0f), 0.16f + rnd(0.0f, 0.05f), 0.12f + rnd(0.0f, 0.03f)));
				// 白点贴着伞盖上表面（按压扁球的表面高度嵌入一半），3~5 颗
				int n_spot = 3 + (int)rnd(0.0f, 2.9f);
				for (int b = 0; b < n_spot; b++) {
					float a = rnd(0.0f, 6.2831f);
					float rr = rnd(0.0f, 0.075f); // 距伞盖中心的水平距离（米，未缩放）
					float sq = 1.0f - (rr / 0.10f) * (rr / 0.10f);
					float cap_h = 0.15f + 0.045f * std::sqrt(sq > 0.0f ? sq : 0.0f) - 0.010f;
					shroom_spots.add(
							Vector3(p.x + std::cos(a) * rr * s, h + cap_h * s, p.y + std::sin(a) * rr * s),
							0.0f, 0.0f, s * rnd(0.8f, 1.2f), Color(0.94f, 0.93f, 0.90f));
				}
			} else {
				// 棕色蘑菇（MC 棕色蘑菇），无白点
				shroom_caps.add(Vector3(p.x, h + 0.15f * s, p.y), yaw, 0.0f, s,
						Color(0.62f + rnd(-0.05f, 0.04f), 0.48f + rnd(-0.04f, 0.03f), 0.33f + rnd(-0.03f, 0.02f)));
			}
			// 伞顶雪毯：小扁白球扣在伞盖穹顶上（略嵌入），红/棕都盖。
			// 位置由 h/s 确定性导出，不加 rnd()——随机序列保持不变。
			shroom_snow.add(Vector3(p.x, h + 0.17f * s, p.y), yaw, 0.0f, s,
					Color(0.95f, 0.96f, 1.0f));
		}
		// 约 40% 的簇夜里冒"孢子光尘"：几粒淡绿微光从整簇蘑菇的体积里缓缓上浮
		if (rnd(0.0f, 1.0f) < 0.4f) {
			spore_puffs.push_back(make_spore_puff(Vector3(base.x, bh + 0.30f, base.y)));
		}
		shroom_clusters++;
	}

	// —— 蒲公英：草地上散点的黄色小球花（高出草毯探头；独立 rng 不动既有序列） ——
	Ref<CylinderMesh> dandy_stem_mesh;
	dandy_stem_mesh.instantiate();
	dandy_stem_mesh->set_top_radius(0.006f);
	dandy_stem_mesh->set_bottom_radius(0.008f);
	dandy_stem_mesh->set_height(0.22f);
	dandy_stem_mesh->set_radial_segments(5);
	dandy_stem_mesh->set_rings(1);
	dandy_stem_mesh->set_material(veg_material());
	Ref<SphereMesh> dandy_puff_mesh;
	dandy_puff_mesh.instantiate();
	dandy_puff_mesh->set_radius(0.065f); // 黄球放大（原 0.045 太小，配黑茎像毛毛虫）
	dandy_puff_mesh->set_height(0.09f);
	dandy_puff_mesh->set_radial_segments(8);
	dandy_puff_mesh->set_rings(5);
	dandy_puff_mesh->set_material(veg_material());
	Scatter dandy_stems, dandy_puffs;
	{
		std::mt19937 rng_d(550913);
		auto rnd_d = [&rng_d](float lo, float hi) -> float {
			std::uniform_real_distribution<float> d(lo, hi);
			return d(rng_d);
		};
		for (int attempt = 0; attempt < 16000 && dandy_puffs.count() < 1400; attempt++) {
			Vector2 p(rnd_d(-inner, inner), rnd_d(-inner, inner));
			if (in_pitch(p.x, p.y)) { // 球场里不飘蒲公英
				continue;
			}
			float h = terrain_height(p.x, p.y);
			if (h < water_level + 0.18f || terrain_slope(p.x, p.y) > 1.1f) {
				continue;
			}
			if ((p - homestead_center).length() < 8.5f) {
				continue;
			}
			if (dist_to_path(p.x, p.y) < 1.0f || on_bridge_deck(p)) {
				continue;
			}
			float s = rnd_d(0.8f, 1.35f);
			dandy_stems.add(Vector3(p.x, h + 0.10f * s, p.y), 0.0f, 0.0f, s,
					Color(0.30f, 0.50f, 0.18f)); // 茎压绿（原深绿近黑）
			dandy_puffs.add(Vector3(p.x, h + 0.26f * s, p.y), rnd_d(0.0f, 6.2831f), 0.0f, s,
					Color(0.98f, 0.84f + rnd_d(0.0f, 0.06f), 0.18f)); // 亮黄蒲公英
		}
	}

	// —— 巨石：全图缓坡稀疏分布的大石头（灰岩色带随机色差，半埋入土） ——
	// 独立 rng（固定种子）——不挪动上方 Scatter 的随机序列，避免整图重排。
	{
		std::mt19937 rng_b(20260424);
		auto rnd_b = [&rng_b](float lo, float hi) -> float {
			std::uniform_real_distribution<float> d(lo, hi);
			return d(rng_b);
		};

		// 直径 1.0·s → s∈[0.8, 2.2] 即 0.8~2.2 m 的巨石；轮廓略扁更像顽石
		Ref<SphereMesh> boulder_mesh;
		boulder_mesh.instantiate();
		boulder_mesh->set_radius(0.5f);
		boulder_mesh->set_height(0.85f);
		boulder_mesh->set_radial_segments(7);
		boulder_mesh->set_rings(4);
		boulder_mesh->set_material(veg_material());

		Scatter boulders;
		for (int attempt = 0; attempt < 8600 && boulders.count() < 72; attempt++) {
			Vector2 p(rnd_b(-inner, inner), rnd_b(-inner, inner));
			if (in_pitch(p.x, p.y)) { // 球场里不滚巨石
				continue;
			}
			if ((p - spawn_point).length() < 6.0f) {
				continue;
			}
			if ((p - homestead_center).length() < 14.0f) {
				continue;
			}
			if (dist_to_path(p.x, p.y) < 2.2f || on_bridge_deck(p)) {
				continue;
			}
			float h = terrain_height(p.x, p.y);
			if (h < water_level + 0.3f || terrain_slope(p.x, p.y) > 0.55f) {
				continue; // 只落缓坡陆地，不下水不爬崖
			}
			float s = rnd_b(0.8f, 2.2f);
			float sink = rnd_b(0.20f, 0.30f); // 下沉 20~30%（半埋入土）
			float g = rnd_b(0.30f, 0.42f);    // 深灰岩底色 + 随机色差（原 0.5~0.6 阳光下白成雪球）
			// 静置中心 h + 0.425·s（半高），再整体下沉 sink·0.85·s
			Vector3 bpos(p.x, h + 0.425f * s - 0.85f * s * sink, p.y);
			boulders.add(bpos,
					rnd_b(0.0f, 6.2831f), rnd_b(-0.15f, 0.15f), s,
					Color(g + 0.02f, g, g - 0.03f));
			// 碰撞：巨石要"能站上去"——球碰撞半径略小于视觉半径（贴脚感更自然）
			StaticBody3D *bb = memnew(StaticBody3D);
			bb->set_position(bpos);
			CollisionShape3D *bcs = memnew(CollisionShape3D);
			Ref<SphereShape3D> bsp;
			bsp.instantiate();
			bsp->set_radius(0.42f * s);
			bcs->set_shape(bsp);
			bb->add_child(bcs);
			add_child(bb);
		}
		commit_scatter(this, "Boulders", boulder_mesh, boulders);
	}

	// —— 湖岸礁石：水线带上的顽石（拍岸浪的"撞击锚点"） ——
	// 独立 rng（固定种子）——不挪动上方 Scatter 的随机序列，避免整图重排。
	// 石头半沉半露：浪推过来时正面撞上露出水面的石脊，在石头两侧炸开白沫。
	// 坐标会写进 water.gdshader 的 shore_rocks，让浪花精确落在石头上。
	{
		std::mt19937 rng_k(771024);
		auto rnd_k = [&rng_k](float lo, float hi) -> float {
			std::uniform_real_distribution<float> d(lo, hi);
			return d(rng_k);
		};

		// 噪声位移 + 逐面法线的顽石网格（原来是光滑球，看着像蛋）
		Ref<ArrayMesh> reef_mesh = make_rock_mesh(11, 6, 0.5f, 0.74f, 0.32f, 1.8f, 51071);
		reef_mesh->surface_set_material(0, rock_material());

		Scatter reefs;
		shore_rocks.clear();
		std::vector<Vector2> reef_pos;
		std::vector<Vector4> spray_data; // (石头半径, 石头处水深, 随机种子, 等比缩放)
		std::vector<Vector3> spray_pos;  // 浪花锚点（静水线）
		for (int attempt = 0; attempt < 40000 && reefs.count() < 32; attempt++) {
			Vector2 p(rnd_k(-inner, inner), rnd_k(-inner, inner));
			if (in_pitch(p.x, p.y) || on_bridge_deck(p)) {
				continue;
			}
			if (dist_to_path(p.x, p.y) < 1.0f) {
				continue;
			}
			// 必须在湖区附近：湖外也有恰好低于水线的洼地（洼地压平 `h<0 → h*=0.18`），
			// 光看高度会把石头撒到没水的草地上（实测过 x=-167 的干洼地）。
			if (!is_near_lake(p.x, p.y, 7.0f)) {
				continue;
			}
			float h = terrain_height(p.x, p.y);
			// 只落水线带：水下 0.85m ~ 水上 0.12m
			if (h < water_level - 0.85f || h > water_level + 0.12f) {
				continue;
			}
			float s = rnd_k(0.55f, 1.45f);
			float r_xz = 0.5f * s;   // SphereMesh r0.5 → 水平视觉半径
			float r_y = 0.37f * s;   // 竖向半高
			float sink = rnd_k(0.28f, 0.46f);
			float cy = h + r_y - 2.0f * r_y * sink; // 中心：按比例埋入水线
			float top = cy + r_y;
			// 露出水面 0.12~0.75m：太低看不见、太高像孤岛
			if (top - water_level < 0.12f || top - water_level > 0.75f) {
				continue;
			}
			bool too_close = false;
			for (const Vector2 &q : reef_pos) {
				if ((p - q).length() < 2.6f) {
					too_close = true;
					break;
				}
			}
			if (too_close) {
				continue;
			}
			// 湿石色：比陆地巨石更深的青灰（被水泡透），带随机色差
			float g = rnd_k(0.22f, 0.34f);
			reefs.add(Vector3(p.x, cy, p.y), rnd_k(0.0f, 6.2831f), rnd_k(-0.12f, 0.12f), s,
					Color(g + 0.01f, g + 0.02f, g + 0.05f));
			reef_pos.push_back(p);
			shore_rocks.push_back(Vector4(p.x, cy, p.y, r_xz));

			// 浪花冠：石头处的水深决定拍岸浪相位（水越深，浪到得越晚）。
			// 冠的等比缩放 = 石头水平半径 × 2.0 —— 单位冠底半径 0.5，缩放后正好贴住石头，
			// 浪峰时顶半径 ≈ 3.2 × 石头半径，形成"炸开"的扇形水花。
			{
				float wdepth = Math::max(water_level - h, 0.0f);
				spray_data.push_back(Vector4(r_xz, wdepth, rnd_k(0.0f, 1.0f), r_xz * 2.0f));
				spray_pos.push_back(Vector3(p.x, water_level - 0.06f, p.y));
			}

			// 碰撞：露出水面的石脊能站人（半径略小于视觉，贴合脚感）
			StaticBody3D *rb = memnew(StaticBody3D);
			rb->set_position(Vector3(p.x, cy, p.y));
			CollisionShape3D *rcs = memnew(CollisionShape3D);
			Ref<SphereShape3D> rsp;
			rsp.instantiate();
			rsp->set_radius(0.44f * s);
			rcs->set_shape(rsp);
			rb->add_child(rcs);
			add_child(rb);
		}
		commit_scatter(this, "ShoreReefs", reef_mesh, reefs);

		// —— 礁石激浪水花：每块石头一撮向上炸开的水滴粒子 ——
		// 真实几何：从任何角度看都是立体的水花，不是贴在水面上的白斑。
		// 全部挂在 ShoreSpray 下，SkyCycle 靠它统一做"结冰封湖 → 停喷"的淡出。
		// 岸边是小石头 → 用收敛的小档水花，大浪留给下面的深水巨礁。
		Ref<Texture2D> splash_dot = make_splash_dot();
		Ref<Mesh> splash_quad = make_splash_quad(splash_dot);
		Node3D *spray_root = memnew(Node3D);
		spray_root->set_name("ShoreSpray");
		add_child(spray_root);
		const SplashStyle shore_style = { 22, 0.72f, 0.80f, 0.26f, 56.0f, 2.6f };
		for (int i = 0; i < (int)spray_pos.size(); i++) {
			spray_root->add_child(make_reef_splash(spray_pos[i], spray_data[i].x,
					spray_data[i].y, splash_dot, splash_quad, shore_style));
		}

		// ══════════════════════════════════════════════════════════════════
		// 深水巨礁：湖心深水处一块大石头，浪打上去炸起一大蓬白色水花
		// （参考实拍：礁石露出水面约 1m，浪花能冲到石头高度的 2 倍以上）
		// ══════════════════════════════════════════════════════════════════
		{
			std::mt19937 rng_g(880417);
			auto rnd_g = [&rng_g](float lo, float hi) -> float {
				std::uniform_real_distribution<float> d(lo, hi);
				return d(rng_g);
			};

			// 在西北大湖里找一块足够深的水域：从偏好点螺旋外扩，取最深处。
			// 巨礁必须立在水里 —— 水深不够就不是"湖心礁石"了。
			Vector2 anchor(-14.0f, 58.0f);
			Vector2 spot = anchor;
			float best_depth = -1.0f;
			for (int r = 0; r <= 9; r++) {
				for (int a = 0; a < 10; a++) {
					float ang = (float)a / 10.0f * Math::TAU;
					Vector2 c = anchor + Vector2(Math::cos(ang), Math::sin(ang)) * ((float)r * 1.6f);
					float d = water_level - terrain_height(c.x, c.y);
					if (d > best_depth) {
						best_depth = d;
						spot = c;
					}
				}
			}
			printf("[TerrainBuilder] 深水巨礁落点 (%.1f, %.1f)，水深 %.2f m\n",
					spot.x, spot.y, best_depth);

			if (best_depth > 0.9f) {
				Ref<ArrayMesh> big_mesh = make_rock_mesh(13, 7, 0.5f, 0.78f, 0.30f, 1.35f, 33071);
				big_mesh->surface_set_material(0, rock_material());

				// 主礁：露出水面约 1.3m（实拍里那块礁石的量级）
				float s = 2.9f;
				float half_y = 0.5f * 0.78f * s; // 网格竖向半高 × 缩放
				float cy = water_level + 1.30f - half_y;
				Scatter big;
				big.add(Vector3(spot.x, cy, spot.y), rnd_g(0.0f, 6.2831f), rnd_g(-0.10f, 0.10f), s,
						Color(0.20f, 0.21f, 0.23f)); // 湿透的深青灰
				commit_scatter(this, "DeepReef", big_mesh, big);

				// 水花位置：静水线。半径给大 —— 影响环和粒子扩散都按它走。
				float r_xz = 0.5f * s;
				float wdepth = Math::max(water_level - terrain_height(spot.x, spot.y), 0.0f);
				spray_root->add_child(make_reef_splash(Vector3(spot.x, water_level - 0.10f, spot.y),
						r_xz, wdepth, splash_dot, splash_quad, SplashStyle{ 900, 2.35f, 3.20f, 0.62f, 52.0f, 0.85f }));

				// 给水面着色器也登记：巨礁周围同样要有一圈拍岸白沫
				shore_rocks.push_back(Vector4(spot.x, cy, spot.y, r_xz));

				// 伴礁：主礁旁边两块小的，把"礁石群"的感觉做出来
				Ref<ArrayMesh> mid_mesh = make_rock_mesh(11, 6, 0.5f, 0.72f, 0.34f, 1.9f, 44071);
				mid_mesh->surface_set_material(0, rock_material());
				Scatter mid;
				const SplashStyle mid_style = { 90, 1.45f, 1.70f, 0.42f, 62.0f, 1.25f };
				for (int k = 0; k < 2; k++) {
					float ang = rnd_g(0.0f, 6.2831f);
					float dist = rnd_g(3.2f, 5.0f);
					Vector2 mp = spot + Vector2(Math::cos(ang), Math::sin(ang)) * dist;
					float mh = terrain_height(mp.x, mp.y);
					if (water_level - mh < 0.45f) {
						continue; // 太浅，不放了
					}
					float ms = rnd_g(1.15f, 1.55f);
					float m_half = 0.5f * 0.72f * ms;
					float mcy = water_level + rnd_g(0.30f, 0.55f) - m_half;
					mid.add(Vector3(mp.x, mcy, mp.y), rnd_g(0.0f, 6.2831f), rnd_g(-0.14f, 0.14f), ms,
							Color(0.22f, 0.23f, 0.25f));
					float mr = 0.5f * ms;
					spray_root->add_child(make_reef_splash(Vector3(mp.x, water_level - 0.08f, mp.y),
							mr, Math::max(water_level - mh, 0.0f), splash_dot, splash_quad, mid_style));
					shore_rocks.push_back(Vector4(mp.x, mcy, mp.y, mr));
				}
				commit_scatter(this, "DeepReefCompanions", mid_mesh, mid);
			}
		}

		apply_shore_rocks(); // 把石头坐标交给水面着色器（含上面那块巨礁）
	}

	// —— 树桩：树林区里的伐木残桩（矮圆柱 + 顶面浅色年轮盘） ——
	// 独立 rng（固定种子）。贴地校验：桩底一圈取四点，高差过大即弃（防悬空）。
	{
		std::mt19937 rng_s(913377);
		auto rnd_s = [&rng_s](float lo, float hi) -> float {
			std::uniform_real_distribution<float> d(lo, hi);
			return d(rng_s);
		};

		// 高 0.44·s → s∈[0.8, 1.15] 即 0.35~0.5 m 的矮桩
		Ref<CylinderMesh> stump_mesh;
		stump_mesh.instantiate();
		stump_mesh->set_top_radius(0.20f);
		stump_mesh->set_bottom_radius(0.25f);
		stump_mesh->set_height(0.44f);
		stump_mesh->set_radial_segments(8);
		stump_mesh->set_rings(1);
		stump_mesh->set_material(veg_material());

		// 顶面浅色圆盘：模拟锯切后的年轮端面（颜色略带随机深浅）
		Ref<CylinderMesh> stump_top_mesh;
		stump_top_mesh.instantiate();
		stump_top_mesh->set_top_radius(0.185f);
		stump_top_mesh->set_bottom_radius(0.185f);
		stump_top_mesh->set_height(0.014f);
		stump_top_mesh->set_radial_segments(9);
		stump_top_mesh->set_rings(1);
		stump_top_mesh->set_material(veg_material());

		Scatter stumps, stump_tops;
		for (int attempt = 0; attempt < 1400 && stumps.count() < 28; attempt++) {
			if (tree_pos.empty()) {
				break;
			}
			// 挂在某棵已种的树旁 1.2~3.2 m 处（林区内天然出现）
			int ti = (int)rnd_s(0.0f, (float)tree_pos.size() - 0.001f);
			float ca = rnd_s(0.0f, 6.2831f);
			Vector2 p = tree_pos[ti] + Vector2(std::cos(ca), std::sin(ca)) * rnd_s(1.2f, 3.2f);
			if (in_pitch(p.x, p.y)) { // 球场里不立树桩
				continue;
			}
			if ((p - homestead_center).length() < 10.0f) {
				continue;
			}
			if (dist_to_path(p.x, p.y) < 1.4f || on_bridge_deck(p)) {
				continue;
			}
			float h = terrain_height(p.x, p.y);
			if (h < water_level + 0.3f || terrain_slope(p.x, p.y) > 0.5f) {
				continue;
			}
			// 贴地校验：桩底外缘三点 + 桩心，最大高差 0.22 m 以内才放
			float r = 0.24f;
			float h0 = terrain_height(p.x - r, p.y);
			float h1 = terrain_height(p.x + r * 0.5f, p.y + r * 0.87f);
			float h2 = terrain_height(p.x + r * 0.5f, p.y - r * 0.87f);
			float hmin = Math::min(h, Math::min(h0, Math::min(h1, h2)));
			float hmax = Math::max(h, Math::max(h0, Math::max(h1, h2)));
			if (hmax - hmin > 0.22f) {
				continue;
			}
			float s = rnd_s(0.8f, 1.15f);
			float y = hmin + 0.22f * s; // 桩底沉到最低采样点，绝不悬空
			stumps.add(Vector3(p.x, y, p.y), rnd_s(0.0f, 6.2831f), rnd_s(-0.04f, 0.04f), s,
					Color(0.50f + rnd_s(-0.05f, 0.05f), 0.39f + rnd_s(-0.04f, 0.04f), 0.27f + rnd_s(-0.03f, 0.03f)));
			stump_tops.add(Vector3(p.x, y + 0.22f * s + 0.008f, p.y), 0.0f, 0.0f, s,
					Color(0.78f + rnd_s(-0.06f, 0.04f), 0.66f + rnd_s(-0.05f, 0.04f), 0.44f + rnd_s(-0.04f, 0.03f)));
			// 碰撞：树桩能站上去（圆柱近似，半径取上下半径均值×0.95）
			StaticBody3D *sb = memnew(StaticBody3D);
			sb->set_position(Vector3(p.x, y, p.y));
			CollisionShape3D *scs = memnew(CollisionShape3D);
			Ref<CylinderShape3D> scy;
			scy.instantiate();
			scy->set_radius(0.215f * s);
			scy->set_height(0.44f * s);
			scs->set_shape(scy);
			sb->add_child(scs);
			add_child(sb);
		}
		commit_scatter(this, "TreeStumps", stump_mesh, stumps);
		commit_scatter(this, "StumpTops", stump_top_mesh, stump_tops);
	}

	// —— 提交为 MultiMesh 节点 ——
	// 树干按原型分桶提交（每个原型一份 MultiMesh，共用的 mesh 没法逐实例换形状）
	for (int i = 0; i < ARCH_N; i++) {
		char nm[64];
		snprintf(nm, sizeof(nm), "TreeTrunks_A%d", i);
		commit_scatter(this, nm, archetypes[i].skeleton, trunks_by_arch[i]);
	}
	static const char *tier_names[3] = { "T1LowSnow", "T2MidSnow", "T3HighSnow" };
	for (int b = 0; b < 3; b++) {
		char name[64];
		for (int i = 0; i < ARCH_N; i++) {
			snprintf(name, sizeof(name), "TreeCanopy%s_A%d", tier_names[b], i);
			commit_scatter(this, name, canopy_mesh_bands[b], canopy_lo[i][b]);
		}
		snprintf(name, sizeof(name), "TreeSnowCaps%s", tier_names[b]);
		commit_scatter(this, name, snowcap_mesh_bands[b], snowcap_bands[b]);
	}
	commit_scatter(this, "TreeFruits", fruit_mesh, fruits);
	commit_scatter(this, "Bushes", bush_mesh, bushes);
	commit_scatter(this, "BerryBushes", bush_mesh, berry_bushes);
	commit_scatter(this, "Berries", fruit_mesh, berries);
	commit_scatter(this, "GrassTufts", grass_mesh, grass);
	commit_scatter(this, "GrassCarpet", carpet_mesh, carpet);
	// 草不投影：几十万片草叶的投影互相切割会让画面显"硬"（且省一半顶点开销）
	for (const char *n : { "GrassTufts", "GrassCarpet", "TallGrass" }) {
		if (MultiMeshInstance3D *gmi = Object::cast_to<MultiMeshInstance3D>(get_node_or_null(NodePath(n)))) {
			gmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		}
	}
	commit_scatter(this, "Reeds", reed_mesh, reeds);
	commit_scatter(this, "TallGrass", tall_mesh, tall);
	commit_scatter(this, "LilyPads", lily_mesh, lilies);
	commit_scatter(this, "LotusFlowers", lotus_mesh, lotuses);
	commit_scatter(this, "Flowers", flower_mesh, flowers);
	commit_scatter(this, "Rocks", rock_mesh, rocks);
	commit_scatter(this, "Pebbles", pebble_mesh, pebbles);
	commit_scatter(this, "FallenLeaves", leaf_mesh, leaves);
	commit_scatter(this, "Scree", rock_mesh, scree);
	commit_scatter(this, "FallenLogs", log_mesh, logs);
	commit_scatter(this, "MushroomStems", shroom_stem_mesh, shroom_stems);
	commit_scatter(this, "MushroomCaps", shroom_cap_mesh, shroom_caps);
	commit_scatter(this, "MushroomSpots", shroom_spot_mesh, shroom_spots);
	commit_scatter(this, "ShroomSnow", shroom_snow_mesh, shroom_snow);
	commit_scatter(this, "DandelionStems", dandy_stem_mesh, dandy_stems);
	commit_scatter(this, "Dandelions", dandy_puff_mesh, dandy_puffs);

	// 孢子光尘：普通子节点挂 TerrainBuilder 下（emitting=false，夜里才开）
	for (CPUParticles3D *p : spore_puffs) {
		add_child(p);
	}
}
