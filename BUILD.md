# 从零构建 PvZ Toolkit Dx

> 本文写的是**在一台干净的 Windows 上，只用本源码包的源码把两个目标构建出来**：
>
> | 目标 | 产物 | 说明 |
> |---|---|---|
> | 主程序（Dear ImGui 前端） | `build\pztk_imgui.exe` | 当前主版本 |
> | 注入组件 | `build\pztk_hook.dll` | **必须与 exe 同目录**，且必须是同一次构建的一对 |
>
> 上游的 FLTK 界面版 `pvztoolkit.exe` **不在本包范围内**：本包不含 FLTK 前端源码，
> 也没有它的构建脚本（`build_toolkit.cmd` / `build_fltk.cmd` / `tool-src\tools\*.makefile` 都没有）。
>
> ⚠️ **但"不用 FLTK"不等于"不需要 zlib"**：两个目标都要 zlib（`.zbl` 阵型与 PAK 打包的压缩，
> 核心层还 `#include <FL/images/zlib.h>`），而这份 zlib 目前取自 **FLTK 的源码树**
> （`env\src\fltk-1.4.5\zlib`，最终链接的是 `env\fltk32\lib\libfltk_z.a`，84 KB）。
> FLTK 在这里**仅作 zlib 来源**，其源码由依赖脚本获取、**不随本包分发**，FLTK 本体也不参与链接。
>
> 所有实际命令都在 `env\scripts\` 里；本文只解释**它们需要什么前置条件**、
> **下载/解压哪些东西**，以及**踩过的坑**。脚本里能读到的东西这里不重复抄。

> ⚠️ **本包是"可上传的精简源码包"**：它**故意不含**上游 FLTK 界面版（`pvztoolkit.exe`）的前端源码
> 与其构建链，也**不含开发内部材料**（完整交接文档、完整改动日志、维护规程、发布者清单、
> 离线自测 `recttest` 与 QA 脚本 —— 那些只在开发仓库里）。
> **不要**用整目录 / 整树同步（`robocopy`、`Copy-Item -Recurse`、`git checkout` 之类）把开发仓库
> 覆盖过来，否则被移除的 FLTK 前端 / 依赖目录会被带回来。

---

## 0. 前置要求

| 项 | 要求 |
|---|---|
| 操作系统 | Windows（32 位程序，32/64 位 Windows 都能跑；构建也只需要 Windows） |
| Python | 3.x（只用来跑 `env\scripts\download_deps.py`） |
| 磁盘 | 约 2 GB（工具链解压后约 900 MB + 依赖压缩包约 280 MB + 构建树） |
| 路径 | **源码路径可以是任意路径**（含中文也行），因为所有构建都走 ASCII 别名，见第 2 节 |
| 编译器 | **不需要**预先安装，但**工具链必须先自己放好**：`download_deps.py` 只把 WinLibs MinGW-w64 **i686** 的压缩包下到 `env\downloads\`，**解压到 `env\mingw32\` 要自己做** —— 见 **0.1（第一步 / Step 0）** 与 1.2 |

> 上游原版用 Visual C++ + CMake 构建；**本修改版的构建链是 MinGW-w64 i686 + 脚本**，
> 产物是 32 位静态链接的主程序 exe + 注入 DLL。上游那份 CMake 工程**不随本包提供**
> （`tool-src\CMakeLists.txt` 现在是说明性标记文件，见它自己的文件头注释）——
> 本包的两个目标全部由 `env\scripts\` 里的 `g++` 命令行直接构建。

### 0.1 ★ 第一步（Step 0）：先把工具链放好 —— 它**不随本包分发**

**做任何别的事之前先做这一步**：`env\scripts\provision.cmd` 要把工具链复制到 ASCII 路径，
而**源目录 `env\mingw32` 不存在时它会直接报错退出**，报的是

```
ERROR: MinGW-w64 i686 toolchain not found - cannot provision
```

* **为什么包里没有**：工具链解压后约 1 GB，被 `.gitignore` 忽略，**不随本包分发**；
  本包只带 `env\scripts\` 里的构建脚本。
* **怎样获得**：
  1. 跑 `python env\scripts\download_deps.py`：它把
     `winlibs-i686-posix-dwarf-gcc-16.2.0-mingw-w64msvcrt-14.0.0-r2.zip` 下到 `env\downloads\`
     （带 sha256 校验）—— **它只下载，不解压**。
  2. 把这个 zip **整个解压到 `env\mingw32\`**（目录名要求与解压后的样子见 1.2）。
     也可以自己去 winlibs 下载同款工具链，但必须是 **i686（32 位）** 的。
* **怎样自检**（两条都过才算放好）：

  ```bat
  dir env\mingw32\bin\gcc.exe
  env\mingw32\bin\gcc.exe --version
  ```

  第一条应列出 `env\mingw32\bin\gcc.exe`，第二条应打印 `gcc ... 16.2.0` 之类的版本行；
  另外 `env\mingw32\bin\g++.exe` 与 `env\mingw32\bin\cmake.exe` 也必须在
  （`provision.cmd` 查的就是这三处，见它自己的 `:pztk_check_toolchain`）。
* 放好之后才能跑 `env\scripts\provision.cmd`（见第 2 节）。残缺解压（例如缺
  `i686-w64-mingw32\include\winresrc.h`）会被它单独报出来，见 4.3。

---

## 1. 取得工具链与依赖

### 1.1 一键下载

```bat
python env\scripts\download_deps.py
```

脚本把 4 个包下到 `env\downloads\`，每个都带 **sha256 校验**（已存在且校验通过会跳过）：

| 下载到的文件 | 用途 |
|---|---|
| `winlibs-i686-posix-dwarf-gcc-16.2.0-mingw-w64msvcrt-14.0.0-r2.zip` | **MinGW-w64 i686 (msvcrt) 工具链**，含 `g++` / `cmake` / `mingw32-make` / `windres` / `objdump` 等 |
| `fltk-1.4.5-source.tar.gz` | FLTK 1.4.5 源码 —— **仅作 zlib 来源**（本项目只静态链接其中的 zlib：`libfltk_z.a`；FLTK 本体不参与构建） |
| `imgui-1.91.8.tar.gz` | Dear ImGui 1.91.8 **正式标签** —— ⚠️ 主程序要的是 **docking 分支**，见 1.4 |
| `glfw-3.4.tar.gz` | GLFW 3.4 源码 |

> `download_deps.py` **只负责下载**，不负责解压。解压见下面几小节。
> （`build_glfw.cmd` 是唯一的例外：它自己会从 `env\downloads\glfw-3.4.tar.gz` 解压。）
>
> ⚠️ **其中工具链这一项就是第一步（0.1）**：`winlibs-...zip` 下完**必须自己解压到 `env\mingw32\`**
> （见 **0.1** 与 1.2），否则 `env\scripts\provision.cmd` 会直接报
> `ERROR: MinGW-w64 i686 toolchain not found - cannot provision` 并退出。

### 1.2 解压工具链 → `env\mingw32\`

把 `winlibs-...-r2.zip` 解压到 **`env\mingw32\`**，解压后应能看到：

```
env\mingw32\bin\g++.exe
env\mingw32\bin\cmake.exe
env\mingw32\i686-w64-mingw32\bin\windres.exe
env\mingw32\i686-w64-mingw32\include\winresrc.h   :: windres 编资源要用（见 4.3）
```

**目录名必须是 `env\mingw32`**：`env\scripts\provision.cmd` 是从这里把工具链复制到 ASCII 路径去的。
漏做本节（也就是 0.1 说的第一步）时，它会在复制之前就报
`ERROR: MinGW-w64 i686 toolchain not found - cannot provision` 并退出。

### 1.3 解压 FLTK → `env\src\fltk-1.4.5\`（**只为了里面的 zlib**）

1. 把 `fltk-1.4.5-source.tar.gz` 解压，得到 `fltk-1.4.5\` 目录，放到 **`env\src\fltk-1.4.5\`**。
   包内的 `env\scripts\build_zlib.cmd` 从这里**只编 zlib**，把静态库装到 `env\fltk32\lib\libfltk_z.a`。
2. **不需要**并入上游那份 FLTK 补丁（`Fl_Choice` / `Fl_Tabs` / `Fl_Screen_Driver` / `Fl_win32` 四改）：
   那四处都是给 **FLTK 界面**用的，本项目只用它的 `zlib/` 子目录，改不改与产物无关。
   （本包也不再带那份补丁文件。）
3. 这一步的产物只有 **zlib**：`env\fltk32\lib\libfltk_z.a`（约 84 KB）。
   主程序只链接这一个 `.a`，**不链接 `libfltk.a` / `libfltk_images.a` / `libfltk_png.a` / `libfltk_jpeg.a`**
   —— 那四个界面库**根本不会被构建出来**。

> **谁来做这一步**：包内的 **`env\scripts\build_zlib.cmd`**（见第 3 节）。它只配置并构建 FLTK
> CMake 工程里的**单个目标 `fltk_z`**（`env\src\fltk-1.4.5\zlib\CMakeLists.txt`），
> **不会**编译 FLTK 的任何界面代码；配置里带 `-D FLTK_USE_SYSTEM_ZLIB=OFF`，
> 也就是"用 FLTK 自带的那份 zlib，而不是系统的"。
>
> * 手动跑：`env\scripts\build_zlib.cmd`
> * 不用手动跑：`build_all.cmd` 在 `libfltk_z.a` 缺失时会**自动调它**（见第 3 节）
> * 幂等：`env\fltk32\lib\libfltk_z.a` 已存在就直接跳过；想重建就删掉它
> * 已经拿到 `libfltk_z.a` 的话，本节可以整节跳过

### 1.4 准备 ImGui 的 **docking 分支** → `env\src\imgui-1.91.8-docking\`

`env\scripts\build_imgui_demo.cmd` 里的路径是写死的：

```bat
set "IMGUI=%WS%\env\src\imgui-1.91.8-docking"
```

也就是它要的是 **ImGui 的 docking 分支（1.91.8）** —— 判断方法：那个版本里 `imgui.h` 有
`#define IMGUI_HAS_DOCK`，并且提供 `ImGui::DockSpace` 等接口。

而 `download_deps.py` 下的是 **正式标签** `imgui-1.91.8.tar.gz`（**没有** docking 支持）。
所以这条依赖需要**自己另外取**：

* 分支页：<https://github.com/ocornut/imgui/tree/docking>
* `git clone --branch docking --depth 1 https://github.com/ocornut/imgui.git`
* 或下载 tag `v1.91.8-docking` 的归档（本地开发环境里对应的压缩包名就是
  `imgui-1.91.8-docking.tar.gz`）

解压/克隆后，**目录名必须是 `env\src\imgui-1.91.8-docking`**；
如果你不想改名，就把 `build_imgui_demo.cmd` 里 `IMGUI=` 那一行改成你的目录名。

> `env\src\glfw-3.4\` **不用手工准备**：`env\scripts\build_glfw.cmd` 会自动用系统 `tar.exe`
> 从 `env\downloads\glfw-3.4.tar.gz` 解压到那里。

---

## 2. 准备 ASCII 构建环境（**必须做，否则编译一定失败**）

```bat
env\scripts\provision.cmd
```

它做两件事，并把 `ROOT=` / `TC=` / `WS=` 三行打印出来（其它脚本就是靠解析这三行拿到路径的）：

1. 建立 junction **`%TEMP%\pztk\ws` → 仓库根**。
   源码与构建产物的路径都通过这个别名表达（**文件仍然落在仓库里**，不是复制一份）。
2. 把 `env\mingw32` **实体复制**到 **`%TEMP%\pztk\mingw32`**。

**为什么必须这样**：

* 本项目的开发路径含中文（`D:\植物大战僵尸相关\...`）。MinGW-w64 的文件 API 走当前 ANSI 代码页，
  **任何含中文的输入/输出路径都会失败**（`Fatal error: can't create ...: No such file or directory`）。
* 只是"建个别名"不够：Windows 会把 junction 解析回真实路径，所以**工具链本身必须物理位于 ASCII 路径**，
  这才是要实体复制的原因。
* 别名路径可以用 `provision.cmd <别的根目录>` 换；默认根是 `%TEMP%\pztk`（会话结束可能被清理，
  重跑一次 `provision.cmd` 即可重建）。
* 也可以用环境变量换根：`set PZTK_BUILD_ROOT=D:\pztk_ws`（命令行参数优先于它）。
  想**从另一份源码副本**构建而不动默认根，就先把根指过去，那份副本自己的脚本会通过
  `<根>\ws` junction 看到它自己 —— 这正是"干净副本验收"的做法。

> 开发期的另一条硬性约束：**不要用 Ninja**。Ninja 通过管道截获每个子进程的输出，在受限环境里会永久卡住；
> 本仓库统一用 `MinGW Makefiles` 生成器 + `mingw32-make`（脚本里已固定）。

---

## 3. 构建依赖库

```bat
:: 生成 env\fltk32\lib\libfltk_z.a —— 只编 FLTK 源码树里的 zlib，不编 FLTK 界面
env\scripts\build_zlib.cmd
env\scripts\build_glfw.cmd      ::  -> env\glfw32\   (libglfw3.a)
```

`build_zlib.cmd` 与 `build_glfw.cmd` 都是**幂等**的：目标产物还在就直接跳过（zlib 看
`env\fltk32\lib\libfltk_z.a` 是否存在，GLFW 看安装前缀里的 `.pztk-alias` 戳 + `libglfw3.a`）。

**`build_zlib.cmd` 做了什么（完整要点）**：

| 项 | 值 |
|---|---|
| 源码 | `env\src\fltk-1.4.5\zlib`（FLTK 自带的那份 zlib，**只作 zlib 来源**） |
| 生成器 | CMake 的 **`MinGW Makefiles`**（**不要 Ninja**，原因见第 2 节末） |
| CMake 源目录 | `env\src\fltk-1.4.5`（FLTK 根，这样才有 `fltk_z` 这个目标；`zlib/CMakeLists.txt` 单独配置会缺 `fl_add_library` 等定义） |
| 构建目录 | `env\fltk-build`（与 GLFW 的 `env\glfw-build` 同一套路） |
| 构建目标 | **只有 `fltk_z`**（`cmake --build <bld> --target fltk_z`）—— **不编译 FLTK 界面** |
| 构建类型 / 编译参数 | `-D CMAKE_BUILD_TYPE=MinSizeRel` + `-D CMAKE_C_FLAGS="-O2"` ⇒ 实际命令行是 `-O2 -Os -DNDEBUG -DFL_LIBRARY`（与旧的全量 FLTK 构建**逐项相同**，可自行核对 `env\fltk-build\zlib\CMakeFiles\fltk_z.dir\flags.make`） |
| 关键配置项 | `-D FLTK_USE_SYSTEM_ZLIB=OFF`（用 FLTK 自带 zlib，**不要**系统 zlib）、`-D FLTK_BUILD_SHARED_LIBS=OFF`（静态库）；其余 `FLTK_BUILD_*` 一律 `OFF`（界面、示例、测试、文档都不生成） |
| 工具链 | 全部取自 provision 解析出的 `%TC%`（`%TC%\bin\cmake.exe` / `mingw32-make.exe` / `gcc.exe` / `ar.exe` / `ranlib.exe`），并把 `PATH` 收窄成 `%TC%\bin;%SystemRoot%\system32` |
| 安装 | 只把 `env\fltk-build\lib\libfltk_z.a` 拷成 **`env\fltk32\lib\libfltk_z.a`**；`env\fltk32\include\FL\images\` 缺 `zlib.h`/`zconf.h`/`zutil.h` 时才补拷（已存在**不覆盖**） |
| 报错 | 缺源码树 / 缺 15 个 `.c` / 缺头文件 / 工具链文件缺失，都**明确报错并指向 `download_deps.py` 与本节**，`exit /b 1` |
| 兜底 | CMake 配置或构建失败、或构建树状态异常时，脚本改用**同一个 `gcc` 命令行直接编这 15 个文件**（`-O2 -Os -DNDEBUG -DFL_LIBRARY`）+ `ar qc` + `ranlib`，产物同名同格式 |
| 环境变量 | 支持 `PZTK_BUILD_ROOT`（与 `provision.cmd` 一致的取根方式）；也可以直接给它 `build_zlib.cmd <TC> <WS>` |

> **它编出来的东西与"全量编 FLTK 时顺手编出来的 zlib"兼容**：同一份源码、同一个编译器、
> 同一组编译参数、同一个 `ar`/`ranlib`，产物同名同符号（`-DZ_PREFIX` 让所有符号带 `fltk_z_` 前缀，
> 链接行里的 `-lfltk_z` 找的就是它）。验收方式：**删掉 `env\fltk32\lib\libfltk_z.a` 后跑一次
> `build_all.cmd`**，它能自动编出来、并让两个目标都构建通过，就说明它可用。

> **只用到 `libfltk_z.a`（约 84 KB）**：核心层 `lineup.h` 用 zlib 压缩阵型，
> `.zbl` 出怪列表的 `crc32` 也来自这份 zlib（`#include <FL/images/zlib.h>` 会被
> `zconf.h` 宏映射成 `fltk_z_crc32`）。**不要**改成链接系统的 zlib —— 校验结果必须与上游一致。

---

## 4. 构建两个目标

| 目标 | 命令 | 产物 |
|---|---|---|
| **主程序（ImGui）** | `env\scripts\build_imgui_demo.cmd [输出名]` | `build\pztk_imgui.exe` |
| **注入 DLL** | `env\scripts\build_hook_dll.cmd [输出名]` | `build\pztk_hook.dll` |

一次跑完两个目标用 `env\scripts\build_all.cmd`。**包内这本脚本就是按下面这几步写的**：

| 步 | 脚本 | 什么时候真的跑 |
|---|---|---|
| 0 | `env\scripts\provision.cmd` | 每次都跑（建 ASCII 别名 + 复制/检查工具链副本） |
| 1 | `env\scripts\build_zlib.cmd` | **只在 `env\fltk32\lib\libfltk_z.a` 不存在时**；已在就打印一行跳过 |
| 2 | `env\scripts\build_glfw.cmd` | 每次都跑（幂等：装好了就跳） |
| 3 | `env\scripts\build_imgui_demo.cmd` | 每次都跑（增量） |
| 4 | `env\scripts\build_hook_dll.cmd` | 每次都跑 |

* **每一步都检查 `errorlevel`**：第一个失败的步骤会打 `ERROR: ...`（英文）并**以退出码 1 结束整个脚本**，
  不会带着上一步的坏产物继续往下跑。步骤 1/2/3/4 跑完还会各自复核一次产物文件是否存在。
* 全部成功时末尾打印 **`BUILD ALL OK`**，紧跟两个产物的完整路径
  （`<根>\ws\build\pztk_imgui.exe`、`<根>\ws\build\pztk_hook.dll`）。**看到这两行才算构建成功。**
* `TC` / `WS` 统一由 `provision.cmd` 解析出来（`build_all.cmd` 只解析它的 `ROOT=` / `TC=` / `WS=` 三行，
  与 `build_glfw.cmd` / `build_zlib.cmd` / `build_imgui_demo.cmd` / `build_hook_dll.cmd` 同一套做法），
  所以第 2 节末尾说的 **`PZTK_BUILD_ROOT` 换根对整条链都有效**：从步骤 3、4 到产物都落在那份副本里，
  不会跑回默认根去。
* 不传输出名就用默认名；**目标文件正被占用时**（旧 exe 还在运行）传一个中间名，例如
  `env\scripts\build_imgui_demo.cmd pztk_imgui_next.exe`，再把中间产物改名成正式名。
  Windows 会锁住正在运行的 exe，直接覆盖会**静默失败**。
* `build_all.cmd` **不含任何 FLTK 界面步骤** —— 上游那条 `build_fltk.cmd` → `build_toolkit.cmd`
  （= 编 `pvztoolkit.exe` 的全量链）**连同前端源码已于 2026-10-13 从开发仓库整体删除**
  （开发仓库与本包两处都没有它了），所以**任何脚本或报错都不会去调它**。
  本包的 FLTK 源码树**只用来编 zlib**（第 3 节的 `build_zlib.cmd`）。
  包内也不存在 `build_fltk.cmd` / `build_toolkit.cmd` 这两个文件，所以**任何脚本或报错都不会去调它们**。
* ImGui 目标在链接前还会先跑一步 **`windres`** 编资源（**图标 + VERSIONINFO**），见 **4.3**；
  这一步是**尽力而为**的：失败**只会少图标/版本信息，不会中断构建**。

### 4.1 主程序（ImGui 前端）

`env\scripts\build_imgui_demo.cmd` 里的一条 `g++` 就是全部（关键参数）：

```
-std=c++17 -O2 -mwindows -static -static-libgcc -static-libstdc++
-DUNICODE -D_UNICODE -D_PTK_CHINESE_UI
-DPZTK_BUILD_DATE="<YYYY-MM-DD>"     <- 「关于」框的编译时间；可复现构建的关键，见下
-I <imgui> -I <imgui>\backends -I <glfw>\include
-idirafter <inc> -idirafter <src> -idirafter <frontend> -idirafter <fltk>\include
                                    ^ 最后这个 <fltk>\include 只为 <FL/images/zlib.h> 而加
输入: frontend\imgui_main.cpp
      imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp
      backends\imgui_impl_glfw.cpp backends\imgui_impl_opengl3.cpp
      src\pvz.cpp process.cpp data.cpp code.cpp lineup.cpp utils.cpp names.cpp pak.cpp
链接: -lglfw3 -lopengl32 -lgdi32 -luser32 -lcomdlg32 -lole32 -lshell32
      -lwinmm -ladvapi32 -lwintrust -lcrypt32 -lfltk_z
      -Wl,--no-insert-timestamp       <- 抹掉 PE 头时间戳，同样是可复现构建的关键
```

三个必须记住的点：

* **`-idirafter` 不能省**：否则本项目的 `inc\process.h` 会顶掉系统的 `<process.h>`，报一堆莫名其妙的错。
* **`-static` 系列不能省**：否则会去 PATH 里找错位数的 `libstdc++`，运行时报 `0xC000007B`。
* **`-D_PTK_CHINESE_UI` 不能省**：`names.cpp` 靠它选中文名字表，否则界面显示英文名。
* 输入列表里那 8 个 `src\*.cpp` 是**核心层**，FLTK 版与 ImGui 版共用同一份；
  界面层只剩 `frontend\imgui_main.cpp`（FLTK 的 `window.cpp` / `main.cpp` / `toolkit.cpp` 已随前端移除）。

**可复现构建（同一份源码连编两次 ⇒ 产物逐字节相同）**：`-DPZTK_BUILD_DATE` 与
`-Wl,--no-insert-timestamp` 缺一不可 —— 前者替掉 `imgui_main.cpp` 里的 `__DATE__`/`__TIME__`，
后者抹掉链接器写进 PE 头的当前时间。构建戳默认取 `tool-src\CHANGELOG.md` 里**最上面那条**
`## YYYY-MM-DD`；想留真实时间戳就 `set PZTK_BUILD_DATE=2026-10-11`（也认 `SOURCE_DATE_EPOCH`）。
脚本每次会回显一行 `[imgui] build stamp: <值>`。
★ **注意**：本包里的 `tool-src\CHANGELOG.md` 是**精简占位版**（只有那条日期标题 + 一段说明），
所以**别删它** —— 删了构建戳会回退到脚本里的默认值，产物就不再与官方一致。
（完整的开发日志只在开发仓库，不随包发布。）

### 4.2 注入 DLL

`env\scripts\build_hook_dll.cmd`（**必须保持纯 ASCII**）。
它的 `%TC%`（工具链副本）与 `%WS%`（工作区别名）**同样解析自 `provision.cmd`**，与其它构建脚本同一套做法，
所以 `PZTK_BUILD_ROOT` 换根时 DLL 会写进**那份副本**的 `build\`，不会被写回默认根指向的另一个仓库。

```
-std=c++17 -O2 -m32 -shared -nostdlib -nostartfiles
-fno-exceptions -fno-rtti -fno-stack-protector
-fno-builtin -fno-tree-loop-distribute-patterns
-Wl,-e,_DllMainCRTStartup@12 -Wl,-s -lkernel32 -luser32
输入: hook\hook_main.cpp hook\hook_crt.cpp hook\minhook\{buffer,hde32,hook,trampoline}.cpp
```

* 这套工具链**只有 `crt2.o`、没有 `dllcrt2.o`** ⇒ 只能 `-nostdlib` + 自备入口点 + 自写 `memcpy/memset`
  （`hook\hook_crt.cpp`）。`-nostdlib` 也就没有 libgcc ⇒ DLL 里**禁用 64 位除法**，计时走 32 位 tick。
* **`-fno-builtin -fno-tree-loop-distribute-patterns` 必须加**：否则 `-O2` 会把我们自己写的
  `memcpy/memset` 循环优化成对 `memcpy/memset` 的调用 → 无限递归 → DLL 加载了但钩子装不上。
* ★ **exe 与 DLL 必须成对**：控制块布局由 `tool-src\hook\hook_ctl.h` 的 `kVersion` 决定，
  工具还会核对 DLL 报回的 `kDllBuild`。**只要改了 `hook_ctl.h`，两边都要重编**。

### 4.3 资源编译（图标 + VERSIONINFO，windres）

ImGui 目标在链接前会先用 `windres` 编一份资源目标文件（脚本里是 `%TEMP%\pztk\version_imgui.o`），
再把它加进 `g++` 的输入列表 —— 这就是 `build\pztk_imgui.exe` 的图标与
"右键 → 属性 → 详细信息"的来源：

```bat
:: 工作目录必须是 tool-src\res（脚本里是 pushd "%RES%" ... popd）
:: %TC% = provision 解析出的工具链副本：默认根时就是 %TEMP%\pztk\mingw32
"%TEMP%\pztk\mingw32\bin\windres.exe" --codepage=65001 -O coff ^
  -I "%TEMP%\pztk\mingw32\i686-w64-mingw32\include" ^
  -D _PTK_CHINESE_UI ^
  -o "%TEMP%\pztk\version_imgui.o" ^
  "version_imgui.rc"
```

* `--codepage=65001`：`version_imgui.rc` 里有中文（`CompanyName` / `FileDescription`），不加会乱码。
* **`-O coff` 不能省**：windres 默认输出 `.res`，`ld` 会以 `not an object` 拒绝，链接直接失败。
* **工作目录必须是 `tool-src\res`**：`.rc` 里的图标是相对路径 `logo.ico`，按**当前目录**解析
  （不是按 `.rc` 所在目录）；从仓库根跑会报 `logo.ico: No such file or directory`。
* **`.rc` 不得内嵌 manifest**：GCC 的 spec 有 `%:if-exists(default-manifest.o%s)`，任何非 shared
  链接都已经带上工具链自带的 `default-manifest.o`；再嵌一份会报
  `.rsrc merge failure: multiple non-default manifests`。
* **缺 `winresrc.h` 的先兆**：`<Windows.h>` 会包含 `winresrc.h`。工具链副本里没有
  `%TEMP%\pztk\mingw32\i686-w64-mingw32\include\winresrc.h` 时，windres 会停在
  `windows.h:27:10: fatal error: winresrc.h: No such file or directory`
  —— `env\scripts\provision.cmd` 已加存在性检查，会在准备阶段就报
  `ERROR: toolchain copy is incomplete: winresrc.h is missing`（不再静默通过）。
* **这一步失败不会中断构建**：`windres.exe` / `version_imgui.rc` / `logo.ico` 任一缺失，
  或 windres 返回非 0，脚本只打 `[imgui] WARNING: ...`（windres 日志逐行加 `[imgui]   windres:`
  前缀）**然后照常链接** ⇒ **最多少图标与版本信息**。想看它有没有成功，构建输出里应有
  `[imgui] windres version_imgui.rc`。

> 图标用的是 `tool-src\res\logo.ico`（上游遗留的现成美术资源，本包直接复用）；
> ImGui 版编的是 `res\version_imgui.rc`。给 FLTK 版用的 `res\version.rc` 已随前端一起移除。
> `pztk_hook.dll` 目前没有资源（注入 DLL 不需要）。

---

## 5. 自测与验证

本包**只带构建链**，**不带离线自测**：核心层自测程序 `recttest`（逐项断言补丁地址 / 阵型编解码 /
PAK 读写的 900+ 项检查）与几个 QA 脚本（相位核对、注入蹦床字节核对、界面截图验证、`.zbl` 造样本）
都**只在开发仓库**里，不随源码包发布。

> 想自己验证构建是否正常，按本节两条做就够了：
> 1. `env\scripts\build_all.cmd` 末行是 **`BUILD ALL OK`**；
> 2. 连编两次，`build\pztk_imgui.exe` 与 `build\pztk_hook.dll` **逐字节相同**（可复现）。

### 5.1 唯一带在包里的生成器

| 脚本 | 作用 | 备注 |
|---|---|---|
| `env\scripts\gen_glyphs.ps1` | 扫描源码字符串 → 生成 `tool-src\frontend\imgui_glyphs.h` | ★ **界面里新增/修改任何中文后必须重跑**，否则新字显示成 `?`。默认只增不减，`-Prune` 才裁。本身含中文，**必须保存为 UTF-8 带 BOM** |
| `env\scripts\gen_cd_font.py` | 从游戏字体位图生成 `tool-src\hook\cd_font.h`（卡片冷却数字的字模表） | 只有要改字模表时才需要跑；生成的 `.h` 已在包内 |

### 5.2 运行期自检（不需要游戏）

直接启动 `build\pztk_imgui.exe`：界面能出来、顶栏显示「未连接」、五个页面都能点，就说明前端与
资源（图标 / VERSIONINFO / 字形表）都没编坏。要连游戏内存才需要 `pztk_hook.dll`。

---

## 6. 运行发布件

发布包（解压即用的绿色包）根目录需要：

| 文件 | 必需 | 说明 |
|---|---|---|
| `pztk_imgui.exe` | ✅ | 主程序 |
| `pztk_hook.dll` | ✅ | **必须与 exe 同目录、且是同一次构建的一对** |
| `使用说明.txt` | ✅ | **文件名必须精确** —— 程序「杂项 → 关于 → 文档」按钮打开的就是"exe 同目录下的 `使用说明.txt`"，改名或换目录就打不开 |
| `lineup.yml` | ⭕ 可选 | 阵型列表。工具启动时会自动加载 exe 目录下**所有** `*.yml`（文件名不限）。★ 官方发布包里**自带**一份（339 条阵型），删掉也能正常用 |

> ⚠️ 从**开发工作区**里直接运行 `build\pztk_imgui.exe` 可能连不上游戏：如果工作区根目录带了
> `Low Mandatory Level` 标签，新建的 exe 会继承它，进程是 Low 完整性，**写不了游戏内存**。
> 实机测试/发布验证请把 exe + dll **拷到工作区之外的普通目录**再运行。

---

## 7. 编码与脚本约定（改脚本前先看）

| 文件类型 | 要求 | 为什么 |
|---|---|---|
| `.cmd` / `.bat` | **纯 ASCII**；行尾 **LF 或 CRLF 都可以**（本仓库工作区为 **LF**） | `cmd.exe` 按 ANSI 读脚本，里面的 UTF-8 中文注释会吞掉换行，后面的行会被当成命令执行（症状：输出里冒出 `ar: invalid option`、`'xxx.cpp' is not recognized`）。行尾本身 cmd 两种都吃（工作区里所有 `.cmd` 已经是 LF，且构建正常）——**只有"纯 ASCII"是硬要求** |
| `.ps1` | **UTF-8 带 BOM** | 没有 BOM 时 Windows PowerShell 5.1 会按 ANSI 解码，脚本里的中文会乱码 |
| 其它文本 | **UTF-8 无 BOM** | 源码里有中文注释与字符串 |

---

## 8. 许可

本项目以 **GPL-3.0** 发布（见 `LICENSE`）。它是 **PvZ Toolkit**（原作者 **lmintlcx**，
依据版本 **1.22.0 (2024-06-22)**）的修改版，修改版作者 **SilenceWoods**。
再发布时必须保留原作者署名、继续以 GPL-3.0 开源并提供完整对应源码。
第三方组件（MinHook / Dear ImGui / GLFW，以及来自 FLTK 源码树的 zlib）许可见包根 `THIRD-PARTY-NOTICES.txt`。
逐组件的许可名称、版权行与**许可全文的仓库内路径**见包根 `THIRD-PARTY-NOTICES.txt`。

> 本包**不含 FLTK 前端源码**，也不链接 FLTK 本体；FLTK 源码只在构建期作为 **zlib 来源**
> 由 `env\scripts\download_deps.py` 获取，不随包分发。因此 `THIRD-PARTY-NOTICES.txt` 里
> FLTK 只作为"构建依赖（仅 zlib）"列出。
