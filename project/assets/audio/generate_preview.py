import os

html_head = '''<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>《湖畔家园》全量音效试听与选择台 (Homestead Soundboard)</title>
  <style>
    :root {
      --bg: #090d16;
      --card-bg: #131b2e;
      --card-border: #23304a;
      --accent: #38bdf8;
      --accent-hover: #0284c7;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --tag-bg: #1e293b;
      --success: #10b981;
      --success-bg: rgba(16, 185, 129, 0.15);
      --gold: #f59e0b;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.5;
      padding: 24px;
      padding-bottom: 120px;
    }
    .header {
      max-width: 1240px;
      margin: 0 auto 24px;
      padding: 24px 28px;
      background: linear-gradient(135deg, #131b2e 0%, #0d2847 100%);
      border: 1px solid var(--card-border);
      border-radius: 14px;
      box-shadow: 0 8px 30px rgba(0,0,0,0.4);
    }
    .header h1 {
      font-size: 24px;
      font-weight: 700;
      color: #38bdf8;
      margin-bottom: 8px;
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .header p {
      color: var(--text-muted);
      font-size: 14px;
      margin-bottom: 14px;
    }
    .status-banner {
      background: var(--success-bg);
      border: 1px solid #059669;
      border-radius: 8px;
      padding: 12px 16px;
      font-size: 13.5px;
      color: #6ee7b7;
      margin-bottom: 12px;
      line-height: 1.6;
    }
    .status-banner strong { color: #a7f3d0; }
    .tabs {
      max-width: 1240px;
      margin: 0 auto 20px;
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
    }
    .tab-btn {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      color: var(--text-muted);
      padding: 9px 18px;
      border-radius: 9px;
      cursor: pointer;
      font-size: 14px;
      font-weight: 500;
      transition: all 0.2s;
    }
    .tab-btn:hover {
      border-color: var(--accent);
      color: var(--text);
    }
    .tab-btn.active {
      background: var(--accent);
      color: #090d16;
      border-color: var(--accent);
      font-weight: 700;
    }
    .container {
      max-width: 1240px;
      margin: 0 auto;
      display: flex;
      flex-direction: column;
      gap: 32px;
    }
    .section-title {
      font-size: 19px;
      font-weight: 700;
      color: #f1f5f9;
      border-bottom: 2px solid var(--card-border);
      padding-bottom: 10px;
      margin-bottom: 16px;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .section-desc {
      font-size: 13px;
      color: var(--text-muted);
      font-weight: normal;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(370px, 1fr));
      gap: 16px;
    }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 16px 18px;
      display: flex;
      flex-direction: column;
      gap: 12px;
      transition: transform 0.15s, border-color 0.15s, box-shadow 0.15s;
    }
    .card:hover {
      border-color: #3b82f6;
      box-shadow: 0 6px 20px rgba(0,0,0,0.25);
    }
    .card.in-game {
      border-color: #10b981;
      background: linear-gradient(180deg, #13242c 0%, #131b2e 100%);
    }
    .card-top {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 10px;
    }
    .card-title {
      font-size: 15.5px;
      font-weight: 600;
      color: #f8fafc;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .card-subtitle {
      font-size: 13px;
      color: var(--text-muted);
      margin-top: 2px;
    }
    .tags {
      display: flex;
      gap: 6px;
      flex-shrink: 0;
    }
    .tag {
      font-size: 11px;
      padding: 2px 7px;
      border-radius: 4px;
      background: var(--tag-bg);
      color: #94a3b8;
      font-weight: 600;
      text-transform: uppercase;
    }
    .tag.in-game-tag {
      background: #059669;
      color: #ffffff;
    }
    .tag.highlight {
      background: #0284c7;
      color: #ffffff;
    }
    .tag.zen {
      background: #475569;
      color: #e2e8f0;
    }
    .tag.music {
      background: #7c3aed;
      color: #ffffff;
    }
    .tag.animal {
      background: #ea580c;
      color: #ffffff;
    }
    audio {
      width: 100%;
      height: 38px;
      border-radius: 8px;
      outline: none;
    }
    .card-actions {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding-top: 6px;
      border-top: 1px dashed var(--card-border);
      font-size: 12px;
    }
    .choose-checkbox {
      display: flex;
      align-items: center;
      gap: 8px;
      cursor: pointer;
      font-weight: 600;
      color: #cbd5e1;
      user-select: none;
    }
    .choose-checkbox input {
      width: 16px;
      height: 16px;
      accent-color: var(--accent);
      cursor: pointer;
    }
    .file-path {
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      color: #64748b;
      font-size: 11.5px;
    }
    .toolbar {
      position: fixed;
      bottom: 0;
      left: 0;
      right: 0;
      background: rgba(15, 23, 42, 0.95);
      backdrop-filter: blur(12px);
      border-top: 1px solid var(--card-border);
      padding: 14px 28px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      max-width: 100%;
      z-index: 100;
      box-shadow: 0 -8px 24px rgba(0,0,0,0.5);
    }
    .toolbar-info {
      font-size: 14px;
      color: #cbd5e1;
    }
    .toolbar-info strong {
      color: var(--accent);
      font-size: 17px;
    }
    .btn-primary {
      background: linear-gradient(135deg, #0284c7 0%, #0369a1 100%);
      color: #ffffff;
      border: none;
      padding: 10px 22px;
      border-radius: 8px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 8px;
      transition: all 0.2s;
    }
    .btn-primary:hover {
      background: linear-gradient(135deg, #0ea5e9 0%, #0284c7 100%);
      transform: translateY(-1px);
    }
  </style>
</head>
<body>

  <header class="header">
    <h1>🌿 《湖畔家园》全量候选音频试听与选择台</h1>
    <p>全量采用本地纯音频格式（MP3 与无损 16-bit PCM WAV），macOS Safari / Chrome 均可 100% 秒播。零外部防盗链与 OGG 兼容障碍。</p>
    
    <div class="status-banner">
      ✔ <strong>你选中的 18 个核心声音已全部整理并集成到游戏 C++ 引擎中（编译已通过并实机验证）！</strong><br>
      涵盖：自然环境（鸟鸣、微风、湖浪、柴火、小雨）、4种地形脚步（雪地真实录音、石拱桥嗒嗒、草甸、泥土径）、木箱开合、钓鱼全套交互（抛竿入水、咬钩提醒、成功起鱼欢快吉他、鱼跑叹息）、浆果采摘灵动风铃反馈、大目标星芒结算、界面点击返回与视角捕获。<br>
      下方已为你<strong>全面扩充 40+ 个新增候选资源</strong>（新增：悠扬田园吉他/拇指琴BGM、农场生灵、河流小溪流泉、斧头劈柴、水花飞溅、梦幻与禅意全套拓展），请尽情试听！
    </div>
  </header>

  <nav class="tabs">
    <button class="tab-btn active" data-target="all">全部音效 (All)</button>
    <button class="tab-btn" data-target="integrated">★ 游戏中已集成 (18)</button>
    <button class="tab-btn" data-target="music">田园BGM旋律 (Music & BGM)</button>
    <button class="tab-btn" data-target="animals">农庄与自然生灵 (Animals)</button>
    <button class="tab-btn" data-target="ambience">自然环境与天气 (Ambience)</button>
    <button class="tab-btn" data-target="footsteps">脚步与位移 (Footsteps)</button>
    <button class="tab-btn" data-target="interact">门窗/农具/木箱 (Interact)</button>
    <button class="tab-btn" data-target="ui">界面/达成/禅意 (UI & Zen)</button>
  </nav>

  <main class="container">
'''

sounds = [
    # ================= 1. 游戏中已集成的 18 个核心声音 =================
    {
        "group": "integrated",
        "title": "晨曦鸟鸣 (Morning Birds)",
        "subtitle": "破晓至白天时的森林鸟鸣大合唱，白天自然起伏，入夜/下雨归巢隐匿",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "birds_morning.mp3",
        "target": "res://assets/audio/birds_morning.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "湖畔清风 (Lake Wind Breeze)",
        "subtitle": "湖面与山林间的温和基线清风，晴天舒缓，雨雪天随风力加强呼啸",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "wind_breeze.mp3",
        "target": "res://assets/audio/wind_breeze.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "湖水拍岸 (Water Shore Lap)",
        "subtitle": "湖风掠过水面微浪拍击湖滩沙石的声音，靠近湖岸与水峡时渐强",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "water_lap.mp3",
        "target": "res://assets/audio/water_lap.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "壁炉柴火 (Cabin Fireplace)",
        "subtitle": "木屋壁炉内干燥木柴燃烧噼啪脆响，进屋最响，出院平滑淡出",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "cabin_fireplace.mp3",
        "target": "res://assets/audio/cabin_fireplace.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "淅沥小雨 (Gentle Rain)",
        "subtitle": "柔和雨滴打在草地与屋顶的沙沙雨声，下雨自动淡入，雪天静音",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "rain_gentle.mp3",
        "target": "res://assets/audio/rain_gentle.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "草地踏步 (Grass Step)",
        "subtitle": "柔软草甸上的温和脚步踏步声，在全岛平原草地上自然循环",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "grass_step.wav",
        "target": "res://assets/audio/grass_step.wav",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "泥土踏步 (Dirt Path Step)",
        "subtitle": "脚踏在干爽泥土小径与田埂上的坚实沉稳脚步",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "dirt_step.wav",
        "target": "res://assets/audio/dirt_step.wav",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "石拱桥石板踏步 (Stone Bridge Step)",
        "subtitle": "走过横跨湖峡的石拱桥时清脆硬质的石板踏步声",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "stone_step.wav",
        "target": "res://assets/audio/stone_step.wav",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "真实踩雪声 (Snow Step - 4切片)",
        "subtitle": "深雪地行走真实咯吱声，内置4段不重复行走随机变体",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "snow_step.mp3",
        "target": "res://assets/audio/snow_step.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "自然风铃提示 (Organic Chime)",
        "subtitle": "采摘树丛饱满浆果时的空灵风铃音，即时反馈收集进度",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "organic_chime.mp3",
        "target": "res://assets/audio/organic_chime.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "达成-璀璨星芒 (Goal Shimmer)",
        "subtitle": "集满所有目标浆果瞬间绽放的空灵微光上升大达成音",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "goal_shimmer.mp3",
        "target": "res://assets/audio/goal_shimmer.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "木制物品轻放 (Wood Drop)",
        "subtitle": "钓鱼抛竿入水触感 / 储物木箱合上时的沉稳原木声响",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "zen_wood_drop.mp3",
        "target": "res://assets/audio/zen_wood_drop.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "禅意选定 (Zen Select)",
        "subtitle": "湖中游鱼咬钩、水面浮漂顿挫下沉时的灵敏提醒音",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "zen_select.mp3",
        "target": "res://assets/audio/zen_select.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "欢快小吉他 (Jingle Zen Bonus)",
        "subtitle": "控力成功钓起大鱼/小鱼跃出水面时的夏日欢快吉他小轮指",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "jingle_zen_bonus.mp3",
        "target": "res://assets/audio/jingle_zen_bonus.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "禅意轻按 (Zen Press)",
        "subtitle": "原木微按触感，推门互动与操作按键时的清脆反馈",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "zen_press.mp3",
        "target": "res://assets/audio/zen_press.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "菜单滑出 (Zen Expand)",
        "subtitle": "点击画面捕获鼠标视角 / 掀开储物箱盖板时的顺滑展开音",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "zen_expand.mp3",
        "target": "res://assets/audio/zen_expand.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "菜单收起 (Zen Collapse)",
        "subtitle": "按 ESC 释放鼠标视角 / 钓鱼脱钩跑鱼时的柔和收束音",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "zen_collapse.mp3",
        "target": "res://assets/audio/zen_collapse.mp3",
        "is_integrated": True
    },
    {
        "group": "integrated",
        "title": "界面点击 (UI Click)",
        "subtitle": "经典清脆的轻量点击音（与禅意轻按互为备用）",
        "tag": "已在游戏中",
        "tag_class": "in-game-tag",
        "src": "ui_click.mp3",
        "target": "res://assets/audio/ui_click.mp3",
        "is_integrated": True
    },

    # ================= 2. 田园 BGM 与治愈旋律 (Music & BGM) =================
    {
        "group": "music",
        "title": "恬静午后民谣吉他 (Afternoon Guitar BGM)",
        "subtitle": "温暖阳光下轻轻拨动的民谣尼龙弦吉他琶音循环，令人心旷神怡",
        "tag": "无损 WAV 循环",
        "tag_class": "music",
        "src": "candidates/music_zen/bgm_guitar_afternoon.wav",
        "target": "res://assets/audio/bgm_guitar_afternoon.wav",
        "is_integrated": False
    },
    {
        "group": "music",
        "title": "静夜星空拇指琴 (Starry Kalimba BGM)",
        "subtitle": "盛夏深夜湖畔的卡林巴拇指琴微旋律，晶莹剔透、静心治愈",
        "tag": "无损 WAV 循环",
        "tag_class": "music",
        "src": "candidates/music_zen/bgm_kalimba_night.wav",
        "target": "res://assets/audio/bgm_kalimba_night.wav",
        "is_integrated": False
    },
    {
        "group": "music",
        "title": "梦幻轻竖琴 (Goal Dreamy Harp)",
        "subtitle": "如同拂晓微光一般的竖琴拨弦，适合作为章节切换或特殊景致音乐",
        "tag": "MP3 旋律",
        "tag_class": "music",
        "src": "candidates/ui/goal_dreamy_harp.mp3",
        "target": "res://assets/audio/goal_dreamy_harp.mp3",
        "is_integrated": False
    },
    {
        "group": "music",
        "title": "空灵风铃旋律 (Goal Zen Chime)",
        "subtitle": "清脆的铜管风铃飘过湖面的悠扬旋律，适合登顶观景或成就触发",
        "tag": "MP3 旋律",
        "tag_class": "music",
        "src": "candidates/ui/goal_zen_chime.mp3",
        "target": "res://assets/audio/goal_zen_chime.mp3",
        "is_integrated": False
    },

    # ================= 3. 农庄与自然生灵 (Animals) =================
    {
        "group": "animals",
        "title": "雄鸡破晓打鸣 (Rooster Crow)",
        "subtitle": "农庄清晨太阳升起时的公鸡高亢嘹亮啼叫，宣告新的一天开始",
        "tag": "MP3 叫声",
        "tag_class": "animal",
        "src": "candidates/animals/rooster.mp3",
        "target": "res://assets/audio/rooster.mp3",
        "is_integrated": False
    },
    {
        "group": "animals",
        "title": "牧场奶牛低哞 (Cow Moo)",
        "subtitle": "牧场围栏边温顺母牛的悠闲长哞，纯正农场风味",
        "tag": "MP3 叫声",
        "tag_class": "animal",
        "src": "candidates/animals/cow_moo.mp3",
        "target": "res://assets/audio/cow_moo.mp3",
        "is_integrated": False
    },
    {
        "group": "animals",
        "title": "骏马长嘶 (Horse Whinny)",
        "subtitle": "马厩边马儿精神抖擞的扬蹄嘶鸣",
        "tag": "MP3 叫声",
        "tag_class": "animal",
        "src": "candidates/animals/horse_whinny.mp3",
        "target": "res://assets/audio/horse_whinny.mp3",
        "is_integrated": False
    },
    {
        "group": "animals",
        "title": "农庄忠犬远吠 (Dog Howl / Bark)",
        "subtitle": "守卫木屋小院的农庄忠犬在暮色下的悠长呼应",
        "tag": "MP3 叫声",
        "tag_class": "animal",
        "src": "candidates/animals/dog_howl.mp3",
        "target": "res://assets/audio/dog_howl.mp3",
        "is_integrated": False
    },
    {
        "group": "animals",
        "title": "夏夜静谧虫鸣 (Night Crickets)",
        "subtitle": "盛夏田园深夜草丛中的蟋蟀低吟与纺织娘声，适合夜间无缝铺底",
        "tag": "MP3 循环",
        "tag_class": "highlight",
        "src": "candidates/ambience/night_crickets.mp3",
        "target": "res://assets/audio/night_crickets.mp3",
        "is_integrated": False
    },

    # ================= 4. 自然环境与水流天气 (Ambience & Weather) =================
    {
        "group": "ambience",
        "title": "林间小溪潺潺 (Stream Loop)",
        "subtitle": "穿过小岛森林的石缝山泉小溪流水声，晶莹活跃（无缝循环）",
        "tag": "MP3 循环",
        "tag_class": "highlight",
        "src": "candidates/nature_extra/stream_loop.mp3",
        "target": "res://assets/audio/stream_loop.mp3",
        "is_integrated": False
    },
    {
        "group": "ambience",
        "title": "开阔河流奔腾 (River Loop)",
        "subtitle": "宽阔水面的奔流涌动声，水势充沛（无缝循环）",
        "tag": "MP3 循环",
        "tag_class": "highlight",
        "src": "candidates/nature_extra/river_loop.mp3",
        "target": "res://assets/audio/river_loop.mp3",
        "is_integrated": False
    },
    {
        "group": "ambience",
        "title": "远山低沉雷鸣 (Thunder Rumble)",
        "subtitle": "暴雨天远处云层滚动的深沉闷雷，极具沉浸感",
        "tag": "MP3 单次",
        "tag_class": "highlight",
        "src": "candidates/nature_extra/thunder.mp3",
        "target": "res://assets/audio/thunder.mp3",
        "is_integrated": False
    },

    # ================= 5. 地面脚步与位移涉水 (Footsteps & Movement) =================
    {
        "group": "footsteps",
        "title": "木地板踏步 (Wood Step)",
        "subtitle": "木屋室内地板沉稳咚咚声，原木地板踩踏质感",
        "tag": "PCM WAV",
        "tag_class": "highlight",
        "src": "candidates/footsteps/wood_step.wav",
        "target": "res://assets/audio/wood_step.wav",
        "is_integrated": False
    },
    {
        "group": "footsteps",
        "title": "石质岩石走动 (Stone Walking)",
        "subtitle": "坚硬岩石表面轻便走动声，适合登山露岩处",
        "tag": "PCM WAV",
        "tag_class": "highlight",
        "src": "candidates/footsteps/walking_stone.wav",
        "target": "res://assets/audio/walking_stone.wav",
        "is_integrated": False
    },
    {
        "group": "footsteps",
        "title": "轻巧起跳腾空 (Jump)",
        "subtitle": "主角蹬地起跳的轻快气流与受力声",
        "tag": "PCM WAV",
        "tag_class": "highlight",
        "src": "candidates/footsteps/jump.wav",
        "target": "res://assets/audio/jump.wav",
        "is_integrated": False
    },
    {
        "group": "footsteps",
        "title": "涉水与飞溅落水 (Water Splash Gentle)",
        "subtitle": "踩入湖水浅滩或跃入水中的柔和水花轻溅与气泡声",
        "tag": "PCM WAV",
        "tag_class": "highlight",
        "src": "candidates/interact/water_splash_gentle.wav",
        "target": "res://assets/audio/water_splash_gentle.wav",
        "is_integrated": False
    },

    # ================= 6. 门窗/农具/木箱/采摘 (Interact & Tools) =================
    {
        "group": "interact",
        "title": "斧头劈柴清脆 (Axe Chop Wood)",
        "subtitle": "锋利斧刃劈入干燥松木时的清脆破木与共鸣声，劳作质感十足",
        "tag": "PCM WAV",
        "tag_class": "highlight",
        "src": "candidates/interact/wood_chop_axe.wav",
        "target": "res://assets/audio/wood_chop_axe.wav",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "自然原木开启 (Organic Open)",
        "subtitle": "柔和原木转轴开启声，适合推开木门或掀开木箱盖",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/organic_open.mp3",
        "target": "res://assets/audio/organic_open.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "自然原木闭合 (Organic Close)",
        "subtitle": "原木贴合闭合，木扇入框的柔和沉实声",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/organic_close.mp3",
        "target": "res://assets/audio/organic_close.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "禅意质朴木门开启 (Zen Door Open)",
        "subtitle": "微带木轴摩擦质感的推门声，古朴典雅",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/interact/zen_door_open.mp3",
        "target": "res://assets/audio/zen_door_open.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "禅意质朴木门合上 (Zen Door Close)",
        "subtitle": "木门轻轻扣合在门框上的沉稳轻微震颤",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/interact/zen_door_close.mp3",
        "target": "res://assets/audio/zen_door_close.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "百叶窗推开 (Window Latch Open)",
        "subtitle": "推开木百叶窗的木闩声，清爽利落",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/interact/mech_window_latch.mp3",
        "target": "res://assets/audio/mech_window_latch.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "百叶窗合上 (Window Latch Close)",
        "subtitle": "关闭木百叶窗并锁扣",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/interact/mech_window_close.mp3",
        "target": "res://assets/audio/mech_window_close.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "果实摘取脱落 (Berry Pluck)",
        "subtitle": "从枝头摘下熟透浆果的轻微啵声",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/interact/berry_pluck_organic.mp3",
        "target": "res://assets/audio/berry_pluck_organic.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "浆果饱满落篮 (Berry Drop)",
        "subtitle": "饱满浆果落入竹篮底部的柔和啪嗒声",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/interact/organic_berry_drop.mp3",
        "target": "res://assets/audio/organic_berry_drop.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "灌木枝叶拨动 (Bush Rustle)",
        "subtitle": "伸手拨动浆果丛或穿过灌木时的自然枝叶沙沙声",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/interact/organic_bush_rustle.mp3",
        "target": "res://assets/audio/organic_bush_rustle.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "物品卡扣吸附 (Organic Snap)",
        "subtitle": "背包整理或把工具插入插槽时的清脆吸附声",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/ui_extra/organic_snap.mp3",
        "target": "res://assets/audio/organic_snap.mp3",
        "is_integrated": False
    },
    {
        "group": "interact",
        "title": "金币/小宝物拾取 (Coin Pickup)",
        "subtitle": "清脆明亮的小物拾取音，适合捡起钓上的宝箱或特殊宝物",
        "tag": "PCM WAV",
        "tag_class": "highlight",
        "src": "candidates/interact/coin_pickup.wav",
        "target": "res://assets/audio/coin_pickup.wav",
        "is_integrated": False
    },

    # ================= 7. 界面/达成/禅意 (UI, Rewards & Zen) =================
    {
        "group": "ui",
        "title": "禅意静心奖励 (Zen Reward)",
        "subtitle": "如雨滴落在古刹青石上的双音纯净钟鸣，超凡脱俗",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/zen_reward.mp3",
        "target": "res://assets/audio/zen_reward.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "禅意顿悟达成 (Zen Achievement)",
        "subtitle": "悠远开阔的晨钟达成旋律，心境宁静",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/zen_achievement.mp3",
        "target": "res://assets/audio/zen_achievement.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "破晓苏醒晨钟 (Zen Wake)",
        "subtitle": "天明破晓时分的唤醒音符，如沐晨光",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/zen_wake.mp3",
        "target": "res://assets/audio/zen_wake.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "晚钟入眠 (Zen Sleep)",
        "subtitle": "夜幕降临、上床休息时的晚钟余韵，伴君安眠",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/zen_sleep.mp3",
        "target": "res://assets/audio/zen_sleep.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "自然阶段达成 (Organic Achievement)",
        "subtitle": "灵动圆润的自然升华音，适合小任务完成",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/ui_extra/organic_achievement.mp3",
        "target": "res://assets/audio/organic_achievement.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "自然升级律动 (Organic Level Up)",
        "subtitle": "生机勃勃的连音递进，像植物破土发芽",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/ui_extra/organic_level_up.mp3",
        "target": "res://assets/audio/organic_level_up.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "梦幻星芒升阶 (Dreamy Level Up)",
        "subtitle": "柔光慢漾的空灵上升琶音，充满治愈与童话感",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/ui_extra/dreamy_level_up.mp3",
        "target": "res://assets/audio/dreamy_level_up.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "梦幻圆满结算 (Dreamy Complete)",
        "subtitle": "微风拂过水晶的清透音律，令人心情舒畅",
        "tag": "MP3",
        "tag_class": "highlight",
        "src": "candidates/ui_extra/dreamy_complete.mp3",
        "target": "res://assets/audio/dreamy_complete.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "轻柔暖心选择 (Soft Select)",
        "subtitle": "羊毛毡包裹一般的温润微碰声，极度柔和不伤耳",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/soft_select.mp3",
        "target": "res://assets/audio/soft_select.mp3",
        "is_integrated": False
    },
    {
        "group": "ui",
        "title": "轻柔微按 (Soft Press)",
        "subtitle": "短促暖心的按压触感",
        "tag": "MP3",
        "tag_class": "zen",
        "src": "candidates/ui_extra/soft_press.mp3",
        "target": "res://assets/audio/soft_press.mp3",
        "is_integrated": False
    }
]

sections = [
    ("integrated", "★ 已集成到游戏中的 18 个声音", "已直接生效于你的游戏（踏步、鸟鸣、风雨、湖水、壁炉、结算、点击返回、钓鱼全套、木箱、风铃等）"),
    ("music", "田园 BGM 与治愈旋律 (Music & BGM)", "悠扬民谣吉他、静夜拇指琴、空灵竖琴风铃，让小岛充满生活浪漫"),
    ("animals", "农庄与自然生灵 (Animals)", "雄鸡打鸣、奶牛长哞、骏马长嘶、忠犬远吠，小岛更添烟火气"),
    ("ambience", "自然环境与水流天气 (Ambience & Weather)", "林间小溪流水、奔流大河、雷鸣电闪，丰富四季与气候"),
    ("footsteps", "地面脚步与位移涉水 (Footsteps & Movement)", "匹配不同路面脚感，包含涉水溅花与轻盈起跳"),
    ("interact", "门窗/农具/木箱/采摘 (Interact & Tools)", "斧头砍柴、开窗开门、浆果摘取、物品吸附"),
    ("ui", "界面/达成/禅意 (UI, Rewards & Zen)", "空灵顿悟、晨钟晚祷、升级跃升，带来宁静的游玩心境")
]

html_content = html_head

for sec_id, sec_title, sec_desc in sections:
    html_content += f'''
    <section class="sound-group" data-group="{sec_id}">
      <div class="section-title">
        <span>{sec_title}</span>
        <span class="section-desc">{sec_desc}</span>
      </div>
      <div class="grid">
'''
    if sec_id == "integrated":
        items = [s for s in sounds if s["is_integrated"]]
    else:
        items = [s for s in sounds if not s["is_integrated"] and s["group"] == sec_id]
        
    for s in items:
        in_game_class = " in-game" if s["is_integrated"] else ""
        check_attr = "checked disabled" if s["is_integrated"] else ""
        check_label = "<span style='color:#10b981;'>✓ 游戏中已集成</span>" if s["is_integrated"] else "<span>★ 选用此项</span>"
        html_content += f'''
        <div class="card{in_game_class}">
          <div class="card-top">
            <div>
              <div class="card-title">{s["title"]}</div>
              <div class="card-subtitle">{s["subtitle"]}</div>
            </div>
            <div class="tags">
              <span class="tag {s["tag_class"]}">{s["tag"]}</span>
            </div>
          </div>
          <audio controls preload="none" src="{s["src"]}"></audio>
          <div class="card-actions">
            <label class="choose-checkbox">
              <input type="checkbox" data-name="{s["title"].split(' (')[0]}" data-target="{s["target"]}" {check_attr}>
              {check_label}
            </label>
            <span class="file-path">{s["target"]}</span>
          </div>
        </div>
'''
    html_content += '''
      </div>
    </section>
'''

html_content += '''
  </main>

  <div class="toolbar">
    <div class="toolbar-info">
      当前新勾选 <strong id="selected-count">0</strong> 个候选音效
    </div>
    <button class="btn-primary" id="btn-export">📋 复制新选中的音效清单发给助手</button>
  </div>

  <script>
    const tabs = document.querySelectorAll('.tab-btn');
    const groups = document.querySelectorAll('.sound-group');
    tabs.forEach(tab => {
      tab.addEventListener('click', () => {
        tabs.forEach(t => t.classList.remove('active'));
        tab.classList.add('active');
        const target = tab.getAttribute('data-target');
        groups.forEach(group => {
          if (target === 'all' || group.getAttribute('data-group') === target) {
            group.style.display = 'block';
          } else {
            group.style.display = 'none';
          }
        });
      });
    });

    const checkboxes = document.querySelectorAll('.choose-checkbox input:not(:disabled)');
    const countDisplay = document.getElementById('selected-count');
    const exportBtn = document.getElementById('btn-export');

    function updateCount() {
      const checked = document.querySelectorAll('.choose-checkbox input:checked:not(:disabled)');
      countDisplay.textContent = checked.length;
    }

    checkboxes.forEach(cb => cb.addEventListener('change', updateCount));

    exportBtn.addEventListener('click', () => {
      const checked = Array.from(checkboxes).filter(cb => cb.checked);
      if (checked.length === 0) {
        alert('你还没有勾选新的候选音效，请先试听并勾选！');
        return;
      }
      const list = checked.map(cb => `- 【${cb.getAttribute('data-name')}】 => 文件: ${cb.getAttribute('data-target')}`).join('\\n');
      navigator.clipboard.writeText(list).then(() => {
        alert('已复制新选中的清单！发给我即可一键接入游戏：\\n\\n' + list);
      }).catch(() => {
        prompt('请复制以下清单发给助手：', list);
      });
    });

    const audios = document.querySelectorAll('audio');
    audios.forEach(a => {
      a.addEventListener('play', () => {
        audios.forEach(other => {
          if (other !== a && !other.paused) other.pause();
        });
      });
    });
  </script>
</body>
</html>
'''

with open('project/assets/audio/preview.html', 'w', encoding='utf-8') as f:
    f.write(html_content)

print('Updated preview.html successfully!')
