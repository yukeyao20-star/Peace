#!/usr/bin/env python
import os
import sys

from methods import print_error

# 产物名：会生成 libgame3d.<platform>.<target>.<arch>.<ext>
libname = "game3d"
projectdir = "project"

localEnv = Environment(tools=["default"], PLATFORM="")

# 允许用 custom.py 覆盖构建参数（例如 target / platform / arch / precision）
customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

# 复用 godot-cpp 的构建环境；api_version 决定绑定到哪个 Godot API（4.3 起向后兼容）
env = SConscript("godot-cpp/SConstruct", {"api_version": 4.3, "env": env, "customs": customs})

# godot-cpp 不为 linux 目标设置 SHLIBSUFFIX（在 macOS 宿主上会回落成 .dylib），按目标平台纠正
if env['platform'] == "linux":
    env["SHLIBSUFFIX"] = ".so"

# Windows（mingw）约定不带 lib 前缀，.gdextension 里期望的是 game3d.windows...dll
if env['platform'] == "windows":
    env["SHLIBPREFIX"] = ""

# 头文件按功能分子目录，全部加进搜索路径 → 源码里保持 #include "xxx.h" 裸文件名风格
env.Append(CPPPATH=["src/", "src/systems/", "src/world/", "src/actors/", "src/interact/"])
sources = Glob("src/*.cpp") + Glob("src/*/*.cpp")

# .dev 不影响兼容性；.universal 表示兼容所有架构 —— 都无需体现在文件名里
suffix = env['suffix'].replace(".dev", "").replace(".universal", "")

lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

library = env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=sources,
)

# 编译后自动复制到 Godot 工程里，编辑器即可直接加载
copy = env.Install("{}/bin/{}/".format(projectdir, env["platform"]), library)

default_args = [library, copy]
Default(*default_args)
