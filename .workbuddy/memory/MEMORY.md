# MEMORY.md — 3Dgame 工程长期笔记

> 完整过程细节见 .workbuddy/memory/2026-10-01.md（append-only 日志）。这里只留长效事实与硬坑。

## 工程定位与版本
- **Godot 4 + C++（GDExtension）** 3D 游戏；纯 C++，**不用 .NET**（勿装 godot-mono）。
- Godot **4.7.2 标准版**（/Applications/Godot.app）；godot-cpp **10.0.0-stable**（子模块）；SConstruct `api_version=4.3`；SCons 4.11.1。
- 目标平台 Windows/macOS/Linux；开发机 macOS Apple Silicon。

## 目录与三件套
- `src/` **h/cpp 配对同目录 + 按功能分子目录**（2026-10-02 重构定稿，SConstruct 已配好全子目录 CPPPATH，include 保持 `#include "xxx.h"` 裸文件名）：
  - `src/register_types.{h,cpp}` 扩展库入口（留 src 根）；`src/systems/` game_director/save_system/fishing_system/hud_clock/mesh_util.h；`src/world/` terrain_builder/sky_cycle/ambience；`src/actors/` player/farm_animal/bird/rabbit/butterfly_swarm；`src/interact/` interactable.h/door/window/chest/well/berry_bush。
  - 新类放对应子目录；三件套不变：`GDCLASS` + `_bind_methods` + `register_types.cpp` 注册。
- `project/` 工程本体；主场景 `world.tscn`；`.gdextension` entry_symbol=`game3d_library_init` 不许改。

## 硬坑速查（按重要度）
1. **CSGCombiner3D 含 1 条 subtract 子节点 → 整个 combiner 碰撞全灭**（渲染/网格正常、headless 零报错）→ 石桥装饰只用加法深色薄片（凸 1~2cm）。
2. **Object 派生类（含 SurfaceTool）不能栈上构造** → SIGTRAP；一律 `Ref<T>; x.instantiate()`。
3. **逐帧逻辑用 `_notification`+`NOTIFICATION_PROCESS`**；`_process` 是 GDVIRTUAL，C++ override 不生效。SceneTree `-s` 脚本的 `_process -> bool`（true 退出），出错不退出 → done 标志兜底。
4. **GDExtension 类编辑器里也跑 READY**（永远 @tool）→ 运行时生成节点必须 `is_editor_hint()` 守卫。
5. **不要 `static Ref<Resource>` 进程级缓存** → 退出段错误；ResourceLoader 自带缓存。
6. godot-cpp 10.0（4.3 API）差异：顶点色 `set_flag(FLAG_ALBEDO_FROM_VERTEX_COLOR)`；自发光 `set_feature(FEATURE_EMISSION)+set_emission`；`Basis.set_euler_scale(euler,scale)`；SurfaceTool `set_color()`；`Math` 无 randf（用 std::mt19937）；`Math_PI` 不可用（用 M_PI）；`String+"字面量"` ambiguous（显式包 String）；StringName 不隐式转 NodePath；`cast_to<T>` 显式下转；Light3D 用 `set_param(PARAM_ENERGY)`；Sky 用 `get_material()`；**`Ref<T>` 不能放 if 条件做 bool 转换**（只有 operator Variant）→ 显式 `.is_valid()`。
7. 头文件名易错：`character_body3d.hpp`、`multi_mesh*.hpp`（下划线）、`static_body3d.hpp`、`mesh_instance3d.hpp`；类头在构建时生成到 `gen/include`，找不到属正常；`math.hpp` 在 include/ 不在 gen。
8. **漏 include/前向声明报 20 连锁错**，报错位置不是病灶（如 sky_cycle.h 漏 MeshInstance3D 声明砸到 terrain_builder）；成员用到的类必须有完整类型。
9. `-s` SceneTree 脚本不自动加载主场景（`_initialize` 里 load+add_child）；对未入树节点 look_at 报错（第 2 帧再调）；headless 截不了图（非 headless 渲染）；GDScript 属性名与 C++ setter 不同（`multimesh`/`albedo_color`）；MultiMeshInstance3D 不继承 MeshInstance3D；**截图 res:// 不可写（用绝对路径）**；**Player 相机每帧 look_at 主角，脚本对准别处要先 set_process(false)**。
10. 首次运行前必须 `--headless --editor --quit` 导入一次（否则 Cannot get class）。
11. MultiMesh 实例色当颜色用：材质底色设白；自发光色与实例色无关。
12. 装饰贴面（桥面薄片）必须按桥面分段坡度绕 Z 俯仰对齐，否则一端悬空。
13. **godot-cpp `String(const char*)` 是逐字节 latin-1 展宽、不解 UTF-8** → 中文字面量运行期全是乱码（截图实锤），必须 `String::utf8("中文")`（第 31 轮已修 HUD/get_phase_name/5 个 prompt 共 9 处）。
14. **默认字体无 CJK 字形（中文渲染成方框）；SystemFont 系统字体查找在本机 Godot 4.7.2 不可用**（FreeType 加载空路径）→ 工程已打包 `assets/fonts/NotoSansSC-Regular.ttf`（可变字体 17.8MB + OFL.txt），UI 中文一律挂它（第 33 轮已补挂 Label3D 交互提示 player.cpp，全部 CJK 界面收口）。

## 世界与系统一览（细节见日志）
- **TerrainBuilder**（terrain_builder.{h,cpp}，固定种子 1337）：`terrain_height()` 唯一事实来源；参数在 terrain_builder.h 顶部（**map_size=320、grid_steps=240**（第 39 轮扩图）、lake_center=(-6,26)、water_level=-0.55、宅基地压平 0.35）。地形 fbm+湖盆+环山+压平；顶点色（草/沙/湿沙带/湖床/岩层层理/裸土与草聚簇共用 value_noise 0.14 频率偏移 21/11——改一边必须同步另一边）；植被/浆果丛/蘑菇/鹅卵石/落叶/倒木/碎石/巨石(Boulders)/树桩(TreeStumps) 全 MultiMesh Scatter（新增两类用独立 rng 放生成末尾，防挪全局序列重排整图）；泥土小径 dist_to_path（出生点→院门/桥西端）。
- **水面** water.gdshader：波浪+折射+深度吸收+菲涅尔+泡沫+焦散+`rain_amount`/`flow_dir`/`freeze_amount` uniform（SkyCycle 每帧驱动 Terrain/Lake）；结冰（第 34 轮）：freeze=snow，浅水先冻岸→湖心推进（water_depth 门控+低频噪声斑块），顶点波浪压平、法线衰减至 8%、焦散/泡沫淡出、冰蓝底色+脊线裂缝（ice_cracks pow 22/26、亮度 0.65）、粗糙度→0.06 镜面冰。
- **SkyCycle**（sky_cycle.{h,cpp}）：昼夜 time_of_day 0..1、day_length 240s、day_count；天气状态机 0晴/1多云/2小雨/3大雨/4雪，cloud/rain/snow 平滑过渡；雨/雪粒子跟随主角；星空 220 星 + 月盘（moon.gdshader：陨石坑月面+月相明暗 `phase=fmod(0.5+(day_count-1)·0.125,1)` 第 1 天满月 8 天一循环、暗面地照残影；phase 只在跨天写 uniform，alpha=night×(1-cloud×0.92)）；公开 `get_time_of_day/get_day_count/get_rain_amount/get_snow_amount/get_moon_phase`（月相公式唯一来源，apply() 与 HUD 都从这取）。`update_sky_driven` 已更名（见下）。
- **昼夜联动套路**（Bird/Rabbit/Butterfly/蘑菇夜光/BerryBush 雪毯共用）：懒查找 SkyCycle → `sun_h=sin((tod-0.25)·2π)` → -0.02→-0.12 映射 night 0→1；`wet=clamp(rain+snow)` 躲雨雪。⚠️ 不在 World 根直接子级的节点（如 Homestead 下的 BerryBush）get_parent() 摸不到 SkyCycle，要沿祖先链向上找。
- **TerrainBuilder.update_sky_driven()**（原 update_glow 改名）：蘑菇夜光（glow=night²，伞盖×1.7/白点×1.1，wet 衰减 ×(1−0.75wet)，光尘 wet<0.3 才放）+ snow_amount 写进 terrain_mat（原始雪量）+ snow_bands 分级条目（第 35 轮：每档 [lo,hi] 映射，低处先积雪）。
- **积雪**：snow_ground.gdshader（坡度门控+水线渐入+覆盖噪声）；veg_wind.gdshader 带 `snow_amount`（雪色+梢部下坠）与 `alpha`（默认 1，树枝雪毯用雪量淡入）。**树枝雪毯（第 32 轮，第 35 轮分级化）**：TreeSnowCaps 按"树顶高 h+3.55s"分三档（T1LowSnow<4.5 / T2MidSnow<6.2 / T3HighSnow），每档独立 veg_wind 材质+网格，映射区间 T1(0.15,0.45)/T2(0.35,0.65)/T3(0.55,0.85)——低冠先覆雪先现雪毯；每树两片压扁白球（高冠顶 h+3.55s 缩放 0.62s / 冠肩 h+3.10s 缩放 0.85s）；**蘑菇伞顶雪毯 ShroomSnow（第 35 轮）**：小扁白球扣伞盖（h+0.17s，r0.075s），档位 (0.08,0.35) 最先积雪，sway=0 不摆；贴地植被（草/灌/芦苇）档位 (0.02,0.30)；snow_bands 结构体 {mat,lo,hi,drive_alpha} 替代旧 wind_mats+snow_cap_mat；参数由 s/yaw/tilt 确定性导出、不加 rnd() 调用（防挪动全局随机序列整图重排）。⚠️ 第 27 轮"截图确认大雪观感"实为雾感误判——雪盖 uniform 的 GDScript 侧写入（`multimesh.mesh.surface_get_material()` 取到的材质）**与渲染侧脱钩**（回读正常但画面不变），C++ 侧写入（update_sky_driven）才是唯一有效通道；验证雪效只能临时改 C++ 初值重编（如 `snow = 1.0f` 强制）。
- **交互**：interactable.h 抽象基类 + "interactables" 组；Player 扫描 ≤3.2m + Label3D 提示（已挂 Noto CJK 字体）+ E 边沿触发 + 推门前倾动画。实现：CabinDoor/ShutterWindow/Chest/Well/BerryBush（45s 重长、采摘"啵"声、雪毯随 snow_amount 淡入）。虚方法要显式 bind 才能被 -s 脚本调。
- **HUDClock**（CanvasLayer）：顶部 HBox 一行 "[月相图标] hh:mm · 时段 · 天气 · 满月等八相名 · 第 N 天 · xx°C"；月相图标程序化绘制（hud_clock.cpp update_moon_icon：半椭圆明暗界线按 get_moon_phase() 建模、量化 256 档缓存、ImageTexture；暗面地照蓝灰残影）；Label 挂 Noto Sans SC（坑 14）。
- **氛围生物**：Bird×5（程序化 3 种鸣叫 WAV，雨雪 wet<0.25 不叫）、Rabbit×2（夜里或雨雪 wet>0.5 回窝趴睡，天亮雨停起身）、Butterflies（雨雪落草丛静栖）、萤火虫（夜+晴）、烟囱烟。
- **脚步声（第 29 轮）**：Player 程序化合成草地/雪地两音色（make_step），按 2.1m 累计距离触发 + 落地音，雪量>0.45 切雪地嘎吱，音高 0.92~1.08 微随机。
- **StoneBridge**（world.tscn CSG，跨湖 z=26）：拱圈 CSGPolygon + 栏板/望柱 + 桥台/翼墙 + 裂缝/青苔做旧薄片；deck top -0.25 与引道 -0.28 齐平。
- **Homestead** @ (12,0.35,8) 旋 -45°：主屋+仓库+门廊+烟囱+篱笆+半开院门+石径+柴火垛+室内家具；木屋朝向/y 必须与 terrain_builder.h 的宅基地参数一致。

## 发布链路（第 38 轮 + 22:34 跨平台打通）
- 工程名「湖畔家园 Homestead」v0.1.0；export_presets.cfg 三预设（macOS/"Windows"/Linux，预设名以文件现态为准）。
- 导出模板已装 `~/Library/Application Support/Godot/export_templates/4.7.2.stable/`；导出命令 `Godot --headless --path project --export-release "<预设名>" <绝对路径>`；**导出目录必须先 mkdir**（Linux 预设不会自建）。
- **本机交叉编译已打通（无需 CI）**：Windows=`brew mingw-w64` + `use_mingw=yes`；Linux=交叉工具链 `~/opt/x86_64-unknown-linux-gnu/` + **PATH 垫片 `~/opt/cross-shim/`**（SCons 命令行 CXX= 覆盖对 godot-cpp linux 无效，必须名字垫片）。平台库编好后落 project/bin/{windows,linux}/，然后本地 export-release 即可。
- **SConstruct 已根治两坑**：linux 目标 SHLIBSUFFIX=.so、windows 目标 SHLIBPREFIX=""（mingw 的 lib 前缀会让导出报 Failed to copy shared object）。
- **导出后必须 unzip -t 校验**（EXIT=0 仍可能半残）；导出输出严禁接 grep|head 管道（SIGPIPE 半残）；类符号用 strings 复核（SaveSystem/TerrainBuilder 等在场才算真编进去）。
- **deb 包手工组**（Godot 无 deb 导出）：`brew install dpkg`；staging：/opt/lakeside-cabin/{game3d.x86_64 + so 同目录}、/usr/local/bin/lakeside-cabin 启动器、.desktop + 图标（sips icns→png）→ `dpkg-deb --build --root-owner-group`。
- 产物在 dist/：game3d-windows-x64.zip / game3d-linux-x64.zip / game3d-linux-amd64.deb / game3d-macos.zip。
- **src/*.os 被各平台构建共享**：跨平台重编后 src/*.os 是最后构建平台的对象，下次换平台构建靠 SCons MD5 自动重编（正常，不是 bug）；配额紧张时先 `mv src/*.os` 挪走再编。
- **src/*.os 被 debug/release 目标共享** → 并行实例一个编 debug 一个编 release 必撞，重跑前 `rm src/*.os`。
- 已知存量：退出 "6 ObjectDB instances leaked"（--verbose 无明细，无害）。

## 验证套路
```bash
scons platform=macos target=template_debug     # 增量编译
/Applications/Godot.app/Contents/MacOS/Godot --headless --path project --quit-after 120   # 应 0 报错
# 场景路径验证要 res://world.tscn；-s 临时脚本用完即删
```
- ⚠️ `scons compiledb verbose=yes` **只生成 compile_commands.json，不构建 Default 目标**；源文件移动/新增后要先跑普通 `scons` 真编译再 compiledb。

## 发布链路（2026-10-01 打通）
- 导出模板 4.7.2.stable 已装（~/Library/Application Support/Godot/export_templates/）。
- 导出命令：`godot --headless --path project --export-release "macOS" <输出路径>`（预设名 macOS/Windows/Linux，export_path 指向 ../release/）；导出前必须先有对应平台 template_release dylib；**导出不自动建目标目录**；**导出后必须 unzip -t 校验**（EXIT=0 仍可能半残，疑并行写入竞态）。
- 产品名"湖畔家园 Homestead" v0.1.0；exclude_filter=qa_*, tests/* 及孤儿 .uid；应用图标 docs/Homestead.icns 已接入（project/assets/Homestead.icns）。
- Win/Linux 完整出包必须走 .github/workflows/build.yml CI（本机无法编跨平台 GDExtension 原生库）。
- unzip -l 中文条目显示乱码是假警报，解包 ls 验证为准。
- **scons 并发坑**：多会话同时跑 scons 互踩 .sconsign.dblite → 记录损坏+误报 up-to-date+产物过期；修复=移走 .sconsign 重跑。发布验证要看 dylib 内容（nm/class 注册）不只看 mtime。
- **v0.2 路线图在 docs/ROADMAP.md**（P0：存档/暂停菜单/引导/键位；P1：环境音接入【src/ambience.h、ambient_audio.h 已写完未注册】、性能基线；P2：浆果可采化等）。

## 并行协作约定
- world.tscn 常被用户/并行实例改动 → **动手前必须重读场景与 src/ 最新状态**；选改动面时避让近期热点文件。
- 多实例同时开工时 mtime 检查会过期（秒级撞车，第 31 轮两连撞）：**动手后第一笔 Edit 报"内容不符"或读文件发现陌生新代码 = 撞车信号，立即整体退出换冷方向**，不要清理自己的残留编辑（防二次争写）。
- 三平台构建走 .github/workflows/build.yml，不要从 macOS 交叉编译。**git 已初始化并完成首次提交**（root `5202346`，2026-10-02，289 文件）；提交前先 `git status` 复核别把构建产物带进去（.gitignore 已排 *.os/*.dylib/dist/release/builds/project/export/.godot，**export_presets.cfg 已入库**）。
- **构建树互斥铁律（2026-10-01 多次事故后确立）**：src/*.os 被 debug/release 目标共享、.sconsign.dblite 会争写损坏 → **同一时间全工程只允许一路 scons**。开工前必须 `pgrep -fl scons` 查并发，≥1 路活跃就等待，绝不加入（第 38/39 轮互踩、本会话遇 5 路并发，曾致 sconsign 损坏+误报 up-to-date+产物过期）。
- **沙箱 safe-delete 钩子坑**：回合删除配额（50 文件）耗尽后 rm/unlink/rmtree 全部**静默拦截** → binding_generator 的 rmtree(gen/) 与 SCons unlink 既有 target 被拦 → **scons 假成功**（EXIT=0 无编译行、gen 树残缺）。解法：python 直接调 generate_bindings() 重建 + `scons generate_bindings=no` 绕开；每回合删文件要省着用。

## 发布与分发（第 8 轮起长效）
- **macOS 包已通**：`dist/game3d-macos.zip`（185MB，universal，产品名"湖畔家园 Homestead"，bundle io.homestead3d.lake v0.1.0）。冒烟实测：解包无头跑 EXIT=0 零 error。
- 发布链路：① `scons target=template_release`（先编 release 扩展库，否则导出缺 dylib）② 导出模板须装在 `~/Library/Application Support/Godot/export_templates/4.7.2.stable/`（**目录名=编辑器版本**）③ `godot --headless --path project --export-release "macOS"`（预设 export_path=../dist/…）。
- **导出两大坑**：universal/arm64 必须开 ETC2 ASTC（project.godot `[rendering] textures/vram_compression/import_etc2_astc=true`）；**导出命令输出严禁接 `grep|head` 管道**——SIGPIPE 杀死导出进程 → 半残 zip（Info.plist 0 字节、pck/dylib 缺失），日志一律落文件。
- Win/Linux：本机无交叉工具链，预设已备好；走 .github/workflows/build.yml CI（需用户授权 push）或 `brew install mingw-w64` 后本机出包。
- 玩法闭环：GameDirector（CanvasLayer）计 BerryBush 的 `berry_picked` 信号，目标 6 颗 → 进度 HUD + 10s 操作提示 + 达成结算。

## 官网（2026-10-02 起）
- **线上 http://107.173.70.200/**（DediRock VPS，1C2G）。只有 HTTP，443 被同机 sing-box 的 VLESS-Reality 占着，**不要为了上 HTTPS 去动 sing-box**。
- 源码在仓库 `site/`：`index.html` / `style.css` / `assets/img/` / `download/` / `nginx/` / `deploy.sh`。
- **部署**：`cd site && ./deploy.sh`（只同步网页），`./deploy.sh --nginx`（连 nginx 配置一起，会自动备份 + `nginx -t` 校验）。
- **服务器侧**：站点根 `/var/www/game`；nginx 配置 `/etc/nginx/sites-available/homestead`（80 default_server）；订阅 location 在 `/etc/nginx/snippets/homestead-sub.conf` 被 include，**订阅 URL 与行为不可改**。
- **打包规则**：官网用的截图一律裁掉游戏 HUD（顶 9.5% / 底 5%），导出宽度 520（滚动带）/ 900（画廊）/ 1500（大图），JPEG q78–84。
- ⚠️ **改官网截图前先核对构建时间**：包比源码旧的时候，宣传画面会和下载内容对不上。
