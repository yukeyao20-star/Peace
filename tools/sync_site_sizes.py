#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 site/index.html 里显示的三平台安装包体积，按 site/download/ 里的实际文件大小更新。

用法：python3 tools/sync_site_sizes.py
说明：找不到对应模式就跳过，不会报错；数字没变化则不写文件。
"""
import os
import re
import sys

HTML = 'site/index.html'
DOWNLOAD = 'site/download'
FILES = ['peace-macos.zip', 'peace-windows-x64.zip', 'peace-linux-amd64.deb']


def main():
    if not os.path.exists(HTML):
        print('  跳过：找不到 %s' % HTML)
        return 0

    sizes = {}
    for f in FILES:
        p = os.path.join(DOWNLOAD, f)
        if os.path.exists(p):
            sizes[f] = int(round(os.path.getsize(p) / 1048576))
    if not sizes:
        print('  跳过：%s 里没有产物' % DOWNLOAD)
        return 0

    s = open(HTML, encoding='utf-8').read()
    changed = 0

    def sub_count(pat, repl, text):
        """替换并只统计真正发生变化的处数。"""
        n = 0

        def _r(m):
            nonlocal n
            out = repl(m)
            if out != m.group(0):
                n += 1
            return out

        return re.sub(pat, _r, text), n

    # 1) 每张下载卡：<p class="dl-file">peace-macos.zip<span>162 MB</span></p>
    for f, n in sizes.items():
        pat = r'(<p class="dl-file">' + re.escape(f) + r'<span>)\d+( MB</span>)'
        s, k = sub_count(pat, lambda m: m.group(1) + str(n) + m.group(2), s)
        changed += k
        print('  %-26s %4d MB%s' % (f, n, '  ← 已更新' if k else ''))

    # 2) 运行要求里的范围：<dd>115 – 162 MB（按平台）</dd>
    if len(sizes) > 1:
        lo, hi = min(sizes.values()), max(sizes.values())
        pat = r'(<dt>安装包</dt><dd>)\d+ – \d+( MB（按平台）</dd>)'
        s, k = sub_count(pat, lambda m: m.group(1) + '%d – %d' % (lo, hi) + m.group(2), s)
        changed += k
        print('  运行要求区间          %4d – %d MB%s' % (lo, hi, '  ← 已更新' if k else ''))

    if changed:
        open(HTML, 'w', encoding='utf-8').write(s)
        print('  ✓ 已更新 %d 处' % changed)
    else:
        print('  ✓ 数字与页面一致，无需改动')
    return 0


if __name__ == '__main__':
    sys.exit(main())
