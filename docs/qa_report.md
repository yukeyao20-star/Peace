# QA 审查报告（qa-audit）

日期：2026-10-01 · 环境：Godot 4.7.2 stable · 模式：`--headless`（未动 dylib、未开 editor、未 commit）

## 结论

**达到"可玩可发布"底线**：两次 600 帧长跑零错误、玩法闭环（采 6 颗浆果→结算）全链路通过、无逻辑帧尖峰。遗留 1 个 P2 级退出时音频小泄漏与两项覆盖缺口（见问题清单），均不阻断发布。

## 【稳定性】

- `--headless --path project res://world.tscn --quit-after 600` 跑 2 次，均 `exit=0`。
- `grep -icE "error|warn|fail|cannot|invalid|script"`：**run1=0 条，run2=0 条**（仅引擎版本横幅，/tmp/qa_run1.log、/tmp/qa_run2.log 各 2 行）。
- 覆盖缺口：headless 600 帧实际游戏时间约 10s（约 1 游戏小时），天气最短持续 20s（sky_cycle.cpp:161-167），**默认长跑未触发天气切换**。已用临时脚本 `Engine.time_scale=20` 补测 1200 帧：
  - 观测到粒子切换 2 次：晴(000) → 雪(001，Snowflakes 480 粒激活) → 晴(000)；昼夜 phase 白天→傍晚，day 计数 0→1。
  - 期间 grep 无 error/warning 输出。
  - 残留缺口：雨天 Drizzle(320)/Downpour(900) 粒子链路仍未实测触发。

## 【场景规模】

节点总数 **535**。关键计数（临时普查脚本实测）：

| 类别 | 数量 | 备注 |
|---|---|---|
| MultiMeshInstance3D | 28 | instance 容量合计 **6651**，其中 GrassTufts 4200、Berries 375、Bushes 206；`visible_instance_count=-1`（全量显示） |
| CPUParticles3D | 10 | 合计 1836 粒子；仅 ChimneySmoke(20) 常发射，雨/雪/萤火虫按需 |
| CSG 节点 | 158 | Box 137 / Cylinder 13 / Combiner 4 / Polygon 3 / Mesh 1 |
| MeshInstance3D | 228 | |
| AudioStreamPlayer3D / Player | 8 / 1 | 另有 AudioStreamPlayer 1（环境音） |
| Light3D | 1 | 仅 DirectionalLight3D（昼夜太阳/月亮） |
| 生物 | Bird 5 / Rabbit 2 / ButterflySwarm 1 | |

评估：MultiMesh 实例总量 6651 略高于既往日志记载的 ~5000，**同一数量级、构成合理（草地占 63%），不判异常**；粒子数无异常。

## 【玩法闭环】—— PASS

临时脚本（time_scale=8 加速 45s 重长节奏）实测：

1. 初始 `get_target()=6`，`get_count()=0`；`berry_bushes` 组含 3 丛 BerryBush。
2. **双采拒绝**：对同一丛同帧连续 `interact()` 两次，count=1→1 不重复计分；采摘后 `get_prompt()` 立即为空（berry_bush.cpp:179 重长期内拒绝，符合设计）。
3. 轮采 3 丛共 6 次，count 每次精确 +1；含 BerryBush1 在 45s 重长后的**二次采摘成功**（重长逻辑工作正常）。
4. GoalLabel 实时更新：`目标 · 采浆果 2/6 → … → 6/6 ✓`。
5. 达成后 `is_done()=true`，中央结算 `DoneLabel.visible=true`。

## 【帧时】

300 帧统计（headless，仅用于发现逻辑尖峰）：

- **avg 7.46ms · p95 6.94ms · max 150ms**。
- 唯一 >100ms 尖峰位于**第 1 帧（150ms，场景实例化预热）**，第 2 帧起无任何尖峰，无随时间恶化的逻辑尖峰。
- 注意：headless 无 GPU 渲染，该数值不能代表真机帧率。

## 【问题清单】

### P0（阻断发布）
未发现。

### P1（应修）
未发现。

### P2（可缓）
1. **退出时音频对象泄漏**：场景内有采摘音效播放/刚结束时退出，泄漏 4 个 ObjectDB 实例（`AudioStreamWAV`×2 + `AudioStreamPlaybackWAV`×2，引用计数 1）。单次采摘即可复现；未采摘的长跑（run1/run2）无泄漏。证据：`--verbose -s` 泄漏明细（Leaked instance: AudioStreamWAV/…/Reference count: 1）。量级极小、仅发生在退出瞬间，建议退出/释放路径先 `stop()` 音频流。
2. **注释漂移**：game_director.cpp:86 初始文案硬编码 "0/10"，实际 target=6；因构造尾部 `refresh_goal()`（game_director.cpp:95）立即覆盖为 "0/6"，玩家不可见，仅维护性隐患。
3. **覆盖缺口（非缺陷）**：a) 雨天 Drizzle/Downpour 粒子链路未被任何一次运行触发；b) headless 无法评估 MultiMesh 6651 实例 + 158 个 CSG 节点的真机渲染帧率。

## 【建议】

1. 真机（带渲染）跑一次 5–10 分钟并观察帧率与温度，补齐 headless 无法覆盖的渲染性能维度。
2. 长跑压测可常态化加 `Engine.time_scale`（临时脚本方案已验证可行），覆盖完整天气序列（晴→多云→雨→大雨→雪）。
3. BerryBush/音效释放路径在 `NOTIFICATION_PREDELETE` 或退出流程中补 `stop()`，消除退出泄漏。
4. 更正 game_director.cpp:86 的 "0/10" 硬编码注释文案，统一走 `refresh_goal()`。

---
*审查方式：只读 + 运行验证；4 个临时 .gd 测试脚本已全部删除，除本报告外未写任何文件。*
