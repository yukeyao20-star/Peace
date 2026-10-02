# 发布流程（RELEASING）

「湖畔家园 Homestead」v0.1.0 发布指南。工程为纯 C++ GDExtension（Godot 4.7.2 标准版，非 .NET），导出配置见 `project/export_presets.cfg`（macOS / Windows Desktop / Linux 三个 preset）。

> **懒人路线**：下面所有步骤已经封成 `tools/package.sh`，一条命令跑完
> 编库 → 导出 → 压 zip → 组 deb → 校验：
>
> ```bash
> ./tools/package.sh --publish     # --publish 顺带把三个产物同步到 site/download/
> ```
>
> 常用变体：`--no-build`（库已是本轮）、`--only=linux`（只出一个平台）、
> `--no-deb`（不组 deb）、`JOBS=1`（串行编译，scons 卡死时的逃生口）。
> 想手工逐步做、或脚本哪一步报错要单独排查，看下面第二、三节。

## 一、发布前置条件

导出前必须满足以下条件，否则导出包无法运行或直接导出失败：

1. **安装 Godot 4.7.2 导出模板**
   - 编辑器路径：打开工程 → 编辑器（Editor）菜单 → 管理导出模板（Manage Export Templates…）→ 下载并安装（Download and Install）。
   - 模板安装后位于 `~/Library/Application Support/Godot/export_templates/4.7.2.stable/`（Windows/Linux 为对应用户配置目录）。模板版本必须与引擎版本一致（4.7.2.stable）。
   - 本仓库模板不进 git，每台发布机都要装一次（约 1 GB）。
2. **构建各平台 release 版 GDExtension 库**（关键）
   - `.gdextension` 文件会随工程自动打包，但其中登记的**对应平台 release 库文件必须真实存在于 `project/bin/` 下**，否则导出时报 `Failed to copy shared object` 直接失败（见第五节实测），或导出包运行时缺库。
   - 各平台 release 库路径（与 `project/bin/game3d.gdextension` 的 `[libraries]` 一致）：
     | 平台 | release 库 |
     |---|---|
     | macOS | `project/bin/macos/libgame3d.macos.template_release.dylib` |
     | Windows | `project/bin/windows/game3d.windows.template_release.x86_64.dll` |
     | Linux | `project/bin/linux/libgame3d.linux.template_release.x86_64.so` |
   - 本机构建命令（在仓库根目录）：`scons platform=macos target=template_release`（Windows/Linux 同理，Windows 也可用 MinGW 交叉编译）。不想本地全平台编译的，直接从 CI 下载 artifact（见第三节）。
3. **准备产物目录**：`dist/`（仓库根目录，已 gitignore 的话无需提交），导出路径 `../dist/...` 相对于 `project/` 目录。

## 二、三平台导出方法

导出路径均为 `project/export_presets.cfg` 中的相对路径 `../dist/<产物名>`。

### macOS（zip，universal，不签名）

- 命令行：
  ```bash
  /Applications/Godot.app/Contents/MacOS/Godot --headless \
    --path project --export-release "macOS" ../dist/game3d-macos.zip
  ```
  （zip 格式用 `--export-release` 即可；`--export-package` 用于只打包 .app 包体、跳过 pck 导出的场景，常规分发不需要。）
- 编辑器：项目 → 导出（Export…）→ 选「macOS」→ 导出项目（Export Project）。
- 产物：`dist/game3d-macos.zip`，内含 `湖畔家园 Homestead.app`。

### Windows Desktop（x86_64，不签名）

- 命令行：
  ```bash
  /Applications/Godot.app/Contents/MacOS/Godot --headless \
    --path project --export-release "Windows" ../dist/game3d-windows.exe
  ```
  **注意 preset 名是 `"Windows"`，不是 `"Windows Desktop"`** —— `"Windows Desktop"` 是
  `platform` 字段的值，不是 preset 名；用错会报 `Invalid export preset name`。
  preset 已开启 `binary_format/embed_pck`，导出为**单个 exe**，同时 Godot 会把
  `project/bin/windows/game3d.windows.template_release.x86_64.dll` 拷到 exe 同目录；
  发布时两个文件一起压成 `game3d-windows-x64.zip`。
- 编辑器：项目 → 导出 → 选「Windows」→ 导出项目。

### Linux（x86_64）

- 命令行：
  ```bash
  /Applications/Godot.app/Contents/MacOS/Godot --headless \
    --path project --export-release "Linux" ../dist/game3d-linux.x86_64
  ```
  同样是单可执行文件（pck 已嵌入），Godot 会把
  `project/bin/linux/libgame3d.linux.template_release.x86_64.so` 拷到同目录；
  发布时两个文件一起压成 `game3d-linux-x64.zip`，产物记得 `chmod +x`。
- 编辑器：项目 → 导出 → 选「Linux」→ 导出项目。

### Linux .deb（**Godot 不支持导出 deb，需手工组包**）

Godot 的 Linux preset 只能导出裸可执行文件 + `.so`，`.deb` 是用 `dpkg-deb` 手工组的。
前置：`brew install dpkg`。完整命令（在仓库根目录执行）：

```bash
PKG=dist/deb-root
rm -rf "$PKG"
mkdir -p "$PKG/opt/lakeside-cabin" "$PKG/usr/local/bin" \
         "$PKG/usr/share/applications" \
         "$PKG/usr/share/icons/hicolor/256x256/apps" "$PKG/DEBIAN"

# 1) 可执行文件 + GDExtension 库 —— 必须放同一目录，否则运行时找不到 .so
cp dist/game3d-linux.x86_64                                   "$PKG/opt/lakeside-cabin/"
cp project/bin/linux/libgame3d.linux.template_release.x86_64.so "$PKG/opt/lakeside-cabin/"
chmod 755 "$PKG/opt/lakeside-cabin/game3d.x86_64"

# 2) 命令行启动器
printf '#!/bin/sh\nexec /opt/lakeside-cabin/game3d.x86_64 "$@"\n' > "$PKG/usr/local/bin/lakeside-cabin"
chmod 755 "$PKG/usr/local/bin/lakeside-cabin"

# 3) 应用菜单项
cat > "$PKG/usr/share/applications/lakeside-cabin.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=湖畔家园 Homestead
Comment=3D lakeside homestead sandbox
Exec=/opt/lakeside-cabin/game3d.x86_64
Icon=lakeside-cabin
Terminal=false
Categories=Game;
EOF

# 4) 图标：icns → 256px png
sips -s format png -Z 256 docs/Homestead.icns \
  --out "$PKG/usr/share/icons/hicolor/256x256/apps/lakeside-cabin.png" >/dev/null

# 5) 控制信息
cat > "$PKG/DEBIAN/control" <<'EOF'
Package: lakeside-cabin
Version: 0.1.0-1
Section: games
Priority: optional
Architecture: amd64
Installed-Size: 90400
Maintainer: lei <lei@localhost>
Depends: libc6 (>= 2.31), libx11-6, libxcursor1, libxinerama1, libxrandr2, libxi6, libvulkan1, libasound2 | libasound2t64
Description: 湖畔家园 Homestead - 3D lakeside sandbox game
 A cozy 3D lakeside homestead world built with Godot 4.7.2 + custom
 C++ GDExtension: 448m procedural terrain, day/night cycle, weather,
 fishing, farm animals and a save system.
 .
 Launch from the app menu or run: lakeside-cabin
EOF

# 6) 打包
dpkg-deb --build --root-owner-group "$PKG" dist/game3d-linux-amd64.deb
```

校验：`dpkg-deb --info dist/game3d-linux-amd64.deb`、`dpkg-deb --contents …`。
安装：`sudo dpkg -i dist/game3d-linux-amd64.deb`（依赖缺失时 `sudo apt -f install`）。

### macOS 签名 / 公证注意事项

当前 preset 的 codesign、entitlements、notarization **全部关闭**（`codesign/codesign=0`、`notarization/notarization=0`），导出的是未签名 zip：

- 用户下载后首次打开会被 Gatekeeper 拦截，需右键 → 打开，或在「系统设置 → 隐私与安全性」中放行；或终端执行 `xattr -cr "湖畔家园 Homestead.app"` 后再打开。
- 个人开发者没有 Apple Developer 账号时，分发未签名 zip 是最简单的可行方式，可直接发布。
- 若有开发者账号，在 preset 中填 `codesign/identity`（Developer ID Application 证书）与 `notarization`（App Store Connect API 密钥）后重新导出，即可免受 Gatekeeper 限制。公证要求先开启 hardened runtime 签名。
- Windows 侧同理，`codesign/enable=false`，未签名 exe 会触发 SmartScreen 提示，个人分发可接受。

## 三、CI 现状说明

`.github/workflows/build.yml` 目前**只构建 GDExtension 库，不导出游戏**：

- 触发：push / pull_request / 手动（workflow_dispatch）。
- 矩阵三平台：linux（ubuntu-22.04）、macos（macos-latest）、windows（windows-latest）。
- 每个平台执行 `scons platform=<平台> target=template_release` 和 `target=template_debug` 两次构建。
- 上传 artifact，命名 `game3d-<平台>`（即 `game3d-linux` / `game3d-macos` / `game3d-windows`），内容为 `project/bin/<平台>/` 目录，含该平台的 template_release + template_debug 两份库（`.so` / `.dylib` / `.dll`）。

也就是说，**CI 已经能产出全部三个平台的 release 库**——发布前可从 Actions 页面下载对应 artifact，解包放进 `project/bin/` 各平台子目录，满足第二节的前置条件 2。

「完整游戏导出」可在此基础上扩展：给 workflow 增加一个 job，安装 Godot 4.7.2 + 导出模板（如用 `godotengine/godot` 官方镜像或下载模板 zip 解压到 `~/Library/Application Support/Godot/export_templates/`），再执行第二节的三条导出命令并上传 `dist/` 产物。本仓库暂未修改 workflow，此为后续可选步骤。

## 四、本机实测结果（2026-10-01，macOS / Godot 4.7.2.stable）

前置状态：
- 导出模板**已安装**：`~/Library/Application Support/Godot/export_templates/4.7.2.stable/`（含 macos.zip、windows/linux 全套模板）。
- 本机仅有 `project/bin/macos/libgame3d.macos.template_debug.dylib`，**三个平台的 release 库均缺失**。
- 导出命令统一为 `Godot --headless --path project --export-release "<preset>" <输出路径>`。

| preset | 结果 | 关键输出 |
|---|---|---|
| macOS | **成功**（修复两处后） | 首次失败①：`Cannot export for universal or arm64 if ETC2 ASTC texture format is disabled` → 将 preset 的 `texture_format/etc2_astc` 改为 `true`。再次失败②：`Target folder does not exist: "../dist"` → `mkdir -p dist` 后导出成功，产物 `dist/game3d-macos.zip`。 |
| Windows Desktop | **失败** | `ERROR: Failed to open '.../bin/./windows/game3d.windows.template_release.x86_64.dll'`，`GDExtension: Failed to copy shared object` → 缺 Windows release 库，导出中止（`dist/` 残留半成品 `game3d-windows.tmp`，不可用）。 |
| Linux | **失败** | `ERROR: Failed to open '.../bin/./linux/libgame3d.linux.template_release.x86_64.so'`，同上 → 缺 Linux release 库，导出中止（`dist/` 残留 `game3d-linux.x86_64` 但包内缺 `.so`，不可运行）。 |

macOS 后续验证：本地执行 `scons platform=macos target=template_release`（universal 双架构全量编译，本机耗时约 1 小时 45 分）生成 `libgame3d.macos.template_release.dylib`（1.1 MB）后重新导出，**成功且包完整**：`dist/game3d-macos.zip` 约 74 MB，解压后 `.app` 含正常 `Info.plist`（1.8 KB）、`PkgInfo`、`Resources/icon.icns`、`Resources/湖畔家园 Homestead.pck`（15 MB）和 `Frameworks/libgame3d.macos.template_release.dylib`——与首次导出（缺库时 Info.plist 为 0 字节、无 dylib）形成对照。

导出日志另见两条无害告警 `Cannot get class 'Spinner'` / `Cannot get class 'SaveSystem'`（GDScript 全局类缓存残留，未阻塞导出）。

结论：
- 导出链路本身可用（模板、preset 配置均正常），失败的根因是**缺平台 release 库**，与文档第一节的判断一致。
- 可复现的完整流程：① 装模板 → ② `scons platform=X target=template_release` 得到三平台 release 库（或从 CI artifact 下载）→ ③ `mkdir -p dist` → ④ 依次执行三平台导出命令 → ⑤ 压缩 Windows/Linux 产物为 zip 后分发。
