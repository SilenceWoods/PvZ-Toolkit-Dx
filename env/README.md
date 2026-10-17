# env —— PvZ Toolkit 的 32 位构建环境

> ★ **本文件在"可上传源码包"里**：下文提到的 `HANDOVER.md`（完整交接文档）等**开发内部材料
> 不随源码包发布**，只在开发仓库里。包里想了解构建细节，请看包根的 **`BUILD.md`**。
> 另外注意：本包**只带构建链**，不带离线自测与 QA 脚本（`recttest` / `check_phases` /
> `verify_*` / `make_test_zbl` 等只在开发仓库）。

这个目录不属于 PvZ Toolkit 源码树，它只是为了让 `tool-src` 能在**本机**编译出来而搭的
工具链与构建脚本。最终产物是 `..\build\pztk_imgui.exe`（主程序）与
`..\build\pztk_hook.dll`（注入组件，**必须与 exe 同目录、同一次构建**）。

> 参考体积：约 1.2 GB（工具链解压后 923 MB + 压缩包 277 MB）。源码与脚本本身只有几 KB。

## 一键构建

```bat
env\scripts\build_all.cmd
```

产物：`build\pztk_imgui.exe`（32 位、GUI 子系统、静态链接的主程序）+
`build\pztk_hook.dll`（注入组件，必须与 exe 同目录、同一次构建）。

分步执行（**`build_all.cmd` 就是按这四步写的**，第 1 步只在缺 zlib 时才跑）：

| 脚本 | 作用 |
| --- | --- |
| `env\scripts\download_deps.py` | 下载 MinGW i686 工具链与 FLTK / ImGui / GLFW 源码（校验 sha256） |
| `env\scripts\provision.cmd` | 建立 ASCII 路径别名 + 复制工具链到可写位置 |
| `env\scripts\build_zlib.cmd` | 只编 FLTK 源码树里的 zlib → `env\fltk32\lib\libfltk_z.a`（**不编 FLTK 界面**） |
| `env\scripts\build_glfw.cmd` | 编译 GLFW 3.4 静态库 → `env\glfw32\lib\libglfw3.a` |
| `env\scripts\build_imgui_demo.cmd` | 编译 ImGui 主程序 → `build\pztk_imgui.exe` |
| `env\scripts\build_hook_dll.cmd` | 编译注入 DLL → `build\pztk_hook.dll` |

## 为什么需要这一层

### 1. 工作区路径含中文，MinGW 工具链处理不了

本仓库路径是 `D:\植物大战僵尸相关\PVZ修改器修改`。MinGW-w64 内部按 UTF-8 处理路径，
但文件 API 走当前 ANSI 代码页（本机 936），于是**任何含中文的输入或输出路径都会失败**：

```
Fatal error: can't create D:\<乱码>\...\foo.o: No such file or directory
```

注意两个反直觉的点：

* **只把工具链搬走没用**——只要传给编译器的路径字符串里有中文就会失败；
* **Windows 会把 junction 解析回真实路径**——所以工具链本身必须**物理**位于纯 ASCII 路径，
  路径别名救不了它。

因此 `provision.cmd` 会：

1. 在本会话的 ASCII 临时目录下建 junction `…\pztk\ws` 指向仓库根，
   用它来表达源码与构建产物的路径（文件依旧落在仓库里）；
2. 把工具链**实体复制**到 `…\pztk\mingw32`，因为它的位置必须真的是 ASCII。

### 2. 沙箱里 Ninja 会挂死

Ninja 通过管道截获每个子进程的输出，而这个环境不允许这样做，结果是 `ninja --version`
正常、一执行构建规则就永久卡住（CPU 占用为 0）。GNU Make 直接继承 stdio，所以这里
统一使用 CMake 的 `MinGW Makefiles` 生成器 + `mingw32-make`。

### 3. Ninja 之外的其它注意事项

* `provision.cmd` 会把 PATH 收敛成只剩工具链目录，避免 CMake 在 PATH 里发现无关的
  `sh.exe`（`MinGW Makefiles` 生成器会因此报错）。
* CMake 无法转义 `-D` 参数里的反斜杠路径，所以脚本统一传正斜杠。

## 环境位置与会话的关系

构建根目录默认在 `%TEMP%\pztk`（本会话的 ASCII 临时目录）：

```
%TEMP%\pztk\mingw32   工具链实体副本（ASCII，必须）
%TEMP%\pztk\ws        junction -> 仓库根
```

临时目录可能随会话结束被清理。清理后重新执行 `build_all.cmd` 即可重建；
`build_zlib.cmd` 与 `build_glfw.cmd` 都是幂等 / 增量的（zlib 看 `env\fltk32\lib\libfltk_z.a`
在不在、GLFW 看 `env\glfw32` 里的 `.pztk-alias` 戳 + `libglfw3.a`），
重建代价主要在复制工具链（约 1 分钟）。

zlib 的源码就在仓库内（`env\src\fltk-1.4.5\zlib` —— FLTK **只作 zlib 来源**），
GLFW 的源码、构建树和安装前缀也在仓库内（`env\src\glfw-3.4`、`env\glfw-build`、`env\glfw32`），
都不会丢；但它们的 CMake 配置里记录了 ASCII 别名路径，所以换会话（换根）后要重新跑一次
`build_glfw.cmd` 让路径刷新 —— `build_zlib.cmd` 复用同一套别名，会跟着一起生效。

## 为了在 MinGW 下编译，改了 tool-src 哪些地方

均为最小化改动，MSVC 路径保持原样：

> ★ **下表是"开发仓库视角"的历史记录**：带 ⊘ 的几行属于 **FLTK 版**（上游 `pvztoolkit.exe`
> 那条链），那些文件已在 **2026-10-13 随 FLTK 前端一起从本仓库移除**，列在这里只为完整溯源。

| 文件 | 改动 | 原因 |
| --- | --- | --- |
| `inc/data.h` | 增加 `#include <cstdint>` | MSVC 的 `<Windows.h>` 会间接带入定长整型，GCC 不会 |
| `inc/lineup.h` | 增加 `#include <cstdint>` | 同上 |
| `inc/process.h` | 把 `ReadMemory<std::string>` 的显式特化声明移出类体 | 类内显式特化是 MSVC 扩展，标准要求在命名空间作用域 |
| ⊘ `inc/window.h` | 声明 `extern Fl_Font ui_font / ms_font` | 原先靠 MSVC 非标准行为把块作用域 `extern` 绑到全局变量（**本仓库已无此文件**） |
| ⊘ `src/main.cpp` | 字体变量改在 `namespace Pt` 内定义并加 `using`；`_MSC_VER` 断言加条件编译 | 同上，且 GCC 没有 `_MSC_VER`（**本仓库已无此文件**） |
| ⊘ `res/version.rc` | `#pragma code_page` 仅 MSVC；不再内嵌清单 | 见下（**本仓库已无此文件**，ImGui 版编 `res/version_imgui.rc`） |
| ⊘ `CMakeLists.txt` | 拆分 MSVC / MinGW 分支；windres 自定义命令；静态链接选项；打开中文界面 | 工具链差异（**本仓库已无 CMake 工程**，该文件只剩 24 行标记注释） |

### 清单文件（manifest）的处理

`res\pvztoolkit.manifest` 里的 `activeCodePage=UTF-8` 决定了界面中文能否正确显示，
不能丢。但 GCC 的 spec 里写死了：

```
%{!shared:%:if-exists(default-manifest.o%s)}
```

也就是任何非 shared 链接都会自动带上工具链自带的 `default-manifest.o`；此时再内嵌一份
清单就会报 `.rsrc merge failure: multiple non-default manifests`。

`env\scripts\install_manifest.cmd` 的做法是：用 windres 把本工程的 `pvztoolkit.manifest`
编译成工具链里的 `i686-w64-mingw32\lib\default-manifest.o`，**替换**掉自带的默认清单。
它由 `provision.cmd` 每次调用，而且**两份工具链副本一起装**（母本 `env\mingw32` + 副本 `<根>\mingw32`）。
这样全链路只有一份清单，且就是我们需要的那份。

## 已知的小问题

* ⊘ 编译 FLTK 版时曾有一条警告：`window.h` 里 `Window::cb_tooltips()` 声明为 `inline` 但从未定义。
  这是上游代码就有的（MSVC 不报），不影响功能。**本仓库已无 `window.h`，构建时不会再出现它。**
* `refs\game-src`（2026-10-14 之前叫 `game-src`）与 `tool-src` 目录在本会话建立时没有被沙箱登记，
  因此**沙箱内启动的命令行进程无法直接写入这两个目录**（DSH 的文件工具不受影响，`build` 已重建因此正常）。
  如果以后需要命令行工具直接写这两个目录，用同样的办法重建（目录内不能有文件）或让
  会话切到完全权限。

## FLTK（**只作 zlib 来源**）

本仓库**不构建、不链接 FLTK 界面**，只把 FLTK 源码树里的 `zlib/` 子目录当作 **zlib 来源**
（`env\scripts\build_zlib.cmd`，见上面的"分步执行"）；`env\src\fltk-1.4.5` 不需要打任何补丁。
上游那份 FLTK 补丁（`deps\pvztoolkit_fltk.diff`，4 处改动：`Fl_Choice` 焦点框、`Fl_Tabs` 重绘与焦点框、
`Fl_Screen_Driver` 缩放快捷键与缩放档位、`Fl_win32` 强制初始缩放为 1.0）
**不需要并入**：那几处都是给 FLTK **界面**用的，与只编 zlib 无关。
★ `tool-src\deps\` 整个目录（含那份补丁）已在 **2026-10-13 随 FLTK 前端一起从本仓库删除**
（本仓库已无此文件）。
