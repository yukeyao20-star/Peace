#!/usr/bin/env bash
# ============================================================================
#  Peace —— 三平台一键出包
#
#  用法：
#    ./tools/package.sh                 # 全流程：编库 → 导出 → 压包 → deb
#    ./tools/package.sh --publish       # 全流程 + 把三个产物同步到 site/download/
#    ./tools/package.sh --deploy        # 全流程 + 同步到 site/download/ + 直接部署到线上 VPS
#    ./tools/package.sh --no-build      # 跳过编译（库已经是本轮的）
#    ./tools/package.sh --only=windows  # 只出某一个平台
#    ./tools/package.sh --no-deb        # 不组 deb（只出裸二进制 zip）
#    ./tools/package.sh --help
#
#  环境变量：
#    JOBS=1        强制串行编译（默认用满 CPU 核数；万一 scons 卡死时试这个）
#    VERSION=0.1.0-1  deb 版本号
#
#  产物（dist/）：
#    peace-macos.zip           macOS .app（universal，未签名）
#    peace-windows-x64.zip     exe + dll
#    peace-linux-x64.zip       裸二进制 + so
#    peace-linux-amd64.deb     deb 安装包
# ============================================================================
set -euo pipefail

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------
GODOT="${GODOT:-/Applications/Godot.app/Contents/MacOS/Godot}"
PKG_NAME="peace"                 # deb 包名（改这里要同时改下面几处路径）
PKG_DISPLAY="Peace"
PKG_VERSION="${VERSION:-0.1.0-1}"
PKG_INSTALL_DIR="/opt/${PKG_NAME}"        # 游戏本体安装位置
ICON_SRC="docs/Homestead.icns"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

DIST="$REPO/dist"
EXE_WIN="peace-windows.exe"
DLL_WIN="game3d.windows.template_release.x86_64.dll"
BIN_LIN="peace-linux.x86_64"
SO_LIN="libgame3d.linux.template_release.x86_64.so"

# ---------------------------------------------------------------------------
# 参数
# ---------------------------------------------------------------------------
DO_BUILD=1; DO_EXPORT=1; DO_DEB=1; DO_PUBLISH=0; DO_DEPLOY=0
PLATFORMS="macos windows linux"

for arg in "$@"; do
	case "$arg" in
		--no-build)  DO_BUILD=0 ;;
		--no-export) DO_EXPORT=0 ;;
		--no-deb)    DO_DEB=0 ;;
		--publish)   DO_PUBLISH=1 ;;
		--deploy)    DO_DEPLOY=1; DO_PUBLISH=1 ;;
		--only=*)    PLATFORMS="${arg#--only=}"; PLATFORMS="${PLATFORMS//,/ }" ;;
		-h|--help)   sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) echo "未知参数：${arg}（--help 看用法）" >&2; exit 2 ;;
	esac
done

want() { [[ " $PLATFORMS " == *" $1 "* ]]; }

# ---------------------------------------------------------------------------
# 工具
# ---------------------------------------------------------------------------
START_TS=$(date +%s)
C_BLUE=$'\033[1;34m'; C_GRN=$'\033[1;32m'; C_RED=$'\033[1;31m'; C_YEL=$'\033[1;33m'; C_OFF=$'\033[0m'

step() { printf '\n%s==> %s%s\n' "$C_BLUE" "$1" "$C_OFF"; }
ok()   { printf '%s  ✓ %s%s\n' "$C_GRN" "$1" "$C_OFF"; }
warn() { printf '%s  ! %s%s\n' "$C_YEL" "$1" "$C_OFF"; }
die()  { printf '\n%s  ✗ %s%s\n' "$C_RED" "$1" "$C_OFF" >&2; exit 1; }

# 只有失败时才把日志吐出来 —— Godot 导出接管道会卡住，所以一律落文件
run_logged() {
	local log="$1"; shift
	if ! "$@" >>"$log" 2>&1; then
		printf '\n%s---- %s 末尾 40 行 ----%s\n' "$C_RED" "$log" "$C_OFF" >&2
		tail -40 "$log" >&2
		die "命令失败：$*"
	fi
}

# ---------------------------------------------------------------------------
# 0. 前置检查
# ---------------------------------------------------------------------------
step "0/5  前置检查"

[[ -f SConstruct ]] || die "请在仓库根目录运行（当前 ${REPO}）"
[[ -d godot-cpp && -n "$(ls -A godot-cpp 2>/dev/null)" ]] \
	|| die "godot-cpp 子模块为空，先跑：git submodule update --init --recursive"
[[ -x "$GODOT" ]] || die "找不到 Godot：${GODOT}（用 GODOT=/path/to/Godot 覆盖）"
ok "Godot：$("$GODOT" --version)"

ENGINE_VER=$("$GODOT" --version | sed 's/^\([0-9.]*\.stable\).*/\1/')
TPL_DIR="$HOME/Library/Application Support/Godot/export_templates/${ENGINE_VER}"
[[ -d "$TPL_DIR" ]] || die "导出模板缺失：$TPL_DIR
    打开 Godot → 编辑器 → 管理导出模板 → 下载并安装"
ok "导出模板：${ENGINE_VER}"

if [[ $DO_BUILD -eq 1 ]] && ( want macos || want windows || want linux ); then
	if pgrep -fl scons >/dev/null 2>&1; then
		echo "已有 scons 在跑（同一时间只能一个）：" >&2
		pgrep -fl scons >&2
		exit 1
	fi
fi
if want windows; then
	command -v x86_64-w64-mingw32-g++ >/dev/null \
		|| die "缺 mingw：brew install mingw-w64"
fi
if want linux; then
	[[ -d "$HOME/opt/cross-shim" ]] \
		|| die "缺 Linux 交叉工具链垫片：~/opt/cross-shim/（软链到 ~/opt/x86_64-unknown-linux-gnu/bin/）"
	if [[ $DO_DEB -eq 1 ]]; then
		command -v dpkg-deb >/dev/null || die "缺 dpkg-deb：brew install dpkg"
	fi
fi
ok "工具链齐备（JOBS=${JOBS}）"

# ---------------------------------------------------------------------------
# 1. 编 release 库
# ---------------------------------------------------------------------------
if [[ $DO_BUILD -eq 1 ]]; then
	step "1/5  编译 release 库"

	if want macos; then
		echo "--- macOS（universal 双架构，最慢，可能 1~2 小时）---"
		scons -j"$JOBS" platform=macos target=template_release
		ok "macOS release 库"
	fi
	if want windows; then
		echo "--- Windows（mingw 交叉）---"
		scons -j"$JOBS" platform=windows arch=x86_64 target=template_release use_mingw=yes
		ok "Windows release 库"
	fi
	if want linux; then
		echo "--- Linux（GCC 交叉，走 PATH 垫片）---"
		PATH="$HOME/opt/cross-shim:$PATH" \
			scons -j"$JOBS" platform=linux arch=x86_64 target=template_release
		ok "Linux release 库"
	fi
else
	step "1/5  编译 —— 已跳过（--no-build）"
fi

# 库文件到位性检查：缺了导出必然失败，早点拦
step "1.5  核对 release 库"
[[ -f project/bin/macos/libgame3d.macos.template_release.dylib ]] \
	|| warn "缺 macOS release 库（导出 macOS 会失败）"
[[ -f "project/bin/windows/$DLL_WIN" ]] || warn "缺 Windows release 库"
[[ -f "project/bin/linux/$SO_LIN"   ]] || warn "缺 Linux release 库"
ls -la project/bin/*/*template_release* 2>/dev/null | awk '{print "   ", $6, $7, $8, $9}'

# ---------------------------------------------------------------------------
# 2. 导出
# ---------------------------------------------------------------------------
mkdir -p "$DIST"
if [[ $DO_EXPORT -eq 1 ]]; then
	step "2/5  导出"

	# preset 名取自 export_presets.cfg 的 name 字段（不是 platform 字段！）
	if want macos; then
		run_logged "$DIST/.export-macos.log" \
			"$GODOT" --headless --path project --export-release "macOS" "../dist/peace-macos.zip"
		ok "macOS → dist/peace-macos.zip"
	fi
	if want windows; then
		run_logged "$DIST/.export-windows.log" \
			"$GODOT" --headless --path project --export-release "Windows" "../dist/${EXE_WIN}"
		ok "Windows → dist/${EXE_WIN}"
	fi
	if want linux; then
		run_logged "$DIST/.export-linux.log" \
			"$GODOT" --headless --path project --export-release "Linux" "../dist/${BIN_LIN}"
		ok "Linux → dist/${BIN_LIN}"
	fi
else
	step "2/5  导出 —— 已跳过（--no-export）"
fi

# ---------------------------------------------------------------------------
# 3. 压包
# ---------------------------------------------------------------------------
step "3/5  压包"

if want windows; then
	[[ -f "$DIST/$EXE_WIN" ]] || die "缺 ${EXE_WIN}，无法压 Windows 包"
	[[ -f "$DIST/$DLL_WIN" ]] || die "缺 ${DLL_WIN} —— 导出没带上 GDExtension 库，检查 project/bin/windows/"
	rm -f "$DIST/peace-windows-x64.zip"
	( cd "$DIST" && zip -q -j peace-windows-x64.zip "$EXE_WIN" "$DLL_WIN" )
	ok "dist/peace-windows-x64.zip  $(du -h "$DIST/peace-windows-x64.zip" | cut -f1)"
fi

if want linux; then
	[[ -f "$DIST/$BIN_LIN" ]] || die "缺 ${BIN_LIN}，无法压 Linux 包"
	[[ -f "$DIST/$SO_LIN"  ]] || die "缺 ${SO_LIN} —— 导出没带上 GDExtension 库，检查 project/bin/linux/"
	chmod 755 "$DIST/$BIN_LIN"   # zip 会保留执行位，先确保它是可执行的
	rm -f "$DIST/peace-linux-x64.zip"
	( cd "$DIST" && zip -q -j peace-linux-x64.zip "$BIN_LIN" "$SO_LIN" )
	ok "dist/peace-linux-x64.zip  $(du -h "$DIST/peace-linux-x64.zip" | cut -f1)"
fi

# ---------------------------------------------------------------------------
# 4. Linux .deb（Godot 不支持导出 deb，手工组包）
# ---------------------------------------------------------------------------
if want linux && [[ $DO_DEB -eq 1 ]]; then
	step "4/5  组 .deb"
	[[ -f "$DIST/$BIN_LIN" ]] || die "缺 ${BIN_LIN}，无法组 deb"

	PKG="$DIST/deb-root"
	rm -rf "$PKG"
	mkdir -p "$PKG${PKG_INSTALL_DIR}" \
	         "$PKG/usr/local/bin" \
	         "$PKG/usr/share/applications" \
	         "$PKG/usr/share/icons/hicolor/256x256/apps" \
	         "$PKG/DEBIAN"

	# 可执行文件与 .so 必须同目录，否则运行时找不到库（且只在启动时才报错）
	cp "$DIST/$BIN_LIN" "$PKG${PKG_INSTALL_DIR}/peace.x86_64"
	cp "project/bin/linux/$SO_LIN" "$PKG${PKG_INSTALL_DIR}/"
	chmod 755 "$PKG${PKG_INSTALL_DIR}/peace.x86_64"

	printf '#!/bin/sh\nexec %s/peace.x86_64 "$@"\n' "$PKG_INSTALL_DIR" \
		> "$PKG/usr/local/bin/${PKG_NAME}"
	chmod 755 "$PKG/usr/local/bin/${PKG_NAME}"

	cat > "$PKG/usr/share/applications/${PKG_NAME}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${PKG_DISPLAY}
Comment=3D lakeside homestead sandbox
Exec=${PKG_INSTALL_DIR}/peace.x86_64
Icon=${PKG_NAME}
Terminal=false
Categories=Game;
EOF

	if [[ -f "$ICON_SRC" ]]; then
		sips -s format png -Z 256 "$ICON_SRC" \
			--out "$PKG/usr/share/icons/hicolor/256x256/apps/${PKG_NAME}.png" >/dev/null
	else
		warn "找不到图标 ${ICON_SRC}，deb 里将没有图标"
	fi

	# Installed-Size 按实际体积算（单位 KiB）
	INSTALLED_KB=$(du -sk "$PKG" | cut -f1)
	cat > "$PKG/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${PKG_VERSION}
Section: games
Priority: optional
Architecture: amd64
Installed-Size: ${INSTALLED_KB}
Maintainer: lei <lei@localhost>
Depends: libc6 (>= 2.31), libx11-6, libxcursor1, libxinerama1, libxrandr2, libxi6, libvulkan1, libasound2 | libasound2t64
Description: ${PKG_DISPLAY} - 3D lakeside sandbox game
 A cozy 3D lakeside homestead world built with Godot 4.7.2 + custom
 C++ GDExtension: 448m procedural terrain, day/night cycle, weather,
 fishing, farm animals and a save system.
 .
 Launch from the app menu or run: ${PKG_NAME}
EOF

	rm -f "$DIST/peace-linux-amd64.deb"
	dpkg-deb --build --root-owner-group "$PKG" "$DIST/peace-linux-amd64.deb" >/dev/null
	rm -rf "$PKG"
	ok "dist/peace-linux-amd64.deb  $(du -h "$DIST/peace-linux-amd64.deb" | cut -f1)"
else
	step "4/5  组 .deb —— 已跳过"
fi

# ---------------------------------------------------------------------------
# 5. 校验 + 可选发布
# ---------------------------------------------------------------------------
step "5/5  校验"

if [[ -f "$DIST/peace-windows-x64.zip" ]]; then
	unzip -tq "$DIST/peace-windows-x64.zip" >/dev/null && ok "windows zip 完整性 OK"
fi
if [[ -f "$DIST/peace-linux-x64.zip" ]]; then
	unzip -tq "$DIST/peace-linux-x64.zip" >/dev/null && ok "linux zip 完整性 OK"
fi
if [[ -f "$DIST/peace-macos.zip" ]]; then
	unzip -tq "$DIST/peace-macos.zip" >/dev/null && ok "macos zip 完整性 OK"
fi
if [[ -f "$DIST/peace-linux-amd64.deb" ]]; then
	dpkg-deb --info "$DIST/peace-linux-amd64.deb" >/dev/null && ok "deb 结构 OK"
	dpkg-deb --contents "$DIST/peace-linux-amd64.deb" | awk '{print "      ", $1, $6}'
fi

# 时间戳核对：产物必须是这一轮生成的，混进上一轮的旧包是最容易犯的错
echo
echo "  产物时间戳（本次运行开始于 $(date -r "$START_TS" '+%H:%M:%S')）："
for f in peace-macos.zip peace-windows-x64.zip peace-linux-x64.zip peace-linux-amd64.deb; do
	[[ -f "$DIST/$f" ]] || continue
	mtime=$(stat -f %m "$DIST/$f")
	if (( mtime >= START_TS )); then
		printf '    %s✓%s %-28s %s\n' "$C_GRN" "$C_OFF" "$f" "$(date -r "$mtime" '+%m-%d %H:%M')"
	else
		printf '    %s!%s %-28s %s  %s← 旧包，本轮没重新生成%s\n' \
			"$C_YEL" "$C_OFF" "$f" "$(date -r "$mtime" '+%m-%d %H:%M')" "$C_YEL" "$C_OFF"
	fi
done

if [[ $DO_PUBLISH -eq 1 ]]; then
	step "同步到官网 site/download/"
	mkdir -p site/download
	for f in peace-macos.zip peace-windows-x64.zip peace-linux-amd64.deb; do
		if [[ -f "$DIST/$f" ]]; then
			cp "$DIST/$f" site/download/
			ok "site/download/${f}"
		else
			warn "dist/${f} 不存在，跳过"
		fi
	done
	if [[ $DO_DEPLOY -eq 1 ]]; then
		echo "    （稍后自动部署到线上）"
	else
		echo "    （上线：cd site && ./deploy.sh，或加 --deploy 一步到位）"
	fi

	# 页面上的体积数字跟着实际包大小走，免得发版后对不上
	if command -v python3 >/dev/null 2>&1; then
		step "核对网页上的体积数字"
		python3 tools/sync_site_sizes.py
	else
		warn "本机没有 python3，跳过体积数字同步（记得手工核对 site/index.html）"
	fi
fi

if [[ $DO_DEPLOY -eq 1 ]]; then
	step "部署到线上 VPS"
	( cd site && ./deploy.sh )
fi

printf '\n%s全部完成，耗时 %d 分 %d 秒%s\n' \
	"$C_GRN" $(( ($(date +%s) - START_TS) / 60 )) $(( ($(date +%s) - START_TS) % 60 )) "$C_OFF"
