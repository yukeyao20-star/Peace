#!/usr/bin/env python3
"""
《湖畔家园 Homestead》候选音频资源批量下载器
运行方式：
    python3 project/assets/audio/download_candidates.py
将所有候选 MP3 / 音频文件下载到 project/assets/audio/candidates/ 目录下，
方便在 macOS 访达 (Finder) 中按空格键直接试听。
"""

import os
import urllib.request
import ssl

# 忽略本地可能的证书链问题
ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

AUDIO_ITEMS = [
    # 1. 地表脚步系列
    {
        "category": "footsteps",
        "name": "grass_step.wav",
        "url": "https://raw.githubusercontent.com/dragon1freak/df-godot-utils/main/footstepper/sounds/grass/grass_footsteps.wav",
        "desc": "草地软草踩踏"
    },
    {
        "category": "footsteps",
        "name": "stone_step.ogg",
        "url": "https://raw.githubusercontent.com/KenneyNL/Starter-Kit-3D-Platformer/main/sounds/walking.ogg",
        "desc": "石板/石桥清脆踏步"
    },
    {
        "category": "footsteps",
        "name": "wood_step.ogg",
        "url": "https://raw.githubusercontent.com/dragon1freak/df-godot-utils/main/footstepper/sounds/wood/wood_footstep.ogg",
        "desc": "宅基地木屋/栈桥沉稳踏步"
    },
    {
        "category": "footsteps",
        "name": "dirt_step.wav",
        "url": "https://raw.githubusercontent.com/dragon1freak/df-godot-utils/main/footstepper/sounds/sand/sand_footstep.wav",
        "desc": "泥土小径沙质踏步"
    },

    # 2. 物件交互系列
    {
        "category": "interact",
        "name": "berry_pluck.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/organic/drop.mp3",
        "desc": "采摘浆果清脆脱落声"
    },
    {
        "category": "interact",
        "name": "berry_rustle.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/organic/drag-start.mp3",
        "desc": "采摘灌木枝叶沙沙声"
    },
    {
        "category": "interact",
        "name": "chest_open.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/organic/open.mp3",
        "desc": "开木箱盖"
    },
    {
        "category": "interact",
        "name": "chest_close.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/organic/close.mp3",
        "desc": "合上木箱"
    },
    {
        "category": "interact",
        "name": "item_collect.ogg",
        "url": "https://raw.githubusercontent.com/KenneyNL/Starter-Kit-3D-Platformer/main/sounds/coin.ogg",
        "desc": "收集/放入背包"
    },

    # 3. 界面与成就反馈
    {
        "category": "ui",
        "name": "goal_achieved.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/organic/complete.mp3",
        "desc": "目标达成治愈上升音"
    },
    {
        "category": "ui",
        "name": "goal_zen.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/zen/achievement.mp3",
        "desc": "空灵治愈风铃完成音"
    },
    {
        "category": "ui",
        "name": "ui_click.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/organic/check.mp3",
        "desc": "轻触点击按键"
    },
    {
        "category": "ui",
        "name": "ui_back.mp3",
        "url": "https://raw.githubusercontent.com/romainsimon/uisfx/main/packages/uisfx/sounds/organic/back.mp3",
        "desc": "退出/取消操作"
    },

    # 4. 自然环境白噪音 (循环)
    {
        "category": "ambience",
        "name": "birds_morning.ogg",
        "url": "https://upload.wikimedia.org/wikipedia/commons/4/4b/European_robin_%28Erithacus_rubecula%29_song.ogg",
        "desc": "晨曦林间鸟鸣"
    },
    {
        "category": "ambience",
        "name": "rain_gentle.ogg",
        "url": "https://upload.wikimedia.org/wikipedia/commons/1/15/Rain_light_loop.ogg",
        "desc": "细雨淅沥声"
    },
    {
        "category": "ambience",
        "name": "water_lap.ogg",
        "url": "https://upload.wikimedia.org/wikipedia/commons/4/4f/Lapping_water_loop.ogg",
        "desc": "湖水轻拍湖岸"
    },

    # 5. 背景音乐
    {
        "category": "bgm",
        "name": "bgm_homestead_guitar.mp3",
        "url": "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Carefree.mp3",
        "desc": "温暖民谣吉他 BGM"
    },
    {
        "category": "bgm",
        "name": "bgm_lake_piano.mp3",
        "url": "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Meditation%20Impromptu%2001.mp3",
        "desc": "宁静湖畔钢琴 BGM"
    },
]

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    candidates_dir = os.path.join(base_dir, "candidates")
    os.makedirs(candidates_dir, exist_ok=True)

    print("=" * 60)
    print(" 开始下载《湖畔家园》候选试听音效...")
    print(f" 保存目标目录: {candidates_dir}")
    print("=" * 60)

    success_count = 0
    headers = {"User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"}

    for item in AUDIO_ITEMS:
        cat_dir = os.path.join(candidates_dir, item["category"])
        os.makedirs(cat_dir, exist_ok=True)
        dest_path = os.path.join(cat_dir, item["name"])

        if os.path.exists(dest_path) and os.path.getsize(dest_path) > 0:
            print(f" [已存在] {item['category']}/{item['name']} - {item['desc']}")
            success_count += 1
            continue

        print(f" [正在下载] {item['category']}/{item['name']} ({item['desc']})...")
        try:
            req = urllib.request.Request(item["url"], headers=headers)
            with urllib.request.urlopen(req, context=ctx, timeout=15) as resp:
                data = resp.read()
                with open(dest_path, "wb") as f:
                    f.write(data)
            print(f"     -> 成功保存 ({len(data)} 字节)")
            success_count += 1
        except Exception as e:
            print(f"     -> 下载失败 ({e})，可直接使用 preview.html 在线试听")

    print("=" * 60)
    print(f" 下载完成: {success_count}/{len(AUDIO_ITEMS)} 个音频文件已就位。")
    print(f" 你现在可以在访达 (Finder) 打开: {candidates_dir}，选中文件按空格键试听！")
    print("=" * 60)

if __name__ == "__main__":
    main()
