# 《湖畔家园 Homestead》v0.2 路线图

> 目标：从"可玩沙盒 v0.1"推进到"可发布 v0.2"——新玩家打开游戏有目标、有引导、有存档意义、有退出节奏。
> 依据：`.workbuddy/memory/2026-10-01.md`（36+ 轮迭代日志）、`MEMORY.md`（硬坑）、`src/*.h` 系统边界、`project/world.tscn` 结构。
> 世界一致性约束：**地形参数固定种子（terrain_builder.h 顶部），任何改布局的改动都会整图重排**——路线图默认不动地形布局。

---

## 一、现状清单（v0.1 能力盘点）

### 1. 玩法循环
- **移动与交互**：WASD + 空格（`src/player.h`，键位硬编码 C++，未走 InputMap）；E 键边沿触发交互，Player 每帧扫 "interactables" 组找最近（≤3.2m）+ Label3D 悬浮提示（已挂 CJK 字体）+ 推门前倾动画。
- **唯一玩法目标**：GameDirector（`src/game_director.h`）监听 BerryBush 的 `berry_picked` 信号，目标 6 颗 → 左上进度 + 底部操作提示（10s 淡出）+ 中央结算（8s 淡出）。**结算之后没有任何后续**。
- **交互物 5 类**：CabinDoor（推门，按站位定方向）、ShutterWindow、Chest、Well（打水动画）、BerryBush（园里 3 丛，45s 重长循环）。木箱是空的、井水无用途——纯摆设交互。

### 2. 内容量
- 320×320m 固定种子地图（`terrain_builder.h`：map_size=320、grid 240、湖心 (-6,26) r15、宅基地压平 0.35）；泥土小径、湖、石拱桥（CSG 拱圈+桥台+翼墙+裂缝青苔做旧+桥头石灯）。
- 植被 MultiMesh：树（42% 结果，纯装饰）、灌木、4200 草、芦苇、睡莲、花、石、浆果丛 14 簇（**林缘浆果丛是装饰，不可采**）、蘑菇（夜光+孢子）、鹅卵石、落叶、倒木、碎石坡、巨石、树桩。
- 宅基地：主屋+仓库+门廊+烟囱+篱笆院+半开院门+石径+柴火垛+室内家具（床/桌/凳/箱/地毯）。

### 3. 引导与反馈
- HUDClock 顶栏：`[月相图标] hh:mm · 时段 · 天气 · 月相名 · 第 N 天 · xx°C`。
- GameDirector 的操作提示只显示 10 秒；交互提示靠走近物体时的 Label3D。
- **无任何指向性引导**：没有告诉玩家"去哪、干什么、浆果在哪"。
- 天数/气温是纯装饰数值，与玩法零关联。

### 4. 音频
- 已有：脚步声 4 音色（草地/雪地/雨湿泥/石板，按天气+地形自动切换、音量差异化）、鸟鸣 3 种（雨天不叫）、采摘"啵"声——全部程序化合成，无音频文件。
- **已写未接入**：`src/ambience.h`（风/雨/虫鸣四层环境音）与 `src/ambient_audio.h`（虫鸣+雨声）两个类**均未注册（register_types.cpp 无）、未挂场景**——环境音层目前实际是缺失的。
- 无音量设置、无音频总线。

### 5. 视觉
- 卡通渲染（toon.gdshader）；CS2 风格水面（折射/焦散/泡沫/雨涟漪/结冰）；昼夜 4 分钟循环 + 五态天气（晴/多云/小雨/大雨/雪）+ 分级积雪（树冠三档/蘑菇/贴地植被）+ 湖面渐冻 + 月相 8 天循环 + 星空 + 桥灯常亮。
- 氛围生物：鸟×5（夜归/雨天低飞收翼）、兔×2（夜/雨雪回窝）、蝴蝶群（雨雪静栖）、萤火虫（晴夜）、烟囱炊烟。

### 6. 性能
- 植被 MultiMesh 每种一次 draw call（设计良好）；雨雪粒子 CPUParticles3D 跟随主角。
- **无任何实测数据**：没有帧率统计、draw call/三角数预算、低配门控。草 4200 + 全场景 SSAO/SSR/Glow 在低配机器上的表现未知。

### 7. 发布工程
- macOS 包已打通：`dist/game3d-macos.zip`（185MB，universal，冒烟通过）；export_presets.cfg 三平台齐（产品名"湖畔家园 Homestead" v0.1.0，exclude_filter 已排除 QA 脚本）；CI build.yml 三平台构建。
- **未收口**：包内 dylib 曾是 template_debug（release 库需在编辑器关闭窗口期编）；`project.godot` 的 `config/name` 仍是 "3D Game (Godot + C++)"；应用图标未替换（docs/ 已备 Homestead.icns/icon_art.png）；Win/Linux 从未出过包；无版本号统一管理（README/presets/代码各说各话）。

---

## 二、可玩性差距分析（新玩家打开游戏的 10 分钟）

按时间线走一遍，**加粗处是断点**：

| 分钟 | 发生什么 | 断点 |
|------|----------|------|
| 0:00 | 出生在小平台，顶部 HUD 时钟、底部操作提示淡入 | ✅ 有基本交代 |
| 0:10 | 操作提示 10 秒后淡出 | ⚠️ 太快，且没说**该干什么** |
| 0:30 | 四处走走，看到小径两个方向（院门/桥） | ⚠️ 无指向：不知道哪边有目标 |
| 1:30 | 摸到木屋，开门/窗/箱/井各玩一遍 | ⚠️ 箱子空的、井水没用途，新鲜感 1 分钟耗尽 |
| 2:30 | 发现院里浆果丛（或者没发现——提示只在 3.2m 内出现） | 🔴 **唯一目标可能根本没被发现** |
| 4:00 | 采满 6 颗，中央弹"目标达成" | 🔴 **游戏到此结束：无事可做** |
| 5:00 | 看看桥、湖、山；等一场雨/雪 | ⚠️ 氛围出色但没有理由继续 |
| 7:00 | 退出游戏 | 🔴 **无存档：第 N 天、时间、目标进度全部清零**；也没有暂停/退出菜单，只能 Cmd+Q |

**四个核心断点**：
1. **目标单一且可能不可见**——6 颗浆果 ≈ 2 分钟节奏，完成后世界没有任何回应；林缘 14 簇装饰浆果丛近在眼前却采不了，是天然的负反馈。
2. **无引导**——玩家不知道目标是什么、在哪；没有存档/暂停/退出的"游戏感"框架。
3. **无存档意义**——天数、月相（8 天一循环）这些精心设计的系统，退出即蒸发。
4. **无退出节奏**——没有达成后的仪式感（回屋睡觉结算？）、没有下一目标，玩家只会摸不着头脑地退出。

---

## 三、v0.2 路线图（按优先级）

> 每项验收标准均含：scons 0 error + `res://world.tscn --quit-after 120` 零 error/warn 的通用回归（不再重复写）。
> 多实例并行开发时遵守 MEMORY.md 协作约定：动手前查 mtime、第一笔 Edit 撞车立即换冷方向。

### P0 —— 可玩性核心

#### 1. 存档系统（退出有据，天数有意义）✅ 已完成（2026-10-24；主体随 #2 批次交付，门窗状态同日补课收官）
> 实现备注：新类 `src/systems/save_system.{h,cpp}`（ConfigFile 存 `user://savegame.cfg`）——触发三入口：READY 读档一次、每 30s 自动存、EXIT_TREE/WM_CLOSE_REQUEST 兜底存（暂停菜单"保存并退出"走 save_now 同一格式）。**v1→v5 键位增量**：v1 SkyCycle 全状态（time_of_day/day_length/day_count/weather + cloud/rain/snow 六因子，*_target 必须一起存，防读档后向 target 自衰减）+ director count/done；v2 director_daily 每日清单全量序列（v1 旧档折算第 1 天浆果清单走同一恢复路径）；v3 farm_plots 菜畦状态（经 GameDirector 聚合/回放，spawn 前挂 pending）；v4 chest_looted 木箱彩蛋标志（回放只补摆毯不发公告）；v5 门窗开合状态——`door_state`（"0"关/"1"推/"-1"拉，连开向一起存）+ `windows_open`（"1,0,0,1" 定位序 FL,FR,L,B 固定路径）。**回放纪律**：字符串键缺省必须空串（防同进程先后读两份档串状态）；存前从节点拉最新值、缺源保留缓存（退出拆树期不得拿默认值覆盖好数据）；门窗回放走 set_open_state 快照就位（不补间不配音，枢轴姿态直接落位）；非法/缺失序列一律落回关闭，旧档缺 v5 键=全关无损迁移。附 trim_logs（user://logs >10MB 按最旧清理）。v5 补课验证：-s 25 断言全 PASS（冷启动全关/键值落盘/含姿态回放推拉 ±1.92/四窗定位序/非法序列容错/旧档缺键迁移+重存回归）+ 120 帧冒烟 0 SCRIPT ERROR + dist/ 四包重出（04:25）。
- **动机**：天数/月相/目标进度是现有最有价值的系统资产，存档让它们产生意义；也是"退出节奏"的地基。
- **验收**：退出重进后，第 N 天 / 时段 / 天气 / 浆果目标进度 / 湖畔小屋门窗状态恢复；首次启动无存档文件时正常新开；存档写坏/缺失字段能降级不崩。
- **涉及**：新类 `src/save_system.{h,cpp}`（ConfigFile 或 JSON 存 `user://`）；GameDirector/SkyCycle 提供 save/load 接口。
- **规模**：M
- **风险与坑**：GDExtension 类退出顺序敏感（MEMORY.md 坑 5：勿用 static Ref 缓存）；保存时机放 `_notification(NOTIFICATION_WM_CLOSE_REQUEST)` 与暂停菜单双入口；SkyCycle 的 weather 是内部状态机，恢复时要同步 cloud/rain/snow 六个因子，漏 snow_target 会像日志第 35 轮那样"自己往 target 衰减"。

#### 2. 目标链与结算后节奏（每日清单）✅ 已完成（2026-10-24）
> 实现备注：GameDirector 升级为每日待办清单——第 1 天采浆果 ×6 引导不变；第 2 天起按天数种子从 采浆果/钓鱼/足球进球/打井水/开木箱 五类确定性抽 3 项（同一天读档清单一致）；当日完成弹中央结算并提示"回床睡觉"；SkyCycle day_count 跨天（自然午夜或床上睡觉）自动发布新清单。FishingSystem/SoccerBall/Well/Chest 新增 fish_caught/goal_scored/water_drawn/chest_opened 信号，照 berry_picked 懒连接套路接入；SaveSystem v2 存档新增 director_daily 序列（"day:type,prog,target;..."），v1 旧档自动迁移为第 1 天浆果清单。无头功能自测（-s 临时脚本）+ 带渲染截图验证通过。
- **动机**：把"采 6 颗浆果"扩成可持续的目标循环——达成后不空转。
- **验收**：首日目标 = 采浆果 6 颗；达成后弹出"第 1 天完成"，进入次日清单（从现有交互物取材：开门·开箱·打井水·采浆果·夜里看一次月亮 之类 3 选 2）；HUD 左上常驻当前目标；至少支撑 3 个游戏日的目标循环。
- **涉及**：`src/game_director.{h,cpp}`（目标状态机 + 新事件信号）；Well/Chest/Door 加一次性事件信号（照抄 berry_picked 套路）。
- **规模**：M
- **风险与坑**：新信号照 BerryBush 的 `ADD_SIGNAL` + 懒连接组套路（READY 顺序无关）；虚方法要显式 `ClassDB::bind_method` 才能被 -s 脚本测（第 28 轮坑）；不要为此新加可交互物（超出小品体量，见"不做什么"）。

#### 3. 新手引导与目标指向 ✅ 已完成（2026-10-24；常驻待办面板 + 本轮补齐方向指示与提示驻留，三条验收全达）
> 实现备注：三条验收逐条落位——①"开局 HUD 显示首目标文案并保持"=每日待办常驻面板（左上玻璃底板，无淡出）；②**方向指示**：GameDirector 懒建世界空间 Label3D 标记「▼ 任务名」（billboard + 穿深度 FLAG_DISABLE_DEPTH_TEST，金色描边，sin 浮动），悬在清单**第一个未完成项**的目标上方——采浆果=就近果满丛（`get_prompt()` 非空判果满，采空自动换丛防标记跳动）、打井水/开木箱=interactables 组按类型解析、足球=SoccerBall 跟球走、**钓鱼无静态目标节点（竿在玩家手上）→ 指湖心**（LakeCenter 与 fishing_system.h 同源固定值，走近 12m 内由"按 F 抛竿"提示接管），玩家距目标 5m 内收牌（交互悬浮提示接管）；③**提示驻留**：新档（第 1 天且零进度）底部操作提示不再 10s 固定淡出，驻留到五个任务信号任一首次触发（note_first_interaction → 1s 全显 + 2s 渐隐），读档恢复 day>1 或已有进度则翻回常规 10s 淡出（apply_daily_serial 内翻转）。验证：-s 21 断言两轮全 PASS（标记懒建/文案切换/湖心定位/井台对位/贴近收牌/当日完成收牌/驻留与首次交互淡出/老玩家常规淡出/极简场景不崩）+ 120 帧冒烟 0 SCRIPT ERROR + 带渲染截图两轮（园丛标记、井台标记 + 第 2 天待办面板联动）。**测试踩坑**：HOME 隔离后仍须在脚本开头删隔离 HOME 内的 savegame.cfg——上一轮退出兜底存档会把 day-2 状态带进下一轮（断言串状态的隐形源）。
- **动机**：10 分钟视角最大断点——玩家不知道"去哪、干什么"。
- **验收**：开局 HUD 显示首目标文案并保持（不只 10s）；首次靠近浆果丛前，屏幕有方向指示（目标丛上方世界空间标记，或 HUD 指南针箭头）；操作提示保留至玩家完成首次交互再淡出。
- **涉及**：`src/game_director.{h,cpp}`（目标文案常驻）、`src/player.{h,cpp}` 或新标记节点（Label3D 已有 CJK 字体先例可复用）。
- **规模**：S
- **风险与坑**：世界空间标记别用 CSG subtract（硬坑 1：一条 subtract 全灭 combiner 碰撞）；Label3D 中文必须 `String::utf8()` + Noto 字体 override（硬坑 13/14，player.cpp 第 33 轮已补挂，新代码照抄）。

#### 4. 暂停菜单 + 退出 + 设置 ✅ 已完成（2026-10-24）
> 实现备注：新类 `src/systems/pause_menu.{h,cpp}`（CanvasLayer layer 100，PROCESS_MODE_ALWAYS），world.tscn 挂在 Player **之前**（Esc 当帧先暂停世界，Player 同帧跳过、看不到幽灵按键边沿）。Esc 轮询边沿（照 player.cpp 套路，不用 _unhandled_input 规避 GDVIRTUAL 坑）；三态机 idle→open→resuming（菜单隐藏后**等 Esc 松开才真正解除暂停**，防 Player 的"Esc 释放鼠标"旧逻辑吃到边沿）。继续 / 音量滑条（Master 总线线性 0..1，持久化 user://settings.cfg，与 savegame.cfg 互不干扰）/ 全屏切换（最大化↔全屏）/ 保存并退出（SaveSystem::save_now 兜底 quit）。未动 project.godot（Esc 走轮询不需要 [input]），未动 player.cpp 键位。无头 15 项断言 + 带渲染截图（中文/布局/50% 音量跨运行恢复）验证通过。
- **动机**：完整"游戏感"框架；设置里至少要音量滑条（脚步声 -10dB 与风声并存，玩家无调节手段）。
- **验收**：Esc 呼出暂停层（暂停世界：set_process/physics 全停或 get_tree().paused）；含 继续 / 音量 / 全屏切换 / 保存并退出；暂停时 HUD 正确冻结；全屏切换即时生效。
- **涉及**：新类 `src/pause_menu.{h,cpp}`（CanvasLayer）；`project/project.godot` 加 `[input]` 段。
- **规模**：M
- **风险与坑**：GDExtension 场景内暂停要处理 `_notification` 路径下 NOTIFICATION_PAUSED；中文 UI 全部 String::utf8 + Noto（硬坑 13/14）；InputMap 补进来后**不要顺手把 player.cpp 的硬编码键位全迁移**（那是下一项的事，一次改一片）。

#### 5. 键位收口 InputMap + 相机转向 ✅ 已完成（2026-10-24）
> 实现备注：project.godot 新增 `[input]` 段 11 个动作（move_forward/back/left/right、jump、interact、roll、firework、map_toggle、camera_left/right，全部物理键码）；player.cpp 全部轮询键位改走 InputMap（act_pressed 动作优先 + 键码兜底，极简测试场景无 [input] 也不崩）。新增键盘转向兜底：`←`/`→` 恒速环绕（≈137°/s，无需鼠标捕获，与鼠标视角可叠加；WASD 本就随 cam_yaw 旋转，无需改动）。Esc 全局单一归属 PauseMenu：呼出 / 菜单内再按返回游戏（补上 tip 承诺的行为）/ 大地图打开时只收起地图（修复"关地图+开暂停"同帧双响应）；player 删除"Esc 释放鼠标"旧逻辑（释放与还原由 PauseMenu 存/还原鼠标模式接管）。-s 自测 25 断言全绿（动作键码、转向机位移动 3.3m、动作驱动位移、地图 Esc 不暂停、暂停三态链路）；带渲染截图验证转向前后与暂停菜单。
- **动机**：README 自己承认键位绕过 InputMap 是技术债；第三人称固定朝向相机在湖面/桥上体验差，至少给一个转视角手段。
- **验收**：project.godot 有 [input] 段，player 改读 Input.is_action_pressed；新增 Q/E 或方向键旋转相机（E 已被交互占用，用左右方向键或 Q/R 之类，定一套无冲突键位并更新 README）；WASD 随相机朝向。
- **涉及**：`project/project.godot`、`src/player.{h,cpp}`、`docs/PLAYING.md`。
- **规模**：M
- **风险与坑**：player 是高频热点文件（日志里三实例曾同时争写），动手前查 mtime；相机 lerp 跟随逻辑改动要防"模型本体不转、仅 model look_at"的既有约定被破坏（第一轮设计：本体不转保相机稳定）；改完必须实机截图验证（headless 截不出图）。

### P1 —— 发布工程收口

#### 6. 环境音接入 + 音频总线 ✅ 已完成（2026-10-24，会话E 收尾总线）
> 实现备注：Ambience 类（src/world/ambience.{h,cpp}）注册（register_types.cpp）并挂进 world.tscn 根下（sky_cycle_path 默认 "../SkyCycle"）。八层音全部就位：明亮风/雪风（随 snow·rain 主导度交叉淡化）、雨声（随 rain，雪天静音）、晨曦鸟鸣、湖水拍岸（近湖增强、结冰减弱）、壁炉柴火（靠近木屋渐入，cabin_fireplace_loop.wav 归一化无缝循环）、夜间虫鸣（3~8s 随机短颤音）、治愈 BGM——外部音频资产优先、缺失时程序化合成保底；进屋（is_sheltered）室外风雨雪自动衰减；低音量自动停混音省资源；EXIT_TREE 全量收口防音频实例泄漏（以上为会话A 交付）。总线收口（会话E）：新增 project/default_bus_layout.tres（Master + SFX 子总线，SFX→Master，exclude_filter 不排除 .tres 会随包发布）；一次性触发音效（Player 脚步/草丛/UI、门/窗/木箱/浆果采摘 3D 音、足球踢球、钓鱼、每日结算、暂停菜单点击共 12 处）set_bus("SFX")，循环环境层（风/雨/鸟鸣/湖水/壁炉/溪流）留 Master；暂停菜单音量滑条写 Master 总线（user://settings.cfg 持久化），SFX 自动跟随生效。src/ 下已无 ambient_audio.*（两套并存按"留 Ambience"收口）。自测 17 项断言（总线结构/12 播放器归属/滑条路径）全绿 + 无头 120 帧冒烟 0 SCRIPT ERROR（退出 9 泄漏与既有基线一致）。
- **动机**：`Ambience`/`AmbientAudio` 已写完却没挂——鸟鸣脚步之外整片世界是哑的；音频类两套并存必须收口（留 Ambience，删 AmbientAudio 或明确其一）。
- **验收**：register_types 注册所选类并挂进 world.tscn；风声常驻、雨声随 wet、虫鸣随夜；Master/SFX 两条 bus，暂停菜单音量滑条生效。
- **涉及**：`src/register_types.cpp`、`project/world.tscn`、删或明确 `src/ambient_audio.*`。
- **规模**：S
- **风险与坑**：Ambience 靠祖先链找 SkyCycle（不在根下的查找套路，第 34 轮 BerryBush 坑）；headless 下音频走 Dummy 驱动只能验零报错，真实听感要非 headless 实测。

#### 7. macOS 正式包收口（template_release + 图标 + 项目名）✅ 已完成（2026-10-24，会话D）
> 实现备注：`scons platform=macos target=template_release` 增量编库（godot-cpp 三平台 release 库已在位，仅重编 10-05 后改动源文件，20s）→ package.sh --only=macos 导出 → `dist/peace-macos-0.2.0.zip`（176MB）。实测：universal 双架构 ✓、Info.plist 版本 0.2.0 ✓、Resources/icon.icns 与 docs/Homestead.icns md5 一致 ✓、Frameworks 内为 template_release dylib ✓、解包 headless 冒烟 120 帧 EXIT=0 且 0 SCRIPT ERROR（退出期 RID/Dummy 泄漏为 headless 渲染器噪声；--verbose 泄漏 9 实例与既有基线完全一致，water.gdshader/环境音静态缓存，无害）。版本号统一 0.2.0：export_presets.cfg 三平台、project.godot config/version、README、package.sh deb 默认值。exclude_filter 追加排除未引用的 `壁炉燃烧.mp3`（14MB 原始素材，代码/场景零引用，文件保留在磁盘）。**偏离验收一处**：config/name 维持用户手改的 "Peace"（memory 铁律勿动），不回改"湖畔家园"。
- **动机**：现包内是 template_debug dylib；project.godot 名字还是工程模板名；图标素材已备好没用上。
- **验收**：`scons target=template_release` 产物在位；project.godot `config/name="湖畔家园 Homestead"`、版本号 0.2.0；Homestead.icns 生效（dock/访达可见）；重新导出 macOS zip，解包 headless 冒烟 120 帧 0 error；产物改名带版本号。
- **涉及**：`project/project.godot`、`project/export_presets.cfg`、SConstruct、docs/ 图标资产。
- **规模**：S
- **风险与坑**：**导出命令严禁接 grep|head 管道**（SIGPIPE 杀进程出半残 zip，日志实锤）；导出模板目录名必须 = 编辑器版本 4.7.2.stable；scons 重建与用户开着 Godot 编辑器互斥（先 pgrep Godot）。

#### 8. Windows / Linux 出包
- **动机**：三平台是既定目标，预设和 CI 都在，只差最后一公里。
- **验收**：CI 产物（或本机 mingw）拿到 Win/Linux template_release 动态库并完成导出；各平台 headless 冒烟通过；发布物三平台齐。
- **涉及**：`.github/workflows/build.yml`（可加导出 job）、`project/export_presets.cfg`。
- **规模**：M
- **风险与坑**：CI push 需用户授权；重新导出后仍要 unzip -t + 包内容核对（MEMORY.md 发布链路坑），Win/Linux 的 bin 布局 10-05 已实测打通（SConstruct SHLIBSUFFIX/PREFIX 修正已在位）。
- **进展（2026-10-24 会话H 地基核查与 CI 补全，未标 ✅）**：
  - ① **本机交叉编译路径健在**（MEMORY.md「22:34 跨平台打通」条，dist/ 10-05 三包为证；旧备注"本机无交叉工具链"已过时更正）：mingw GCC 16.2.0、Linux 交叉 GCC 15.2.0 + `~/opt/cross-shim` 垫片、dpkg-deb 1.23.11 复核全部在位；godot-cpp windows/linux template_release 静态库已预编（10-01），交叉重编只增量 src/*.；`tools/package.sh --only=windows|linux` 全流程已脚本化（编库→导出→压包/deb→完整性校验→时间戳核对，含 scons 单路自检）。
  - ② **新增 `.github/workflows/export.yml`**（手动 workflow_dispatch，不动 build.yml 的 push 编库流）：Win/Linux 两 job——编 release 库 → 装 4.7.2-stable 导出模板 → export-release（预设名 Windows/Linux）→ 导出产物本机 headless 冒烟 120 帧（退出码 + SCRIPT ERROR 扫描）→ zip 完整性校验并上传 Artifact。**这是"各平台 headless 冒烟"验收项的唯一自动化通道**（macOS 无法运行 win/linux 二进制）；deb 组包依赖 sips 转图标仍留本机。workflow 未实测，push 授权后首跑按日志现场修。
  - ③ **收尾步骤（等 farm_plot v2 冻结）**：会话G 交付并冒烟后，本机 `tools/package.sh --only=windows` + `--only=linux` 出 v0.2.0 包替换 dist/ 的 10-05 旧包（当前旧包是 v0.1 代码）；CI push 授权后再用 export.yml 补跨平台冒烟。
  - ④ **出包管线全链路实测通过（2026-10-24 会话I，产物写 tmp/ 未动 dist/）**：① mingw 交叉编译 10-05 后首次重跑——10-05 之后新增的全部源码（pause_menu/ui_style/farm_plot v2 中途态等 ~30 文件）在 mingw GCC 16.2.0 与 Linux 交叉 GCC 15.2.0 下一次编过（`project/bin/{windows,linux}/*template_release*` 落位，dll 2.7MB / so 4.6MB）；② `--export-release "Windows"/"Linux"` headless 导出 0 错误，exe（234MB）/bin（198MB）pck 嵌入正常、GDExtension 库随行且 `file` 验明 PE32+/ELF x86-64；③ zip 压包 `unzip -t` 与 deb 组包（dpkg-deb --build + --info/--contents、sips 转图标）干跑全过。**结论：冻结后正式出包只是原命令重跑一遍，工具链风险已清零。**
  - ⑤ **本机正式出包收官（2026-10-24 会话I）**：farm_plot v2 冻结（会话G 03:12 交接宣布）后执行 ③——`tools/package.sh --only=windows,linux` 一轮 40 秒出 `dist/peace-windows-x64.zip`（161M）、`dist/peace-linux-x64.zip`（161M）、`dist/peace-linux-amd64.deb`（128M），10-05 v0.1 旧包全部替换；产物时间戳核对全新鲜，unzip -t / deb 结构 / 包内容（exe+dll、bin+so）/ pck 内嵌（default_bus_layout.tres 随包在位，strings 验证）全过。此前 ④ 已先行验证管线（交叉编译含新全量 src + 导出预演）。**#8 剩余**：跨平台 headless 冒烟只能走 export.yml CI（macOS 无法运行 win/linux 二进制，push 需用户授权）；Win/Linux 包用户实机验收即玩。
  - ⑥ **三平台包全量重出（2026-10-24，#11 会话）**：#11（木箱/水井内容化）与会话J 的 #12（冰面可行走）落地后，发现 02:07 的 macOS 包缺 farm_plot v2/#11/#12 三特性——`tools/package.sh` 全平台重跑 58 秒（增量），`peace-macos-0.2.0.zip`（184.5M）+ win/linux zip + deb 四产物全部刷新（03:41，unzip -t/deb 结构全过）；dist/ 现态 = 工作区全特性 v0.2.0。
  - ⑦ **macOS 包版本化命名固化进脚本（2026-10-24 会话O）**：D/K/M 三轮"出包后手工改名 peace-macos-0.2.0.zip"的惯例正式落进 `tools/package.sh`——产物名 `peace-macos-<ver>.zip` 直接由导出产出，`<ver>` 默认取 project.godot `config/version`（升版本只改一处，GAME_VERSION 环境变量可覆盖），导出前自动清场旧无版本名 peace-macos.zip；`--publish` 落 site/download/ 时映射回官网稳定无版本 URL（site/index.html、sync_site_sizes.py、deb 安装命令零改动）。`--only=macos` 重出包 20 秒实测：产物名带版本 ✓、unzip -t ✓、Info.plist 0.2.0 ✓、dylib 与既有基线同尺寸 ✓、导出日志 0 错误 ✓、派生/覆盖两分支 ✓。

#### 9. 性能基线与低配兜底 ✅ 已完成（2026-10-24，会话F 画质开关 + 会话G 帧成本实测）
> 实现备注：**画质开关**走 Environment 特效路线（不动草密度、零随机流风险）：暂停菜单新增「画质：高/低（流畅）」按钮，低档关 SSR/SSAO/Glow 三特效 **+ 渲染缩放 scaling_3d_scale 0.7（高档复位 1.0；G 实测 0.7 档 −36% 帧成本，为几何瓶颈下的主杠杆）**，即时生效，持久化 settings.cfg `[video] quality_low` 开机自动应用（pause_menu.cpp；PERF.md 第四节）。**基线建档**（PERF.md 第一节）：窗口探针实测每帧计数器（不受 macOS 后台节流影响）——宅基地全景 ~2100–2640 draw calls / ~19M 三角形 / ~2890 物体 / 1.13GB 显存，湖面开阔 ~500 calls；-s 自测 18 断言（三特效+缩放+落盘+跨运行）+ 无头冒烟 0 报错 + 高低档截图对比全绿。**偏离**：帧率/帧时间无法在 macOS 脚本窗口直接测得（后台节流 ~10fps 假象）——帧时间已由离屏回读探针补测（新工具 `tools/perf_probe.gd`，PERF.md「帧成本阶梯」：默认画质 M4 亦仅 10~18fps，高/低/极低三档阶梯齐，60fps 属 v0.3 几何预算议题）；真实游玩帧率仍留 PERF.md 第三节清单作用户实机验收项。
- **动机**：SSAO/SSR/Glow + 4200 草 + 多粒子层在老机器上未知；发布前至少要有实测数字和一个兜底开关。
- **验收**：记录目标机 60fps 下的 draw call/三角数/帧时间基线（写入 docs）；提供简单画质开关（Environment 特效或草密度二选一，走 terrain_builder.h 顶部参数，不碰种子）。
- **涉及**：`src/terrain_builder.h`（参数）、`project/world.tscn`（Environment）、新 UI（挂暂停菜单）。
- **规模**：M
- **风险与坑**：⚠️ **改草量上限等 Scatter 参数会挪随机序列导致整图重排**（terrain_builder 固定种子 1337 的连锁反应，第 32/35 轮雪毯都刻意"不加 rnd() 调用"绕此坑）——画质档若减实例数，必须在收集时按确定性规则跳过而非中途 continue 随机流。

### P2 —— 打磨（时间允许再做）

#### 10. 林缘浆果丛可采化 ✅ 已完成（2026-10-24）
> 实现备注：撒点循环内按簇序号每 3 簇选 1 簇（58 簇 → 20 簇），选中簇的装饰 MultiMesh 实例替换为运行时生成的真 BerryBush 节点（E 采摘、45s 重长、发 berry_picked 计入每日待办）。**随机流零错位**：rnd() 全部先行求值、仅把 add 包进条件，改动前后 Bushes/草/花/石/落叶/鹅卵石各 MultiMesh 实例缓冲逐字节一致（带渲染指纹实测）；无穿模（选中簇装饰实例不再生成，簇心落真丛+同规格占位碰撞）。
- **动机**：14 簇装饰浆果丛近在眼前却采不了，是负反馈；让世界目标密度对齐引导。
- **验收**：林缘浆果丛中至少一部分变为真 BerryBush（可采、重长、计入目标）；装饰 MultiMesh 与可采实体不重叠穿模。
- **涉及**：`src/terrain_builder.{h,cpp}`（布点导出为真实坐标）、`project/world.tscn` 或运行时生成 BerryBush。
- **规模**：M
- **风险与坑**：改 terrain_builder 撒点逻辑务必**不新增 rnd() 调用**（防整图重排，见第 9 项）；BerryBush 沿祖先链找 SkyCycle 的坑已解（第 34 轮），照抄即可。

#### 11. 木箱/水井内容化 ✅ 已完成（2026-10-24）
> 实现备注：**木箱一次性彩蛋**——首次打开发新信号 `chest_looted`：GameDirector 中央公告"从木箱里翻出一条野餐毯！"（先于 chest_opened 计分信号发出，若同箱补完当日清单则结算横幅自然覆盖公告，优先级正确）+ 箱旁摆出野餐毯+藤篮纯视觉小道具（无碰撞、不引入背包；摆位截图核验——首版 +X 穿东墙、y-0.18 埋地板，定稿箱西侧屋内地板 y0.0175）+ 程序合成"叮咚"两音上行提示音（延时 0.55s 等开箱吱呀过半，照 make_pop 套路 22050Hz 16-bit）。`looted` 标志经 SaveSystem 新键 `chest_looted` 持久化（v4 增量：存前从 Chest 拉取/缺源保留缓存、读档 restore 推送 `set_looted(true)` 只补摆毯子不发公告不重播音效；旧档缺键=未发现）。**水井打水音效**——Well 新增 AudioStreamPlayer3D（SFX 总线），程序合成约 1.15s 复合音（三声辘轳木轴滑频吱呀 + 尾部水花噪声爆 + 井筒低频"咚"；噪声用确定性哈希采样不碰全局随机流），与 1.2s 升程同步，打水方向触发。验证：-s 功能自测 38 断言全 PASS（fresh 24 + 冷启动回放 7 + 旧档迁移 7：信号单次/关箱不重触发/公告上屏/存档含键/回放无信号无公告/缺键不摆毯/farm_daily 回归）+ 120 帧冒烟 EXIT=0 + 带渲染截图两轮读后即删。⚠️ 撞车记录：会话J 03:18 认领后 03:25 主动整体退让（本会话 03:20 起已在写 chest/well），#11 归本会话，J 的冰面可行走（#12）与本项零文件交集。
- **动机**：现交互物"开了没反应"，用最小代价给目标链提供素材。
- **验收**：木箱首次打开有一次性反馈（如发现"野餐毯"道具直接计入清单完成）；水井打水有一次动作音效反馈；不引入背包/物品系统。
- **涉及**：`src/chest.{h,cpp}`、`src/well.{h,cpp}`、game_director 信号。
- **规模**：S
- **风险与坑**：音效照 make_pop/make_chirp 的程序化合成套路（无外部资源）；别为"道具"发明携带系统（见不做什么）。

#### 12. 冰面可行走 ✅ 已完成（2026-10-24，会话J）
> 实现备注：新类 `src/world/ice_surface.{h,cpp}`（StaticBody3D）——一张无限大水平 WorldBoundary 碰撞面（y=-0.54，水位 -0.55 上方 1cm），**一次建好、默认 disabled，只在 NOTIFICATION_PHYSICS_PROCESS 里切形状 disabled 开关**（不增删物理体，天然满足物理帧同步约束）。由 SkyCycle READY 时 spawn 为子节点（与 build_rain/build_snow 同套路，场景文件零改动、零并行冲突面）；自己沿祖先链懒找 SkyCycle 读雪量（freeze_amount=snow 直映），滞回开关 ≥0.85 启用 / ≤0.70 解除（防临界抖动）。**惊喜发现：滑冰手感系统早已存在**——player.cpp 的 is_ice_skating（is_on_frozen_lake 判定：1.65x 移速 + 超低阻尼惯性滑行 + 冰上起跳空翻 + 自动穿滑冰鞋）此前只在浅水湖床上触发过（玩家够不到水面高度），本碰撞面补上"深水冰面站得住"这块基石，站上冰面（y=-0.54 恰在其判定窗口）滑冰玩法全链路自动激活，player 零改动。解冻落水是自然行为（湖底可蹚水，岸边重新上岸）。验证：-s 无头 12 断言全 PASS（spawn 恰 1 个/晴天禁用/雪 1.0 启用+站冰 y=-0.54+on_floor/雪 0.0 解除+落水 y=-1.40/再冻结重启用，落水断言自校准不依赖湖底深度）+ 120 帧冒烟 0 SCRIPT ERROR（退出泄漏=既有基线）+ 带渲染截图（雪天湖面站冰，冰蓝裂纹视觉与脚下齐平）读后即删。**双冰系统收口（2026-10-24 补）**：与已提交基线 terrain_builder 湖面冰 box（雪量 0.20 即激活）重叠——雪量 0.20~0.85 区间玩家能走上视觉未封冻的湖面（会话L 探针实证）。已按选项 (b) 处置：terrain_builder 湖面 box 删除，湖面冰碰撞归 IceSurface 独有（滞回语义恢复生效）；溪流/瀑布深潭冰保留 0.20 早冻（高处溪段高于无限平面，平面盖不住）；is_on_frozen_lake 大湖分支改为直接问 IceSurface 激活态（顺修 0.20~0.85 蹚浅水误判滑冰）。验证：统一探针 20 断言全 PASS（树中无 box/恰 1 个 IceSurface/满雪站冰脚下 collider=IceSurface/雪 0.50 蹚水不误判/0.75 爬升不启用/0.85 重启用/0.75 下探滞回保持/溪流 0.20 层开关保留）+ 120 帧冒烟 0 SCRIPT ERROR + dist/ 四包重出（03:55，探针留 tmp/scripts/tmp_zcode_ice_unify.gd 可复跑）。
- **动机**：湖面结冰视觉已很完整，"雪天上冰"是小品里少见的惊喜时刻。
- **验收**：freeze>阈值时湖面生成可行走碰撞（StaticBody 平面），玩家上冰有滑动手感（摩擦系数调整）或至少可站立不落水；解冻自动移除。
- **涉及**：`src/terrain_builder.{h,cpp}` 或新 `ice_surface` 节点（跟随 freeze_amount 开关）、player 摩擦参数。
- **规模**：L
- **风险与坑**：碰撞生成/销毁与物理帧同步（勿在 _process 里改 physics body 状态）；日志第 34 轮已标注"较大"，放最后，砍掉不影响 v0.2 发布。

### 发布门槛后追加（2026-10-24，已并入 0.2.0 四包 11:29–11:30 版）

两项不占上述门槛的超纲小特性，由会话A 在发布收尾后追加交付并随第 40 轮四包重出进入 0.2.0 发布物：

- **菜畦 v3 作物多样性**：空畦旁按 `R` 换种——浆果 90s/+2 · 萝卜 60s/+3 · 南瓜 150s/+5（提示文案与 0.45 倍缩放预览跟随切换）；存档序列扩为 4 段 "state,%,浇灌,作物"，v2 三段旧档兼容解析（默认浆果）；收获按作物产量计入待办。
- **鱼获图鉴**：每条鱼带随机体长（大鱼 48~78cm / 小鱼 16~34cm），破纪录追加 🏅；`B` 键呼出「湖畔玻璃」图鉴面板（累计条数/最长纪录），`user://fishing_records.cfg` 独立持久化、与主存档解耦。

两项均已过全量回归（120 帧冒烟 0 SCRIPT ERROR + 双冰探针 20 断言 ALL PASS，零漂移），验收项已同步 `docs/ACCEPTANCE.md`。

---

## 四、明确不做什么（小品级体量不划算）

| 方向 | 不做的理由 |
|------|-----------|
| **地形/地图布局重排** | 固定种子 + 宅基地/桥端/水面参数强耦合（world.tscn 与 terrain_builder.h 必须一致，否则悬空陷地）；改布局 = 整图重排 + 全场景坐标手调，收益极低 |
| **背包 / 物品 / 制作系统** | 一个 UI 框架 + 数据模型的完整系统，远超"采浆果清单"的需求；目标链用信号直连即可 |
| **NPC 与对话系统** | 兔子小鸟是氛围不是角色；对话树/打字机/好感度是另一个量级的工程 |
| **主线剧情 / 任务编辑器** | "每日清单"已覆盖目标感；任务系统（前置/分支/失败态）无必要 |
| **第一人称模式** | 第三人称+推门前倾动画是既有资产；双视角适配相机/模型/交互判定全要重验 |
| **移动端 / 手柄适配** | 触屏 UI 与摇杆相机是独立工程；键鼠三平台已够 v0.2 |
| **多语言 i18n** | 单语中文 + 已打包 Noto 字体已收口；翻译框架对 10 分钟小品无收益 |
| **全量替换外部美术资产** | 程序化/CSG 风格已统一且零许可负担（README 待办里的 Quaternius/Kenney 只在需要个别模型时考虑） |
| **成就平台 / 云存档 / 排行榜** | 本地存档（路线图第 1 项）已是上限；接 Steam/平台 SDK 属 v0.3+ |
| **可保存的世界修改（砍树/建房）** | 世界是固定种子运行时生成，持久化玩家改动需快照 diff 系统，体量失控 |

---

## 五、建议顺序与里程碑

```
M1 可玩闭环（P0）     : 1 存档 → 2 目标链 → 3 引导 → 4 暂停菜单 → 5 键位/相机
M2 发布收口（P1）     : 6 环境音 → 7 macOS 正式包 → 9 性能基线 → 8 Win/Linux
M3 打磨（P2，可裁剪） : 10 浆果可采化 → 11 箱/井内容化 → (12 冰面可行走)
```

v0.2 发布门槛：M1 + M2 全部完成、M3 至少完成 10/11 两项；发布物三平台齐、带版本号 0.2.0、冒烟全绿。
