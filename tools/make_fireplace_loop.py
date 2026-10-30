#!/usr/bin/env python3
"""把 cabin_fireplace.mp3 做成可用的"壁炉燃烧"无缝循环。

背景：原始素材 cabin_fireplace.mp3 长 714s，但整体录得极轻 ——
实测 RMS 只有 -50.8 dBFS（峰值 -4.9 dBFS），动态范围 46dB。
Ambience 里壁炉层增益是 -14dB，两者相加实际只有 -65dB，
比风声（-49.7dB）、雨声（-42.6dB）、湖水（-42.9dB）低 20dB 以上，
所以玩家完全听不见。而且素材峰值已经到 -4.9dB，直接补增益必然削顶，
所以必须离线做"响度归一化 + 软限幅"，不能靠运行时调 volume_db。

本脚本：
  1. 在整段素材里挑一个 40s 窗口（RMS 最高 → 火最旺、噼啪最密的那段）；
  2. 迭代补增益 + tanh 软限幅，把 RMS 拉到目标值，峰值压在 -1.5dBFS 以内；
  3. 首尾 0.6s 交叉淡化，做成无缝循环；
  4. 输出 16-bit 单声道 22050Hz WAV。

用法：
  python3 tools/make_fireplace_loop.py
（需要先用 afconvert 把 mp3 转成临时 WAV，见文件末尾说明）
"""

import math
import os
import struct
import sys
import wave

SRC = "/tmp/audm/cabin_fireplace.wav"          # afconvert 出来的单声道 WAV
DST = "project/assets/audio/cabin_fireplace_loop.wav"

LOOP_SEC = 45.0        # 循环长度
XFADE_SEC = 0.8        # 首尾交叉淡化长度
TARGET_RMS_DB = -14.0  # 目标响度（提升 6dB，显著增强整体燃烧与柴火噼啪存在感）
CEIL_DB = -0.5         # 软限幅天花板（充分利用动态范围）


def db(x):
    return 20.0 * math.log10(x) if x > 0.0 else -999.0


def read_mono(path):
    w = wave.open(path, "rb")
    n, sr, ch, sw = w.getnframes(), w.getframerate(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(n)
    w.close()
    if sw != 2:
        raise SystemExit("需要 16-bit WAV，实际 sampwidth=%d" % sw)
    s = struct.unpack("<%dh" % (len(raw) // 2), raw)
    if ch > 1:  # 多声道就取平均
        s = [sum(s[i:i + ch]) / ch for i in range(0, len(s), ch)]
    return [v / 32768.0 for v in s], sr


def rms_of(seq):
    if not seq:
        return 0.0
    return math.sqrt(sum(v * v for v in seq) / len(seq))


def best_window(x, sr, sec):
    win = int(sec * sr)
    step = sr  # 1s 步长足够
    best_r, best_i = 0.0, 0
    for i in range(0, max(1, len(x) - win), step):
        r = rms_of(x[i:i + win])
        if r > best_r:
            best_r, best_i = r, i
    return best_i, best_r


def soft_limit(x, ceil):
    """tanh 软限幅：小信号近似线性，大峰值被压向天花板，不会硬削出方波。"""
    return [ceil * math.tanh(v / ceil) for v in x]


def main():
    if not os.path.exists(SRC):
        raise SystemExit(
            "缺少 %s\n先执行：\n"
            "  mkdir -p /tmp/audm && afconvert -f WAVE -d LEI16@22050 -c 1 \\\n"
            "    project/assets/audio/cabin_fireplace.mp3 /tmp/audm/cabin_fireplace.wav" % SRC)

    x, sr = read_mono(SRC)
    print("源素材：%.1fs  RMS=%.1f dB  峰值=%.1f dB" % (len(x) / sr, db(rms_of(x)), db(max(abs(v) for v in x))))

    n_loop = int(LOOP_SEC * sr)
    n_cf = int(XFADE_SEC * sr)
    start, seg_rms = best_window(x, sr, LOOP_SEC)
    print("选中窗口：start=%.1fs  RMS=%.1f dB（全曲最旺的一段）" % (start / sr, db(seg_rms)))

    # 多取 n_cf 个样本在头部，用于尾→首交叉淡化
    seg = x[start:start + n_loop + n_cf]
    if len(seg) < n_loop + n_cf:
        seg = seg + [0.0] * (n_loop + n_cf - len(seg))

    # 迭代归一化：补增益 → 软限幅 → 量 RMS → 修正增益
    target = 10.0 ** (TARGET_RMS_DB / 20.0)
    ceil = 10.0 ** (CEIL_DB / 20.0)
    g = 1.0
    out = seg
    for _ in range(6):
        out = soft_limit([v * g for v in seg], ceil)
        r = rms_of(out)
        if r <= 0.0:
            break
        g *= target / r
        if abs(db(r) - TARGET_RMS_DB) < 0.05:
            out = soft_limit([v * g for v in seg], ceil)
            break
    out = soft_limit([v * g for v in seg], ceil)

    # 无缝循环：把**头部**与"循环末尾之后"的样本交叉淡化。
    #   out[i] = S[N+i]*(1-u) + S[i]*u，u = i/(cf-1) → 0..1
    # 于是 out[0] = S[N]，正好接在 out[N-1] = S[N-1] 之后（相邻样本，接缝无跳变），
    # 再用 0.6s 把内容从"S[N..]"平滑过渡回循环自身的 S[0..]。
    # 注意：淡化必须在补增益之后做，且不能拿 S 自己的开头去淡尾部
    # （那会把尾部淡到素材的前导静音上，每 40 秒"喘一口气"）。
    for i in range(n_cf):
        u = i / float(n_cf - 1)
        out[i] = out[n_loop + i] * (1.0 - u) + out[i] * u
    loop = out[:n_loop]

    peak = max(abs(v) for v in loop)
    print("输出循环：%.1fs  RMS=%.1f dB  峰值=%.1f dB  增益=%+.1f dB"
          % (len(loop) / sr, db(rms_of(loop)), db(peak), db(g)))

    os.makedirs(os.path.dirname(DST), exist_ok=True)
    w = wave.open(DST, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(sr)
    w.writeframes(struct.pack("<%dh" % len(loop), *[max(-32768, min(32767, int(round(v * 32767.0)))) for v in loop]))
    w.close()
    print("已写出 %s（%.2f MB）" % (DST, os.path.getsize(DST) / 1048576.0))


if __name__ == "__main__":
    main()
