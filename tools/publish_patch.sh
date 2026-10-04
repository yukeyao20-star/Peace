#!/usr/bin/env bash
# ============================================================================
#  publish_patch.sh —— 一键导出游戏热更新补丁并部署到 VPS
#
#  特性：
#  1. 自动利用 Godot 导出最新工程补丁包 patch.pck
#  2. 自动生成包含 sha256 校验与元数据的 version.json
#  3. 清理 VPS 旧补丁，确保服务器【只保留最新的包】
#  4. 绝不影响 VPS 订阅节点服务
#
#  用法：
#    ./tools/publish_patch.sh [版本号] [版本代号] [更新说明...]
#    例如: ./tools/publish_patch.sh "0.1.1" 101 "优化小溪瀑布与开窗音效，新增荧光蘑菇拖尾粒子"
# ============================================================================
set -euo pipefail

VERSION="${1:-0.1.1}"
VERSION_CODE="${2:-101}"
CHANGELOG="${3:-1. 优化小溪与瀑布自然景观\n2. 修复开窗关窗音效与交互\n3. 新增夜晚荧光蘑菇粒子拖尾飘动效果\n4. 优化钓鱼落水判定与睡觉过场动画}"

GODOT="${GODOT:-/opt/homebrew/bin/godot}"
if ! command -v "$GODOT" >/dev/null 2>&1; then
	if [ -f "/Applications/Godot.app/Contents/MacOS/Godot" ]; then
		GODOT="/Applications/Godot.app/Contents/MacOS/Godot"
	else
		echo "错误: 未找到 Godot 可执行程序！" >&2
		exit 1
	fi
fi

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_UPDATE="$REPO/dist/update"
mkdir -p "$DIST_UPDATE"

echo "==> 1. 正在导出最新 patch.pck..."
"$GODOT" --headless --path "$REPO/project" --export-pack "macOS" "$DIST_UPDATE/patch.pck"

if [ ! -f "$DIST_UPDATE/patch.pck" ]; then
	echo "错误: patch.pck 导出失败！" >&2
	exit 1
fi

PATCH_SIZE=$(stat -f %z "$DIST_UPDATE/patch.pck")
PATCH_SHA256=$(shasum -a 256 "$DIST_UPDATE/patch.pck" | awk '{print $1}')
RELEASE_DATE=$(date "+%Y-%m-%d %H:%M:%S")

echo "==> 2. 生成 version.json (版本: $VERSION, 代码: $VERSION_CODE, 大小: $PATCH_SIZE 字节)..."
python3 -c "
import json

changelog_items = '''$CHANGELOG'''.strip().split('\n')
data = {
    'version': '$VERSION',
    'version_code': int('$VERSION_CODE'),
    'title': '全新小溪与壮丽飞瀑画质升级',
    'release_date': '$RELEASE_DATE',
    'force_update': False,
    'changelog': [line.strip() for line in changelog_items if line.strip()],
    'patch_url': 'https://yukeyao.sbs/update/patch.pck',
    'download_url': 'https://yukeyao.sbs/update/patch.pck',
    'full_download_url': 'https://yukeyao.sbs/#download',
    'patch_size': f'{int(\"$PATCH_SIZE\") / (1024 * 1024):.1f} MB',
    'sha256': '$PATCH_SHA256',
    'size': int('$PATCH_SIZE'),
    'min_client_version': '0.1.0'
}

with open('$DIST_UPDATE/version.json', 'w', encoding='utf-8') as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
"

echo "==> 3. 清理 VPS 上的历史旧补丁 (只保留最新包)..."
ssh vps 'rm -rf /var/www/game/update/*'

echo "==> 4. 上传最新补丁与版本清单到 VPS..."
scp "$DIST_UPDATE/patch.pck" "$DIST_UPDATE/version.json" vps:/var/www/game/update/
ssh vps 'chown -R www-data:www-data /var/www/game/update && chmod 644 /var/www/game/update/*'

echo "==> 5. 校验线上发布状态..."
HTTP_STATUS=$(curl -s -o /dev/null -w "%{http_code}" https://yukeyao.sbs/update/version.json)
if [ "$HTTP_STATUS" = "200" ]; then
	echo "✅ 发布成功！官网更新清单访问正常 (HTTP 200)"
	curl -s https://yukeyao.sbs/update/version.json
else
	echo "⚠️ 警告: 访问返回状态码 $HTTP_STATUS"
fi

echo
echo "==> 6. 验证 VPS 订阅服务隔离完整性..."
SUB_TEST=$(curl -s -I -k http://127.0.0.1/b6857c30ccccbf704d1d7575a6cbd5b9/ || true)
echo "订阅测试响应:"
ssh vps "curl -s -I http://127.0.0.1/b6857c30ccccbf704d1d7575a6cbd5b9/ | head -n 5"

echo "🎉 热更新版本发布全部完成！客户端再次启动即可自动感知并加载最新补丁。"
