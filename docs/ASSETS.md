# 开源素材清单（Open-source asset shortlist）

> 用途：给 Peace 挑选可商用、许可清晰的外部素材。原则：**只收 CC0 或明确允许商用的许可**；
> 任何素材落地前都要在仓库里留下来源与许可，不确定的宁可不收。
> 当前游戏绝大多数声音/模型都是程序化生成或本机录音（录音不入库），这份清单是“以后要换/要加”的候选池。

---

## 1. 许可速查

| 许可 | 能否商用 | 是否必须署名 | 备注 |
| --- | --- | --- | --- |
| CC0 1.0 | 可以 | 不需要（建议署名） | 最省心，优先选 |
| CC BY 4.0 | 可以 | **必须** | 需要 ATTRIBUTION 记录作者/链接/改动 |
| CC BY-SA | 可以 | 必须，且衍生作品同许可 | 与闭源发布有冲突风险，不建议 |
| CC BY-NC | 不可以（商业） | — | 直接排除 |
| Pixabay License | 可以 | 不需要 | 不能单独转售原素材 |
| Sonniss GDC Bundle | 可以用于游戏 | 不需要 | **禁止再分发素材本身**，不能放进公开仓库 |
| 商店/EULA 素材 | 看条款 | 看条款 | 不入开源仓库，避免许可外溢 |

---

## 2. 音效来源

### 2.1 环境/自然声（湖、雨、风、鸟、虫）

| 来源 | 许可 | 链接 | 说明 |
| --- | --- | --- | --- |
| sfxmint — Ambience | CC0 | https://sfxmint.com/category/ambience | 310 条环境音，含 River Ambience、Cave Wind、Rain、Forest；WAV/MP3，无需注册 |
| sfxmint — water-stream-43 | CC0 | https://sfxmint.com/sounds/water-stream-43 | 6 秒流水，可循环；同站还有多条 stream/rock 水声 |
| Lots of Sounds — Nature | CC0 | https://www.lotsofsounds.com/sounds/tag/nature-recording | 可循环的溪流/自然录音，站内 12,000+ CC0 音效 |
| Lots of Sounds — Calming | CC0 | https://www.lotsofsounds.com/sounds/tag/calming | 海浪、瀑布等舒缓环境音 |
| OpenGameArt | 逐条确认（多为 CC0/CC-BY） | https://opengameart.org/ | 搜 “lake / forest ambience / birds”，必须看每个条目的 License 字段 |
| Freesound | 逐条确认（可过滤 CC0） | https://freesound.org/ | 注册后可下；只取 CC0 条目，避开 NC |
| Pixabay Sound Effects | Pixabay License | https://pixabay.com/sound-effects/ | 免费商用、无需署名；不能把素材本身当商品转售 |

### 2.2 UI / 交互音效

| 来源 | 许可 | 链接 | 说明 |
| --- | --- | --- | --- |
| Kenney — UI Audio | CC0 | https://kenney.nl/assets/ui-audio | 按钮/开关/悬停，50 个左右；适合替换当前程序化 UI 点击 |
| Kenney — Interface Sounds | CC0 | https://kenney.nl/assets/interface-sounds | 100 个点击/提示音 |
| Kenney — Impact Sounds | CC0 | https://kenney.nl/assets/impact-sounds | 脚步/落地/碰撞可作补充 |
| Kenney — RPG Audio | CC0 | https://kenney.nl/assets/rpg-audio | 魔法/开箱/金币类，可挑钓鱼收获音 |
| gamesounds.xyz — Kenney Pack | CC0 | https://gamesounds.xyz/?dir=Kenney%27s%20Sound%20Pack | Kenney 音频的镜像下载，省去逐个页面下载 |

> 注意：Kenney 的音频包以 UI/交互/冲击为主，**没有**官方的自然/鸟鸣/水声包；自然声要走 sfxmint / Lots of Sounds / OpenGameArt。

### 2.3 音乐 / 氛围床

| 来源 | 许可 | 链接 | 说明 |
| --- | --- | --- | --- |
| RadioLamp Studio ambient pack | 署名即可（非 CC0） | https://itch.io/profile/radiolamp-studio | Wind / Lake / Night 三首氛围，OGG/MP3/WAV；商用需 credit |
| Pixabay Music | Pixabay License | https://pixabay.com/music/ | 免费商用，注意避开 Content ID 曲目 |
| Free Music Archive | 逐条确认 | https://freemusicarchive.org/ | 只取 CC0/CC-BY 且允许商用的曲目 |

---

## 3. 模型 / 贴图来源

| 来源 | 许可 | 链接 | 适用 |
| --- | --- | --- | --- |
| Poly Haven | CC0 | https://polyhaven.com/ | HDRI、PBR 贴图、模型；木纹/石材/地面贴图质量高 |
| AmbientCG | CC0 | https://ambientcg.com/ | PBR 材质（木、石、泥土、雪），适合替换程序化纹理的参考 |
| Quaternius | CC0 | https://quaternius.com/ | 低模自然/动物/道具包，风格与当前 low-poly 接近 |
| Kenney — 3D assets | CC0 | https://kenney.nl/assets?q=3d | 低模道具/自然套件，可直接改色 |
| OpenGameArt — 3D | 逐条确认 | https://opengameart.org/art-search-advanced?field_art_type_tid%5B%5D=10 | 低模植物/石头/船，注意 License |
| itch.io free assets | 逐条确认 | https://itch.io/game-assets/free | 质量参差，必须读每个包的许可 |

---

## 4. 落地规范

1. **入库前先记来源**：在 `ATTRIBUTION.md` 里写 素材名 / 作者 / 来源 URL / 许可 / 是否修改。
2. **音频默认不入库**（见 `.gitignore` 的 `project/assets/audio/` 与 `*.mp3/*.wav/*.ogg/*.flac`）：
   CC0 素材在许可上可以入库，但是否打破现有策略由用户定；未确认前只下载到本地验证，不提交。
3. **CC BY 素材必须署名**，并且修改过要注明“已修改”。
4. **禁止 NC / 禁止再分发的包**（Sonniss GDC、部分商店 EULA）只在本机使用，不放进公开仓库、不随包分发素材源文件。
5. **风格优先**：Peace 是低模 + 手绘纹理风格，优先 Quaternius / Kenney / Poly Haven 的低频细节；写实照片贴图容易和现有 toon 材质打架。
6. **音频优先找可循环的**：环境层要无缝循环；一次性音效控制在 0.2–1.5s，避免和现有程序化音效撞频。

---

## 5. 第一批候选（按收益排序）

1. **湖面/溪流环境循环**：sfxmint “River Ambience” 或 Lots of Sounds 的 loopable stream，替换现在程序合成的湖水层。
2. **UI 点击/悬停**：Kenney UI Audio 挑 3–4 个，替换暂停菜单/钓鱼的合成点击声。
3. **鸟鸣/虫鸣**：OpenGameArt 的 CC0 鸟鸣 + 现有程序化虫鸣叠加，增加层次。
4. **木纹/石材 PBR 参考**：Poly Haven / AmbientCG 只作为程序化纹理的对照，不直接替换（保持低模风格）。
5. **低模道具**：Quaternius 的船/木桶/灯笼，可用于后续码头扩展；先确认与现有比例和配色一致。
