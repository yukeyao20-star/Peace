#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <random>
#include <vector>

#include "interactable.h"

namespace godot {
class AudioStream;
class AudioStreamPlayer;
class AudioStreamWAV;
class CanvasLayer;
class ColorRect;
class Control;
class CPUParticles3D;
class Label;
class OmniLight3D;
class StandardMaterial3D;
class TextureRect;
class TerrainBuilder;
} // namespace godot

using namespace godot;

// Player —— 键盘控制的主角（第三人称）。
class Player : public CharacterBody3D {
	GDCLASS(Player, CharacterBody3D)

	// —— 可在编辑器 Inspector 里调整的参数 ——
	double move_speed = 6.0;       // 水平移动速度（米/秒）
	double jump_velocity = 8.2;    // 起跳初速度（配 30 重力：跳得干脆不飘）
	double gravity = 30.0;         // 重力加速度（原 22 太飘）
	double camera_distance = 5.5;  // 第三人称相机初始距离（滚轮 2.5~9m 缩放）
	double camera_height = 1.55;   // 视线支点高度（脚底到胸口上方）
	double camera_smoothing = 12.0; // 相机跟随平滑（原 8 太肉，走位有拖拽感）

	Node3D *model = nullptr;
	Camera3D *camera = nullptr;

protected:
	static void _bind_methods();
	void _notification(int p_what);

	void _physics_step();
	void _update_look(double delta);
	void _update_camera(double delta);
	void _update_interaction();
	void _update_footsteps(double delta, bool on_floor);

	// —— 脚步声（程序化合成，无外部资源） ——
	godot::Ref<godot::AudioStreamWAV> make_grass();
	godot::Ref<godot::AudioStreamWAV> make_dirt();
	godot::Ref<godot::AudioStreamWAV> make_pitch();
	godot::Ref<godot::AudioStreamWAV> make_snow();
	godot::Ref<godot::AudioStreamWAV> make_puddle();
	godot::Ref<godot::AudioStreamWAV> make_stone();
	godot::Ref<godot::AudioStreamWAV> make_wood();
	godot::Ref<godot::AudioStreamWAV> make_rustle(bool deep);
	godot::Ref<godot::AudioStreamWAV> make_land_grass();
	godot::Ref<godot::AudioStreamWAV> make_land_wet();
	godot::Ref<godot::AudioStreamWAV> make_land_pitch();
	void play_step(int kind, float pitch); // kind: 0荒草 1雪地咯吱 2雨天踩水 3石板 4木板 5球场草坪 6泥土小径
	void play_land(int kind);              // 专属跳跃落地音效（双脚缓冲着地与特定地面冲击质感）
	int _step_kind() const;                // 按天气/所处地形判定当前地面音色

public:
	Player() = default;
	~Player() override = default;

	// —— 必须放在 public，不能放 protected ——
	// godot-cpp 的 Node::register_virtuals<T,B>() 里写的是
	//     decltype(&B::_input)  vs  decltype(&T::_input)
	// 这段代码是 Node 的成员，等于"从基类上下文取派生类成员的指针"，
	// 取 protected 成员在 C++ 里直接编译失败：
	//   error: '_input' is a protected member of 'Player'
	// 另外签名必须是 void（Node::_input 是 virtual void），写成 bool 也编不过；
	// 头文件里也必须真 include <godot_cpp/classes/input_event.hpp>，
	// 光靠 node.hpp 的 class InputEvent; 前向声明，实例化 Ref<InputEvent> 时不够。
	// 鼠标原始位移：_input 里累加、NOTIFICATION_PROCESS 里消费。
	void _input(const Ref<InputEvent> &p_event) override;

	void set_move_speed(double p_v) { move_speed = p_v; }
	double get_move_speed() const { return move_speed; }

	void set_jump_velocity(double p_v) { jump_velocity = p_v; }
	double get_jump_velocity() const { return jump_velocity; }

	void set_camera_distance(double p_v) { camera_distance = p_v; }
	double get_camera_distance() const { return camera_distance; }

	void set_camera_height(double p_v) { camera_height = p_v; }
	double get_camera_height() const { return camera_height; }

	double get_land_timer() const { return land_timer; }
	Vector3 get_last_land_pos() const { return last_land_pos; }
	Vector3 get_facing() const { return facing; }

	// —— 外衣换装 / 滚地 / 睡觉 / 地图传送 ——
	void set_costume(int p_c);
	int get_costume() const { return costume; }
	void start_sleep(const Vector3 &p_bed_pos, float p_yaw);
	void stop_sleep();
	bool get_is_sleeping() const { return is_sleeping; }
	bool get_is_map_open() const { return is_map_open; } // PauseMenu 判定"Esc 只关地图"用
	void toggle_map(); // 已 bind ClassDB；PauseMenu 在 Esc 路径上直接调用
	void teleport_to(const Vector3 &p_pos);
	void play_ui(int type = 0);
	void _on_map_gui_input(const Ref<InputEvent> &p_event);
	void _on_map_bg_gui_input(const Ref<InputEvent> &p_event);
	void _on_map_mouse_exited();
	void _on_firework_burst(Node *burst, Node *sparks, Node *willow, Node *bang_voice, Node *flash_light, float max_energy);
	void _on_firework_cleanup(Node *burst_root, Node *rocket, Node *launch_voice);

private:
	// —— 外衣与换装 ——
	int costume = 0; // 0: 原版便服, 1: 双汇王中王火腿肠, 2: 甜玉米肠, 3: 黑椒牛肉肠
	Node3D *costume_root = nullptr;
	void rebuild_costume();

	// —— 滚地移动与附着泥/雪系统 ——
	bool is_rolling = false;
	double roll_spin = 0.0;
	bool r_prev = false;
	double roll_track_acc = 0.0;
	float snow_stain = 0.0f; // 身上附着的雪花量 0..1
	float mud_stain = 0.0f;  // 身上附着的泥巴量 0..1
	Node3D *stains_root = nullptr;
	Ref<StandardMaterial3D> mud_dynamic_mat;  // 泥巴动态渐变材质
	Ref<StandardMaterial3D> snow_dynamic_mat; // 雪花动态渐变材质
	void update_body_stains(double delta);

	// —— 踩过发光蘑菇带走微弱荧光粒子系统（持续 4 秒缓慢飘动渐隐） ——
	CPUParticles3D *shroom_glow_trail = nullptr;
	float shroom_glow_timer = 0.0f; // 荧光残留倒计时（秒）
	void build_shroom_glow_trail();
	void update_shroom_glow_trail(double delta);

	// —— 上床睡觉状态与黑屏过场 ——
	bool is_sleeping = false;
	Vector3 bed_pos;
	float bed_yaw = 0.0f;
	float sleep_target_tod = 0.28f;
	int sleep_day_advance = 0;      // 这次睡觉是否跨过午夜（跨过则 day_count +1）
	float sleep_fade_t = 0.0f;      // 过场动画阶段与计时器（秒）
	bool sleep_advancing = false;   // 是否处于时光飞逝纯黑阶段
	bool sleep_key_released = false; // 按键松开防抖，防止按 E 交互上床同一帧立刻中断
	Basis sleep_lie_basis;          // 平躺姿态：身体沿床的长轴、头朝床头
	Vector3 sleep_lie_pos;          // 平躺时 Model 相对 Player 的位移（已减去胶囊中心偏移）
	CanvasLayer *sleep_layer = nullptr;
	ColorRect *sleep_overlay = nullptr;
	Label *sleep_title = nullptr;
	Label *sleep_subtitle = nullptr;
	void build_sleep_ui();
	void update_sleep_cutscene(double delta);

	// —— 湖面滑冰物理与滑冰鞋 ——
	bool is_ice_skating = false;
	bool is_ice_jumping = false;        // 滑冰起跳空中旋转中
	double ice_jump_spin = 0.0;         // 空中 360 度旋转累计角度
	Node3D *skate_shoes_root = nullptr; // 踏上冰面时自动穿上的专业滑冰鞋（银亮合金冰刀+运动鞋靴）
	double skate_track_acc = 0.0;       // 冰刀切冰划痕累积器
	bool skate_leg_left = false;        // 左一下右一下交替滑步标记
	void build_skate_shoes();
	void set_skate_shoes_visible(bool p_vis);

	// —— 二段跳系统 ——
	int jump_count = 0;                 // 当前空中跳跃次数（0: 在地面, 1: 一段跳, 2: 二段跳）
	bool space_prev = false;            // 空格键边沿检测，防止按住长跳
	bool is_double_jumping = false;     // 空中二段跳特效/空翻标记
	double double_jump_spin = 0.0;      // 二段跳空中前空翻角度
	godot::Ref<godot::AudioStreamWAV> make_double_jump_snd(); // 二段跳专属空气动力清脆爆鸣声
	godot::Ref<godot::AudioStreamWAV> snd_double_jump;

	// —— 全地图与传送 ——
	godot::CanvasLayer *map_layer = nullptr;
	godot::Control *map_panel = nullptr;
	godot::Control *map_crosshair = nullptr;
	godot::TextureRect *map_texture_rect = nullptr;
	godot::Control *map_player_marker = nullptr;
	godot::Label *map_coords_label = nullptr;
	bool is_map_open = false;
	bool m_prev = false;
	float map_hover_x = -9999.0f;
	float map_hover_z = -9999.0f;
	void build_map_ui();

	// —— 夜晚放烟花功能（G 键触发） ——
	bool g_prev = false;
	godot::Label *firework_hint = nullptr; // 夜晚右上角或屏幕右侧按 G 放烟花提示
	void launch_firework();
	struct ActiveFireworkFlash {
		godot::OmniLight3D *light = nullptr;
		float lifetime = 0.0f;
		float max_energy = 85.0f;
		godot::Color color;
		ActiveFireworkFlash() = default;
		ActiveFireworkFlash(godot::OmniLight3D *l, float lt, float me, const godot::Color &c)
			: light(l), lifetime(lt), max_energy(me), color(c) {}
	};
	std::vector<ActiveFireworkFlash> active_firework_flashes;

	// —— 互动状态 ——
	class Interactable *near_target = nullptr; // 当前最近的可交互物
	class Label3D *prompt = nullptr;           // 悬浮提示（"按 E 开门"等）
	godot::CanvasLayer *hud_layer = nullptr;   // 交互提示的屏幕空间层（永不裁剪）
	godot::Label *hud_prompt = nullptr;        // 屏幕下方居中的交互提示
	float hud_prompt_timer = 0.0f;             // 瞬态提示自动消失倒计时（秒）
	bool e_prev = false;                       // 上一物理帧 E 键状态（边沿触发）
	double lean_time = 0.0;                    // 推门动画剩余时间（>0 表示播放中）
	Vector3 facing = Vector3(0.0, 0.0, -1.0);  // 模型朝向（用于加前倾角度）

	// —— 第三人称鼠标视角状态 ——
	double cam_yaw = 0.0;    // 环绕角色的偏航角（0 = 相机在 +Z 后方，看 -Z）
	double cam_pitch = 0.47; // 俯仰角（>0 相机抬高向下看；鼠标上抬=看向地平线）
	double cam_dist = 5.5;   // 当前相机距离（滚轮缩放）
	bool mouse_captured = false; // 鼠标是否已捕获（点击捕获；释放与还原由 PauseMenu 统一接管）
	bool lmb_prev = false;

	// —— 鼠标视角必须用"像素位移"，不能用"速度×帧时间" ——
	// get_last_mouse_velocity() 给的是像素/秒，再乘 delta 积分回去，等于拿
	// 上一段事件的平均速度去推算这一帧该转多少：慢速微调转不到位、快速甩又过冲，
	// 帧率一抖灵敏度还跟着变（用户报的"有延迟/有积分"就是这个 + 相机平滑）。
	// 改成 _input 里累加本帧原始位移，_update_look 里一次性消费掉。
	Vector2 mouse_rel;            // 本帧累计位移（消费后清零）
	bool input_seen = false;      // _input 是否真的被派发过（没派发则退回老路线）
	double mouse_sens = 0.0032;   // 偏航灵敏度（弧度/像素，与旧版等效）
	double mouse_sens_y = 0.0028; // 俯仰灵敏度（弧度/像素）
	void set_mouse_sensitivity(double p_v) {
		mouse_sens = p_v;
		mouse_sens_y = p_v * 0.875; // 保持与默认 0.0032/0.0028 相同的纵横比
	}
	double get_mouse_sensitivity() const { return mouse_sens; }

	// 相机跟随点：平滑只作用在这里，环绕（鼠标转动）直接给到位。
	// 原来是对最终机位做 lerp —— 转视角时相机位置在追绕球面的目标点，
	// 时间常数约 1/12 ≈ 83ms，手感就是"视角永远慢你半拍"。
	Vector3 cam_follow;
	bool cam_follow_init = false;

	// —— 脚步声状态 ——
	godot::Ref<godot::AudioStream> snd_grass;      // 普通草甸脚步（优先加载 grass_step.wav）
	godot::Ref<godot::AudioStream> snd_dirt;       // 泥土小径坚实脚步（优先加载 dirt_step.wav）
	godot::Ref<godot::AudioStreamWAV> snd_pitch;      // 足球场平整草坪脚步
	godot::Ref<godot::AudioStream> snd_land_grass;    // 荒草/泥土地跳跃着地沉稳冲击音（优先加载 land_thud.wav）
	godot::Ref<godot::AudioStream> snd_land_wet;      // 雨天踩水跳跃着地饱满水花泥泞冲击音（优先加载 land_wet.wav）
	godot::Ref<godot::AudioStream> snd_land_snow;     // 雪天跳跃着地深陷入雪沉稳挤压音（优先加载 land_snow.wav）
	godot::Ref<godot::AudioStreamWAV> snd_land_pitch; // 足球场专用高弹性草皮吸收缓冲着地音
	godot::Ref<godot::AudioStream> snd_snow;          // 真实雪地踩雪声（用户提供）
	godot::Ref<godot::AudioStream> snd_snow_variants[4]; // 4 种真实踏雪切片变体（源自用户音频）
	int snow_variant_idx = 0;                         // 踏雪变体轮换索引
	godot::Ref<godot::AudioStream> snd_wet;           // 雨天真实踩水声（用户提供 puddle_step.wav）
	godot::Ref<godot::AudioStream> snd_stone;         // 真实石板脚步声（用户提供 stone_step.wav）
	godot::Ref<godot::AudioStream> snd_stone_variants[4]; // 4 种真实石板切片变体
	int stone_variant_idx = 0;                        // 石板踏步轮换索引
	godot::Ref<godot::AudioStream> snd_wood;          // 真实踩木板声（用户提供 wood_step.wav）
	godot::Ref<godot::AudioStream> snd_wood_variants[4];  // 4 种真实木板切片变体
	int wood_variant_idx = 0;                         // 木板踏步轮换索引
	godot::Ref<godot::AudioStreamWAV> snd_rustle;      // 过草沙沙声（普通草地，轻柔）
	godot::Ref<godot::AudioStreamWAV> snd_rustle_deep; // 高草丛拨草声（深沉茂密）
	mutable godot::TerrainBuilder *terrain_cache = nullptr; // 懒查找地形与地貌判定
	godot::AudioStreamPlayer *steps_voice = nullptr; // 脚步声源
	godot::AudioStreamPlayer *rustle_voice = nullptr; // 穿草声源（与脚步分声道）
	godot::AudioStreamPlayer *ui_voice = nullptr;     // 界面/交互声源
	godot::Ref<godot::AudioStream> snd_ui_click;     // 界面点击声
	godot::Ref<godot::AudioStream> snd_ui_back;      // 界面返回声
	godot::Ref<godot::AudioStream> snd_ui_expand;    // 界面滑出/聚焦声
	bool weather_toggle_prev = false;                 // T / F8 键边缘检测
	double rustle_acc = 0.0;                          // 穿草触发距离累计
	std::mt19937 rng{1337};                           // 音高微随机（世界坐标哈希播种）
	double step_acc = 0.0;                            // 已走距离累计（够 2.1m 一步）
	bool foot_side = false;                           // 当前步足印左右交替（false=左脚, true=右脚）
	double bob_phase = 0.0;                           // 走路颠簸相位（与步距同步）
	bool was_on_floor = true;                         // 上一物理帧是否落地（落地音边沿）
	double land_timer = 0.0;                          // 落地倒伏缓冲时间（秒，用于连跳时接地点草叶回弹）
	Vector3 last_land_pos;                            // 上次接地点坐标（世界空间）
};
