# 自动化执行记录（02e3f6db：每 10 分钟地图细化一轮）

## 2026-10-01 02:48 — 第 1 次触发（第 9 轮）
- 开工前确认状态：已迭代 8 轮（最后一轮 02:44 萤火虫昼夜/天气联动）；源码完整可编译；并行自动化 85b20c81 同样在跑。
- 选方向 2（地貌细节），改动限定在 terrain_builder.{h,cpp}，不碰 world.tscn 以避冲突：
  1. 山体岩层裸露：露岩顶点色加水平层理条带（噪声扰动边界 + 颗粒纹）；新增 Scree 碎石坡 Scatter（≤140）。
  2. 草地聚簇 + 裸土：草丛密度门控与地形裸土顶点色共用 value_noise(0.14 频率, 偏移 21/11)，斑块严格对齐。
- 验证全绿：scons ✅ / 无头导入 0 error ✅ / res://world.tscn --quit-after 60 零报错 ✅。
- 明细已记当日日志（2026-10-01.md 第 9 轮小节）。
- 下轮候选：桥面裂纹装饰、鸟归巢/兔子回窝、HUD 时钟、雪天。

## 2026-10-01 03:52 — 第 2 次触发（第 12 轮）
- 确认状态：并行实例 85b20c81 于 03:49 刚改完 rabbit（兔子回窝）；选改动面最小、与其零重叠的方向 5——**HUD 时钟**。
- 新增 `src/hud_clock.{h,cpp}`（HUDClock，CanvasLayer，已注册）：每帧读 SkyCycle 的 time_of_day 显示 "hh:mm · 时段 · 天气"（Label 全宽贴顶居中，白字描边）；找不到 SkyCycle 自动隐藏。
- 接线：register_types.cpp 注册 + world.tscn 末尾加 HUDClock 节点（sky_cycle_path → ../SkyCycle）。
- 新坑：godot-cpp `String + const char*` 二义 → 字面量包 String()。
- 验证全绿：scons ✅ / 无头导入 0 error ✅ / --quit-after 60 零 error/warn ✅。明细见当日日志第 12 轮小节。
- 下轮候选：雪天、桥面装饰裂纹（建筑做旧）、果子可拾取。

## 2026-10-01 04:57 — 第 3 次触发（第 15 轮）
- 确认状态：已迭代 14 轮（最后一轮 04:51 HUD 白天数，改的 sky_cycle/hud_clock 已收尾）；本轮选方向 1+2（植被+地貌），改动只在 terrain_builder.{h,cpp}，零并行冲突。
- 改动：① MC 蘑菇——树荫下林缘成簇（≤12 簇 × 3~7 朵），红伞白点/棕伞混生，三 Scatter（Stems/Caps/Spots），白点按伞盖表面公式嵌入；② 湖岸湿沙带——水线上方 0.02~0.24m 深色湿沙顶点色过渡。
- 验证全绿：scons ✅ / 无头导入 0 error ✅ / res://world.tscn --quit-after 60 零报错 ✅。明细见当日日志第 15 轮小节。
- 下轮候选：雪天、桥面裂纹装饰、果子可拾取。

## 2026-10-01 06:09 — 第 4 次触发（第 18 轮）
- 确认状态：已迭代 17 轮（05:55 石桥做旧）；查实烟囱冒烟/落叶/鹅卵石已存在 → 选方向 5 新增蝴蝶群（新类 ButterflySwarm + register_types + world.tscn 末尾追加，避开并行实例热的 sky_cycle/terrain_builder）。
- 9 只 MC 式三配色蝴蝶：圆弧漫游+轻浮+切线朝向+11~16Hz 扑翼；昼夜联动 SkyCycle（黄昏爬升离场整群隐藏）。位置 y=0.9 z=-24（出生点压平区上空防陷地）。
- 新坑：cpp 用 `using namespace godot;` 时末尾别再写 `} // namespace godot`（报 extraneous closing brace 且报错在末行，难定位）；`String` 头在 variant/ 下；CapsuleMesh 要显式 include。
- 验证全绿：scons ✅ / 无头导入 0 error ✅ / --quit-after 120 零 error/warn ✅。明细见当日日志第 18 轮小节。
- 下轮候选：雪天、果子可拾取、蘑菇夜光、鸟鸣音效占位。

## 2026-10-01 07:20 — 第 5 次触发（第 21 轮）
- 确认状态：已迭代 20 轮（07:08 青苔扩展，85b20c81 正动 world.tscn；72e892d1 的 sky_cycle 自 06:24 未动）→ 选候选清单里的「蘑菇夜光」，改动只在 terrain_builder.{h,cpp}，零并行冲突，不加新类。
- 实现：① Caps/Spots 拆专用材质开自发光（能量默认 0），update_glow() 每帧按昼夜（glow=night²，映射同 Bird/Rabbit）写能量——伞盖暖橘×1.7、白点冷白×1.1；② 约 40% 蘑菇簇挂"孢子光尘" CPUParticles3D（淡绿小球缓上飘+透明渐变，glow>0.05 才 emitting）。
- 未做：夜光×天气联动（SkyCycle 无 rain/snow 公开 getter，避文件冲突跳过）→ 下轮候选。
- 新坑：`-s` SceneTree 脚本 `_process` 签名 `-> bool`（返回 true 退出），不是 `-> void`。
- 验证全绿：scons ✅ / 无头导入 0 error ✅ / --quit-after 60 零 error/warn ✅ / 午夜实测 PUFFS=5 EMITTING=5、正午回归 EMITTING=0（临时脚本已删）。明细见当日日志第 21 轮小节。
- 下轮候选：积雪视觉、果子可拾取、夜光×天气联动、鸟鸣音效占位。

## 2026-10-01 08:35 — 第 6 次触发（第 24 轮）
- 确认状态：已迭代 23 轮（08:16 月盘，85b20c81 刚动过 sky_cycle 08:23，可能仍在收尾）→ 选候选里的「鸟鸣音效占位」，改动只在 bird.{h,cpp}（02:57 后无人动，最冷文件），零并行冲突、零 world.tscn 改动、不加新类。
- 实现：① make_chirp(kind) 程序化合成 3 种 AudioStreamWAV 短哨（22050Hz 单声道 16-bit 纯代码，上扬/下滑/连音双哨，基频+35% 二次谐波+16ms 起音+指数衰减）；② AudioStreamPlayer3D 跟鸟移动（-4dB/max 48m/unit_size 12）；③ PROCESS 末尾 4~11s 随机鸣叫 + 音高微随机，夜里归巢不叫；rng 用世界坐标哈希播种整图可复现。
- 新备忘：godot-cpp 的 Math 没有 randf/randf_range（4.x 移除）→ 用 std::mt19937；AudioStreamWAV 在 gen 头文件 audio_stream_wav.hpp。
- 验证全绿：scons ✅ / 无头导入 0 error ✅ / --quit-after 120 零 error/warn ✅（headless 音频 Dummy 驱动 play 无报错）。明细见当日日志第 24 轮小节。
- 下轮候选：积雪视觉（地面/树冠）、果子可拾取、夜光×天气联动（SkyCycle 加 rain/snow getter）、月相/月面陨石坑着色器。

## 2026-10-01 09:42 — 第 7 次触发（第 27 轮）
- 确认状态：已迭代 26 轮（09:24 鸟鸣×天气，85b20c81 刚收尾 bird/sky_cycle 属热点）→ 选候选里的「积雪视觉」+「蝴蝶雨雪躲藏」，只动 terrain_builder / veg_wind.gdshader / butterfly_swarm + 新增 snow_ground.gdshader，零热点冲突、零 world.tscn 改动。
- 积雪视觉：地形雪盖着色器（坡度+水线+覆盖噪声门控，terrain_mat 成员）+ veg_wind 加 snow_amount（混雪色+梢部下坠，wind_mats 收集逐帧驱动）；update_glow 改名 update_sky_driven（夜光+雪量二合一）；snow=0 外观与旧版一致。
- 蝴蝶躲藏：wet→1 落草丛静栖（高度/漫游/扑翼全按 wet 衰减）；连带修复 set_global_position 误用局部坐标的潜在 bug（改 set_position）。
- 验证全绿 + 双态实测（湿透 0.13/干燥 1.54）+ 截图 docs/shot_snow_cover.png。
- 新坑：Godot 4 MultiMeshInstance3D 不继承 MeshInstance3D；Mesh 基类无 set_material；-s 脚本不自动加载主场景。
- 下轮候选：果子可拾取、月相/月面陨石坑、踩雪音效占位、树枝挂雪。

## 2026-10-01 11:00 — 第 8 次触发（第 30 轮）
- 确认状态：已迭代 29 轮（10:36 脚步声收尾）→ 选候选「月相/月面陨石坑」，只改 sky_cycle.{h,cpp}（08:23 冷）+ 新增 moon.gdshader，零 world.tscn 改动、不加新类。
- 实现：① moon.gdshader——值噪声月海+陨石坑（球面方向投 XY/ZY 双平面防接缝）、椭圆弧相位照明（lit: x > cos(2π·phase)·√(1-y²)，照亮侧朝局部 +X）、暗面 8% 地照残影、EMISSION 借 Glow 微晕；② SkyCycle build_moon 优先着色器路径（失败回退纯色）、球细分 48×24，apply() 相位只在跨天写（fmod(0.5+(day-1)·0.125,1)，第 1 天满月、8 天一循环）。
- 新坑：-s 脚本 res:// 不可写（截图用绝对路径）；Player 相机每帧 look_at 会覆盖脚本对准（先 set_process(false)）。
- 验证全绿 + 双态实测：满月/残月截图 docs/shot_moon_full.png、shot_moon_crescent.png；脚本拨 phase=0.15 不再被 apply 冲掉。明细见当日日志第 30 轮小节。
- 下轮候选：树枝挂雪（独立雪盖网格）、雨天脚步变体（湿泥声）、脚步声按地形材质区分（石桥面/木屋地板）、HUD 加月相名称。

## 2026-10-01 11:14 — 第 8 次触发（第 32 轮）
- 确认状态：已迭代 31 轮（第 30 轮=我方 11:00 月相；第 31 轮=72e892d1 11:20 正在做浆果采摘音效，berry_bush 属热点且撞过一次编译中态）。选「树枝挂雪」，只动 terrain_builder + veg_wind.gdshader。
- 实现：TreeSnowCaps 雪毯 Scatter（每树冠顶+冠肩两片压扁白球，veg_wind 的 alpha 雪量淡入，与树冠同摆；参数确定性导出不动 rnd 序列）。
- 🔴 重大发现：第 27 轮"大雪截图"是雾感误判；-s 脚本经 multimesh.mesh.surface_get_material() 改材质参数回读正常但渲染脱钩（MultiMesh 特有，普通 MeshInstance3D 有效）→ 雪盖验证只能临时改 C++ 初值重编。C++ 写入路径确认有效（红雪色对照实验）。
- 验证全绿：scons/导入/120 帧 0 报错；隔离实测雪帽雪环清晰抱冠（docs/shot_snow_caps.png）。
- 下轮候选：雨天脚步变体、脚步声按地形材质区分、灌木顶雪毯、低冠先积雪分级。

## 2026-10-01 11:51 — 第 9 次触发（第 34 轮）
- 确认状态：已迭代 33 轮（第 33 轮=85b20c81 11:42 Label3D CJK 字体，遗留 bug 清零）。候选里脚步类要动热文件 player、灌木雪毯/低冠分级在偏热的 terrain_builder → 选新方向「湖面结冰」，只改 water.gdshader（6.5h 冷）+ sky_cycle.cpp（25 分钟）。
- 实现：freeze_amount uniform——浅水先冻岸→湖心推进+噪声斑块前沿；波浪压平/法线衰减/焦散泡沫淡出/冰蓝+脊线裂缝/粗糙度 0.06 镜面冰；sky_cycle apply() 写 freeze=snow（消融走平滑曲线）。
- 验证全绿 + 强制雪量截图 docs/shot_ice.png（裂缝密度调优一轮：pow 14→22、亮度 0.85→0.65，去大理石感）。
- 下轮候选：脚步声按地形材质区分、灌木顶雪毯、低冠先积雪分级、冰面可行走（大改）。

## 2026-10-01 12:16 — 第 10 次触发（第 35 轮）
- 确认状态：已迭代 34 轮（12:04 并行实例刚动 sky_cycle.h）→ 选「树冠积雪分级（低冠先积雪）+ 蘑菇伞顶雪毯」，只改 terrain_builder.{h,cpp}（本实例最熟的冷文件），零并行冲突。
- 实现：SnowBand{mat,lo,hi,drive_alpha} 分级条目替代 wind_mats+snow_cap_mat；树冠/雪毯按树顶高分三档（T1/T2/T3，区间逐档上移，低冠先积雪）；ShroomSnow 蘑菇伞顶小雪毯（最先积雪档）；贴地植被 (0.02,0.30)。零新增 rnd() 调用。
- 验证全绿 + 数值精确命中（snow=0.4 强制：T1=0.9259/T2=0.0741/T3=0/蘑菇·草=1.0）+ 截图 docs/shot_snow_bands.png。TEMP 已还原、临时脚本已删。
- 明细见当日日志第 35 轮小节。下轮候选：冰面可行走、雪/雨脚步音量差异化、灯光昼夜开关。
