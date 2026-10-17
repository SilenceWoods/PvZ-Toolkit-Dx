// ---------------------------------------------------------------------------
// PvZ Toolkit Dx 新前端 (Dear ImGui + GLFW + OpenGL2) —— 骨架
//
// 第 1 步: 主窗 + 左侧标签栏 + 五个空标签页 + 底部状态栏 + 凹陷风格 + 只读接入。
// 核心逻辑完全复用 Pt:: 命名空间 (pvz/data/process/... 与 names)。
//
// 布局:
//   ┌─ 顶栏: 标题 + 连接状态徽章 ────────────────────┐
//   │ ┌ 侧栏 ┬ 内容区 ──────────────────────────────┐ │
//   │ │ 战场 │  (每个功能块: 标题居中 + 凹陷边框)   │ │
//   │ │ 资源 │                                       │ │
//   │ │ 阵型 │                                       │ │
//   │ │ 关卡 │                                       │ │
//   │ │ 杂项 │                                       │ │
//   │ └──────┴───────────────────────────────────────┘ │
//   │ 底部状态栏 (20px 徽章)                          │
//   └──────────────────────────────────────────────────┘
// ---------------------------------------------------------------------------
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_glyphs.h" // 自动生成: 界面实际用到的全部字符区间

#include <GLFW/glfw3.h>
#include <windows.h>
#include <commdlg.h> // GetOpenFileNameW / GetSaveFileNameW (.zbl 导入导出)
#include <shlobj.h>  // SHBrowseForFolderW (选择打包文件夹) / SHGetFolderPathW

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <algorithm>
#include <fstream>
#include <regex>
#include <vector>
#include <array>
#include <string>

#include "pvz.h"
#include "pak.h"     // PAK 解包 / 打包
#include "version.h" // 版本号 (关于对话框)

// FLTK 自带的 zlib (符号已加 fltk_z_ 前缀, 由 zconf.h 宏映射, 在 libfltk_z.a 里)。
// .zbl 的 crc32 必须与 FLTK 版一致, 所以用这一份而不是系统 zlib。
#include <FL/images/zlib.h>

// ---------------------------------------------------------------------------
// 构建戳 (可复现构建)
//
// 原来是编译器自动填的 __DATE__ / __TIME__, 于是"同一份源码每次编出来的 exe
// 哈希都不一样", 发布时没法拿哈希核对产物是否由这份源码生成。
// 现在改由 env/scripts/build_imgui_demo.cmd 用 -DPZTK_BUILD_DATE="YYYY-MM-DD"
// 显式传入 (默认取 tool-src/CHANGELOG.md 最上面那条日期; 可用环境变量
// PZTK_BUILD_DATE 或 SOURCE_DATE_EPOCH 覆盖), 链接时再加
// -Wl,--no-insert-timestamp 抹掉 PE 头时间戳 —— 连编两次逐字节相同。
// 下面的回退值也是**固定**的 (单独编本文件时用), 千万别改回 __DATE__。
// ---------------------------------------------------------------------------
#ifndef PZTK_BUILD_DATE
#define PZTK_BUILD_DATE "2026-10-10"
#endif

// ---------------------------------------------------------------------------
// 尺寸与配色 —— 2026-10-15 界面风格重置: **浅色 + 蓝色强调**（用户拍板变体 C）
//
// 设计口径 (要改主题, 只动这一段 + ApplyStyle):
//   * 页底 = 极浅灰蓝 #EEF2F8, 卡片 = 纯白 —— 靠"底色差 + 1px 描边 + 7px 圆角"分层。
//     **不用阴影** —— ImGui 画不出柔和阴影, 硬画会脏; 也不再走"更深的底 = 凹陷"那套。
//   * 强调色 = 蓝 #0A84FF (hover #006FE8 / active #005CC7 / 选中底 #E1F0FF)。
//   * 文字 = 近黑石板 #1F2937; 次级 #64748B; 更淡 #94A3B8。
//   * 侧栏 / 状态栏 = #F5F8FC; 输入框 / 表头 = #F3F6FB。
//   * 语义色: 绿 #15803D (已生效) / 橙 #B45309 (未生效) / 红 #B91C1C (错误)。
//   ★ 尺寸仍然全部乘 g_dpi。本主题**刻意没有**改 FramePadding / ItemSpacing /
//     块内分隔线的行高 —— 所以 20 多个块高常量不需要重算 (改块高必须靠截图量)。
// ---------------------------------------------------------------------------
static const int kWinW = 460;      // 逻辑宽 (窄而高)
static const int kWinH = 720;
static const int kWinWMax = 620;
static const float kSideW = 78.0f; // 侧栏宽
static const float kStatusH = 20.0f;

// 0..255 写颜色, 一眼能和设计稿对上
static constexpr ImVec4 Rgb(int r, int g, int b, int a = 255)
{
    return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
}

static const ImVec4 kAccent     = Rgb(0x0A, 0x84, 0xFF); // 主蓝
static const ImVec4 kAccentHov  = Rgb(0x00, 0x6F, 0xE8);
static const ImVec4 kAccentAct  = Rgb(0x00, 0x5C, 0xC7);
static const ImVec4 kAccentSoft = Rgb(0xE1, 0xF0, 0xFF); // 选中底
static const ImVec4 kInk        = Rgb(0x1F, 0x29, 0x37); // 正文
static const ImVec4 kInkDim     = Rgb(0x64, 0x74, 0x8B); // 次级
static const ImVec4 kInkFaint   = Rgb(0x94, 0xA3, 0xB8); // 更淡
static const ImVec4 kLine       = Rgb(0xD8, 0xE0, 0xEC); // 卡片描边
static const ImVec4 kLineSoft   = Rgb(0xE9, 0xEE, 0xF6); // 块内分隔线
static const ImVec4 kWhite      = Rgb(0xFF, 0xFF, 0xFF);
static const ImVec4 kPane       = Rgb(0xF5, 0xF8, 0xFC); // 侧栏 / 状态栏
static const ImVec4 kPaneHov    = Rgb(0xEA, 0xF1, 0xF9); // 侧栏悬停
static const ImVec4 kPage       = Rgb(0xEE, 0xF2, 0xF8); // 内容区底 (浅灰蓝)
// ---- 可交互层: 同一条蓝色梯度 (2026-10-15 用户拍板方案 A + "小方框/小圆框也协调") ----
//   白卡片 #FFFFFF → 极浅蓝输入框/勾选框 #EDF4FE → 淡蓝按钮 #E1F0FF → 纯蓝强调 #0A84FF
//   ★ 勾选框的"小方框"、单选的"小圆框"、下拉与输入框的底都属于 ImGuiCol_FrameBg,
//     在 ImGui 里**是同一个颜色槽** (没法只给小方框上色而不动输入框) ⇒ 干脆整族一起走
//     同一条蓝: 白卡片上"极浅蓝"既能看出是可编辑区, 又不会跳出去。
static const ImVec4 kField      = Rgb(0xED, 0xF4, 0xFE); // 输入框 / 勾选框 / 表头
static const ImVec4 kFieldHov   = Rgb(0xE2, 0xED, 0xFD);
static const ImVec4 kFieldAct   = Rgb(0xD6, 0xE7, 0xFC);
static const ImVec4 kBtn        = Rgb(0xE1, 0xF0, 0xFF); // 普通按钮 (方案 A: 淡蓝填充 + 1px 描边)
static const ImVec4 kBtnHov     = Rgb(0xD2, 0xE7, 0xFF);
static const ImVec4 kBtnAct     = Rgb(0xBF, 0xDC, 0xFF);
static const ImVec4 kOk         = Rgb(0x15, 0x80, 0x3D);
static const ImVec4 kWarn       = Rgb(0xB4, 0x53, 0x09);
static const ImVec4 kBad        = Rgb(0xB9, 0x1C, 0x1C);
static const ImVec4 kOkSoft     = Rgb(0xE7, 0xF6, 0xEC); // 状态胶囊底
static const ImVec4 kBadSoft    = Rgb(0xFD, 0xEC, 0xEC);
static const ImVec4 kNote       = Rgb(0x47, 0x55, 0x69); // 说明文字

// DrawList 直接要 ImU32 的那几个
static const ImU32 kU32Accent   = IM_COL32(0x0A, 0x84, 0xFF, 0xFF);
static const ImU32 kU32Line     = IM_COL32(0xD8, 0xE0, 0xEC, 0xFF);

// ---------------------------------------------------------------------------
// 全局状态
// ---------------------------------------------------------------------------
static float g_dpi = 1.0f;
// ---------------------------------------------------------------------------
// 字形集: 工具自身字符串的字集在 imgui_glyphs.h 里 (由 gen_glyphs.ps1 扫描源码生成),
// 但**阵型名是用户数据** (exe 目录下的 *.yml), 脚本扫不到 ⇒ 那些字不在图集里 ⇒ 显示成 '?'。
// 所以这里在创建字体之前, 先把 exe 目录下所有 *.yml 里出现的字合并进字形集。
//
// 说明: ImGui 1.91.8 **没有**运行时"按需加载字形"那套东西 (docs/TODO.txt:290 还只是待办,
// ImFontLoader/RendererHasTextures 是 1.92 才有的), 所以用"启动时把用户数据里的字扫进来"这个
// 等价做法 —— 覆盖精确、图集只增大实际用到的那几百个字, 不需要下载任何字体
// (字体本来就是系统的微软雅黑 msyh.ttc, 字是全的)。
// 注意: 返回值必须长期有效 (ImFontConfig::GlyphRanges 要求), 所以用 static 存着。
// ---------------------------------------------------------------------------
static const ImWchar *GlyphRangesWithLineups()
{
    static ImVector<ImWchar> ranges;
    static bool built = false;
    if (built)
        return ranges.Data;
    built = true;

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(kGlyphRanges); // 工具自身界面用到的字

    wchar_t module[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::wstring dir(module);
    const size_t slash = dir.find_last_of(L"\\/");
    dir.resize(slash == std::wstring::npos ? 0 : slash + 1);

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW((dir + L"*.yml").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                continue;
            HANDLE hFile = CreateFileW((dir + fd.cFileName).c_str(), GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile == INVALID_HANDLE_VALUE)
                continue;
            const DWORD size = GetFileSize(hFile, nullptr);
            if (size != INVALID_FILE_SIZE && size > 0 && size < (8u << 20))
            {
                std::vector<char> raw(size);
                DWORD got = 0;
                if (ReadFile(hFile, raw.data(), size, &got, nullptr) && got > 0)
                {
                    // yml 规定是 UTF-8 (misc/setups.yml 里写明)
                    const int wn = MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)got, nullptr, 0);
                    if (wn > 0)
                    {
                        std::wstring wide((size_t)wn, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)got, &wide[0], wn);
                        for (size_t i = 0; i < wide.size(); i++)
                            if (wide[i] >= 0x20)
                                builder.AddChar((ImWchar)wide[i]);
                    }
                }
            }
            CloseHandle(hFile);
        } while (FindNextFileW(hFind, &fd) != 0);
        FindClose(hFind);
    }

    builder.BuildRanges(&ranges);
    return ranges.Data;
}
static ImFont *g_font_small = nullptr;
static ImFont *g_font_big = nullptr; // 资源页三个数值用的大号字
// 板块标题 / 顶栏标题用的**粗体** (msyhbd.ttc)。加载失败时为 nullptr ⇒ 代码里必须判空,
// 那只是标题不够粗, 不能因为缺一个字体文件就崩。
static ImFont *g_font_title = nullptr;
// 子窗上下文也各有一份粗体 (ImGui 的字体属于上下文)
static ImFont *s_font_title_of_context = nullptr;


static int  g_page = 0; // 0=战场 1=资源 2=阵型 3=关卡设置 4=杂项
static bool g_show_speed = false;
static ImFont *g_sub_font_small = nullptr; // 子窗说明区用小号字
static const char *kPageNames[] = {"战场", "资源", "阵型", "关卡设置", "杂项"};
static const int kPageCount = 5;

// 只读接入
static Pt::PvZ g_pvz;
static bool  g_attached = false;
static const char *g_attach_msg = "未找到游戏窗口";
static double g_attach_next_try = 0.0;  // 未连接时的下次重试时刻 (glfwGetTime)
static bool  g_reapply_toggles = false;  // 连接/重连后把已勾选的开关重新写一遍
// ★★ v39: 自动注入的**状态机** (判据是共享头纯函数 `PtHook::HookInjectPolicy`,
//   这里只是它的一个实例; 每帧喂 `in_level` / `alive_now`, 按返回值决定要不要真注入)。
//   放在文件作用域 = 主循环更新、左下角诊断行 (PZTK_DEV=1) 读取, 两边看同一份状态。
static PtHook::HookInjectPolicy g_inject_policy;

// 这两项在 FLTK 版里默认就是勾上的 (window.cpp 的 check_background->value(1) /
// check_limbo_page->value(1)), 而且启动时会被 keep_selected_feature() 应用下去 ——
// 其中"允许后台运行"就是"点修改器时游戏不会被暂停"的原因 (核心会打补丁让游戏在
// 失去焦点时继续跑)。默认值必须保持一致, 否则就是功能回归。
static bool g_background = true; // 允许后台运行
static bool g_limbo = true;      // 显示隐藏页面
static std::vector<Pt::ZombieSpeedInfo> g_zs;
static std::vector<Pt::PlantSpeedInfo>  g_ps;
static std::array<float, Pt::ZSC_COUNT> g_zval{};
static std::array<bool, Pt::ZSC_COUNT>  g_zen{};
static std::array<float, Pt::PSC_COUNT> g_pint{};
static std::array<bool, Pt::PSC_COUNT>  g_pen{};
static std::array<float, Pt::PSC_COUNT> g_wsec{};
static std::array<bool, Pt::PSC_COUNT>  g_wen{};

// ---------------------------------------------------------------------------
// 风格
// ---------------------------------------------------------------------------
static void ApplyStyle(float dpi)
{
    ImGuiStyle &s = ImGui::GetStyle();

    s.WindowPadding    = ImVec2(6, 6);
    s.FramePadding     = ImVec2(6, 3);
    s.CellPadding      = ImVec2(5, 3);
    s.ItemSpacing      = ImVec2(5, 5);
    s.ItemInnerSpacing = ImVec2(5, 4);
    s.ScrollbarSize    = 11.0f;
    s.GrabMinSize      = 8.0f;

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize  = 1.0f;
    // ★ 2026-10-15: 0 → **1**。浅色主题下按钮 / 输入框的底色与页底 (#EEF2F8) 太接近,
    //   实测"僵尸移速 / 植物攻速"两个按钮与背景融成一块 ⇒ 全部 Frame 类控件描 1px 边。
    //   ★ 安全: 边框画在 frame 矩形**之内**, `GetFrameHeight() = 字号 + 2*FramePadding.y`,
    //     不含边框 ⇒ **不会改变任何控件高度**, 20 多个块高常量依旧不用重算。
    s.FrameBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;

    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 7.0f;
    s.FrameRounding     = 5.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 6.0f;

    s.AntiAliasedLines = true;
    s.AntiAliasedFill  = true;
    s.ScaleAllSizes(dpi);

    // ---- 浅色主题 (白底 + 蓝强调): 见文件顶部"尺寸与配色"那段的口径 ----
    ImVec4 *c = s.Colors;
    c[ImGuiCol_Text]                 = kInk;
    c[ImGuiCol_TextDisabled]         = kInkFaint;
    c[ImGuiCol_WindowBg]             = kWhite;
    c[ImGuiCol_ChildBg]              = kWhite;
    c[ImGuiCol_PopupBg]              = kWhite;
    c[ImGuiCol_Border]               = kLine;
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = kField;
    c[ImGuiCol_FrameBgHovered]       = kFieldHov;
    c[ImGuiCol_FrameBgActive]        = kFieldAct;
    c[ImGuiCol_TitleBg]              = kPane;
    c[ImGuiCol_TitleBgActive]        = kPane;
    c[ImGuiCol_TitleBgCollapsed]     = kPane;
    c[ImGuiCol_MenuBarBg]            = kPane;
    c[ImGuiCol_Button]               = kBtn;
    c[ImGuiCol_ButtonHovered]        = kBtnHov;
    c[ImGuiCol_ButtonActive]         = kBtnAct;
    c[ImGuiCol_Header]               = kAccentSoft;
    c[ImGuiCol_HeaderHovered]        = Rgb(0xD5, 0xEA, 0xFF);
    c[ImGuiCol_HeaderActive]         = Rgb(0xC7, 0xE2, 0xFF);
    c[ImGuiCol_TableHeaderBg]        = kField;
    c[ImGuiCol_TableRowBg]           = kWhite;
    c[ImGuiCol_TableRowBgAlt]        = Rgb(0xF8, 0xFA, 0xFD);
    c[ImGuiCol_TableBorderStrong]    = kLine;
    c[ImGuiCol_TableBorderLight]     = kLineSoft;
    c[ImGuiCol_Separator]            = kLine;
    c[ImGuiCol_SeparatorHovered]     = kAccent;
    c[ImGuiCol_SeparatorActive]      = kAccentAct;
    c[ImGuiCol_CheckMark]            = kAccent;
    c[ImGuiCol_SliderGrab]           = kAccent;
    c[ImGuiCol_SliderGrabActive]     = kAccentAct;
    c[ImGuiCol_ScrollbarBg]          = Rgb(0xF4, 0xF7, 0xFB);
    c[ImGuiCol_ScrollbarGrab]        = Rgb(0xCB, 0xD5, 0xE3);
    c[ImGuiCol_ScrollbarGrabHovered] = Rgb(0xB3, 0xC1, 0xD6);
    c[ImGuiCol_ScrollbarGrabActive]  = Rgb(0x93, 0xA5, 0xC0);
    c[ImGuiCol_NavCursor]            = kAccent;
    // 页签 / 弹层 / 拖放 / 导航窗口这几组在 1.91.8 里**默认是深色预设**的颜色,
    // 浅色主题必须全部显式覆盖 —— 漏一个就会在黑底里冒出一块深蓝 (出怪详情窗的页签就是这样)。
    c[ImGuiCol_Tab]                      = kPane;
    c[ImGuiCol_TabHovered]               = kAccentSoft;
    c[ImGuiCol_TabSelected]              = kAccentSoft;
    c[ImGuiCol_TabSelectedOverline]      = kAccent;
    c[ImGuiCol_TabDimmed]                = kPane;
    c[ImGuiCol_TabDimmedSelected]        = kAccentSoft;
    c[ImGuiCol_TabDimmedSelectedOverline]= kLine;
    c[ImGuiCol_DockingPreview]           = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.70f);
    c[ImGuiCol_DockingEmptyBg]           = kPane;
    c[ImGuiCol_PlotLines]                = kAccent;
    c[ImGuiCol_PlotLinesHovered]         = kAccentHov;
    c[ImGuiCol_PlotHistogram]            = kAccent;
    c[ImGuiCol_PlotHistogramHovered]     = kAccentHov;
    c[ImGuiCol_TextLink]                 = kAccent;
    c[ImGuiCol_TextSelectedBg]           = Rgb(0xC7, 0xE2, 0xFF);
    c[ImGuiCol_DragDropTarget]           = kAccentAct;
    c[ImGuiCol_NavWindowingHighlight]    = Rgb(0x1F, 0x29, 0x37, 0x70);
    c[ImGuiCol_NavWindowingDimBg]        = Rgb(0x64, 0x74, 0x8B, 0x30);
    c[ImGuiCol_ModalWindowDimBg]         = Rgb(0x33, 0x3D, 0x4D, 0x55);
    c[ImGuiCol_ResizeGrip]               = Rgb(0xD8, 0xE0, 0xEC);
    c[ImGuiCol_ResizeGripHovered]        = kAccentSoft;
    c[ImGuiCol_ResizeGripActive]         = kAccent;
}

// ---------------------------------------------------------------------------
// 小组件
// ---------------------------------------------------------------------------
// 状态胶囊 (顶栏连接状态 / DPI / 底栏计数): 圆角底色 + 可选的前置圆点。
//   ★ 高度只比文字高 3*dpi (老 Badge 是 +2) —— 顶栏那一行不能长太高, 否则整页
//     内容被挤下去、末尾"其他修改"块会掉到地板高度以下并撑出滚动条。
static void Pill(const char *text, const ImVec4 &fg, const ImVec4 &bg,
                 const ImVec4 *dot = nullptr)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 sz = ImGui::CalcTextSize(text);
    const float pad    = 7.0f * g_dpi;
    const float dot_d  = 7.0f * g_dpi;
    const float dot_gap = (dot != nullptr) ? (dot_d + 5.0f * g_dpi) : 0.0f;
    const float h = sz.y + 3.0f * g_dpi;
    const float w = sz.x + pad * 2 + dot_gap;

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(bg), h * 0.5f);
    if (dot != nullptr)
        dl->AddCircleFilled(ImVec2(p.x + pad + dot_d * 0.5f, p.y + h * 0.5f), dot_d * 0.5f,
                            ImGui::GetColorU32(*dot));
    dl->AddText(ImVec2(p.x + pad + dot_gap, p.y + (h - sz.y) * 0.5f), ImGui::GetColorU32(fg), text);
    ImGui::Dummy(ImVec2(w, h));
}

// 子窗 (移速 / 攻速 / 出怪详情) 里的小节标题: 深色 + 粗体。
//   子窗上下文的粗体在 SetupContextStyle() 里加载 (ImGui 的字体属于上下文),
//   缺字体时退回普通字重, 不影响布局高度。
static void SectionTitle(const char *text)
{
    if (s_font_title_of_context != nullptr)
        ImGui::PushFont(s_font_title_of_context);
    ImGui::TextColored(kInk, "%s", text);
    if (s_font_title_of_context != nullptr)
        ImGui::PopFont();
}

// 板块: 白卡片 (1px 浅描边 + 圆角) + 左侧蓝色强调条 + 左对齐加粗标题。
//   ★ 标题那一行的高度必须与"居中标题 + Separator"的老版**完全一致**
//     (文字行高 + 分隔线 + ItemSpacing), 否则 20 多个 `xxx * g_dpi` 块高全要重算。
static void BeginBlock(const char *title, float height)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kWhite);
    ImGui::PushStyleColor(ImGuiCol_Border, kLine);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f * g_dpi);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * g_dpi, 4 * g_dpi));
    ImGui::BeginChild(title, ImVec2(0, height), ImGuiChildFlags_Borders);

    // 标题行: 3px 蓝色竖条 + 标题 (左对齐)。
    //   ★ 竖条用 DrawList 画在光标处, 再只把**文字**右移 —— 不要用 Dummy+SameLine
    //     那套 (实测会在块内把布局搅成死循环: 第一帧就卡住不返回)。
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float lh = ImGui::GetTextLineHeight();
        const float bw = 3.0f * g_dpi;
        dl->AddRectFilled(ImVec2(p.x, p.y + 1.5f * g_dpi),
                          ImVec2(p.x + bw, p.y + lh - 1.5f * g_dpi), kU32Accent, bw * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + bw + 7.0f * g_dpi);
    }
    if (g_font_title != nullptr)
        ImGui::PushFont(g_font_title);
    ImGui::TextColored(kInk, "%s", title);
    if (g_font_title != nullptr)
        ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Separator, kLineSoft);
    ImGui::Separator();
    ImGui::PopStyleColor();
}

static void EndBlock()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2); // ChildBg + Border (与 BeginBlock 开头成对)
}

// ---------------------------------------------------------------------------
// 各标签页 (第 1 步只放占位, 后续逐步填充)
// ---------------------------------------------------------------------------
// 三个修改板块的状态 (第 1 步先只做界面, 逻辑后续接)
static bool g_plant_mod[6] = {};
static bool g_zombie_mod[6] = {};
static bool g_other_mod[6] = {};

// 快捷布阵模式 (阵型页): 开启时强制打开 6 项便利功能, 并锁住对应勾选框;
// 取消时恢复成用户原来的勾选状态。列表与 FLTK 版 check_lineup_mode 的
// check_buttons 完全一致 (自动收集 / 免费用卡 / 植物无敌 / 立刻装填 /
// 暂停刷怪 / 清除浓雾; 植物脆弱在 FLTK 里也是"跳过"的, 所以不接管)。
static bool g_lineup_mode = false;
static const bool kLmLockPlant[6]  = {true, false, true, false, false, false};
static const bool kLmLockZombie[6] = {false, false, true, false, false, false};
static const bool kLmLockOther[6]  = {true, false, false, false, true, false};

// ---------------------------------------------------------------------------
// 2026-10-07 新增功能的状态 (核心实现见 src/pvz.cpp 的 FrameHacks)
//   * 冻结 / 魅惑 / 无视进家 / 血量倍数: 前端每帧把选择同步给核心
//   * 女仆秘籍: 0 关 1 保持前进 2 跳舞 3 召唤伴舞
//   * 智慧树指令: 勾选即发一次按键序列, 之后每帧回读游戏标志位做回显
// ---------------------------------------------------------------------------
static bool  g_kill_home = false;
static int   g_maid_mode = 0;
static float g_zombie_hp_mul = 1.0f;
// 多倍出怪 (出怪点数 ×N): 勾选后打进游戏自己的出怪分配, 挑怪仍由游戏决定
static bool  g_wave_mul_on = false;
static int   g_wave_mul_n = 1;   // 默认 1 = 关闭; 上限 24
static const int kWaveMulMax = 24;
// 高级暂停: 勾上后**不是主动暂停游戏**, 而是把游戏自带的暂停(按空格)变成高级暂停
static bool  g_adv_pause = false;
// ★★ 2026-10-14 新增: 「取消必选卡片」—— 冒险模式**二周目** Crazy Dave 预选 ★★
//   两套机制 (核心层实现, 见 pvz.cpp 的同名小节与 HANDOVER §4.14):
//     * 在**选卡界面**勾上 ⇒ 那 3 张立刻被放开, 能点下来 (机制 B: 清 mCrazyDavePicked)
//     * 在**别处**勾上 ⇒ 进关卡时根本不抽卡 (机制 A: 把那条 `jle` 改成 `jmp` 的字节补丁)
//   勾选框状态在这里, 实际动作在下面主循环里"开关变化 / 游戏换进程"时触发, 机制 B 每帧做。
static bool  g_no_dave_pick = false;
static char  g_zombie_action_msg[96] = {}; // 魅惑/冻结按钮的结果提示
static std::string g_maid_patch_msg;       // 女仆秘籍用的是补丁模式还是钉时钟模式
static bool  g_wisdom_checked[7] = {};
static bool  g_wisdom_pending[7] = {};
static int   g_wisdom_hold[7] = {};   // 刚发完按键后的几帧内显示"(已发送)"
static int   g_wisdom_verify[7] = {}; // 发送后等这么多帧再去核对游戏里是否真的生效 (0 = 不核对)
static int   g_wisdom_result[7] = {}; // 核对结果: 0=无, 1=未生效, 2=已生效
static int   g_wisdom_result_hold[7] = {}; // 结果显示多少帧
static bool  g_wisdom_inited = false; // 连上游戏 (含游戏换进程重开) 后先读一遍状态来打勾 (不发送)
static bool  g_wisdom_touched = false; // 用户手动点过勾选框 -> 之后不再自动回显 (免得覆盖他的操作)
static bool  g_wisdom_in_level = false; // 是否已在"关卡内"补扫过一次 (主菜单时记录可能无效)
// ---------------------------------------------------------------------------
// 开发者模式: 只有设了环境变量 PZTK_DEV=1 时才显示"给开发者看的"诊断
// (钩点地址 / 调用次数 / 原始字节 / 失败码 / 工具采集方框数)。
// 普通用户看到的是干净界面 —— 出问题时我自己 PZTK_DEV=1 启动就能看到全部细节。
// ---------------------------------------------------------------------------
static bool DevMode()
{
    static int cached = -1;
    if (cached < 0)
    {
        const char *v = std::getenv("PZTK_DEV");
        cached = (v != nullptr && v[0] != '\0' && v[0] != '0') ? 1 : 0;
    }
    return cached != 0;
}
// ★★ v40: 「碰撞箱」整条功能 (勾选框 / 状态行 / tooltip / 透明叠加窗降级路径) 已删除。
//   用户拍板 (原话意思): **越改越乱、不实用、测试用不到、常规关卡里看着难看**。
//   卡片冷却时间显示 (2026-10-10 新增): **默认开启** —— 它只是只读的显示, 不改游戏行为
//   ★★ v37: 从"一个勾选框"改成**多选一**; v40: 界面上只留**两项** (见下面「卡槽」块) ★★
//       `关闭` / `游戏字体(新方式)`
//     为什么**默认 = 游戏字体(新方式)**: 用户实机用的就是它 (v37 的"矩形(老方式)"已按
//       用户要求从界面**隐藏**、只保留代码与环境变量后门 `PZTK_CD_MODE=1`)。
//     环境变量 **`PZTK_CD_MODE=0|1|2`** 仍可覆盖界面选择 (启动工具前设; 排查用) ——
//       设了以后界面上的单选钮置灰并显示"(环境变量覆盖)", 免得用户以为点了没反应。
static int  g_cd_mode = static_cast<int>(PtHook::CDMODE_NATIVE);
static bool g_cd_mode_forced = false; // true = 环境变量在控制

// 读一次 `PZTK_CD_MODE` (幂等; 在"下发"和"界面"两处各调一次)
static void EnsureCdModeInit()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    char buf[16] = {0};
    const DWORD n = GetEnvironmentVariableA("PZTK_CD_MODE", buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf))
        return;
    // ★ 用**共享头里的同一个纯函数**解析 (与离线单测跑的是同一份实现);
    //   fallback 用 -1 这个**非法值**当哨兵, 才能区分"解析成功得到 0"和"解析失败"。
    const int32_t parsed =
        PtHook::CdParseModeText(buf, static_cast<int32_t>(n), -1);
    if (parsed < 0 || !PtHook::CdModeValid(static_cast<uint32_t>(parsed)))
        return;
    g_cd_mode = static_cast<int>(parsed);
    g_cd_mode_forced = true;
}




// pztk_hook.dll 的路径: 与工具 exe 同目录
static std::wstring HookDllPath()
{
    wchar_t path[MAX_PATH] = {0};
    const DWORD got = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring result(path, got);
    const size_t slash = result.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        result.resize(slash + 1);
    result += L"pztk_hook.dll";
    return result;
}

// 魅惑/冻结按钮的结果文字
static const char *ZombieActionText(bool mind, int count)
{
    std::snprintf(g_zombie_action_msg, sizeof(g_zombie_action_msg), mind ? "已魅惑 %d 只" : "已冻结 %d 只",
                  count);
    return g_zombie_action_msg;
}

// 开关接线: 只在勾选状态变化时写入游戏 (只读模式不写)。
// 无缝传送 / 清除浓雾 / 罐子透视 三项核心层暂未找到同名接口, 先留空。
static void WireToggles()
{
    static bool inited = false;
    static bool pv[6] = {}, zv[6] = {}, ov[6] = {};
    if (!g_attached)
        return;

    // 刚连上游戏 (或游戏重开后又连上): 把缓存值设成"与当前相反", 于是下面每个
    // 开关都会被判定为"变化"并重新写一次 —— 对应 FLTK 版 keep_selected_feature()
    // 启动时把已勾选项目应用一遍的做法。
    if (g_reapply_toggles)
    {
        g_reapply_toggles = false;
        for (int i = 0; i < 6; i++)
        {
            pv[i] = !g_plant_mod[i];
            zv[i] = !g_zombie_mod[i];
            ov[i] = !g_other_mod[i];
        }
        inited = true;
    }
    if (!inited)
    {
        for (int i = 0; i < 6; i++)
        {
            pv[i] = g_plant_mod[i];
            zv[i] = g_zombie_mod[i];
            ov[i] = g_other_mod[i];
        }
        inited = true;
        return;
    }
    for (int i = 0; i < 6; i++)
    {
        if (g_plant_mod[i] != pv[i])
        {
            pv[i] = g_plant_mod[i];
            switch (i)
            {
            case 0: g_pvz.PlantInvincible(pv[0]); break;
            case 1: g_pvz.PlantWeak(pv[1]); break;
            case 2: g_pvz.ReloadInstantly(pv[2]); break;
            case 3: g_pvz.LockButter(pv[3]); break;
            case 4: g_pvz.MushroomsAwake(pv[4]); break;
            case 5: g_pvz.NoCrater(pv[5]); break;
            }
        }
        if (g_zombie_mod[i] != zv[i])
        {
            zv[i] = g_zombie_mod[i];
            switch (i)
            {
            case 0: g_pvz.ZombieInvincible(zv[0]); break;
            case 1: g_pvz.ZombieWeak(zv[1]); break;
            case 2: g_pvz.StopSpawning(zv[2]); break;
            case 3: g_pvz.StopZombies(zv[3]); break;
            case 4:
                g_pvz.NoIceTrail(zv[4]);
                g_zombie_action_msg[0] = 0; // 悬停提示里会显示用的哪种方式
                break;
            case 5: g_pvz.ZombieNotExplode(zv[5]); break;
            }
        }
        if (g_other_mod[i] != ov[i])
        {
            ov[i] = g_other_mod[i];
            switch (i)
            {
            case 0: g_pvz.FreePlanting(ov[0]); break;
            case 1: g_pvz.PlacedAnywhere(ov[1]); break;
            case 2: g_pvz.FastBelt(ov[2]); break;   // 无缝传送 (针对传送带)
            case 3: g_pvz.LockShovel(ov[3]); break;
            case 4: g_pvz.NoFog(ov[4]); break;      // 清除浓雾
            case 5: g_pvz.SeeVase(ov[5]); break;    // 罐子透视
            default: break;
            }
        }
    }
}
// 3x2 选项网格 (宽 3 高 2), 三块修改板块统一使用
// locked != nullptr 时按掩码把对应项置灰 (快捷布阵模式接管中)
static void Grid3x2(const char *id, const char *const labels[6], bool *vals,
                    const bool *locked = nullptr, const char *const *tips = nullptr)
{
    if (ImGui::BeginTable(id, 3, ImGuiTableFlags_SizingStretchSame))
    {
        for (int i = 0; i < 6; i++)
        {
            if (i % 3 == 0)
                ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(i % 3);
            ImGui::PushID(i);
            if (locked != nullptr && locked[i])
                ImGui::BeginDisabled();
            ImGui::Checkbox(labels[i], &vals[i]);
            if (tips != nullptr && tips[i] != nullptr)
                ImGui::SetItemTooltip("%s", tips[i]);
            if (locked != nullptr && locked[i])
                ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// (三个修改板块的状态数组已上移到 WireToggles 之前)


struct SubWindow
{
    GLFWwindow   *win = nullptr;
    ImGuiContext *ctx = nullptr;
    bool          hidden = false;
};
static SubWindow g_sub_speed;
static bool g_want_speed = false; // 请求打开/显示移速窗 (帧末执行)
static SubWindow g_sub_plant;    // 植物攻速独立子窗
static bool g_want_plant = false; // 请求打开/显示植物攻速窗

// "清除"按钮: 按下拉里选的对象清除 (逐条对照 FLTK 版 Toolkit::cb_clear 的 switch)
//   0 植物 -> ClearAllPlants      1 僵尸 -> KillAllZombies
//   2 梯子 -> ClearGridItems{3}   3 墓碑 -> ClearGridItems{1}
//   4 钉耙 -> ClearGridItems{11}
static void ClearGridByCategory(int category)
{
    switch (category)
    {
    case 0: g_pvz.ClearAllPlants(); break;
    case 1: g_pvz.KillAllZombies(); break;
    case 2: g_pvz.ClearGridItems({3}); break;
    case 3: g_pvz.ClearGridItems({1}); break;
    case 4: g_pvz.ClearGridItems({11}); break;
    default: break;
    }
}

static void PageBattle()
{
    // 下拉列表内容 (只构造一次)
    static std::string g_slot_items, g_row_items, g_col_items, g_plant_items, g_zombie_items,
        g_item_items, g_mower_items;
    if (g_slot_items.empty())
    {
        char buf[160];
        for (int i = 0; i < 10; i++)
        {
            std::snprintf(buf, sizeof(buf), "第 %d 格", i + 1);
            g_slot_items += buf;
            g_slot_items.push_back('\0');
        }
        // 行: 所有行 + 第 1~6 行  (索引 0 -> 核心的 -1 = 所有行)
        g_row_items += "所有行";
        g_row_items.push_back('\0');
        for (int i = 1; i <= 6; i++)
        {
            std::snprintf(buf, sizeof(buf), "第 %d 行", i);
            g_row_items += buf;
            g_row_items.push_back('\0');
        }
        // 列: 所有列 + 第 1~10 列  (第 10 列是额外加的位置, 不算进"所有列":
        // 核心"所有列"走的是 0~8 的循环, 只有单独选第 10 列才会用到 col = 9)
        g_col_items += "所有列";
        g_col_items.push_back('\0');
        for (int i = 1; i <= 10; i++)
        {
            std::snprintf(buf, sizeof(buf), "第 %d 列", i);
            g_col_items += buf;
            g_col_items.push_back('\0');
        }
        // 植物列表只到 [47] 玉米加农炮 (与 FLTK 版一致: window.cpp 也是 i < 48)。
        // 名字表 plants_zh[] 只有 48 项 (0~47), 写 49 会越界读到野字符串 —— 界面上那个
        // 莫名其妙的 "Zombies" 就是这么来的, 而它的索引 48 在游戏里是 SEED_IMITATER
        // (模仿者), 直接放置会闪退。
        for (int i = 0; i < 48; i++)
        {
            std::snprintf(buf, sizeof(buf), "[%d] %s", i, Pt::plants_zh[i]);
            g_plant_items += buf;
            g_plant_items.push_back('\0');
        }
        for (int i = 0; i < 33; i++)
        {
            std::snprintf(buf, sizeof(buf), "[%d] %s", i, Pt::zombies_zh[i]);
            g_zombie_items += buf;
            g_zombie_items.push_back('\0');
        }
        // 清除对象 (逐条对照 FLTK 版 choice_item: 植物/僵尸/梯子/墓碑/钉耙)
        static const char *clear_items[] = {"植物", "僵尸", "梯子", "墓碑", "钉耙"};
        for (int i = 0; i < 5; i++)
        {
            g_item_items += clear_items[i];
            g_item_items.push_back('\0');
        }
        // 小推车 (对照 FLTK 版 button_lawn_mower 的三个菜单项)
        static const char *mower_items[] = {"启动", "删除", "恢复"};
        for (int i = 0; i < 3; i++)
        {
            g_mower_items += mower_items[i];
            g_mower_items.push_back('\0');
        }
        g_slot_items.push_back('\0');
        g_row_items.push_back('\0');
        g_col_items.push_back('\0');
        g_plant_items.push_back('\0');
        g_zombie_items.push_back('\0');
        g_item_items.push_back('\0');
        g_mower_items.push_back('\0');
    }

    // 默认停在"墓碑", 与 FLTK 版 choice_item->value(3) 一致
    // 默认停在"第 1 行"/"第 1 列" (列表第 0 项是"所有行"/"所有列", 与 FLTK 版默认一致)
    static int v_slot = 0, v_row = 1, v_col = 1, v_plant = 0, v_zombie = 0, v_item = 3, v_mower = 0;
    static bool v_imitater_card = false, v_imitater_place = false;

    // ---- 修改选卡 ----
    //   ★ 高度沿革 (每次动那两行诊断都要重新量):
    //     92 → 116 (v27: 加了一行**常驻**冷却诊断) → 132 (v29: 又加一行常驻"数字几何")
    //     → **76 (v32: 两行常驻诊断都挪进 PZTK_DEV=1 ⇒ 内容只剩"1 行下拉 + 1 行开关/状态")**
    //     → **80 (v33: 用户要求本块 +4px; 底部余量从 ~10px@100% 放到 ~14px@100%)**
    //   ★ v33 的总账 (三个数必须一起改, 否则会出滚动条或被裁切):
    //     卡槽 76 → **80** (+4); 「其他修改」是**自动填满剩余高度**的块 (`avail - kTrailingH`),
    //     卡槽 +4 会让它 −4 ⇒ 必须把 `kTrailingH` 减小让它**净 +4**。★ 减多少**不是推的**:
    //     按照实测 (`avail = 175.5 @150%`, 见下面「其他修改」那段的三步量法),
    //     `kTrailingH` 要用 **33** (@100%) —— 原来的"−8"(→50) 因为**地板 80*dpi** 而完全无效;
    //     末尾两个按钮因此下移 6px (@150%), 仍然完整、整页仍然填满、无滚动条 (截图实测)。
    //   ★ 为什么是 76 (实测, 不是估的): 132 时用截图像素量出本块外框 y=93..227 (135px @150%),
    //     内容(勾选框那一行)末行墨迹到 y≈195 ⇒ 底部空 ~31px@150% ≈ **21px@100%**;
    //     去掉"两行诊断"占的高度、再留约半行的合理余量 ⇒ **76**。
    //     改完复量: 底部空 ~10px@100% (约半行, 不是"一大块"), **无滚动条**
    //     —— 块右侧 x=675 那条竖线是**整页**滚动条, 块自己那段 (x=650..674) 全是干净的块底色。
    //   ★ PZTK_DEV=1 启动时本块还会把精确数字写进 `pztk_dev_layout.txt` (当前工作目录),
    //     以后调高度照它回读; 正式发布前要重编一次**不含该探针**的 exe (见下方探针注释)。
    //   ★★ 2026-10-14: 80 → **104** (+24) —— 本块新增第 3 行「取消必选卡片」+ 状态。
    //     为什么 +24 而不是推的: 实测本块每行("第 1 格"那一行 / "卡片冷却时间"那一行)
    //     的行高 + 行距 = 24px @100%, 而 80 时底部只剩约 10px 余量 ⇒ 不加高就会把
    //     新行裁掉。**整整 +24 的依据 = 探针实测的"内容高/余量"** (见下面 PZTK_LAYOUT_PROBE
    //     那段: 加行后余量仍是"约半行"这个量级, 既没被裁也没多留一大块空白)。
    //   ★ 为什么不怕"整页撑出滚动条": 「其他修改」是**自动填满剩余高度**的块
    //     (`avail - kTrailingH`), 本块长高多少它就被挤掉多少, 直到它自己的地板
    //     `80 * g_dpi`; 本块 +24 只会把它的 126@150% 压到地板 120@150%, 净增
    //     ~30px@150% —— 页尾原本留的 ~31px@150% 正好吞掉它 (截图实测无整页滚动条)。
    BeginBlock("卡槽", 104 * g_dpi);
    ImGui::SetNextItemWidth(96 * g_dpi);
    ImGui::Combo("##slot", &v_slot, g_slot_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::SetNextItemWidth(-166 * g_dpi);
    ImGui::Combo("##seed", &v_plant, g_plant_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("换卡片", ImVec2(72 * g_dpi, 0)) && g_attached) g_pvz.SetSlotSeed(v_slot, v_plant, v_imitater_card);
    if (ImGui::IsItemHovered())
    {
        const std::string last = g_pvz.SlotSeedMsg();
        const std::string tail = last.empty() ? std::string() : ("最近一次: " + last);
        ImGui::SetItemTooltip(
            "把这一格换成你选的植物 (勾「模仿者」= 换成它的模仿者版本)。\n"
            "两条时机都有效:\n"
            "  战斗中点: 立刻写进卡槽;\n"
            "  选卡界面点: 先记下来, 等战斗正式开始那一刻再写进去\n"
            "    (只写你点过的格, 不会覆盖你在选卡界面自己选的卡; 每关只写一次)。\n"
            "%s",
            tail.c_str());
    }
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::Checkbox("模仿者", &v_imitater_card);

    // 卡片冷却时间 (v37: 多选一; v40: 界面只留**两项** —— `关闭` / `游戏字体(新)`)。
    //   由 DLL 画在**游戏自己那一帧**里, 钩点取在卡槽那条渲染 case 的
    //   `call Board::DrawUIBottom` (0x4172E7), 画在原调用**之后** ⇒ 数字盖在卡面上。
    //   ★★ v40: `矩形(旧)` 那一项按用户要求**从界面藏起来** (`cd_font.h` / 预算阶梯 /
    //     灰度覆盖度那套全部保留, 发布源码后对其他人有用) —— 仍可用环境变量
    //     `PZTK_CD_MODE=1` 启用, 设了它界面按既有做法置灰。
    //   ★ 界面文案**必须短**: 这一行的可用宽度就是本块宽度, 两个单选钮本身已经占了大部分;
    //     完整解释与诊断都挂在 tooltip 上 (普通界面要保持干净 —— 用户 v32 的要求)。
    EnsureCdModeInit();
    ImGui::TextUnformatted("卡片冷却时间");
    ImGui::SameLine(0.0f, 6.0f);
    //   ★ 环境变量覆盖时置灰 (点不动 = 一眼看出"不是界面在控制")
    ImGui::BeginDisabled(g_cd_mode_forced);
    if (ImGui::RadioButton("关闭##cdmode", g_cd_mode == static_cast<int>(PtHook::CDMODE_OFF)))
        g_cd_mode = static_cast<int>(PtHook::CDMODE_OFF);
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::RadioButton("开启##cdmode",
                           g_cd_mode == static_cast<int>(PtHook::CDMODE_NATIVE)))
        g_cd_mode = static_cast<int>(PtHook::CDMODE_NATIVE);
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 6.0f);
    //   ★ v32: 这里只保留**最小状态** (几个字), 与「僵尸修改」块里那几行同一套措辞。
    //     `cd_hook_ok` 只是"补丁已打上"的**闩锁**, **不**代表这一帧真的画了东西 ⇒
    //     细节全部交给 PZTK_DEV=1 的诊断行 (模式 / 本帧画了几张 / 跳过原因 / 耗时)。
    if (g_cd_mode == static_cast<int>(PtHook::CDMODE_OFF))
        ImGui::TextDisabled("关");
    else if (g_pvz.CooldownHookOk())
        ImGui::TextColored(kOk, "已生效");
    else if (g_attached)
        ImGui::TextColored(kWarn, "未生效");
    else
        ImGui::TextDisabled("待注入");
    if (ImGui::IsItemHovered())
        ImGui::SetItemTooltip(
            "正在冷却的种子卡上显示剩余秒数 (一位小数, 如 2.4); 几张卡一起转时一眼看出哪张先好。\n"
            "只在冷却中显示, 就绪时不显示; 数字用游戏自己的绘制画在卡面上。\n"
            "\n"
            "两种方式 (可运行时切):\n"
            "  关闭: 什么都不画;\n"
            "  开启: 直接调游戏自己的字体绘制 (真抗锯齿 / 真混合); 任何自检不过会自动降级\n"
            "    回矩形方式 (它已从界面隐藏), 原因见 PZTK_DEV=1 的诊断行。\n"
            "环境变量 PZTK_CD_MODE=0|1|2 可覆盖这里的选项 (启动工具前设; =1 = 矩形方式)。\n"
            "数字画在卡面标题条以下、阳光费用以上那块可画区里居中 (含 2px 目测校准)。\n"
            "%s",
            DevMode() ? ("\n诊断: " + g_pvz.CooldownDrawStatus()) : std::string());
    // ★★ v32: 原来的**两行常驻诊断**挪进 PZTK_DEV=1 ★★
    //   为什么挪 (用户要求): 普通用户界面要**干净** —— 「卡槽」块里不再常驻
    //     `工具=开 模式=... | DLL=开 模式=... | 画了 N 张 | ...` 与
    //     `游戏字体: 本帧 N 张 | 文字 WxH | 耗时 ...` / `字模: 单字 10x14 ...`
    //     这两行; 排查时设 `PZTK_DEV=1` 启动, 两行原样回来 (生产/消费代码一个字段都没删,
    //     见 pvz.cpp 的 CooldownDrawBrief / CooldownDrawGeom 与 hook_ctl.h 里的探针)。
    //   ★ v37: 两行现在都能打出**当前模式**与**两种方式的耗时(µs/帧)**, 这就是用户
    //     对比"质量与开销"要的数据 (模式本身在单选钮上, 诊断行里再打一遍是为了"看到的
    //     与 DLL 回读的一致")。
    if (DevMode() && g_attached && g_cd_mode != static_cast<int>(PtHook::CDMODE_OFF))
    {
        const std::string cd_brief = g_pvz.CooldownDrawBrief();
        ImGui::TextColored(kNote, "%s", cd_brief.c_str());
        // 第二行 = "数字到底多大 + 这一帧下了多少矩形" (v31 的档位也在这里):
        //   v32 = `单字 10x14 1x1逐像素`, 完整解释见 tooltip 的第三、四行。
        const std::string cd_geom = g_pvz.CooldownDrawGeom();
        ImGui::TextColored(kNote, "%s", cd_geom.c_str());
    }

    // ---- 第 3 行: 取消必选卡片 (冒险模式**二周目** Crazy Dave 预选) —— 2026-10-14 ----
    //   用户要的行为 (两套机制, 可同时启用, 勾上就生效):
    //     * 在**选卡界面**勾上 ⇒ 那 3 张**立刻**被放开, 能点下来 (机制 B: 清 mCrazyDavePicked);
    //     * 在**别处**勾上 ⇒ 进关卡时**根本不抽卡** (机制 A: 把 `jle` 改成 `jmp` 的字节补丁);
    //     * 本版不支持 (签名没命中 / 自检不过) ⇒ **一个字节都不改**, 界面如实报"未生效"。
    //   状态行只留最小信息 (普通界面要干净), 完整解释在 tooltip, 证据在 PZTK_DEV=1 的 DEV 行。
    ImGui::Checkbox("取消必选卡片", &g_no_dave_pick);
    if (ImGui::IsItemHovered())
        ImGui::SetItemTooltip(
            "冒险模式二周目里, Crazy Dave 会先替你在选卡界面选好 3 张卡; 那 3 张点不动。\n"
            "勾上它 = 取消这个预选:\n"
            "  在选卡界面勾: 立刻可以点掉 / 换掉 (清掉游戏里的\"戴夫已选\"标记);\n"
            "  在别处勾: 进关卡时根本不抽卡 (把\"二周目就抽卡\"那处跳转改成无条件跳过,\n"
            "    进关卡前改、可逆, 退出还原)。\n"
            "只改游戏进程内存, 不动存档、不改游戏文件。右边状态:\n"
            "  已生效 = 已确认生效;  等待选卡界面 = 进选卡界面时再放开;\n"
            "  未生效(不支持此版本) = 版本对不上, 一个字节都不改。");
    ImGui::SameLine(0.0f, 8.0f);
    //   状态文字的三个分支各一种颜色 (绿 = 生效 / 橙 = 不支持 / 灰 = 等界面), 与「僵尸修改」块同套措辞。
    if (!g_attached)
        ImGui::TextDisabled("未连接游戏");
    else if (g_pvz.DavePickPatched())
        ImGui::TextColored(kOk, "%s", g_pvz.DavePickMsg().c_str());
    else if (g_pvz.DavePickMsg() == "未生效(不支持此版本)")
        ImGui::TextColored(kWarn, "%s", g_pvz.DavePickMsg().c_str());
    else
        ImGui::TextDisabled("%s", g_pvz.DavePickMsg().c_str());
    //   ★ DEV 行 (只有 PZTK_DEV=1): 签名是否命中 / 补丁地址 / call 目标 / B 是否已清 / 清了几张。
    //     放在**同一个** `if` 里, 与"卡片冷却时间"那两行同一套做法 (普通界面一个字都不多)。
    if (DevMode() && g_attached)
    {
        std::string dp_dev = g_pvz.DavePickDevLine();
        if (g_pvz.PendingSlotSeedCount() > 0)
        {
            char tmp[96];
            std::snprintf(tmp, sizeof(tmp), " | 换卡片台账 %d 格", g_pvz.PendingSlotSeedCount());
            dp_dev += tmp;
        }
        ImGui::TextColored(kNote, "%s", dp_dev.c_str());
    }
    // ★ v32 版面探针 —— **只在开发构建**里存在 (`-DPZTK_LAYOUT_PROBE=1`) ★★
    //   为什么用编译开关而不是 DevMode(): 探针要写文件, 而"正式 exe 里绝对不能有
    //   随手写文件的代码" —— 编译开关能保证**成品里根本没有这段代码**
    //   (判据: 成品 exe 里搜不到 `pztk_dev_layout.txt`, v32 就是这么核对的)。
    //   怎么用:
    //     set PZTK_LAYOUT_PROBE=1 & env\scripts\build_imgui_demo.cmd pztk_probe.exe
    //     (build_imgui_demo.cmd 会把该环境变量翻成 -DPZTK_LAYOUT_PROBE=1)
    //     → 设 PZTK_DEV=1 启动 → 当前工作目录下出现 `pztk_dev_layout.txt`,
    //       里面是"块高 / 内容高 / 余量"三个数 (余量 = 内容画完后还剩多少可用高度;
    //       ≈0 或略负 = 刚好放得下, 十几 px = 底部留了一大块空白)。
    //   ★ 为什么**不画在界面上**: 画出来会自己占一行, 量到的就不是"只剩那几行内容"时的真值。
#ifdef PZTK_LAYOUT_PROBE
    {
        char devbuf[192];
        std::snprintf(devbuf, sizeof(devbuf),
                      "[PZTK][DEV] dev=%d attached=%d cd_on=%d 卡槽: 块高 %.0f 内容高 %.0f 余量 %.1f\n",
                      DevMode() ? 1 : 0, g_attached ? 1 : 0, g_cd_mode,
                      ImGui::GetWindowSize().y, ImGui::GetCursorPosY() + 4 * g_dpi,
                      ImGui::GetContentRegionAvail().y);
        OutputDebugStringA(devbuf);
        HANDLE f = CreateFileA("pztk_dev_layout.txt", GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE)
        {
            DWORD wrote = 0;
            WriteFile(f, devbuf, static_cast<DWORD>(std::strlen(devbuf)), &wrote, nullptr);
            CloseHandle(f);
        }
    }
#endif
    EndBlock();

    // ---- 放置与清除 (版式按用户要求: 第3行=冒墓碑/搭梯子/放钉耙, 第4行=清除, 第5行=小推车) ----
    BeginBlock("放置与清除", 158 * g_dpi);
    // 第 1 行: 行 + 植物 + 种植物 + 模仿者
    // 注意: 下拉第 0 项是"所有行"/"所有列", 要减 1 传给核心 (-1 = 全部)
    ImGui::SetNextItemWidth(96 * g_dpi);
    ImGui::Combo("##row", &v_row, g_row_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::SetNextItemWidth(-150 * g_dpi);
    ImGui::Combo("##p2", &v_plant, g_plant_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("种植物", ImVec2(68 * g_dpi, 0)) && g_attached) g_pvz.PutPlant(v_row - 1, v_col - 1, v_plant, v_imitater_place);
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::Checkbox("模仿者##2", &v_imitater_place);

    // 第 2 行: 列 + 僵尸 + 出僵尸 (僵尸下拉与第 1 行的植物下拉同宽并右对齐)
    ImGui::SetNextItemWidth(96 * g_dpi);
    ImGui::Combo("##col", &v_col, g_col_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::SetNextItemWidth(-150 * g_dpi);
    ImGui::Combo("##z1", &v_zombie, g_zombie_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("出僵尸", ImVec2(68 * g_dpi, 0)) && g_attached) g_pvz.PutZombie(v_row - 1, v_col - 1, v_zombie);

    // 第 3 行: 场地物品 (冒墓碑 / 搭梯子 / 放钉耙)
    if (ImGui::Button("冒墓碑", ImVec2(96 * g_dpi, 0)) && g_attached) g_pvz.PutGrave(v_row - 1, v_col - 1);
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("搭梯子", ImVec2(96 * g_dpi, 0)) && g_attached) g_pvz.PutLadder(v_row - 1, v_col - 1);
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("放钉耙", ImVec2(96 * g_dpi, 0)) && g_attached) g_pvz.PutRake(v_row - 1, v_col - 1);

    // 第 4 行: 清除 (先选对象再点"清除", 与 FLTK 版 cb_clear 的 switch 逐条对应)
    ImGui::SetNextItemWidth(140 * g_dpi);
    ImGui::Combo("##item", &v_item, g_item_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("清除", ImVec2(72 * g_dpi, 0)) && g_attached)
        ClearGridByCategory(v_item);

    // 第 5 行: 小推车 (启动 / 删除 / 恢复; FLTK 版是菜单按钮, 这里用下拉 + 按钮)
    ImGui::SetNextItemWidth(140 * g_dpi);
    ImGui::Combo("##mower", &v_mower, g_mower_items.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("小推车", ImVec2(72 * g_dpi, 0)) && g_attached)
        g_pvz.SetLawnMowers(v_mower);
    EndBlock();

    // ---- 植物修改 3x2 ----
    static const char *plant_labels[6] = {"植物无敌", "植物脆弱", "立刻装填",
                                          "只投黄油", "蘑菇睡醒", "核弹无坑"};
    BeginBlock("植物修改", 88 * g_dpi);
    Grid3x2("plant_mod", plant_labels, g_plant_mod, g_lineup_mode ? kLmLockPlant : nullptr);
    EndBlock();

    // ---- 僵尸修改 (第1-2行 5 个开关, 第3行 暂停刷怪+无视进家, 第4行 魅惑/冻结, 第5行 女仆秘籍) ----
    // 勾选状态仍用原来的下标: "暂停刷怪" 留在 g_zombie_mod[2] (快捷布阵模式要接管它),
    // 只是从网格里挪到第 3 行; 网格里放其余 5 项。
    // ★★ v40: 原来还有"第 6 行: 碰撞箱开关/状态" —— 整条功能已删除 ⇒ 本块高度 196 → **166**
    //   (−30 = 一行按钮/文字的高度, 与 v37 加那一行时的 +30 同一个数) ⇒ 内容刚好放得下,
    //   无滚动条、无裁切 (由 `verify_imgui.ps1 -Page 0` 的截图确认)。
    static const char *zombie_labels[5] = {"僵尸无敌", "僵尸虚弱", "停滞不前", "冰车无痕", "丑椒不爆"};
    static const int zombie_grid_idx[5] = {0, 1, 3, 4, 5};
    // 冰车无痕的悬停说明 (显示当前用的哪种方式: 真无痕 / 退回旧补丁)
    static char ice_tip_buf[192] = {};
    {
        const std::string &m = g_pvz.IceTrailMsg();
        if (!m.empty())
            std::snprintf(ice_tip_buf, sizeof(ice_tip_buf),
                          "冰车无痕: %s\n真无痕 = 让游戏自己根本不产生冰痕 (冰痕会妨碍种植)", m.c_str());
        else
            std::snprintf(ice_tip_buf, sizeof(ice_tip_buf),
                          "真无痕: 让游戏自己根本不产生冰痕 (冰痕会妨碍种植)");
    }
    static const char *zombie_tips[5] = {nullptr, nullptr, nullptr, ice_tip_buf, nullptr};
    // ★ 高度里已经把"卡片冷却时间"那一行算进去了 (原 196 -> 226): 那一行也在本块的滚动区里,
    //   少给 30px 就会被裁在块底边 (实际就是第一版的样子 —— 能滚到, 但看着像没做完)。
    //   ★ v40: 碰撞箱那一行删掉 ⇒ 196 - 30 = **166** (见上面那段注释)。
    BeginBlock("僵尸修改", 166 * g_dpi);
    if (ImGui::BeginTable("zombie_mod", 3, ImGuiTableFlags_SizingStretchSame))
    {
        for (int k = 0; k < 5; k++)
        {
            if (k % 3 == 0)
                ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(k % 3);
            ImGui::PushID(k);
            const int i = zombie_grid_idx[k];
            if (g_lineup_mode && kLmLockZombie[i])
                ImGui::BeginDisabled();
            ImGui::Checkbox(zombie_labels[k], &g_zombie_mod[i]);
            if (zombie_tips[k] != nullptr)
                ImGui::SetItemTooltip("%s", zombie_tips[k]);
            if (g_lineup_mode && kLmLockZombie[i])
                ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // 第 3 行: 暂停刷怪 + 无视僵尸进家
    if (g_lineup_mode && kLmLockZombie[2])
        ImGui::BeginDisabled();
    ImGui::Checkbox("暂停刷怪", &g_zombie_mod[2]);
    if (g_lineup_mode && kLmLockZombie[2])
        ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::Checkbox("无视僵尸进家", &g_kill_home);
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::TextDisabled("(战至最后一刻，自刎归天！)");

    // 第 4 行: 魅惑 / 冻结 (按一下只对"当前场上"的僵尸生效一次)
    if (ImGui::Button("魅惑当前僵尸", ImVec2(140 * g_dpi, 0)))
        ZombieActionText(true, g_pvz.ZombieMindControlNow());
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button("冻结当前僵尸", ImVec2(140 * g_dpi, 0)))
        ZombieActionText(false, g_pvz.ZombieFreezeNow());
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::TextDisabled("%s", g_zombie_action_msg);

    // 第 5 行: 女仆秘籍 + 下拉 (用暂停控制舞王相位)
    static const char *kMaidModes[4] = {"关闭", "保持前进(舞王一直前进)",
                                        "跳舞(舞王不前进且不会召唤舞伴)",
                                        "召唤伴舞(舞王不前进且一直尝试召唤舞伴)"};
    if (ImGui::Button("女仆秘籍", ImVec2(96 * g_dpi, 0)))
        ImGui::OpenPopup("maid_menu");
    if (ImGui::BeginPopup("maid_menu"))
    {
        for (int i = 0; i < 4; i++)
        {
            if (ImGui::Selectable(kMaidModes[i], g_maid_mode == i))
            {
                g_maid_mode = i;
                // 优先走 AvZ 那种"代码补丁"模式 (相位恒为常数, 且不冻结时钟);
                // 扫不到补丁点就自动退回"钉时钟"模式 (功能不受影响, 只是植物浮动动画会停)
                g_maid_patch_msg = (g_pvz.ApplyMaidPatch(i) == 0)
                                       ? (i == 0 ? "已关闭" : "已开启")
                                       : (i == 0 ? "已关闭" : "已开启");
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::TextDisabled("%s", kMaidModes[g_maid_mode]);
    if (!g_maid_patch_msg.empty())
    {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::TextDisabled("[%s]", g_maid_patch_msg.c_str());
    }

    // 冰车无痕当前用的方式 —— 直接显示出来 (不用悬停去找)
    if (g_zombie_mod[4])
        ImGui::TextDisabled("冰车无痕: %s", g_pvz.IceTrailMsg().c_str());
    EndBlock();

    // ---- 其他修改 3x2 ----
    static const char *other_labels[6] = {"免费用卡", "随意放置", "无缝传送",
                                          "连续铲子", "清除浓雾", "罐子透视"};
    // 其他修改: **自动填满剩余高度** —— 这样整页永远不会有滚动条。
    //   末尾还有两个按钮, 先把它们的高度扣掉 (卡片冷却时间的开关已挪到「卡槽」块)。
    //   ★★ v33: `kTrailingH` = 58 → **33** (不是"约 −8") —— 这个数**是量出来的**, 不是推的 ★★
    //     ① 实测 (改前, 76/58 的 v32): 本块**顶框 y=897、底框 y=1016**(@150%) ⇒ 高 **120**
    //        —— 正好等于**地板** `80 * dpi` (= 120) ⇒ 说明 `other_h` **小于地板**, 高度被
    //        `(other_h > 80*dpi) ? other_h : 80*dpi` 钳在地板上;
    //     ② 所以"−8"**一点用都没有**: `other_h = avail - kTrailingH`, 只要 `other_h` 还在
    //        地板以下, 本块高度就**纹丝不动** (实测: 58 → 50 时块高仍是 120, 只有跟着
    //        「卡槽」+4 往下挪了 6px)。要让它**真的 +4px**, 必须让 `other_h > 地板`;
    //     ③ 量 `avail` 的办法 (可复现): 把 `kTrailingH` 临时改成 `5 * dpi` 重编一版, 截图量到
    //        本块 = **168** (@150%) ⇒ `avail = 168 + 7.5 = **175.5**` (@150%, 该窗口 712x1136);
    //     ④ 目标 = 本块 **126** (= 84 @100%, 比地板的 120 高 6 = +4 @100%) ⇒
    //        `kTrailingH = 175.5 - 126 = 49.5` (@150%) = **33 @100%**;
    //     ⑤ 校验页尾放得下: 本块 126 + 与按钮的间距 15 + 按钮高 31 = **172 <= 175.5** ✓
    //        (余量 3.5px @150%) ⇒ 仍然填满、**没有滚动条**、按钮完整; 而且本块高过内容
    //        (标题 + 2 行勾选 ≈ 123) ⇒ 它自己那条**内部滚动条也消失** (与「卡槽」同一效果)。
    //     ★ 为什么不做"把地板从 80 提到 84": 那样**小窗口**下也会强制留 84px, 更容易撑出
    //       滚动条; 改 `kTrailingH` 只影响"还剩多少高度"这一项, 小窗口下 `other_h` 掉到地板
    //       以下时行为与原来完全一致 (地板仍是 80)。
    {
        const float kTrailingH = 33 * g_dpi; // 末尾两个按钮 + 间距 (实测: 见上面 ③④)
        const float other_h = ImGui::GetContentRegionAvail().y - kTrailingH;
        BeginBlock("其他修改", (other_h > 80 * g_dpi) ? other_h : 80 * g_dpi);
    }
    Grid3x2("other_mod", other_labels, g_other_mod, g_lineup_mode ? kLmLockOther : nullptr);
    EndBlock();

    // ---- 随机数值入口 ----
    ImGui::Spacing();
    if (ImGui::Button("僵尸移速 ...", ImVec2(150 * g_dpi, 0)))
            g_want_speed = true; // 帧末处理: 不存在则创建, 已存在则显示
    ImGui::SameLine();
    if (ImGui::Button("植物攻速 ...", ImVec2(150 * g_dpi, 0))) g_want_plant = true;
}

static void PagePlaceholder(const char *name)
{
    BeginBlock(name, ImGui::GetContentRegionAvail().y);
    ImGui::TextDisabled("(待实现)");
    EndBlock();
}

// ---------------------------------------------------------------------------
// 僵尸移速窗 (三区独立滚动)
//   左上: 固定模块 (勾选 | 名称 | 区间 | 数值)   —— 独立滚动
//   左下: 固定值与说明 (小字号, 不滚动)
//   右侧: 实时监测器 (编号 | 种类 | 速度)        —— 独立滚动, 按个体列出
//
// 逻辑说明: g_zval / g_zen 已经在主循环里交给 FixRandomValues 了, 所以这个
// 窗口只要改这两个数组, 游戏就会被写入 —— 不需要额外接线。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// 独立子窗 (GLFW 真多窗口) —— 重写版
//
// 生命周期要点 (踩过的坑都体现在这里):
//   1. 窗口与 ImGui 上下文只创建一次, 关闭时只隐藏, 绝不销毁
//      (销毁共享 GL 上下文的窗口会让主窗卡死并崩溃)
//   2. 帧中途绝不能切换 ImGui 上下文; 创建也必须在帧与帧之间进行
//   3. 每帧顺序: 主窗完整走完 NewFrame..SwapBuffers -> 再逐个渲染子窗
//      -> 切回主窗时不仅要 MakeCurrent, 还要重新设置视口
//   4. 最小化时帧缓冲为 0x0, 必须跳过, 否则会干扰别的窗口
// ---------------------------------------------------------------------------


// 统一的小工具: 让某个子窗成为当前渲染目标
static void FocusSub(SubWindow &sw)
{
    glfwMakeContextCurrent(sw.win);
    ImGui::SetCurrentContext(sw.ctx);
}

// 子窗的字体与风格 (每个 ImGui 上下文各一次)
static ImFont *s_font_small_of_context = nullptr; // 当前上下文的说明区小字体
static void SetupContextStyle(float dpi)
{
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.FontGlobalScale = 0.95f;

    ImFontConfig fcfg;
    fcfg.OversampleH = 1;
    fcfg.OversampleV = 1;
    fcfg.PixelSnapH = true;

    // 小号字 (说明区用), 存起来给绘制时切换
    s_font_small_of_context = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc",
                                                          15.0f * dpi, &fcfg, GlyphRangesWithLineups()); // 说明区字号
    // 板块标题的粗体 (与正文同字号, 只换字重 —— 新版界面的层次就靠它 + 蓝色竖条)
    s_font_title_of_context = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyhbd.ttc",
                                                          16.0f * dpi, &fcfg, GlyphRangesWithLineups());
    ImFont *f = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f * dpi, &fcfg,
                                             GlyphRangesWithLineups());
    if (f == nullptr)
        f = io.Fonts->AddFontDefault();
    io.Fonts->Build();
    io.FontDefault = f;
    ApplyStyle(dpi);
}

static void SubCb_MousePos(GLFWwindow *w, double x, double y)
{
    SubWindow *sw = (SubWindow *)glfwGetWindowUserPointer(w);
    if (sw == nullptr || sw->ctx == nullptr) return;
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(sw->ctx);
    ImGui_ImplGlfw_CursorPosCallback(w, x, y);
    ImGui::SetCurrentContext(prev);
}
static void SubCb_MouseBtn(GLFWwindow *w, int b, int a, int m)
{
    SubWindow *sw = (SubWindow *)glfwGetWindowUserPointer(w);
    if (sw == nullptr || sw->ctx == nullptr) return;
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(sw->ctx);
    ImGui_ImplGlfw_MouseButtonCallback(w, b, a, m);
    ImGui::SetCurrentContext(prev);
}
static void SubCb_Scroll(GLFWwindow *w, double dx, double dy)
{
    SubWindow *sw = (SubWindow *)glfwGetWindowUserPointer(w);
    if (sw == nullptr || sw->ctx == nullptr) return;
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(sw->ctx);
    ImGui_ImplGlfw_ScrollCallback(w, dx, dy);
    ImGui::SetCurrentContext(prev);
}
static void SubCb_Key(GLFWwindow *w, int k, int s, int a, int m)
{
    SubWindow *sw = (SubWindow *)glfwGetWindowUserPointer(w);
    if (sw == nullptr || sw->ctx == nullptr) return;
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(sw->ctx);
    ImGui_ImplGlfw_KeyCallback(w, k, s, a, m);
    ImGui::SetCurrentContext(prev);
}
// 创建子窗 (只在帧与帧之间调用!)
static bool CreateSubWindow(SubWindow &sw, GLFWwindow *share, const char *title, int w, int h)
{
    if (sw.win != nullptr)
        return true;

    // ★ 显式复位并声明本窗口要的样式: GLFW 的 hint 是全局状态, 别处 (例如碰撞箱叠加层)
    //   设过的 hint 会泄漏过来 -> 子窗口变成无边框/不可拖动 (踩过)。这里自己声明, 谁都影响不了它。
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // 共享主窗的 GL 资源, 否则字体纹理会有两份并互相干扰 (主窗镜像)
    sw.win = glfwCreateWindow(w, h, title, nullptr, share);
    glfwDefaultWindowHints();
    if (sw.win == nullptr)
        return false;

    sw.ctx = ImGui::CreateContext();
    glfwMakeContextCurrent(sw.win);
    ImGui::SetCurrentContext(sw.ctx);
    SetupContextStyle(g_dpi);
    glfwSwapInterval(1);
    // 不自动装 ImGui 回调 (它们用"当前上下文" -> 会串到主窗), 改为自己装
    ImGui_ImplGlfw_InitForOpenGL(sw.win, false);
    glfwSetWindowUserPointer(sw.win, &sw);
    glfwSetCursorPosCallback(sw.win, SubCb_MousePos);
    glfwSetMouseButtonCallback(sw.win, SubCb_MouseBtn);
    glfwSetScrollCallback(sw.win, SubCb_Scroll);
    glfwSetKeyCallback(sw.win, SubCb_Key);
    ImGui_ImplOpenGL3_Init("#version 130");
    sw.hidden = false;
    return true;
}

// 显示已存在的子窗
static void ShowSubWindow(SubWindow &sw)
{
    if (sw.win == nullptr)
        return;
    glfwShowWindow(sw.win);
    sw.hidden = false;
}

// 渲染一个子窗 (必须在主窗 SwapBuffers 之后调用)
static void RenderSubWindow(SubWindow &sw, void (*draw)())
{
    if (sw.win == nullptr || sw.hidden)
        return;

    // 关窗请求: 只隐藏, 不销毁
    if (glfwWindowShouldClose(sw.win))
    {
        glfwSetWindowShouldClose(sw.win, 0);
        glfwHideWindow(sw.win);
        sw.hidden = true;
        return;
    }

    // 最小化时帧缓冲为 0x0 -> 跳过
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(sw.win, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0)
        return;

    FocusSub(sw);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("subwin", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse);
    draw();
    ImGui::End();

    ImGui::Render();
    glViewport(0, 0, fbw, fbh);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // 浅色主题: 子窗同底色
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(sw.win);
}

// ---------------------------------------------------------------------------
// 僵尸移速窗的内容
// ---------------------------------------------------------------------------
// 可调整的分类数。**不要写成 ZSC_COUNT - N 这种硬编码** —— 表尾的"固定值"分类
// 数量会变 (例如 2026-10-06 加了"潜水（游泳）"就从 5 个变成 6 个), 直接用枚举里的
// ZSC_ADJUSTABLE_COUNT 才对得上。
static const int kAdjustableSpeedClasses = Pt::ZSC_ADJUSTABLE_COUNT;

// 每个僵尸分类包含哪些僵尸 (数据驱动: 按 ZombieSpeedClassOf 反查, 不手写)
static char g_members_buf[Pt::ZSC_COUNT][768];
static bool g_members_built = false;
static void DrawSpeedContent()
{
    const float left_w  = 402 * g_dpi; // 留出竖向滚动条的宽度, 否则 ± 按钮会被挤掉
    const float notes_h = 324 * g_dpi; // 固定值有 6 条, 留够高度免得第一条被滚出去
    const ImVec2 avail  = ImGui::GetContentRegionAvail();
    const float start_x = ImGui::GetCursorPosX();
    const float start_y = ImGui::GetCursorPosY();
    const float fix_h   = avail.y - notes_h - 6 * g_dpi - 34 * g_dpi; // 34 = 底部"恢复默认值"按钮那一行

    // ---- 左上: 只列可调整的分类 ----
    ImGui::BeginChild("speed_fix", ImVec2(left_w, fix_h), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("speed_list", 4,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("##c", ImGuiTableColumnFlags_WidthFixed, 20 * g_dpi);
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 150 * g_dpi);
        ImGui::TableSetupColumn("区间", ImGuiTableColumnFlags_WidthFixed, 82 * g_dpi);
        ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthFixed, 104 * g_dpi);
        for (int i = 0; i < kAdjustableSpeedClasses; i++)
        {
            ImGui::TableNextRow(0, 23 * g_dpi);
            ImGui::PushID(i);
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("##en", &g_zen[i]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(Pt::zombie_speed_classes[i].name_zh);
            // 与 FLTK 老版一致: 这两类额外说明"含哪些僵尸"
            {
                const char *members = nullptr;
                if (i == Pt::ZSC_NORMAL)
                    members = "含: 普僵 / 路障 / 铁桶 / 铁门 / 鸭子 / 豌豆头 / 机枪头 / "
                              "辣椒头 / 坚果头 / 高坚果头";
                else if (i == Pt::ZSC_GARGANTUAR)
                    members = "含: 白眼巨人 / 红眼巨人";
                if (members != nullptr)
                    ImGui::SetItemTooltip("%s\n%s\n游戏原值 %.2f\n静态区间 %.2f ~ %.2f",
                                          Pt::zombie_speed_classes[i].name_zh, members,
                                          Pt::zombie_speed_classes[i].default_value,
                                          Pt::zombie_speed_classes[i].min,
                                          Pt::zombie_speed_classes[i].max);
                else
                    ImGui::SetItemTooltip("%s\n游戏原值 %.2f\n静态区间 %.2f ~ %.2f",
                                          Pt::zombie_speed_classes[i].name_zh,
                                          Pt::zombie_speed_classes[i].default_value,
                                          Pt::zombie_speed_classes[i].min,
                                          Pt::zombie_speed_classes[i].max);
            }
            ImGui::TableSetColumnIndex(2);
            // ★ 实测区间只能"放宽", 绝不能"收窄" —— 与 FLTK 老版一致:
            //     range_min[i] = info.min; if (obs_min < range_min) range_min = obs_min;
            //     range_max[i] = info.max; if (obs_max > range_max) range_max = obs_max;
            //   以前这里是直接覆盖 ✗: 场上只出现 0.30 的普僵时, 输入框会被夹在 0.30~0.30,
            //   合法的 0.23 / 0.37 反而打不进去 (用户反馈的"逻辑问题")。
            float rmin = Pt::zombie_speed_classes[i].min;
            float rmax = Pt::zombie_speed_classes[i].max;
            float obs_min = 0.0f, obs_max = 0.0f;
            if (g_pvz.ObservedSpeedRange(i, obs_min, obs_max))
            {
                if (obs_min < rmin)
                    rmin = obs_min;
                if (obs_max > rmax)
                    rmax = obs_max;
            }
            ImGui::TextColored(kInkDim, "%.2f~%.2f", rmin, rmax);
            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(98 * g_dpi);
            // 用 InputFloat 而不是 DragFloat: FLTK 版这里是 Fl_Value_Input, 可以直接
            // 打字; DragFloat 只能拖 (要 Ctrl+点击才能输入, 用户找不到)。
            // 两侧 +/- 按钮走 step, 输入越界按上面显示的区间夹紧。
            ImGui::InputFloat("##v", &g_zval[i], 0.01f, 0.10f, "%.2f",
                              ImGuiInputTextFlags_CharsDecimal |
                                  ImGuiInputTextFlags_AutoSelectAll);
            if (g_zval[i] < rmin)
                g_zval[i] = rmin;
            if (g_zval[i] > rmax)
                g_zval[i] = rmax;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 6.0f);

    // ---- 右侧: 实时监测器 ----
    ImGui::BeginChild("speed_mon", ImVec2(0, avail.y), ImGuiChildFlags_Borders);
    SectionTitle("实时僵尸移速");
    ImGui::Separator();
    if (ImGui::BeginTable("speed_mon_t", 3,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("编号", ImGuiTableColumnFlags_WidthFixed, 48 * g_dpi);
        ImGui::TableSetupColumn("种类", ImGuiTableColumnFlags_WidthFixed, 68 * g_dpi);
        ImGui::TableSetupColumn("速度", ImGuiTableColumnFlags_WidthFixed, 62 * g_dpi);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)g_zs.size(); i++)
        {
            ImGui::TableNextRow(0, 22 * g_dpi);
            ImGui::PushID(1000 + i);
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(kInkFaint, "%d", g_zs[i].index);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(Pt::ZombieTypeName(g_zs[i].type));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(kInk, "%.3f", g_zs[i].speed);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (g_zs.empty())
        ImGui::TextDisabled("(未连接游戏, 或场上没有僵尸)");
    ImGui::EndChild();

    // ---- 左下: 恢复默认值 (与 FLTK 老版一致) ----
    //   老版窗口左下有个"恢复默认值"按钮: 把所有分类的勾选取消、数值写回游戏原值,
    //   等于"不再固定任何移速"。
    ImGui::SetCursorPos(ImVec2(start_x, start_y + avail.y - notes_h - 30 * g_dpi));
    if (ImGui::Button("恢复默认值", ImVec2(120 * g_dpi, 0)))
    {
        for (int i = 0; i < kAdjustableSpeedClasses; i++)
        {
            g_zen[i] = false; // 取消固定
            g_zval[i] = Pt::zombie_speed_classes[i].default_value;
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetItemTooltip("取消所有勾选, 并把数值写回游戏原值 (不再固定任何移速)");

    // ---- 左下: 固定值与说明 (小字号, 不滚动) ----
    ImGui::SetCursorPos(ImVec2(start_x, start_y + avail.y - notes_h));
    ImGui::BeginChild("speed_notes", ImVec2(left_w, notes_h), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    if (s_font_small_of_context != nullptr)
        ImGui::PushFont(s_font_small_of_context);

    SectionTitle("固定值与说明");
    ImGui::Separator();
    ImGui::TextWrapped("勾选后该分类移速固定为设定值; 区间取自游戏实际随机值。");
    ImGui::TextWrapped("以下分类为游戏内固定值, 无需调整 (不参与修改):");
    for (int i = kAdjustableSpeedClasses; i < Pt::ZSC_COUNT; i++)
    {
        // fixed_text 非空时用它 (老版 FLTK 就是这样, 例如雪人 "0.40 / 0.80"、矿工 "0.12 / 0.23")
        const char *ft = Pt::zombie_speed_classes[i].fixed_text;
        if (ft != nullptr)
            ImGui::TextColored(kInkDim, "   %s   %s",
                               Pt::zombie_speed_classes[i].name_zh, ft);
        else
            ImGui::TextColored(kInkDim, "   %s   %.2f",
                               Pt::zombie_speed_classes[i].name_zh,
                               Pt::zombie_speed_classes[i].default_value);
    }
    ImGui::Spacing();
    SectionTitle("说明");
    ImGui::TextWrapped("1) 冰车: 速度按位置插值: 离房子越近越慢 (0.25 → 0.10)。"
                       "固定后会变成匀速前进。");
    ImGui::TextWrapped("2) 气球: "
                       ""
                       "速度值: 仅代表动画播放速度, 同值不一定同速度。");
    ImGui::TextWrapped("3) 蹦极和僵王位置固定, 不进行统计和修改。");
    ImGui::TextWrapped("4) 除倭瓜头之外的植物僵尸都被包含在普僵里。");

    if (s_font_small_of_context != nullptr)
        ImGui::PopFont();
    ImGui::EndChild();
}
// ---------------------------------------------------------------------------
// 植物攻速窗的内容 (含"攻击间隔"与"首发延迟"两块, 与 FLTK 版一致)
//   g_pint/g_pen -> 攻击间隔 (秒),  g_wsec/g_wen -> 首发延迟 (秒)
//   两者都已经在主循环里交给 FixRandomValues, 所以这里改数组即等于写入游戏。
// ---------------------------------------------------------------------------
static void DrawPlantContent()
{
    // 分类成员提示表 (与 FLTK 版一致)
    static const char *local_members[Pt::PSC_COUNT] = {
        "豌豆射手 / 寒冰射手 / 三线射手 / 机枪射手",
        "双发射手 / 裂荚射手",
        "猫尾草",
        "大喷菇 / 仙人掌",
        "杨桃",
        "小喷菇 / 海蘑菇 / 胆小菇",
        nullptr,
        "卷心菜投手 / 玉米投手 / 西瓜投手 / 冰瓜投手",
        "向日葵 / 双子向日葵 / 阳光菇 / 金盏花",
        "地刺 / 钢地刺",
        "大嘴花",
    };
    const float left_w  = 422 * g_dpi; // 留出竖向滚动条的宽度, 否则 ± 按钮会被挤掉
    const float notes_h = 150 * g_dpi;
    const float gap     = 6 * g_dpi;
    const ImVec2 avail  = ImGui::GetContentRegionAvail();
    const float start_x = ImGui::GetCursorPosX();
    const float start_y = ImGui::GetCursorPosY();
    const float list_h  = (avail.y - notes_h - gap * 2) * 0.5f;

    // ---- 左上: 攻击间隔 ----
    ImGui::BeginChild("plant_rate", ImVec2(left_w, list_h), ImGuiChildFlags_Borders);
    SectionTitle("植物攻速 (攻击间隔, 秒)");
    if (ImGui::BeginTable("plant_rate_t", 4,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("##c", ImGuiTableColumnFlags_WidthFixed, 20 * g_dpi);
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 168 * g_dpi);
        ImGui::TableSetupColumn("区间", ImGuiTableColumnFlags_WidthFixed, 82 * g_dpi);
        ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthFixed, 104 * g_dpi);
        for (int i = 0; i < Pt::PSC_COUNT; i++)
        {
            const Pt::PlantSpeedClassInfo &info = Pt::plant_speed_classes[i];
            if (info.counter_max <= 0)
                continue;
            ImGui::TableNextRow(0, 23 * g_dpi);
            ImGui::PushID(i);
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("##en", &g_pen[i]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(info.name_zh);
            if (local_members[i] != nullptr)
                ImGui::SetItemTooltip("%s", local_members[i]);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(kInkDim, "%.2f~%.2f",
                               (float)info.counter_min / 100.0f,
                               (float)info.counter_max / 100.0f);
            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(98 * g_dpi);
            // 可键入 (见僵尸窗里的说明), 越界按本类区间夹紧
            ImGui::InputFloat("##v", &g_pint[i], 0.01f, 0.10f, "%.2f",
                              ImGuiInputTextFlags_CharsDecimal |
                                  ImGuiInputTextFlags_AutoSelectAll);
            if (g_pint[i] < (float)info.counter_min / 100.0f)
                g_pint[i] = (float)info.counter_min / 100.0f;
            if (g_pint[i] > (float)info.counter_max / 100.0f)
                g_pint[i] = (float)info.counter_max / 100.0f;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    // ---- 右侧: 实时监测器 (全高, 与左上顶对齐) ----
    ImGui::BeginChild("plant_mon", ImVec2(0, avail.y), ImGuiChildFlags_Borders);
    SectionTitle("实时植物攻速");
    ImGui::Separator();
    if (ImGui::BeginTable("plant_mon_t", 3,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("编号", ImGuiTableColumnFlags_WidthFixed, 52 * g_dpi);
        ImGui::TableSetupColumn("种类", ImGuiTableColumnFlags_WidthFixed, 90 * g_dpi);
        ImGui::TableSetupColumn("间隔(秒)", ImGuiTableColumnFlags_WidthFixed, 76 * g_dpi);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)g_ps.size(); i++)
        {
            ImGui::TableNextRow(0, 22 * g_dpi);
            ImGui::PushID(3000 + i);
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(kInkFaint, "%d", g_ps[i].index);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(Pt::PlantTypeName(g_ps[i].seed_type));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(kInk, "%.2f",
                               (float)g_ps[i].interval / 100.0f);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (g_ps.empty())
        ImGui::TextDisabled("(未连接游戏, 或场上没有植物)");
    ImGui::EndChild();

    // ---- 左中: 首发延迟 ----
    ImGui::SetCursorPos(ImVec2(start_x, start_y + list_h + gap));
    ImGui::BeginChild("plant_windup", ImVec2(left_w, list_h), ImGuiChildFlags_Borders);
    SectionTitle("首发延迟 (植物在僵尸出现时的反应时间)");
    if (ImGui::BeginTable("plant_windup_t", 4,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("##c2", ImGuiTableColumnFlags_WidthFixed, 20 * g_dpi);
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 168 * g_dpi);
        ImGui::TableSetupColumn("区间", ImGuiTableColumnFlags_WidthFixed, 82 * g_dpi);
        ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthFixed, 104 * g_dpi);
        for (int i = 0; i < Pt::PSC_COUNT; i++)
        {
            const Pt::PlantSpeedClassInfo &info = Pt::plant_speed_classes[i];
            if (info.first_max <= 0)
                continue;
            ImGui::TableNextRow(0, 23 * g_dpi);
            ImGui::PushID(200 + i);
            ImGui::TableSetColumnIndex(0);
            ImGui::Checkbox("##on", &g_wen[i]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(info.name_zh);
            if (local_members[i] != nullptr)
                ImGui::SetItemTooltip("%s", local_members[i]);
            ImGui::TableSetColumnIndex(2);
            // 可设下限 = 0.01 秒 (= 1 帧)。方案 B 在游戏帧内每帧写回, 1 帧也钉得住;
            // 旧的外部写入路径另有 first_min 保护 (它才是需要余量的那个)。
            const float windup_min = 0.01f;
            const float windup_max = (float)info.first_max / 100.0f;
            ImGui::TextColored(kInkDim, "%.2f~%.2f", windup_min, windup_max);
            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(98 * g_dpi);
            // 可键入 (见僵尸窗里的说明), 越界按本类区间夹紧
            ImGui::InputFloat("##w", &g_wsec[i], 0.01f, 0.10f, "%.2f",
                              ImGuiInputTextFlags_CharsDecimal |
                                  ImGuiInputTextFlags_AutoSelectAll);
            if (g_wsec[i] < windup_min)
                g_wsec[i] = windup_min;
            if (g_wsec[i] > windup_max)
                g_wsec[i] = windup_max;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    // ---- 左下: 说明 ----
    ImGui::SetCursorPos(ImVec2(start_x, start_y + avail.y - notes_h));
    ImGui::BeginChild("plant_notes", ImVec2(left_w, notes_h), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    if (s_font_small_of_context != nullptr)
        ImGui::PushFont(s_font_small_of_context);
    SectionTitle("说明");
    ImGui::Separator();
    ImGui::TextWrapped("本修改器只固定每发的随机抖动, 不改写游戏定义表里的固定值。");
    ImGui::TextWrapped("0.01 秒 = 1 帧 —— 下限就是 1 帧。");
    ImGui::TextWrapped("首发延迟精确到帧。");
    ImGui::TextWrapped("由于游戏机制, 双发、裂荚、猫尾草的实际反应时间上限如上, 本修改器可使其不会丢弹。");
    ImGui::TextWrapped("由于杨桃索敌过于抽象, 且有场上其他僵尸干扰, 本修改器不修改其首发延迟。");
    // 帧级钩子状态 (自动注入; 失败会在这里提示)
    ImGui::Separator();
    if (!g_attached)
        ImGui::TextColored(kInkDim, "DLL未生效");
    else if (!g_pvz.HookAlive())
        ImGui::TextColored(kWarn,
                           "DLL未生效");
    else
        ImGui::TextColored(kOk, "DLL已生效");
    // 诊断原文 (未生效时把它发我就能定位): 路径 / fail 码 / init_step / 调用次数
    ImGui::TextWrapped("%s", g_pvz.HookStatus().c_str());
    // ★★ v38: 一行"中断原因" ★★
    //   用户报的是"注入不稳定, 偶尔中断, 拖主界面也会中断" ⇒ 必须**不悬停**就能看见
    //   到底是哪一类 ("钩点被撤" / "补丁没打上" / "钩点在但没画")。判据在共享头
    //   CdInterruptKindOf(), 文案在 pvz.cpp。正常时只在 PZTK_DEV=1 下显示 (普通界面保持干净)。
    if (g_attached && (DevMode() || !g_pvz.HookAlive()))
    {
        const std::string ir = g_pvz.HookInterruptText();
        ImGui::TextWrapped("%s", ir.c_str());
    }
    // ★★ v40: 注入**策略**状态 (只在 PZTK_DEV=1 显示; 普通界面一个字都不多) ★★
    //   用户要的是"能看出来现在走的是哪条规矩": 已注入? 重试了几次? 有几个请求被合并?
    //   这一行是**工具自己**的状态 (不跨进程), 直接在本文件里拼;
    //   字形表只扫 frontend/ + src/ + inc/ ⇒ 这里的字也会进表, 改完要重跑 gen_glyphs。
    if (g_attached && DevMode())
    {
        static const char *const st_name[] = {"未开始", "已注入", "重试中"};
        const uint32_t st = static_cast<uint32_t>(g_inject_policy.state);
        ImGui::TextWrapped("注入: %s | 重试计数 %d | 待执行请求 %d",
                           st < 3 ? st_name[st] : "?", g_inject_policy.retry_count,
                           g_inject_policy.merged_requests);
    }

    // ---- 事件日志 (只在状态跳变时记, 用来核查"发射不整齐"这类时序问题) ----
    static bool v_hook_log = false;
    if (DevMode() && ImGui::Checkbox("记录钩子事件日志 (排查用)", &v_hook_log))
        g_pvz.SetHookLog(v_hook_log);
    if (ImGui::IsItemHovered())
        ImGui::SetItemTooltip("只记三种关键时刻: 僵尸进这排 / 放手 / 钉住, 每条带植物编号。\n"
                              "同一帧发生的会写在相邻行 (时间相同) —— 由此能看出同排植物有没有对齐。");
    if (v_hook_log)
        ImGui::TextWrapped("已写入: %s", g_pvz.HookLogPathA().c_str());
    if (s_font_small_of_context != nullptr)
        ImGui::PopFont();
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// 资源页: 阳光 / 金币 / 英尺 + 四项无限 + 自动收集 / 不掉战利品
// ---------------------------------------------------------------------------
static int  g_sun = 8000;
static int  g_money = 80000;
static int  g_tree = 1000;
static bool g_unlock_sun = false;
static bool g_fertilizer = false;
static bool g_bugspray = false;
static bool g_treefood = false;
static bool g_chocolate = false;
static bool g_autocollect = false;
static bool g_nodroploot = false;

// 资源输入行: 名称 + 输入框 + 应用按钮.
//   与 FLTK 版一致的是"输入框旁边有一个按钮才写入" (Fl_Value_Input + button_sun/
//   button_money/button_wisdom_tree); 回车同样算应用. 取值范围照抄 FLTK 的 bounds.
//   名称放在最左边, 名称列 112px 固定宽度, 三行输入框左对齐.
static bool ResourceRow(const char *id, int *value, int lo, int hi, const char *label)
{
    // 版式: [名称]  [输入框] [应用] —— 名称在最左边, 名称列固定宽度,
    // 保证三行的输入框左对齐 (与 FLTK 版一致的是"点按钮才写入")
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(112 * g_dpi);

    ImGui::SetNextItemWidth(132 * g_dpi);
    // 注意: InputInt/InputScalar 不支持 ImGuiInputTextFlags_EnterReturnsTrue
    // (ImGui 会 assert: imgui_widgets.cpp "EnterReturnsTrue == 0")。回车/失焦提交
    // 要用 IsItemDeactivatedAfterEdit()。
    ImGui::InputInt(id, value, 0, 0);
    const bool committed = ImGui::IsItemDeactivatedAfterEdit();
    if (*value < lo)
        *value = lo;
    if (*value > hi)
        *value = hi;
    ImGui::SameLine(0.0f, 6.0f);
    char button_id[64];
    std::snprintf(button_id, sizeof(button_id), "应用%s", id);
    const bool clicked = ImGui::Button(button_id, ImVec2(62 * g_dpi, 0));
    return committed || clicked;
}

static void PageResource()
{
    BeginBlock("资源", 200 * g_dpi); // 空间够, 加高更舒展

    // 阳光 / 金币 / 智慧树高度 (大一号字, 输入后点"应用"或回车生效)
    if (g_font_big != nullptr)
        ImGui::PushFont(g_font_big);
    if (ResourceRow("##sun", &g_sun, 0, 9990, "阳光") && g_attached)
        g_pvz.SetSun(g_sun);
    if (ResourceRow("##money", &g_money, 0, 99999, "金币") && g_attached)
        g_pvz.SetMoney(g_money);
    if (ResourceRow("##tree", &g_tree, 0, 1000, "智慧树高度") && g_attached)
        g_pvz.SetTreeHeight(g_tree);
    if (g_font_big != nullptr)
        ImGui::PopFont();

    ImGui::Spacing();
    if (ImGui::Checkbox("解除阳光上限", &g_unlock_sun))
        if (g_attached) g_pvz.UnlockSunLimit(g_unlock_sun);
    ImGui::SameLine(0.0f, 16.0f);
    if (g_lineup_mode)
        ImGui::BeginDisabled(); // 快捷布阵模式接管中 (阵型页)
    if (ImGui::Checkbox("自动收集", &g_autocollect))
        if (g_attached) g_pvz.AutoCollected(g_autocollect);
    if (g_lineup_mode)
        ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Checkbox("不掉战利品", &g_nodroploot))
        if (g_attached) g_pvz.NotDropLoot(g_nodroploot);

    ImGui::Spacing();
    if (ImGui::Checkbox("花肥无限", &g_fertilizer))
        if (g_attached) g_pvz.FertilizerUnlimited(g_fertilizer);
    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Checkbox("杀虫剂无限", &g_bugspray))
        if (g_attached) g_pvz.BugSprayUnlimited(g_bugspray);
    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Checkbox("树肥无限", &g_treefood))
        if (g_attached) g_pvz.TreeFoodUnlimited(g_treefood);
    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Checkbox("巧克力无限", &g_chocolate))
        if (g_attached) g_pvz.ChocolateUnlimited(g_chocolate);

    EndBlock();

    BeginBlock("说明", 100 * g_dpi);
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextWrapped("阳光 / 金币 / 智慧树高度: 改完数值后点右侧的『应用』按钮 "
                       "(或按回车) 才会写入游戏。");
    ImGui::TextWrapped("未连接游戏时所有修改都不会写入。");
    if (g_font_small != nullptr)
        ImGui::PopFont();
    EndBlock();
}

// ---------------------------------------------------------------------------
// 出怪详情独立子窗 (对应 FLTK 版 SpawnWindow "出怪数量统计")
//
//   数据: GetSpawnList() 读回的 1000 项 = 20 波 x 50 槽, 每项是僵尸类型 (0..32),
//         -1 = 空槽。聚合方式与 FLTK 的 SpawnTable 完全一致: 只统计 0 <= type < 33,
//         合计为 0 的种类默认不显示。
//   ① 数量统计: 33 种 x 20 波 + 合计列
//   ② 原始列表: 1000 项 (序号 / 波 / 槽 / 类型 / 名称)
//   保存 / 加载: .zbl = header 0x58434c52 + 版本 1 + 长度 1000 + 1000 个 int +
//                crc32(前 1003 个 int), 与 FLTK 版逐字节兼容。
// ---------------------------------------------------------------------------
static SubWindow g_sub_spawn;
static bool g_want_spawn = false;            // 请求打开/显示详情窗 (帧末执行)
static bool g_spawn_refresh_pending = false; // 打开窗口时先刷新一次 (FLTK 版行为)

static std::array<int, 1000>    g_spawn_list{}; // 当前显示/导出的出怪列表
static std::array<int, 33 * 21> g_spawn_stat{}; // [类型 * 21 + 列], 列 20 = 合计
static int  g_spawn_total = 0;
static bool g_spawn_has_data = false;
static bool g_spawn_show_empty = false;
static std::string g_spawn_msg = "点『刷新』读取当前关卡的出怪列表, 也可加载 .zbl 文件";

// 表格列标题 (静态字符串, 每帧重建表格时复用)
static const char *kStatCols[22] = {"种类", "w1",  "w2",  "w3",  "w4",  "w5",  "w6",  "w7",
                                    "w8",   "w9",  "w10", "w11", "w12", "w13", "w14", "w15",
                                    "w16",  "w17", "w18", "w19", "w20", "合计"};
static const char *kRawCols[5] = {"序号", "波", "槽", "类型", "名称"};

static const int kZblHeader = 0x58434c52;
static const int kZblCount  = 1000;
static const int kZblInts   = 3 + kZblCount + 1; // 1004 个 int = 4016 字节

// 1000 项 -> 33x21 统计表
static void SpawnAggregate()
{
    g_spawn_stat.fill(0);
    g_spawn_total = 0;
    for (int wave = 0; wave < 20; wave++)
        for (int slot = 0; slot < 50; slot++)
        {
            const int type = g_spawn_list[wave * 50 + slot];
            if (type < 0 || type >= 33)
                continue;
            g_spawn_stat[type * 21 + wave] += 1;
            g_spawn_stat[type * 21 + 20] += 1;
            g_spawn_total += 1;
        }
}

// 从游戏读取当前出怪列表 (与 FLTK 版 cb_update_details 同规则)
static void SpawnRefresh()
{
    g_spawn_list.fill(-1);
    g_spawn_has_data = false;
    SpawnAggregate();

    if (!g_attached || !g_pvz.GameOn())
    {
        g_spawn_msg = "未连接游戏: 可以『加载 .zbl』离线查看";
        return;
    }
    const int ui = g_pvz.GameUI();
    if (ui != 2 && ui != 3)
    {
        g_spawn_msg = "当前不在选卡界面或战斗中, 游戏里还没有出怪列表";
        return;
    }

    std::array<int, 1000> list = g_pvz.GetSpawnList();
    const int mode = g_pvz.GameMode();
    const bool adventure = (mode >= 1 && mode <= 5);
    if (adventure)
        for (int i = 500; i < 1000; i++)
            list[i] = -1; // 非无尽关卡只有前 500 项有意义 (与 FLTK 版一致)

    g_spawn_list = list;
    g_spawn_has_data = true;
    SpawnAggregate();
    g_spawn_msg = std::string("已读取当前出怪列表") +
                  (adventure ? " (非无尽关卡, 只统计前 500 项)" : "");
}

// 打包 .zbl (保存与加载校验共用这套格式代码)
static void SpawnBuildBlob(const std::array<int, 1000> &list, int *data)
{
    data[0] = kZblHeader;
    data[1] = 1;
    data[2] = kZblCount;
    for (int i = 0; i < kZblCount; i++)
        data[3 + i] = list[i];
    unsigned long crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char *)data, (3 + kZblCount) * sizeof(int));
    data[3 + kZblCount] = (int)crc;
}

// 校验并解析 .zbl
static bool SpawnParseBlob(const int *data, int count, std::array<int, 1000> &out)
{
    if (count != kZblInts)
        return false;
    if (data[0] != kZblHeader || data[1] != 1 || data[2] != kZblCount)
        return false;
    unsigned long crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char *)data, (3 + kZblCount) * sizeof(int));
    if ((int)crc != data[3 + kZblCount])
        return false;
    out.fill(-1);
    for (int i = 0; i < kZblCount; i++)
        out[i] = data[3 + i];
    return true;
}

// exe 目录下的 zombies\ (没有就建; 与 FLTK 版保存位置的习惯一致)
static std::wstring SpawnDir()
{
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring dir(buf);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        dir.resize(slash);
    dir += L"\\zombies";
    CreateDirectoryW(dir.c_str(), nullptr); // 已存在会失败, 忽略
    return dir;
}

static bool SpawnSaveFile(const wchar_t *path)
{
    int data[kZblInts] = {0};
    SpawnBuildBlob(g_spawn_list, data);
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const bool ok = WriteFile(hFile, data, sizeof(data), &written, nullptr) != 0 &&
                    written == sizeof(data);
    CloseHandle(hFile);
    return ok;
}

static bool SpawnLoadFile(const wchar_t *path)
{
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        g_spawn_msg = "加载失败: 打不开文件";
        return false;
    }
    LARGE_INTEGER size{};
    GetFileSizeEx(hFile, &size);
    int data[kZblInts] = {0};
    DWORD read = 0;
    const bool size_ok = (size.QuadPart == sizeof(data));
    const bool read_ok = ReadFile(hFile, data, sizeof(data), &read, nullptr) != 0 &&
                         read == sizeof(data);
    CloseHandle(hFile);

    if (!size_ok)
    {
        g_spawn_msg = "加载失败: 文件大小不对(不是本工具的阵型文件)";
        return false;
    }
    if (!read_ok)
    {
        g_spawn_msg = "加载失败: 读取错误";
        return false;
    }

    std::array<int, 1000> list;
    if (!SpawnParseBlob(data, kZblInts, list))
    {
        g_spawn_msg = "加载失败: 文件已损坏或不是阵型文件";
        return false;
    }

    g_spawn_list = list;
    g_spawn_has_data = true;
    SpawnAggregate();
    g_spawn_msg = "已加载 .zbl (共 " + std::to_string(g_spawn_total) + " 只僵尸)";
    return true;
}

// 文件对话框 (默认目录 = exe\zombies)
static bool SpawnDialog(bool save, std::wstring &path)
{
    static wchar_t file[MAX_PATH];
    file[0] = 0;
    if (save)
    {
        // 与 FLTK 版相同的命名习惯: 年.月.日_时.分.秒.zbl
        SYSTEMTIME t;
        GetLocalTime(&t);
        std::swprintf(file, MAX_PATH, L"%d.%d.%d_%d.%d.%d.zbl", t.wYear, t.wMonth, t.wDay,
                      t.wHour, t.wMinute, t.wSecond);
    }

    const std::wstring dir = SpawnDir();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"PvZ 出怪列表 (*.zbl)\0*.zbl\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = dir.c_str();
    ofn.lpstrDefExt = L"zbl";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (save)
    {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameW(&ofn) == FALSE)
            return false;
    }
    else
    {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&ofn) == FALSE)
            return false;
    }
    path = file;
    return true;
}

// 单元格背景: 数量越多越蓝 (浅色主题下用蓝色强调色的 alpha 叠加)
static ImU32 SpawnCellBg(int v, int vmax)
{
    if (v <= 0)
        return 0;
    const int a = 40 + (int)(120.0f * (float)(v < vmax ? v : vmax) / (float)vmax);
    return IM_COL32(0x0A, 0x84, 0xFF, a);
}

static void DrawSpawnContent()
{
    // ---- 工具条 ----
    if (ImGui::Button("刷新", ImVec2(64 * g_dpi, 0)))
        SpawnRefresh();
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("保存 .zbl", ImVec2(96 * g_dpi, 0)))
    {
        std::wstring path;
        if (SpawnDialog(true, path))
        {
            if (SpawnSaveFile(path.c_str()))
            {
                g_spawn_msg = "已保存 .zbl (共 " + std::to_string(g_spawn_total) + " 只僵尸)";
                MessageBoxW(nullptr, L"出怪列表已保存。", L"保存成功", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                g_spawn_msg = "保存失败: 无法写入文件";
                MessageBoxW(nullptr, L"保存失败: 无法写入文件。", L"保存失败",
                            MB_OK | MB_ICONWARNING);
            }
        }
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("加载 .zbl", ImVec2(96 * g_dpi, 0)))
    {
        std::wstring path;
        if (SpawnDialog(false, path) && SpawnLoadFile(path.c_str()))
        {
            // 与 FLTK 版一致: 加载后写进游戏 (选卡界面会顺带刷新出怪预览)
            if (g_attached && g_pvz.GameOn())
            {
                const int ui = g_pvz.GameUI();
                if (ui == 2 || ui == 3)
                {
                    g_pvz.SetSpawnList(g_spawn_list);
                    SpawnRefresh();
                    g_spawn_msg += ", 并已写入游戏";
                }
                else
                    g_spawn_msg += " (当前不在选卡/战斗界面, 未写入游戏)";
            }
            else
                g_spawn_msg += " (未连接游戏, 仅显示)";
            MessageBoxW(nullptr, L"出怪列表已加载。", L"加载成功", MB_OK | MB_ICONINFORMATION);
        }
    }
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::Checkbox("显示数量为 0 的种类", &g_spawn_show_empty);
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::TextDisabled("%s", g_spawn_msg.c_str());
    ImGui::Separator();

    // ---- 两个视图 ----
    if (ImGui::BeginTabBar("spawn_tabs"))
    {
        if (ImGui::BeginTabItem("数量统计 (33 种 x 20 波)"))
        {
            if (ImGui::BeginTable("spawn_stat", 22,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableSetupColumn(kStatCols[0], ImGuiTableColumnFlags_WidthFixed, 92 * g_dpi);
                for (int w = 0; w < 20; w++)
                    ImGui::TableSetupColumn(kStatCols[1 + w], ImGuiTableColumnFlags_WidthFixed,
                                            32 * g_dpi);
                ImGui::TableSetupColumn(kStatCols[21], ImGuiTableColumnFlags_WidthFixed, 52 * g_dpi);
                ImGui::TableHeadersRow();

                for (int t = 0; t < 33; t++)
                {
                    const int t_total = g_spawn_stat[t * 21 + 20];
                    if (t_total == 0 && !g_spawn_show_empty)
                        continue; // 不出的种类不显示 (与 FLTK 版一致)
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("[%d] %s", t, Pt::zombies_zh[t]);
                    for (int w = 0; w < 20; w++)
                    {
                        ImGui::TableSetColumnIndex(1 + w);
                        const int v = g_spawn_stat[t * 21 + w];
                        if (v > 0)
                        {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, SpawnCellBg(v, 20));
                            ImGui::Text("%d", v);
                        }
                    }
                    ImGui::TableSetColumnIndex(21);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, SpawnCellBg(t_total, 300));
                    ImGui::Text("%d", t_total);
                }
                ImGui::EndTable();
            }
            ImGui::Text("合计出怪: %d 只%s", g_spawn_total,
                        g_spawn_has_data ? "" : "   (还没有数据: 先『刷新』或『加载 .zbl』)");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("原始列表 (1000 项)"))
        {
            if (ImGui::BeginTable("spawn_raw", 5,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(kRawCols[0], ImGuiTableColumnFlags_WidthFixed, 60 * g_dpi);
                ImGui::TableSetupColumn(kRawCols[1], ImGuiTableColumnFlags_WidthFixed, 50 * g_dpi);
                ImGui::TableSetupColumn(kRawCols[2], ImGuiTableColumnFlags_WidthFixed, 50 * g_dpi);
                ImGui::TableSetupColumn(kRawCols[3], ImGuiTableColumnFlags_WidthFixed, 60 * g_dpi);
                ImGui::TableSetupColumn(kRawCols[4], ImGuiTableColumnFlags_WidthFixed, 110 * g_dpi);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(1000);
                while (clipper.Step())
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        const int type = g_spawn_list[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", i);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", i / 50 + 1);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d", i % 50 + 1);
                        ImGui::TableSetColumnIndex(3);
                        if (type >= 0 && type < 33)
                        {
                            ImGui::Text("%d", type);
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextUnformatted(Pt::zombies_zh[type]);
                        }
                        else
                        {
                            ImGui::TextUnformatted("-");
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextUnformatted("-");
                        }
                    }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// ---------------------------------------------------------------------------
// 阵型页: 快捷布阵 + 场地 + 阵型代码
//   核心接口: AutoLadder / LilyPadOnPool / FlowerPotOnRoof / SetScene /
//             GetLineup / SetLineup
//   (原 FLTK 版的"截图到剪贴板"已删除: 现代 Windows 自带截图, 见 CHANGELOG)
//   阵型列表文件: YAML 子集 (.yml, 第一行必须是 "#! pvztoolkit"), 解析规则与
//   FLTK 版 window.cpp 的 import_lineup_list_file 完全一致; 放在 exe 目录的
//   *.yml 会在启动时自动加载 (这正是 setups.yml 里写的约定)。
// ---------------------------------------------------------------------------
static bool g_lm_saved_plant[6] = {};      // 进入快捷布阵模式前的勾选状态
static bool g_lm_saved_zombie[6] = {};
static bool g_lm_saved_other[6] = {};
static bool g_lm_saved_autocollect = false;

static const char *kSceneNames[6] = {"白天", "黑夜", "泳池", "雾夜", "屋顶", "月夜"};

// 睡莲 / 花盆的列范围 (逐字对照 FLTK 版 window.h 的两个数组)
static const unsigned int kLilyLower[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static const unsigned int kLilyUpper[8] = {2, 3, 4, 5, 6, 7, 8, 9};
static const unsigned int kPotLower[7] = {1, 4, 4, 4, 4, 4, 4};
static const unsigned int kPotUpper[7] = {3, 4, 5, 6, 7, 8, 9};

static std::vector<Pt::Lineup> g_lineups;      // 已加载的阵型
static int g_lineup_count[6] = {0};            // 每个场景各有几条
static int g_lineup_sel = 0;                   // 当前场景下拉选中项
static int g_scene = 2;                        // 默认泳池 (与 FLTK 版一致)
static char g_lineup_buf[2048] = {0};          // 阵型代码 (只读显示)
static bool g_lineup_items_dirty = true;
static std::string g_scene_items, g_lily_items, g_pot_items, g_lineup_items;
static std::string g_lineup_msg = "阵型列表文件 (*.yml) 放在 exe 目录会自动加载";

// ---- 快捷布阵模式: 强制打开 6 项便利功能 / 恢复原状 ----------------------
static void LineupModeApply(bool on)
{
    if (on)
    {
        for (int i = 0; i < 6; i++)
        {
            g_lm_saved_plant[i] = g_plant_mod[i];
            g_lm_saved_zombie[i] = g_zombie_mod[i];
            g_lm_saved_other[i] = g_other_mod[i];
        }
        g_lm_saved_autocollect = g_autocollect;

        for (int i = 0; i < 6; i++)
        {
            if (kLmLockPlant[i])
                g_plant_mod[i] = true;
            if (kLmLockZombie[i])
                g_zombie_mod[i] = true;
            if (kLmLockOther[i])
                g_other_mod[i] = true;
        }
        g_autocollect = true;
        // 修改板块的开关由 WireToggles 按变化写游戏; 自动收集没有变化检测, 这里直接写
        if (g_attached)
            g_pvz.AutoCollected(true);
    }
    else
    {
        for (int i = 0; i < 6; i++)
        {
            g_plant_mod[i] = g_lm_saved_plant[i];
            g_zombie_mod[i] = g_lm_saved_zombie[i];
            g_other_mod[i] = g_lm_saved_other[i];
        }
        g_autocollect = g_lm_saved_autocollect;
        if (g_attached)
            g_pvz.AutoCollected(g_autocollect);
    }
}

// ---- 剪贴板 (阵型代码是 ASCII, 按字节转宽字符即可) ------------------------
static bool ClipboardSetText(const std::string &str)
{
    const std::wstring w(str.begin(), str.end());
    if (!OpenClipboard(nullptr))
        return false;
    EmptyClipboard();
    const size_t bytes = (w.size() + 1) * sizeof(wchar_t);
    bool ok = false;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem != nullptr)
    {
        void *p = GlobalLock(hMem);
        if (p != nullptr)
        {
            std::memcpy(p, w.c_str(), bytes);
            GlobalUnlock(hMem);
            ok = SetClipboardData(CF_UNICODETEXT, hMem) != nullptr;
        }
        if (!ok)
            GlobalFree(hMem);
    }
    CloseClipboard();
    return ok;
}

static std::string ClipboardGetText()
{
    std::string out;
    if (!OpenClipboard(nullptr))
        return out;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != nullptr)
    {
        const wchar_t *p = (const wchar_t *)GlobalLock(hData);
        if (p != nullptr)
        {
            const int need = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
            if (need > 1)
            {
                out.resize((size_t)need - 1);
                WideCharToMultiByte(CP_UTF8, 0, p, -1, &out[0], need, nullptr, nullptr);
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return out;
}

// ---- 阵型列表文件 (.yml) --------------------------------------------------
static int ImportLineupFile(const std::wstring &path,
                            std::vector<std::pair<int, std::string>> &errors)
{
    std::ifstream ifs(path.c_str());
    if (!ifs)
        return -1;

    int line = 0;
    int added = 0;
    std::string str;
    while (std::getline(ifs, str))
    {
        line++;
        if (line == 1)
        {
            if (str != "#! pvztoolkit") // 第一行必须是这个标记
                break;
            continue;
        }
        if (str.empty() || str[0] == '#') // 空行 / 注释
            continue;

        std::regex re("\".*\": ([a-zA-Z0-9+/=]{1,}|[a-zA-Z0-9-_=]{1,})");
        if (std::regex_match(str, re))
        {
            const size_t p = str.find_last_of(": ");
            const std::string name = str.substr(1, p - 3); // 去掉首尾引号
            const std::string code = str.substr(p + 1);
            Pt::Lineup lineup(name, code);
            if (lineup.OK())
            {
                g_lineups.push_back(lineup);
                added++;
            }
            else
            {
                errors.push_back(std::make_pair(line, str));
            }
        }
        else
        {
            errors.push_back(std::make_pair(line, str));
        }
    }
    return added;
}

static void RebuildLineupIndex()
{
    // 与 FLTK 版一致: 按 weight 升序排序
    std::sort(g_lineups.begin(), g_lineups.end(),
              [](const Pt::Lineup &a, const Pt::Lineup &b) { return a.weight < b.weight; });
    for (int i = 0; i < 6; i++)
        g_lineup_count[i] = 0;
    for (size_t i = 0; i < g_lineups.size(); i++)
    {
        const int sc = g_lineups[i].scene;
        if (sc >= 0 && sc < 6)
            g_lineup_count[sc]++;
    }
    g_lineup_items_dirty = true;
}

// 当前场景的阵型名下拉内容 (重名在末尾补空格, 与 FLTK 版一致)
static void RebuildLineupItems()
{
    g_lineup_items.clear();
    const int sc = (g_scene >= 0 && g_scene < 6) ? g_scene : 0;
    const int n = g_lineup_count[sc];
    if (g_lineup_sel >= n)
        g_lineup_sel = 0;

    int start = 0;
    for (int i = 0; i < sc; i++)
        start += g_lineup_count[i];

    std::vector<std::string> used;
    for (int i = 0; i < n; i++)
    {
        std::string name = g_lineups[start + i].lineup_name;
        while (std::find(used.begin(), used.end(), name) != used.end())
            name += " ";
        used.push_back(name);
        g_lineup_items += name;
        g_lineup_items.push_back('\0');
    }
    g_lineup_items.push_back('\0');
    g_lineup_items_dirty = false;
}

static void LineupSelectCode()
{
    const int sc = (g_scene >= 0 && g_scene < 6) ? g_scene : 0;
    int start = 0;
    for (int i = 0; i < sc; i++)
        start += g_lineup_count[i];
    if (g_lineup_sel < 0 || g_lineup_sel >= g_lineup_count[sc])
    {
        // 当前场景没有阵型 -> 清空 (与 FLTK 版 cb_show_lineup_string 一致)
        if (g_lineup_count[sc] == 0)
            g_lineup_buf[0] = 0;
        return;
    }
    std::snprintf(g_lineup_buf, sizeof(g_lineup_buf), "%s",
                  g_lineups[start + g_lineup_sel].lineup_code.c_str());
    g_lineup_msg = "已选中阵型: " + g_lineups[start + g_lineup_sel].lineup_name;
}

// exe 目录下的 *.yml 自动加载
static void LineupAutoLoad()
{
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring pattern(buf);
    const size_t slash = pattern.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        pattern.resize(slash + 1);
    else
        pattern.clear();
    const std::wstring prefix = pattern;
    pattern += L"*.yml";

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    int files = 0;
    int added = 0;
    do
    {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            continue;
        std::vector<std::pair<int, std::string>> errors;
        const int n = ImportLineupFile(prefix + fd.cFileName, errors);
        if (n > 0)
        {
            files++;
            added += n;
        }
    } while (FindNextFileW(hFind, &fd) != 0);
    FindClose(hFind);

    if (added > 0)
    {
        RebuildLineupIndex();
        LineupSelectCode(); // 与 FLTK 版一样: 导入后直接显示当前场景第一条的代码
        g_lineup_msg = "已自动加载 " + std::to_string(added) + " 条阵型 (来自 " +
                       std::to_string(files) + " 个 yml 文件)";
    }
}

static void LineupImportDialog()
{
    static wchar_t file[MAX_PATH];
    file[0] = 0;
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"YAML 阵型列表 (*.yml)\0*.yml\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"yml";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn) == FALSE)
        return;

    std::vector<std::pair<int, std::string>> errors;
    const int added = ImportLineupFile(file, errors);
    if (added < 0)
    {
        g_lineup_msg = "阵型列表: 打不开文件";
        return;
    }
    RebuildLineupIndex();
    LineupSelectCode();
    g_lineup_msg = "阵型列表: 新增 " + std::to_string(added) + " 条, 共 " +
                   std::to_string(g_lineups.size()) + " 条";

    if (!errors.empty())
    {
        std::string text;
        for (size_t i = 0; i < errors.size() && i < 12; i++)
        {
            std::string s = errors[i].second;
            if (s.size() > 60)
                s = s.substr(0, 57) + " ...";
            text += "第 " + std::to_string(errors[i].first) + " 行: " + s + "\r\n";
        }
        if (errors.size() > 12)
            text += "(还有更多没有显示 ...)\r\n";
        const std::wstring wtext(text.begin(), text.end()); // 仅 ASCII 安全
        MessageBoxW(nullptr, wtext.c_str(), L"阵型列表文件格式错误", MB_OK | MB_ICONWARNING);
    }
}

// ---- 阵型代码的四个按钮 ---------------------------------------------------
static void SetBufFromString(const std::string &s)
{
    std::snprintf(g_lineup_buf, sizeof(g_lineup_buf), "%s", s.c_str());
}

static void LineupGetCode()
{
    if (!g_attached)
    {
        g_lineup_msg = "获取代码: 未连接游戏";
        return;
    }
    Pt::Lineup lineup = g_pvz.GetLineup();
    const std::string code = lineup.Generate();
    SetBufFromString(code);
    g_lineup_msg = "已获取当前场上阵型代码 (" + std::to_string(code.size()) + " 字符)";
}

static void LineupCopy()
{
    if (ClipboardSetText(g_lineup_buf))
        g_lineup_msg = "阵型代码已复制到剪贴板";
    else
        g_lineup_msg = "复制失败: 打不开剪贴板";
}

static void LineupPaste()
{
    std::string str = ClipboardGetText();
    if (str.empty())
    {
        g_lineup_msg = "粘贴导入: 剪贴板里没有文本";
        return;
    }
    // 与 FLTK 版 cb_paste_lineup 完全相同的清洗规则
    str.erase(std::remove_if(str.begin(), str.end(),
                             [](char c) {
                                 return c == '\r' || c == '\n' || c == '\t' || c == '\v' ||
                                        c == '\f';
                             }),
              str.end());
    const size_t a = str.find_first_not_of(' ');
    const size_t b = str.find_last_not_of(' ');
    if (a == std::string::npos)
    {
        g_lineup_msg = "粘贴导入: 剪贴板内容为空";
        return;
    }
    str = str.substr(a, b - a + 1);
    str = std::regex_replace(str, std::regex("( ){2,}"), " ");
    str = std::regex_replace(str, std::regex(", "), ",");
    str = std::regex_replace(str, std::regex(" ,"), ",");

    // 统一转成新格式
    Pt::Lineup lineup(str);
    const std::string code = lineup.Generate();
    SetBufFromString(code);
    g_lineup_msg = code.empty() ? "粘贴导入: 内容无法解析成阵型" : "已导入并规范化阵型代码";
}

static void LineupApply()
{
    if (!g_attached)
    {
        g_lineup_msg = "布置阵型: 未连接游戏";
        return;
    }
    std::string str(g_lineup_buf);
    if (str.empty())
    {
        g_lineup_msg = "布置阵型: 没有阵型代码";
        return;
    }
    // Base64Url -> 标准 Base64 (与 FLTK 版 cb_set_lineup 一致)
    std::replace(str.begin(), str.end(), '-', '+');
    std::replace(str.begin(), str.end(), '_', '/');
    switch (str.size() % 4)
    {
    case 0:
        break;
    case 1:
        return; // 长度非法, 直接放弃 (与 FLTK 版一致)
    case 2:
        str += "==";
        break;
    case 3:
        str += "=";
        break;
    }

    Pt::Lineup lineup(str);
    g_pvz.SetLineup(lineup);
    g_lineup_msg = "已布置阵型";
}

static void PageLineup()
{
    static int v_lily = 0, v_pot = 0;

    // 下拉内容只构造一次
    if (g_scene_items.empty())
    {
        char buf[64];
        for (int i = 0; i < 6; i++)
        {
            std::snprintf(buf, sizeof(buf), "[%d] %s", i, kSceneNames[i]);
            g_scene_items += buf;
            g_scene_items.push_back('\0');
        }
        g_scene_items.push_back('\0');

        for (int i = 0; i < 8; i++) // 睡莲: 9 列的 ■/□ 图案
        {
            std::string label;
            for (unsigned int col = 1; col <= 9; col++)
                label += (kLilyLower[i] <= col && col <= kLilyUpper[i]) ? "■" : "□";
            g_lily_items += label;
            g_lily_items.push_back('\0');
        }
        g_lily_items.push_back('\0');

        for (int i = 0; i < 7; i++) // 花盆
        {
            std::string label;
            for (unsigned int col = 1; col <= 9; col++)
                label += (kPotLower[i] <= col && col <= kPotUpper[i]) ? "■" : "□";
            g_pot_items += label;
            g_pot_items.push_back('\0');
        }
        g_pot_items.push_back('\0');
    }
    if (g_lineup_items_dirty)
        RebuildLineupItems();

    // ---- 快捷布阵 ----
    BeginBlock("快捷布阵", 140 * g_dpi);
    if (ImGui::Checkbox("快捷布阵模式", &g_lineup_mode))
        LineupModeApply(g_lineup_mode);
    ImGui::SameLine(0.0f, 12.0f);
    if (ImGui::Button("智能搭梯", ImVec2(96 * g_dpi, 0)) && g_attached)
        g_pvz.AutoLadder(true);
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextWrapped("勾选后强制打开 6 项便利功能 (自动收集 / 免费用卡 / 植物无敌 / "
                       "立刻装填 / 暂停刷怪 / 清除浓雾), 并锁住战场页与资源页上对应的"
                       "勾选框; 取消勾选时恢复成原来的状态。");
    ImGui::TextWrapped("智能搭梯: 先清空场上所有梯子, 再给每一个『模仿者南瓜头』所在的"
                       "草地格自动铺梯子。");
    if (g_font_small != nullptr)
        ImGui::PopFont();
    EndBlock();

    // ---- 场地 ----
    BeginBlock("场地", 146 * g_dpi);
    if (ImGui::Button("重置场地", ImVec2(96 * g_dpi, 0)) && g_attached)
        g_pvz.SetScene(g_scene, true);
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##scene", &g_scene, g_scene_items.c_str()))
    {
        g_lineup_sel = 0;
        g_lineup_items_dirty = true;
        LineupSelectCode();
    }

    ImGui::SetNextItemWidth(150 * g_dpi);
    ImGui::Combo("##lily", &v_lily, g_lily_items.c_str());
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("铺睡莲", ImVec2(84 * g_dpi, 0)) && g_attached)
        g_pvz.LilyPadOnPool(kLilyLower[v_lily], kLilyUpper[v_lily]);

    ImGui::SetNextItemWidth(150 * g_dpi);
    ImGui::Combo("##pot", &v_pot, g_pot_items.c_str());
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("摆花盆", ImVec2(84 * g_dpi, 0)) && g_attached)
        g_pvz.FlowerPotOnRoof(kPotLower[v_pot], kPotUpper[v_pot]);

    if (g_attached)
    {
        const int cur = g_pvz.GetScene();
        ImGui::TextDisabled("游戏当前场景: %s   ■ = 该列种满",
                            (cur >= 0 && cur < 6) ? kSceneNames[cur] : "?");
    }
    else
        ImGui::TextDisabled("未连接游戏   ■ = 该列种满");
    EndBlock();

    // ---- 阵型代码 ----
    BeginBlock("阵型代码", 286 * g_dpi);
    if (ImGui::Button("加载阵型列表文件 (*.yml)", ImVec2(196 * g_dpi, 0)))
        LineupImportDialog();
    // 状态单独占一行并自动换行 (阵型名可能很长, 挤在按钮右边会被裁掉)
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextWrapped("%s", g_lineup_msg.c_str());
    if (g_font_small != nullptr)
        ImGui::PopFont();

    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##lineup", &g_lineup_sel, g_lineup_items.c_str()))
        LineupSelectCode();

    ImGui::InputTextMultiline("##lineup_code", g_lineup_buf, sizeof(g_lineup_buf),
                              ImVec2(-1, 92 * g_dpi), ImGuiInputTextFlags_ReadOnly);

    if (ImGui::Button("获取代码", ImVec2(78 * g_dpi, 0)))
        LineupGetCode();
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("复制导出", ImVec2(78 * g_dpi, 0)))
        LineupCopy();
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("粘贴导入", ImVec2(78 * g_dpi, 0)))
        LineupPaste();
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("布置阵型", ImVec2(84 * g_dpi, 0)))
        LineupApply();

    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextWrapped("获取代码 = 读当前场上阵型; 复制导出 = 把代码框内容复制到剪贴板; "
                       "粘贴导入 = 从剪贴板读入并规范化; 布置阵型 = 按代码在场上种满。"
                       "代码框只读, 请用上面的按钮操作。");
    if (g_font_small != nullptr)
        ImGui::PopFont();
    EndBlock();
}

// ---------------------------------------------------------------------------
// 杂项页: 游戏 / 变速 / PAK 解包打包 / 关于
//   核心接口: SetMusic / BackgroundRunning / UserdataReadonly / UnlockLimboPage /
//             GetFrameDuration / SetFrameDuration / GamePath
//   PAK: Pt::PAK::Unpack(src.pak, dst_dir) / Pack(src_dir, dst_pak)   (pak.cpp)
//   与 FLTK 版的有意差异 (这些设置对 ImGui 版没有意义, 或已被用户否决):
//     - choice_scheme (base/plastic/gtk+/gleam/oxy): FLTK 的主题; ImGui 有自己的配色
//     - English Tooltips: "中文界面显示英文提示"; ImGui 版只有中文界面, 也没有 tooltip
//     - 截图到剪贴板: 用户决定删除 (现代 Windows 自带截图)
//     - 调试 (DebugMode): 用户实测点一下游戏会崩; 它写的就是游戏自己的 mDebugTextMode
//       (Board+0x55F8), 游戏里按 'z' 键即可循环切换, 不必由修改器提供入口
// ---------------------------------------------------------------------------
static bool g_readonly_on = false; // 禁止存档删档 (默认关, 与 FLTK 版一致)
static int  g_music_sel = 4;       // 默认 [5] Graze the Roof (与 FLTK 一致)
static int  g_speed_sel = 3;       // 默认 1x
static std::string g_misc_msg = "存档目录会在游戏目录与几个常见位置里找存在的那个打开";

// 背景音乐 (12 首, 名称与 FLTK 版一致) / 变速
static const char *kMusicNames[12] = {"[1] Grasswalk",   "[2] Moongrains",  "[3] Watery Graves",
                                      "[4] Rigor Mormist", "[5] Graze the Roof",
                                      "[6] Choose Your Seeds", "[7] Crazy Dave", "[8] Zen Garden",
                                      "[9] Cerebrawl",   "[10] Loonboon",   "[11] Ultimate Battle",
                                      "[12] Brainiac Maniac"};
static const char *kSpeedNames[7] = {"10x", "5x", "2x", "1x", "0.5x", "0.2x", "0.1x"};
static const int   kSpeedMs[7] = {1, 2, 5, 10, 20, 50, 100}; // 与 FLTK cb_speed 的表一致

static std::string g_music_buf, g_speed_buf;

// PAK 解包 / 打包 (放线程里跑, 与 DirectWin 同样的做法)
static volatile LONG g_pak_busy = 0;
static volatile LONG g_pak_done = 0; // 有结果待弹窗
static std::wstring  g_pak_text;     // 结果文本
static bool          g_pak_ok = false;
static std::wstring  g_pak_file;     // 解包源文件
static std::wstring  g_pak_dir;      // 解包目标目录 / 打包源目录

static void BuildMiscCombos()
{
    for (int i = 0; i < 12; i++)
    {
        g_music_buf += kMusicNames[i];
        g_music_buf.push_back('\0');
    }
    g_music_buf.push_back('\0');
    for (int i = 0; i < 7; i++)
    {
        g_speed_buf += kSpeedNames[i];
        g_speed_buf.push_back('\0');
    }
    g_speed_buf.push_back('\0');
}

// 长路径显示: 只留尾部, 且不切在 UTF-8 字符中间
static std::string ShortPathText(const std::wstring &w, size_t keep = 40)
{
    std::string s = Pt::utf8_encode(w);
    if (s.size() <= keep)
        return s;
    size_t start = s.size() - keep;
    while (start < s.size() && ((unsigned char)s[start] & 0xC0) == 0x80)
        start++;
    return "..." + s.substr(start);
}

// 打开存档目录: 与 FLTK 版 cb_userdata 同一套探测顺序
static void MiscOpenUserdata()
{
    bool found = false;
    auto open_dir = [&found](const std::wstring &path) {
        const DWORD fa = GetFileAttributesW(path.c_str());
        if (fa != INVALID_FILE_ATTRIBUTES && (fa & FILE_ATTRIBUTE_DIRECTORY))
        {
            ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            found = true;
        }
    };

    // 1) 游戏安装目录\userdata (只有 2000/XP 才用这个位置)
    if (g_attached)
    {
        const std::string exe = g_pvz.GamePath();
        const size_t p = exe.find_last_of("\\");
        if (p != std::string::npos)
            open_dir(Pt::utf8_decode(exe.substr(0, p + 1)) + L"userdata");
    }

    // 2) 常见存档位置
    wchar_t appdata[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, appdata)))
    {
        const wchar_t *subs[] = {L"\\PopCap Games\\BloomAndDoom\\userdata",
                                 L"\\PopCap Games\\PlantsVsZombies\\userdata",
                                 L"\\Steam\\PlantsVsZombies\\userdata",
                                 L"\\Zoo\\PlantsVsZombies\\userdata",
                                 L"\\Reflexive\\PlantsVsZombies\\userdata"};
        for (const wchar_t *s : subs)
            open_dir(std::wstring(appdata) + s);
    }

    // 3) Steam 的 userdata\<账号>\3590\remote
    std::wstring steam_userdata;
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", 0, KEY_QUERY_VALUE, &hKey) ==
        ERROR_SUCCESS)
    {
        wchar_t steam_path[MAX_PATH] = {0};
        DWORD type = REG_SZ;
        DWORD size = sizeof(steam_path);
        if (RegQueryValueExW(hKey, L"InstallPath", 0, &type, (LPBYTE)steam_path, &size) ==
            ERROR_SUCCESS)
            steam_userdata = std::wstring(steam_path) + L"\\userdata";
        RegCloseKey(hKey);
    }
    const DWORD fa = GetFileAttributesW(steam_userdata.c_str());
    if (!steam_userdata.empty() && fa != INVALID_FILE_ATTRIBUTES && (fa & FILE_ATTRIBUTE_DIRECTORY))
    {
        WIN32_FIND_DATAW fd{};
        HANDLE hFind = FindFirstFileW((steam_userdata + L"\\*").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                const std::wstring name = fd.cFileName;
                if (name == L"." || name == L".." || name == L"thumbs.db" || name == L"Thumbs.db")
                    continue;
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    open_dir(steam_userdata + L"\\" + name + L"\\3590\\remote");
            } while (FindNextFileW(hFind, &fd) != 0);
            FindClose(hFind);
        }
    }

    g_misc_msg = found ? "已打开存在的存档目录" : "没找到存档目录 (游戏可能还没建过存档)";
}

static bool PickPakFile(std::wstring &path)
{
    static wchar_t file[MAX_PATH];
    file[0] = 0;
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"PAK (*.pak)\0*.pak\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"pak";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn) == FALSE)
        return false;
    path = file;
    return true;
}

static bool PickFolder(std::wstring &path)
{
    BROWSEINFOW bi{};
    bi.hwndOwner = nullptr;
    bi.lpszTitle = L"选择要打包的文件夹：";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl == nullptr)
        return false;
    wchar_t buf[MAX_PATH] = {0};
    const bool ok = SHGetPathFromIDListW(pidl, buf) != FALSE;
    CoTaskMemFree(pidl);
    if (!ok)
        return false;
    path = buf;
    return true;
}

// 打包输出: 源目录的上一级 + main_<时间戳>.pak (与 FLTK 版一致)
static std::wstring PakPackTarget()
{
    const size_t p = g_pak_dir.find_last_of(L"\\/");
    const std::wstring parent = (p == std::wstring::npos) ? g_pak_dir : g_pak_dir.substr(0, p);
    return parent + L"\\main_" + std::to_wstring((long long)std::time(nullptr)) + L".pak";
}

static DWORD WINAPI PakWorker(LPVOID param)
{
    const int mode = (int)(intptr_t)param; // 1 = 解包, 2 = 打包
    Pt::PAK pak;
    int ret = 0x7FFFFFFF;
    std::wstring target;

    if (mode == 1)
    {
        target = g_pak_dir;
        ret = pak.Unpack(g_pak_file, g_pak_dir);
    }
    else
    {
        target = PakPackTarget();
        ret = pak.Pack(g_pak_dir, target);
    }

    // 结果文本 (错误码与 FLTK 版一致)
    std::wstring text;
    if (mode == 1)
    {
        switch (ret)
        {
        case UNPACK_SRC_NOT_EXIST:      text = L"打开源文件失败！"; break;
        case UNPACK_SRC_SIZE_ERROR:     text = L"获取源文件大小失败！"; break;
        case UNPACK_SRC_LOAD_ERROR:     text = L"读取源文件内容失败！"; break;
        case UNPACK_SRC_HEADER_ERROR:   text = L"文件头格式不正确！"; break;
        case UNPACK_SRC_DATA_ERROR:     text = L"文件数据已经损坏！"; break;
        case UNPACK_PATH_CREATE_ERROR:  text = L"解包路径创建失败！"; break;
        case UNPACK_FILE_CREATE_ERROR:  text = L"解包文件创建失败！"; break;
        case UNPACK_FILE_WRITE_ERROR:   text = L"解包文件写入失败！"; break;
        case UNPACK_SUCCESS:            text = L"解包完成, 文件夹位于：\n" + target; break;
        default:                        text = L"未知结果"; break;
        }
    }
    else
    {
        switch (ret)
        {
        case PACK_SRC_NOT_EXIST:      text = L"打开源文件夹失败！"; break;
        case PACK_SRC_EMPTY_ERROR:    text = L"源文件夹为空！"; break;
        case PACK_PATH_CREATE_ERROR:  text = L"打包路径创建失败！"; break;
        case PACK_FILE_CREATE_ERROR:  text = L"打包文件创建失败！"; break;
        case PACK_FILE_WRITE_ERROR:   text = L"打包文件写入失败！"; break;
        case PACK_SRC_READ_ERROR:     text = L"打包源文件读取失败！"; break;
        case PACK_SUCCESS:            text = L"打包完成, 文件位于：\n" + target; break;
        default:                      text = L"未知结果"; break;
        }
    }

    g_pak_ok = (ret == (mode == 1 ? UNPACK_SUCCESS : PACK_SUCCESS));
    g_pak_text = text;
    InterlockedExchange(&g_pak_done, 1);
    InterlockedExchange(&g_pak_busy, 0);
    return 0;
}

// 主循环里轮询: 解包/打包完成后弹结果 (不占着界面线程)
static void MiscPollPak()
{
    if (InterlockedCompareExchange(&g_pak_done, 0, 1) != 1)
        return;
    MessageBoxW(nullptr, g_pak_text.c_str(), g_pak_ok ? L"完成" : L"出错",
                MB_OK | (g_pak_ok ? MB_ICONINFORMATION : MB_ICONWARNING));
}

static void MiscStartPak(int mode)
{
    if (mode == 1 && (g_pak_file.empty() || g_pak_dir.empty()))
    {
        MessageBoxW(nullptr, L"请先选择 .pak 文件和输出文件夹。", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }
    if (mode == 2 && g_pak_dir.empty())
    {
        MessageBoxW(nullptr, L"请先选择要打包的文件夹。", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }
    if (InterlockedCompareExchange(&g_pak_busy, 1, 0) != 0)
        return; // 上一次还没跑完
    HANDLE hThread = CreateThread(nullptr, 0, PakWorker, (LPVOID)(intptr_t)mode, 0, nullptr);
    if (hThread != nullptr)
        CloseHandle(hThread);
    else
        InterlockedExchange(&g_pak_busy, 0);
}

static void ShowAbout()
{
    // 构建戳来自构建脚本 (见文件头的 PZTK_BUILD_DATE), 形如 "2026-10-10"。
    // 不再用 __DATE__/__TIME__: 那会让同一份源码每次编出不同的二进制。
    const std::string date = PZTK_BUILD_DATE;
    // 版权年份取构建戳的前 4 位; 不是 "YYYY-..." 形式时回退到固定值。
    const bool iso = date.size() >= 4 &&
                     date[0] >= '0' && date[0] <= '9' &&
                     date[1] >= '0' && date[1] <= '9' &&
                     date[2] >= '0' && date[2] <= '9' &&
                     date[3] >= '0' && date[3] <= '9';
    std::string year = iso ? date.substr(0, 4) : std::string("2026");

    std::wstring text;
    text += L"植物大战僵尸辅助工具 · PvZ Toolkit Dx\n";
    text += L"发行版本：\t" + Pt::utf8_decode(VERSION_NAME) + L" (" +
            std::to_wstring((long long)VERSION_BUILD) + L")\n";
    text += L"编译时间：\t" + Pt::utf8_decode(date) + L"\n\n";
    text += L"原版版本：\tPvZ Toolkit 1.22.0 (2024-06-22)\n";
    text += L"原作者：\tlmintlcx (PvZ Toolkit, GPL-3.0)\n\n";
    text += L"原版官网：\thttps://pvz.lmintlcx.com/\n";
    text += L"版权归属：\t© 2020 ~ " + Pt::utf8_decode(year) + L" L.Mint. LCX\n\n";
    text += L"修改版作者：\tSilenceWoods\n\n";
    text += L"最初参考：\tPVZ Helper 1.8.7\n";
    text += L"原始作者：	zhumxiang @@ GitHub\n\n";
    text += L"本版为社区增强版(界面重做 + 功能增补), 非官方发布。\n";
    text += L"仅供单机娱乐与学习交流: 请勿用于联机作弊, 请勿出售或商用。\n";
    text += L"详细说明 / 许可 / 免责声明见程序目录下的「使用说明.txt」(GPL-3.0)。\n";

    MessageBoxW(nullptr, text.c_str(), L"关于 PvZ Toolkit Dx", MB_OK | MB_ICONINFORMATION);
}

// 「文档」按钮: 打开与程序同目录的使用说明 (发布包里随附 使用说明.txt)
static void MiscOpenManual()
{
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    const size_t slash = path.find_last_of(L"\\/");
    path.resize(slash == std::wstring::npos ? 0 : slash + 1);
    path += L"使用说明.txt";
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxW(nullptr, L"没找到「使用说明.txt」—— 它应与本程序放在同一目录。", L"PvZ Toolkit Dx",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void PageMisc()
{
    if (g_music_buf.empty())
        BuildMiscCombos();

    // ---- 游戏 ----
    BeginBlock("游戏", 180 * g_dpi);
    ImGui::SetNextItemWidth(-96 * g_dpi);
    ImGui::Combo("##music", &g_music_sel, g_music_buf.c_str());
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("背景音乐", ImVec2(88 * g_dpi, 0)) && g_attached)
        g_pvz.SetMusic(g_music_sel + 1);

    if (ImGui::Button("存档文件夹", ImVec2(96 * g_dpi, 0)))
        MiscOpenUserdata();
    ImGui::SameLine(0.0f, 10.0f);
    if (ImGui::Checkbox("允许后台运行", &g_background))
        if (g_attached)
            g_pvz.BackgroundRunning(g_background);

    if (ImGui::Checkbox("禁止存档删档", &g_readonly_on))
        if (g_attached)
            g_pvz.UserdataReadonly(g_readonly_on);
    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Checkbox("显示隐藏页面", &g_limbo))
        if (g_attached)
            g_pvz.UnlockLimboPage(g_limbo);

    // 高级暂停: 不是主动暂停游戏, 而是把**游戏自带的暂停(按空格)**变成高级暂停
    ImGui::Checkbox("高级暂停", &g_adv_pause);
    ImGui::SameLine(0.0f, 8.0f);
    {
        std::string st;
        if (g_pvz.AdvPausePatched())
            st = g_pvz.AdvPauseActive() ? "已生效 · 当前: 暂停中 (再按空格继续)"
                                        : "已生效 · 当前: 运行中 (按空格暂停)";
        else if (!g_attached)
            st = "未安装 (先连接游戏)";
        else
            st = g_pvz.AdvPauseMsg().empty() ? "未安装" : g_pvz.AdvPauseMsg();
        if (g_font_small != nullptr)
            ImGui::PushFont(g_font_small);
        ImGui::TextWrapped("%s", st.c_str());
        if (g_font_small != nullptr)
            ImGui::PopFont();
    }
    // ★ 常见误会提示(用户实机反馈, 纯提示, 不做任何判断):
    //   点了修改器窗口后**键盘焦点在工具上**, 这时按空格完全没反应, 看起来像"高级暂停失效"。
    //   其实是正常现象 —— 点一下游戏画面即可。(游戏的 LostFocus 自动暂停又被 !mTodCheatKeys
    //   挡掉了, 连暂停框都不弹, 所以更难判断。)
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
    ImGui::TextWrapped("提示: 若按空格没反应, 先点一下游戏画面(键盘焦点必须在游戏里)");
    ImGui::PopStyleColor();
    if (g_font_small != nullptr)
        ImGui::PopFont();
    ImGui::SetItemTooltip(
        "勾上后把游戏自带的暂停(按空格)变成高级暂停:\n"
        "不弹暂停对话框、鼠标与种植预览照常、选卡/种植/铲子/工具都能用、音乐不停,\n"
        "手上拿着的卡片、加农炮落点、种植预览(幽灵植物)都会照常跟随鼠标;\n"
        "但特效与整个世界都冻住 (粒子/掉阳光/池边闪光/僵尸/植物/子弹/刷怪/雾/关卡结束)。\n"
        "再按一次空格继续。\n"
        ""
        ""
        ""
        "");

    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextWrapped("%s", g_misc_msg.c_str());
    if (g_font_small != nullptr)
        ImGui::PopFont();
    EndBlock();

    // ---- 变速 ----
    // "调试"那一项删掉了: 它就是游戏自己的 mDebugTextMode (Board+0x55F8,
    // ConstEnums.h 里的 DebugTextMode: 出怪/音乐/内存/碰撞 文字叠加), 游戏里
    // 按 'z' 键就能循环切换 —— 而且用户实测点它会把游戏搞崩 (很可能是版本偏移
    // 不匹配时写坏了内存), 风险大于收益, 所以不再提供入口。
    BeginBlock("变速", 62 * g_dpi);
    ImGui::SetNextItemWidth(120 * g_dpi);
    ImGui::Combo("##speed", &g_speed_sel, g_speed_buf.c_str());
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("变速", ImVec2(64 * g_dpi, 0)) && g_attached)
        g_pvz.SetFrameDuration(kSpeedMs[g_speed_sel]);
    ImGui::SameLine(0.0f, 8.0f);
    if (g_attached)
        ImGui::TextDisabled("当前 %d ms/帧", g_pvz.GetFrameDuration());
    else
        ImGui::TextDisabled("未连接游戏");
    EndBlock();

    // ---- 智慧树指令 ----
    // 勾选 = 向游戏窗口发一次对应按键序列 (与玩家在关卡里手敲一致), 再点一次取消。
    // 之所以走按键而不是直接写标志位: 游戏的 SetMustacheMode/SetFutureMode/SetSuperMowerMode
    // 会**逐僵尸/逐小推车**改 reanim, 直接写标志位对场上已有的不生効。
    BeginBlock("智慧树指令", 216 * g_dpi);
    for (int i = 0; i < Pt::PvZ::WisdomCodeCount(); i++)
    {
        ImGui::PushID(900 + i);
        char label[160];
        std::snprintf(label, sizeof(label), "%s（%s）", Pt::PvZ::WisdomCodeText(i),
                      Pt::PvZ::WisdomCodeLabel(i));
        if (ImGui::Checkbox(label, &g_wisdom_checked[i]))
        {
            g_wisdom_pending[i] = true;
            g_wisdom_result[i] = 0; // 新的操作: 清掉上一次的核对结果
            g_wisdom_touched = true; // 用户手动点过: 之后不再自动回显, 免得覆盖他的操作
        }
        if (g_wisdom_hold[i] > 0)
        {
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextDisabled("(已发送)");
            g_wisdom_hold[i]--;
        }
        else if (g_wisdom_result[i] == 2)
        {
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextColored(kOk, "(已生效)");
        }
        else if (g_wisdom_result[i] == 1)
        {
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextColored(kBad, "(未生效)");
            if (ImGui::IsItemHovered())
                ImGui::SetItemTooltip("发出指令后回读游戏状态发现它没有生效, 已自动取消勾选。\n"
                                      "pinata / dance / daisies 需要智慧树高度分别≥1000 / 500 / 100 "
                                      "(可在「资源」页设置)。");
        }
        ImGui::PopID();
    }
    EndBlock();



    // ---- PAK 解包 / 打包 ----
    BeginBlock("PAK 解包 / 打包", 176 * g_dpi);
    if (ImGui::Button("选择pak文件", ImVec2(106 * g_dpi, 0)))
    {
        std::wstring path;
        if (PickPakFile(path))
            g_pak_file = path;
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("选择输出/输入目录", ImVec2(150 * g_dpi, 0)))
    {
        std::wstring path;
        if (PickFolder(path))
            g_pak_dir = path;
    }

    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextDisabled("%s", g_pak_file.empty() ? "(未选择pak文件)"
                                                 : ShortPathText(g_pak_file).c_str());
    ImGui::TextDisabled("%s", g_pak_dir.empty() ? "(未选择目录)"
                                                : ShortPathText(g_pak_dir).c_str());
    if (g_font_small != nullptr)
        ImGui::PopFont();

    if (ImGui::Button("解包", ImVec2(72 * g_dpi, 0)))
        MiscStartPak(1);
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("打包", ImVec2(72 * g_dpi, 0)))
        MiscStartPak(2);

    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextDisabled("解包：选择pak和输出目录");
    ImGui::TextDisabled("打包：选择输入目录（输出和pak同级）");
    if (g_font_small != nullptr)
        ImGui::PopFont();

    ImGui::SameLine(0.0f, 8.0f);
    if (g_pak_busy)
        ImGui::TextDisabled("处理中, 请稍候 ...");
    EndBlock();

    // ---- 关于 ----
    BeginBlock("关于", 96 * g_dpi);
    if (ImGui::Button("文档", ImVec2(72 * g_dpi, 0)))
        MiscOpenManual();
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("关于 ...", ImVec2(84 * g_dpi, 0)))
        ShowAbout();
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextWrapped("PvZ Toolkit Dx %s (%d) · 核心层复用 lmintlcx 原版 (GPL-3.0)",
                       VERSION_NAME, (int)VERSION_BUILD);
    if (g_font_small != nullptr)
        ImGui::PopFont();
    EndBlock();
}

// ---------------------------------------------------------------------------
// 关卡设置页: 出怪方式 + 僵尸种类 + 关卡模块
//   核心接口: InternalSpawn / CustomizeSpawn / MixMode / EndlessRounds /
//             UnlockTrophy / DirectWin
//   出怪是否写入由核心层自己判断 (只在选卡界面或战斗中生效), 界面不重复判断。
// ---------------------------------------------------------------------------
// 与 FLTK 版 spawn_type[20] 完全一致的 20 种可勾选僵尸
static const int kSpawnTypes[20] = {2,  3,  4,  5,  6,  7,  8,  11, 12, 14,
                                    15, 16, 17, 18, 19, 20, 21, 22, 23, 32};

// 关卡模式表 (73 项, 逐字对照 FLTK 版 window.cpp 的 modes_zh)
static const char *kModesZh[73] = {
    "冒险模式",
    "生存模式: 白天",
    "生存模式: 黑夜",
    "生存模式: 泳池",
    "生存模式: 雾夜",
    "生存模式: 屋顶",
    "生存模式: 白天 (困难)",
    "生存模式: 黑夜 (困难)",
    "生存模式: 泳池 (困难)",
    "生存模式: 雾夜 (困难)",
    "生存模式: 屋顶 (困难)",
    "生存模式: 白天 (无尽)",
    "生存模式: 黑夜 (无尽)",
    "生存模式: 无尽",
    "生存模式: 雾夜 (无尽)",
    "生存模式: 屋顶 (无尽)",
    "植物僵尸",
    "坚果保龄球",
    "老虎机",
    "雨中种植物",
    "宝石迷阵",
    "隐形食脑者",
    "看星星",
    "僵尸水族馆",
    "宝石迷阵转转看",
    "小僵尸大麻烦",
    "保护传送门",
    "你看, 他们像柱子一样",
    "雪橇区",
    "僵尸快跑",
    "锤僵尸",
    "谁笑到最后",
    "植物僵尸 2",
    "坚果保龄球 2",
    "跳跳舞会",
    "僵王博士的复仇",
    "坚果艺术",
    "晴天",
    "无草皮之地",
    "重要时间",
    "太阳花艺术",
    "空袭",
    "冰冻关卡",
    "禅境花园",
    "超乎寻常的压力",
    "坟墓模式",
    "你能把它挖出来吗?",
    "暴风雨之夜",
    "蹦极闪电战",
    "松鼠",
    "智慧树",
    "破罐者",
    "一路向左",
    "第三个罐子",
    "连锁反应",
    "M 的意思是金属",
    "胆怯的制陶工",
    "变戏法",
    "另一个连锁反应",
    "罐子王牌",
    "无尽的试炼",
    "我是僵尸",
    "我也是僵尸",
    "你能把它挖出来吗?",
    "完全傻了",
    "死亡飞艇",
    "我烂了!",
    "僵尸摇摆",
    "三连击",
    "你所有大脑都是属于我的",
    "我是僵尸无尽版",
    "促销",
    "介绍",
};

// 直接过关要等"炮最亮"的那个帧, 核心层内部会 Sleep 若干帧;
// FLTK 版把它放在线程里, 这里同样用 Win32 线程, 避免界面主循环卡住。
static volatile LONG g_direct_win_busy = 0;
static bool g_direct_win_brightest = true;
static DWORD WINAPI DirectWinProc(LPVOID)
{
    g_pvz.DirectWin(g_direct_win_brightest);
    InterlockedExchange(&g_direct_win_busy, 0);
    return 0;
}

// 按当前模式与勾选生成出怪列表 (出怪方式板块与僵尸种类板块的应用按钮共用)
static void SpawnApply(int mode, const bool *picked, bool limit_giga, int giga_idx)
{
    if (!g_attached)
        return;
    std::array<bool, 33> zombies{};
    for (int i = 0; i < 20; i++)
        zombies[kSpawnTypes[i]] = picked[i];
    zombies[0] = true; // 普僵必须勾上, 否则列表生成不出来
    if (mode == 0)
    {
        g_pvz.InternalSpawn(zombies);
    }
    else
    {
        zombies[1] = true;
        g_pvz.CustomizeSpawn(zombies, limit_giga, mode == 2, 1000 + 100 * giga_idx);
    }
    // 详情窗开着就顺手刷新 (与 FLTK 版 cb_set_spawn 的行为一致)
    if (g_sub_spawn.win != nullptr && !g_sub_spawn.hidden)
        SpawnRefresh();
}

static void PageLevel()
{
    static int  v_mode = 0;        // 默认: 自然 (游戏自己生成)       // 0=自然 1=极限 (默认) 2=模拟
    static bool v_zombies[20] = {};
    static int  v_giga_idx = 0;   // 红眼权重 = 1000 + 100 * idx
    static bool v_limit_giga = false;
    static int  v_mix_mode = 13;  // 生存模式: 无尽
    static int  v_mix_level = 39; // 4 - 10
    static int  v_rounds = 1010;

    // 下拉内容只构造一次
    static std::string s_modes, s_levels, s_gigas;
    if (s_modes.empty())
    {
        char buf[128];
        for (int i = 0; i < 73; i++)
        {
            std::snprintf(buf, sizeof(buf), "[%d] %s", i, kModesZh[i]);
            s_modes += buf;
            s_modes.push_back('\0');
        }
        for (int j = 1; j <= 5; j++)
            for (int k = 1; k <= 10; k++)
            {
                std::snprintf(buf, sizeof(buf), "%d - %d", j, k);
                s_levels += buf;
                s_levels.push_back('\0');
            }
        for (int w = 1000; w <= 6000; w += 100)
        {
            std::snprintf(buf, sizeof(buf), "%d", w);
            s_gigas += buf;
            s_gigas.push_back('\0');
        }
        s_modes.push_back('\0');
        s_levels.push_back('\0');
        s_gigas.push_back('\0');
    }

    // ---- 出怪方式 ----
    BeginBlock("出怪方式", 152 * g_dpi);

    ImGui::RadioButton("自然", &v_mode, 0);
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::RadioButton("极限", &v_mode, 1);
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::RadioButton("模拟", &v_mode, 2);

    if (v_mode != 2)
        ImGui::BeginDisabled(); // 红眼权重只在"模拟"下有效
    ImGui::SetNextItemWidth(96 * g_dpi);
    ImGui::Combo("##giga", &v_giga_idx, s_gigas.c_str());
    if (v_mode != 2)
        ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextUnformatted("红眼权重");
    ImGui::SameLine(0.0f, 10.0f);
    if (v_mode == 0)
        ImGui::BeginDisabled(); // 自然出怪不涉及红眼限制
    ImGui::Checkbox("11~19 波不含红眼", &v_limit_giga);
    if (v_mode == 0)
        ImGui::EndDisabled();

    const char *apply_label = (v_mode == 0) ? "自然出怪 (游戏生成)"
                              : (v_mode == 1) ? "极限出怪 (均匀填充)"
                                              : "模拟出怪 (加权随机)";
    if (ImGui::Button(apply_label, ImVec2(200 * g_dpi, 0)))
        SpawnApply(v_mode, v_zombies, v_limit_giga, v_giga_idx);
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("出怪详情 ...", ImVec2(110 * g_dpi, 0)))
    {
        g_spawn_refresh_pending = true; // 打开时先刷新一次
        g_want_spawn = true;
    }

    // 多倍出怪 (出怪点数 ×N): 改的是游戏**自己**那个"补满循环"的预算, 挑哪种僵尸
    // (权重表 / 首现波限制 / 无尽权重曲线 / 50 只上限) 全部照游戏原样跑。
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("多倍出怪");
    g_wave_mul_on = (g_wave_mul_n > 1); // 1 = 关闭 (不再用勾选框, 由数值派生)
    ImGui::SameLine(0.0f, 8.0f);
    if (v_mode != 0)
        ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(80 * g_dpi);
    if (ImGui::InputInt("##wave_mul_n", &g_wave_mul_n, 1, 4))
    {
        if (g_wave_mul_n < 1)
            g_wave_mul_n = 1;
        if (g_wave_mul_n > kWaveMulMax)
            g_wave_mul_n = kWaveMulMax;
    }
    if (v_mode != 0)
        ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 6.0f);
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextUnformatted("倍点数 (1 = 关闭; 仅『自然』模式可调)");
    ImGui::SetItemTooltip(
        "把游戏自己的出怪点数预算乘 N 倍, 挑哪种僵尸仍由游戏决定。\n"
        "固定刷出的僵尸 (旗帜波先塞的普僵、新僵尸介绍、末波巨人、末波补齐全种类)\n"
        "不参与倍增; 每波仍受 50 只上限。\n"
        ""
        "");
    if (g_font_small != nullptr)
        ImGui::PopFont();

    // 状态行: 关键状态直接显示成一行字 (不藏在悬停提示里)
    {
        std::string wm = "多倍出怪: " + g_pvz.WaveMulMsg();
        if (g_pvz.WaveMulMsg().empty())
            wm = "多倍出怪: 未启用";
        if (g_font_small != nullptr)
            ImGui::PushFont(g_font_small);
        ImGui::TextWrapped("%s", wm.c_str());
        if (g_font_small != nullptr)
            ImGui::PopFont();
    }
    EndBlock();

    // ---- 僵尸种类 (20 项, 4 列 x 5 行; "清空已选"与"应用"放在本板块左上角,
    //      左下角放僵尸血量倍数) ----
    BeginBlock("僵尸种类", 228 * g_dpi);
    if (ImGui::Button("清空已选", ImVec2(84 * g_dpi, 0)))
        for (int i = 0; i < 20; i++)
            v_zombies[i] = false;
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("应用", ImVec2(84 * g_dpi, 0)))
        SpawnApply(v_mode, v_zombies, v_limit_giga, v_giga_idx);
    if (ImGui::BeginTable("spawn_kinds", 4, ImGuiTableFlags_SizingStretchSame))
    {
        for (int i = 0; i < 20; i++)
        {
            if (i % 4 == 0)
                ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(i % 4);
            ImGui::PushID(i);
            ImGui::Checkbox(Pt::zombies_zh[kSpawnTypes[i]], &v_zombies[i]);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // 左下角: 僵尸血量倍数 (1.00 = 原版; 新出现的僵尸按倍数改血量上限与当前血量)
    // 按用户要求: 文字加大并放在最左边
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("僵尸血量倍数");
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetNextItemWidth(84 * g_dpi);
    ImGui::InputFloat("##hp_mul", &g_zombie_hp_mul, 0.5f, 1.0f, "%.2f",
                      ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll);
    if (g_zombie_hp_mul < 0.05f)
        g_zombie_hp_mul = 0.05f;
    if (g_zombie_hp_mul > 100.0f)
        g_zombie_hp_mul = 100.0f;
    ImGui::SameLine(0.0f, 6.0f);
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextDisabled("(1.00 = 原版)");
    ImGui::SetItemTooltip("只影响之后新出现的僵尸; 改回 1.00 会还原已经改过的僵尸。");
    if (g_font_small != nullptr)
        ImGui::PopFont();
    EndBlock();

    // ---- 关卡 ----
    BeginBlock("关卡", 140 * g_dpi);

    ImGui::SetNextItemWidth(150 * g_dpi);
    ImGui::Combo("##mix_mode", &v_mix_mode, s_modes.c_str());
    ImGui::SameLine(0.0f, 6.0f);

    if (v_mix_mode != 0)
        ImGui::BeginDisabled(); // 冒险模式的关卡号才有意义
    ImGui::SetNextItemWidth(108 * g_dpi);
    ImGui::Combo("##mix_level", &v_mix_level, s_levels.c_str());
    if (v_mix_mode != 0)
        ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("混乱关卡", ImVec2(84 * g_dpi, 0)) && g_attached)
        g_pvz.MixMode(v_mix_mode, v_mix_mode == 0 ? v_mix_level + 1 : v_mix_level);

    ImGui::SetNextItemWidth(110 * g_dpi);
    if (ImGui::InputInt("##rounds", &v_rounds, 0, 0))
    {
        if (v_rounds < 0)
            v_rounds = 0;
        if (v_rounds > 999999)
            v_rounds = 999999;
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("无尽轮数", ImVec2(84 * g_dpi, 0)) && g_attached)
        g_pvz.EndlessRounds(v_rounds);
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::TextDisabled("(仅无尽模式)");

    // 第 3 行: 通关存档 (解锁所有奖杯)。★ 2026-10-15 按用户要求: 「直接过关」从这一行挪到第 4 行。
    if (ImGui::Button("通关存档", ImVec2(84 * g_dpi, 0)) && g_attached)
        g_pvz.UnlockTrophy();
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::TextDisabled("(解锁所有奖杯)");

    // 第 4 行: 直接过关 (单独一行, 与上一行隔开, 免得误点)
    // (原「刚结束时炮最亮」勾选框已按用户要求移除: 该选项固定为开, 见 g_direct_win_brightest)
    if (ImGui::Button("直接过关", ImVec2(84 * g_dpi, 0)) && g_attached)
    {
        // 防止连点: 上一次还没跑完就不再开新线程
        if (InterlockedCompareExchange(&g_direct_win_busy, 1, 0) == 0)
        {
            g_direct_win_brightest = true; // 固定为开 (勾选框已移除)
            HANDLE hThread = CreateThread(nullptr, 0, DirectWinProc, nullptr, 0, nullptr);
            if (hThread != nullptr)
                CloseHandle(hThread);
            else
                InterlockedExchange(&g_direct_win_busy, 0);
        }
    }
    EndBlock();

    // ---- 说明 ----
    BeginBlock("说明", 100 * g_dpi);
    if (g_font_small != nullptr)
        ImGui::PushFont(g_font_small);
    ImGui::TextWrapped("出怪修改只在『选卡界面』或『战斗中』生效, 其它界面不会写入游戏。");
    ImGui::TextWrapped("自然 = 游戏自己生成出怪; 极限 = 均匀填充僵尸直到达到出怪上限; "
                       "模拟 = 按权重随机填充僵尸直到达到出怪上限; 这三种都是选择已勾选的出怪种类进行生成。");
    if (g_font_small != nullptr)
        ImGui::PopFont();
    EndBlock();
}

// ---------------------------------------------------------------------------
// 连接游戏
//   FLTK 版的连接方式是 main() 里 FindPvZ() + 之后每次 GameOn() 内部会自动重查找,
//   也就是"游戏晚于修改器启动也能连上"; 这里用同样的做法, 未连接时定时重试。
//   连上以后应用一次默认开关 (FLTK 的 Window::keep_selected_feature 会按默认勾选
//   状态应用一遍, 默认开的只有"允许后台运行"和"显示隐藏页面"两项) —— 前者关系到
//   "点修改器时游戏会不会被暂停"。
// ---------------------------------------------------------------------------
static void ApplyGameToggles()
{
    // 连上游戏后, 把界面上这两个开关的当前状态同步给游戏。FLTK 版在启动时做的
    // Window::keep_selected_feature() 就是这件事 —— 默认勾选的"允许后台运行"
    // 因此生效, 点修改器时游戏不会被暂停。
    g_pvz.BackgroundRunning(g_background);
    g_pvz.UnlockLimboPage(g_limbo);
    // 其它修改板块的开关由 WireToggles 按"变化"写入; 连接后让它重新写一遍,
    // 这样"先开修改器勾好开关、再开游戏"也能生效。
    g_reapply_toggles = true;
}

static void TryAttachGame()
{
    // FindPvZ 会先按已知窗口标题逐个试, 再做一次"任意 MainWindow"兜底, 与 FLTK 一致
    const bool found = g_pvz.FindPvZ();
    if (found && g_pvz.IsValid() && g_pvz.RandomFixSupported())
    {
        g_attached = true;
        g_attach_msg = "已连接";
        ApplyGameToggles();
        return;
    }
    g_attached = false;
    g_attach_msg = "未找到游戏窗口";
}

int main()
{
    // 必须在建窗之前: 让窗口像素 == ImGui 逻辑像素
    SetProcessDPIAware();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    if (!glfwInit())
        return 1;

    float msx = 1.0f, msy = 1.0f;
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &msx, &msy);
    if (msy < 1.0f)
        msy = 1.0f;

    GLFWwindow *window = glfwCreateWindow((int)(kWinW * msx), (int)(kWinH * msy), "PvZ Toolkit Dx",
                                          nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeLimits(window, (int)(380 * msx), (int)(480 * msy), (int)(kWinWMax * msx),
                            GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    float xs = 1.0f, ys = 1.0f;
    glfwGetWindowContentScale(window, &xs, &ys);
    g_dpi = (ys > 1.0f) ? ys : 1.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiContext *main_ctx = ImGui::GetCurrentContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.FontGlobalScale = 0.95f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImFontConfig fcfg;
    fcfg.OversampleH = 1;
    fcfg.OversampleV = 1;
    fcfg.PixelSnapH = true;
    g_font_small = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 15.0f * g_dpi,
                                                &fcfg, GlyphRangesWithLineups());
    // 资源页的阳光/金币/智慧树用大一号 (ImGui 1.91 的 PushFont 只收字体指针, 不能改字号)
    g_font_big = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 19.0f * g_dpi,
                                              &fcfg, GlyphRangesWithLineups());
    // 板块 / 顶栏标题的粗体 (msyhbd.ttc); 缺字体时为 nullptr, 用到的两处都判空
    g_font_title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyhbd.ttc", 16.0f * g_dpi,
                                                &fcfg, GlyphRangesWithLineups());
    ImFont *font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f * g_dpi,
                                                &fcfg, GlyphRangesWithLineups());
    if (font == nullptr)
        font = io.Fonts->AddFontDefault();
    io.Fonts->Build();
    io.FontDefault = font;

    ApplyStyle(g_dpi);

    TryAttachGame(); // 启动时连一次; 之后未连接会定时重试 (游戏晚开也能连上)

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // exe 目录下的 *.yml 阵型列表自动加载 (setups.yml 里写明的约定)
    LineupAutoLoad();

    // 移速窗默认值
    for (int i = 0; i < Pt::ZSC_COUNT; i++)
        g_zval[i] = Pt::zombie_speed_classes[i].min;

    // 植物攻速窗默认值: 原来没初始化, 界面上全是 0 (0 不在合法区间里, 看着像坏了)。
    // 按用户要求取"范围最小值": 攻击间隔取 counter_min, 首发延迟取 first_min
    // (后者与 FLTK 版一致; FLTK 的攻击间隔默认用的是游戏原值 counter_default)。
    for (int i = 0; i < Pt::PSC_COUNT; i++)
    {
        g_pint[i] = (float)Pt::plant_speed_classes[i].counter_min / 100.0f;
        g_wsec[i] = (float)Pt::plant_speed_classes[i].first_min / 100.0f;
    }

    // 自动化验证钩子: PZTK_START_PAGE=0..4 指定启动页 (默认 0 = 战场)。
    // 只为截图/回归验证方便, 不影响正常使用。
    if (const char *sp = std::getenv("PZTK_START_PAGE"))
    {
        int n = std::atoi(sp);
        if (n >= 0 && n < kPageCount)
            g_page = n;
    }

    // 自动化验证钩子: PZTK_OPEN_SPAWN=1 直接打开出怪详情窗;
    // 若给的是 .zbl 路径 (UTF-8), 则先把它读进来显示 (只显示, 不写游戏内存)。
    if (const char *os = std::getenv("PZTK_OPEN_SPAWN"))
    {
        g_want_spawn = true;
        if (os[0] != '\0' && std::strcmp(os, "1") != 0)
        {
            const int need = MultiByteToWideChar(CP_UTF8, 0, os, -1, nullptr, 0);
            if (need > 0)
            {
                std::wstring wp((size_t)need, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, os, -1, &wp[0], need);
                if (!wp.empty() && wp.back() == L'\0')
                    wp.pop_back();
                SpawnLoadFile(wp.c_str());
            }
        }
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // ============ 第 1 段: 主窗完整渲染 ============
        glfwMakeContextCurrent(window);
        ImGui::SetCurrentContext(main_ctx);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!g_attached && glfwGetTime() >= g_attach_next_try)
        {
            g_attach_next_try = glfwGetTime() + 1.0; // 1 秒重试一次 (FLTK 是 0.2~0.4s)
            TryAttachGame();
        }
        if (g_attached)
        {
            // ★ 游戏换进程 = 关掉又重开: 注入策略的记忆也要归零。
            //   只靠"断开一帧"会漏掉**快速重启**(工具没看到断开帧), 那时策略还停在
            //   "已注入" ⇒ 新进程里永远不会再注入。这里用进程号变化兜住 (与智慧树回显同一招)。
            static uint32_t last_hook_pid = 0;
            if (g_pvz.GamePid() != last_hook_pid)
            {
                last_hook_pid = g_pvz.GamePid();
                g_inject_policy.Reset();
            }

            // ---- 自动注入帧级钩子 (玩家不需要点任何按钮) ★ v40 改策略 ★ ----
            //   连上游戏后自动定位 Plant::UpdateShooter 并注入 pztk_hook.dll。
            //   判据全部在**共享头纯函数** `PtHook::HookInjectPolicy` 里 (可离线断言),
            //   这里只负责"把 `alive_now` 与"想不想要"传进去、按返回值调 InjectHookDll()"。
            //
            //   ★ 用户要求 (v40, 唯一事实来源):
            //     ① **不要区分关卡内外** —— 只要没注入成功就**持续重试, 直到成功**
            //        (v39 的"关卡内只执行一次 / 多次请求合并"两条已删除: 它们会让
            //         "关卡内没注进去"变成永远不再试);
            //     ② **已经注入成功时不要重复装卸** ⇒ `alive_now` 为真时策略永远返回 false
            //        (反复 `UnloadHookDll()`+`LoadLibraryW` 就是用户看到的"横跳");
            //     ③ **同一时刻只允许一个尝试在飞**: `InjectHookDll()` 是同步的, 而策略只在
            //        冷却到期那一帧返回 true ⇒ 冷却窗口内的多次请求合并成一次
            //        (`merged_requests` 诊断计数)。
            //   ★ 与 v38 的"心跳"机制的关系: 心跳新鲜度**没有**被引回来 ——
            //     "DLL 什么时候卸载自己"仍然只看 `CdUnloadDecision`(工具进程存活 / 明确要求卸载);
            //     本策略只管"工具要不要**重新注入**"。拖动主窗口导致心跳停 ⇒ 既不会卸载、
            //     也不会在这里触发重注入 (HookAlive() 仍是真)。
            EnsureCdModeInit(); // 幂等 (保持"每帧一次"的既有顺序)
            // 注意变量名: 下面首发延迟那段已经有一个 `hook_alive` (同一作用域), 别重名
            const bool hook_alive_now = g_pvz.HookAlive();
            // `request` 的语义 = "此刻希望 DLL 在工作而它没在" —— 只用来数"被合并掉的请求"
            const bool need_hook = g_inject_policy.WantInject(hook_alive_now, glfwGetTime());
            if (g_inject_policy.Apply(hook_alive_now, need_hook, glfwGetTime()))
            {
                const uintptr_t addr = g_pvz.FindUpdateShooter();
                if (addr != 0)
                    g_pvz.InjectHookDll(HookDllPath(), addr);
            }

            // 首发延迟交给钩子在游戏帧内执行 (方案 B):
            //   把「植物攻速」窗口里的首发延迟设置下发给 DLL, 同时**屏蔽核心的老路径**,
            //   避免 60 Hz 的外部写入和帧内写入打架。
            //   注意用 HookAlive() 而不是 HookInjected(): 只有 DLL 真的在报告 hook_ok 才算数,
            //   否则 (DLL 挂了/没装上) 老路径照旧生效 —— 不能让两边都停摆。
            static std::array<bool, Pt::PSC_COUNT> no_windup{};
            const bool hook_alive = g_pvz.HookAlive();
            if (hook_alive)
                g_pvz.SetWindupHook(true, g_wsec, g_wen);

            g_pvz.FixRandomValues(g_zval, g_zen, g_pint, g_pen, g_wsec,
                                  hook_alive ? no_windup : g_wen, g_zs, g_ps);
            // 把 DLL 产生的事件日志取走并落盘 (DLL 那边只在状态跳变时才产生条目)
            g_pvz.DrainHookLog();
            // ★★ v28: 冷却开关**每帧、在渲染之前**下发 ★★
            //   为什么要挪到这里 (v27 是在帧**末**、且只在 `g_attached` 分支里发的):
            //     ① 常驻诊断行是在**本帧渲染时**读控制块的, 而 v27 的下发在渲染**之后** ——
            //        于是诊断看到的永远是"上一帧下发的结果"; 勾选框刚被点开的那一帧必然
            //        显示"DLL=关", 看起来就像"勾了没生效"。现在先发后读, 同一帧内自洽。
            //     ② 它必须**每帧**写、且在**任何**分支之外 —— 跨进程开关的硬规矩:
            //        工具每帧写、DLL 每帧读, 勾选框一变下一帧立刻生效 (见 hook_ctl.h 的
            //        CdWiringHealth 上面那段硬规矩)。
            //   没注入 DLL 时 PublishCooldown 自己会直接返回, 什么也不做。
            //   ★★ v37: 模式 (三选一) 与开关一起下发 —— `cd_on = (模式 != 关闭)`,
            //     `cd_mode` 原样送出; DLL 会回读并比对 (CdModeHealth), 原生路径的
            //     "任何自检不过就降级成矩形" 完全在 DLL 里做, 工具只负责如实传达用户选择。
            EnsureCdModeInit(); // 幂等; 环境变量 PZTK_CD_MODE 在这里生效 (必须在界面之前也算一次)
            g_pvz.PublishCooldown(g_cd_mode != static_cast<int>(PtHook::CDMODE_OFF),
                                  static_cast<uint32_t>(g_cd_mode));
        }
        else
        {
            g_pvz.MarkHookDetached();
        }

        // 2026-10-07 新增: 无视进家 / 血量倍数 / 女仆秘籍 每帧执行 (核心 FrameHacks)
        g_pvz.SetZombieKillAtHome(g_kill_home);
        g_pvz.SetZombieHealthMul(g_zombie_hp_mul);
        g_pvz.SetMaidMode(g_maid_mode);
        g_pvz.FrameHacks();

        // 2026-10-09 新增: 多倍出怪 (出怪点数 ×N)。
        //   只在"开关/倍率变化"或"游戏换进程"时才动作 —— SetWaveMul 首次调用要扫
        //   4KB 内存定位锚点, 不能每帧做。游戏关掉又重开时 pid 变, 旧补丁随之消失,
        //   这里自动重打一遍 (对应"开启后每一关都有效")。
        {
            static uint32_t wm_pid = 0;
            static bool wm_on = false;
            static int wm_n = 0;
            const bool wm_want = g_wave_mul_on && g_wave_mul_n > 1;
            if (wm_want != wm_on || g_wave_mul_n != wm_n || g_pvz.GamePid() != wm_pid)
            {
                wm_on = wm_want;
                wm_n = g_wave_mul_n;
                wm_pid = g_pvz.GamePid();
                g_pvz.SetWaveMul(wm_want, g_wave_mul_n);
            }
        }

        // 2026-10-09 新增: 高级暂停 (把游戏自带的暂停变成"世界冻结但鼠标照常")。
        //   同样只在"开关变化/游戏换进程"时动作; 关掉时会**先把标志清 0 再还原补丁**,
        //   免得把游戏留在冻结状态 (SetAdvPause(false) 里做的)。
        {
            static uint32_t ap_pid = 0xFFFFFFFFu;
            static bool ap_on = false;
            if (g_adv_pause != ap_on || g_pvz.GamePid() != ap_pid)
            {
                ap_on = g_adv_pause;
                ap_pid = g_pvz.GamePid();
                g_pvz.SetAdvPause(g_adv_pause);
            }
        }

        // ★★ 2026-10-14 新增: 取消必选卡片 (冒险模式二周目 Crazy Dave 预选) ★★
        //   与多倍出怪/高级暂停同一套做法: 只在"开关变化 / 游戏换进程"时动作 ——
        //   机制 A 要扫 .text 里那条签名 (2.4 MB 的读), **不能每帧做**。
        //   勾上就立刻生效: 机制 A 打补丁 (自检不过 ⇒ 拒绝并如实报"未生效");
        //   机制 B (选卡界面里清 mCrazyDavePicked) 由 FrameHacks 每帧做 (它就是改内存字节,
        //   便宜且幂等)。游戏关掉又重开时 pid 变 ⇒ 这里自动重打一遍 (对应"每一关都有效")。
        {
            static bool nd_on = false;
            static uint32_t nd_pid = 0xFFFFFFFFu;
            if (g_no_dave_pick != nd_on || g_pvz.GamePid() != nd_pid)
            {
                nd_on = g_no_dave_pick;
                nd_pid = g_pvz.GamePid();
                g_pvz.SetNoDavePick(g_no_dave_pick);
            }
        }
        if (g_attached)
        {
            // ★ 游戏换进程 = 关掉又重开: 也要重新回显一遍。
            //   只靠"断开一帧就把 g_wisdom_inited 清掉"会漏掉**快速重启** (工具没看到断开帧),
            //   于是勾选状态还停在上一次游戏里 -> 错标。这里用进程号变化兜住。
            static uint32_t last_game_pid = 0;
            if (g_pvz.GamePid() != last_game_pid)
            {
                last_game_pid = g_pvz.GamePid();
                g_wisdom_inited = false;
                g_wisdom_in_level = false;
                g_wisdom_touched = false; // 新的一局: 之前的"点过"不作数
            }

            // ★ 另一种漏扫: 工具比游戏先开、或连上时还在**主菜单** —— 那时智慧树记录未必是有效值,
            //   照它回显就会错标。所以等"确实进关卡了"再补扫一次。
            //   用 g_wisdom_touched 保护: 用户手动点过之后就不再自动覆盖他的勾选。
            if (!g_wisdom_touched && !g_wisdom_in_level && g_pvz.GameUI() == 3)
                g_wisdom_inited = false; // 触发下面那段重新回显

            // 连上游戏 (或游戏换进程 / 刚进关卡): 先读一遍游戏里的状态来打勾 —— 只回显, **不发送**
            // (用户要求: 判断指令是否已生效, 已生效的就打上勾)
            if (!g_wisdom_inited)
            {
                for (int i = 0; i < Pt::PvZ::WisdomCodeCount(); i++)
                    g_wisdom_checked[i] = g_pvz.WisdomCodeEnabled(i);
                g_wisdom_inited = true;
                if (g_pvz.GameUI() == 3)
                    g_wisdom_in_level = true; // 已在关卡内扫过, 不必再补扫

                // 女仆秘籍自愈: 如果游戏里已经是"打过补丁"的状态 (上次工具被强杀/没还原),
                // 把下拉框同步成那个模式, 并准备好原始字节, 之后换模式/关闭都能正常处理
                const int maid_now = g_pvz.DetectMaidPatch();
                if (maid_now > 0)
                {
                    g_maid_mode = maid_now;
                    g_maid_patch_msg = "补丁模式(接续上次)";
                }
            }
            for (int i = 0; i < Pt::PvZ::WisdomCodeCount(); i++)
            {
                if (g_wisdom_pending[i])
                {
                    g_wisdom_pending[i] = false;
                    if (g_pvz.SendWisdomCode(i))
                    {
                        g_wisdom_hold[i] = 20;   // 显示"(已发送)"20 帧
                        g_wisdom_verify[i] = 45; // 0.45 秒后核对: 游戏到底有没有真的生效
                        g_wisdom_result[i] = 0;
                    }
                }

                // ★ 反馈: 发完指令后回读游戏状态 —— 这才是"事实"。
                //   游戏重启后所有指令本来就会重置 (残留只是存档造成的), 所以与其在连接时"猜",
                //   不如每次发完都核对一次: 没生效就把勾去掉并提示 (例如智慧树高度不够)。
                if (g_wisdom_verify[i] > 0 && --g_wisdom_verify[i] == 0)
                {
                    const bool now_on = g_pvz.WisdomCodeEnabled(i);
                    g_wisdom_checked[i] = now_on; // 以游戏实际状态为准
                    g_wisdom_result[i] = now_on ? 2 : 1;
                    g_wisdom_result_hold[i] = 180; // 提示显示 3 秒
                }
                if (g_wisdom_result_hold[i] > 0)
                    g_wisdom_result_hold[i]--;
                else
                    g_wisdom_result[i] = 0;

                // 注意: 勾选框状态**不回写** (回写会让它"闪一下就没了"), 见面板里的注释
            }
        }
        else
        {
            g_wisdom_inited = false; // 断开后下次连上重新回显一次 (换进程的情况由上面的 pid 判断兜住)
        }

        // 游戏断开/退出时, 把打过的代码补丁全部还原 (代码补丁会一直留在游戏进程里)
        if (!g_attached && g_pvz.PatchCount() > 0)
            g_pvz.RestoreAllPatches();

        WireToggles();
        MiscPollPak(); // 解包/打包完成后弹结果

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always); // 明确锚定左上角
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * g_dpi, 6 * g_dpi));
        ImGui::Begin("root", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoScrollWithMouse);

        // ---- 顶栏: 左 = 应用名(粗体) + 版本; 右 = 连接状态胶囊 + DPI 胶囊 ----
        //   高度只比老版那一行高 ~3px (胶囊上下留 3*dpi), 不能再高 —— 顶栏每长高
        //   1px, 内容区就矮 1px, 页尾"其他修改"块会先掉到地板高度、随后撑出滚动条。
        {
            const float row_y = ImGui::GetCursorPosY();
            if (g_font_title != nullptr)
                ImGui::PushFont(g_font_title);
            ImGui::TextColored(kInk, "PvZ Toolkit Dx");
            const float title_h = ImGui::GetTextLineHeight();
            if (g_font_title != nullptr)
                ImGui::PopFont();

            ImGui::SameLine(0.0f, 6.0f);
            if (g_font_small != nullptr)
                ImGui::PushFont(g_font_small);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (title_h - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::TextColored(kInkFaint, "v%s", VERSION_NAME);
            if (g_font_small != nullptr)
                ImGui::PopFont();

            // 右侧两个胶囊: 先量宽度, 再把光标定位到右边缘
            const ImVec2 sz_att = ImGui::CalcTextSize(g_attach_msg);
            const ImVec2 sz_dpi = ImGui::CalcTextSize("000%");
            const float pad    = 7.0f * g_dpi;
            const float dotgap = 7.0f * g_dpi + 5.0f * g_dpi;
            const float w_att  = sz_att.x + pad * 2 + dotgap;
            const float w_dpi  = sz_dpi.x + pad * 2;
            const float gap    = 6.0f * g_dpi;
            const float right_x = ImGui::GetWindowContentRegionMax().x - (w_att + gap + w_dpi);

            ImGui::SameLine(0.0f, 0.0f);
            if (right_x > ImGui::GetCursorPosX() + 8.0f * g_dpi)
            {
                ImGui::SetCursorPosX(right_x);
                ImGui::SetCursorPosY(row_y);
            }
            Pill(g_attach_msg, g_attached ? kOk : kBad, g_attached ? kOkSoft : kBadSoft,
                 &(g_attached ? kOk : kBad));
            ImGui::SameLine(0.0f, gap);
            {
                char buf[48];
                std::snprintf(buf, sizeof(buf), "%.0f%%", g_dpi * 100.0f);
                Pill(buf, kInkDim, kPane);
            }
        }
        ImGui::Spacing();

        const float body_h = ImGui::GetContentRegionAvail().y - kStatusH * g_dpi - 4 * g_dpi;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, kPane);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f * g_dpi);
        ImGui::BeginChild("side", ImVec2(kSideW * g_dpi, body_h), ImGuiChildFlags_Borders);
        for (int i = 0; i < kPageCount; i++)
        {
            const bool sel = (g_page == i);
            // 未选中 = 与侧栏底色一致 (纯文字导航); 选中 = 浅蓝底 + 蓝字 + 左侧竖条
            ImGui::PushStyleColor(ImGuiCol_Button, sel ? kAccentSoft : kPane);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sel ? kAccentSoft : kPaneHov);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, sel ? kAccentSoft : kPaneHov);
            ImGui::PushStyleColor(ImGuiCol_Text, sel ? kAccent : kInk);
            if (ImGui::Button(kPageNames[i], ImVec2(-1, 30 * g_dpi)))
                g_page = i;
            ImGui::PopStyleColor(4);
            if (sel)
            {
                ImDrawList *dl = ImGui::GetWindowDrawList();
                const ImVec2 a = ImGui::GetItemRectMin();
                const ImVec2 b = ImGui::GetItemRectMax();
                const float bw = 3.0f * g_dpi;
                dl->AddRectFilled(ImVec2(a.x, a.y + 2 * g_dpi), ImVec2(a.x + bw, b.y - 2 * g_dpi),
                                  kU32Accent, bw * 0.5f);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 5.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, kPage);
        ImGui::BeginChild("content", ImVec2(0, body_h));
        switch (g_page)
        {
        case 0: PageBattle(); break;
        case 1: PageResource(); break;
        case 2: PageLineup(); break;
        case 3: PageLevel(); break;
        case 4: PageMisc(); break;
        default: PagePlaceholder("杂项"); break;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, kPane);
        ImGui::BeginChild("status", ImVec2(0, kStatusH * g_dpi), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);
        {
            // 顶部 1px 分隔线 (状态栏与内容区之间)
            ImDrawList *dl = ImGui::GetWindowDrawList();
            const ImVec2 wp = ImGui::GetWindowPos();
            dl->AddLine(ImVec2(wp.x, wp.y + 0.5f),
                        ImVec2(wp.x + ImGui::GetWindowWidth(), wp.y + 0.5f), kU32Line);
        }
        if (g_font_small != nullptr)
            ImGui::PushFont(g_font_small);
        {
            char buf[64];
            const float th = ImGui::GetTextLineHeight();
            ImGui::SetCursorPos(ImVec2(8 * g_dpi, (kStatusH * g_dpi - th) * 0.5f));
            std::snprintf(buf, sizeof(buf), "僵尸 %d / 植物 %d", (int)g_zs.size(), (int)g_ps.size());
            ImGui::TextColored(kInkDim, "%s", buf);

            char buf2[64];
            if (g_attached)
                std::snprintf(buf2, sizeof(buf2), "已连接 · PID %u", (unsigned)g_pvz.GamePid());
            else
                std::snprintf(buf2, sizeof(buf2), "未连接");
            const float w2 = ImGui::CalcTextSize(buf2).x;
            const float rx = ImGui::GetWindowContentRegionMax().x - w2 - 8 * g_dpi;
            if (rx > ImGui::GetCursorPosX() + 8 * g_dpi)
            {
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::SetCursorPosX(rx);
            }
            else
            {
                ImGui::SameLine(0.0f, 10.0f);
            }
            ImGui::TextColored(g_attached ? kOk : kInkFaint, "%s", buf2);
        }
        if (g_font_small != nullptr)
            ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Render();
        {
            int dw = 0, dh = 0;
            glfwGetFramebufferSize(window, &dw, &dh);
            glViewport(0, 0, dw, dh);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // 浅色主题: 窗口外露出的底色也必须是白的
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }

        // ============ 第 2 段: 帧末处理子窗 (创建/显示 或 渲染) ============
        if (g_want_speed)
        {
            if (g_sub_speed.win == nullptr)
                CreateSubWindow(g_sub_speed, window, "僵尸移速", (int)(700 * g_dpi),
                                (int)(760 * g_dpi));
            else
                ShowSubWindow(g_sub_speed);
            g_want_speed = false;
        }

        if (g_want_plant)
        {
            if (g_sub_plant.win == nullptr)
                CreateSubWindow(g_sub_plant, window, "植物攻速", (int)(740 * g_dpi),
                                (int)(700 * g_dpi));
            else
                ShowSubWindow(g_sub_plant);
            g_want_plant = false;
        }

        if (g_want_spawn)
        {
            if (g_spawn_refresh_pending)
            {
                SpawnRefresh(); // 打开窗口顺带刷新一次 (与 FLTK 版一致)
                g_spawn_refresh_pending = false;
            }
            if (g_sub_spawn.win == nullptr)
                CreateSubWindow(g_sub_spawn, window, "出怪数量统计", (int)(1020 * g_dpi),
                                (int)(680 * g_dpi));
            else
                ShowSubWindow(g_sub_spawn);
            g_want_spawn = false;
        }

        RenderSubWindow(g_sub_speed, DrawSpeedContent);
        RenderSubWindow(g_sub_plant, DrawPlantContent);
        RenderSubWindow(g_sub_spawn, DrawSpawnContent);

        // 卡片冷却时间: 每帧把"开关 + 钩点地址"下发给 DLL (地址是每帧现算/自检的,
        //   游戏关掉重开也不会拿着旧地址乱改代码)。没注入 DLL 时这里什么也不做。
        //   ★ v28: 挪到帧首去了 (见上面 `g_pvz.PublishCooldown` 那段的说明) ——
        //     这里**不再**下发, 免得同一帧写两次、又变回"诊断读到的是上一帧的结果"。

        // ============ 第 3 段: 切回主窗, 恢复视口 (关键!) ============
        glfwMakeContextCurrent(window);
        ImGui::SetCurrentContext(main_ctx);
        {
            int dw = 0, dh = 0;
            glfwGetFramebufferSize(window, &dw, &dh);
            if (dw > 0 && dh > 0)
                glViewport(0, 0, dw, dh);
        }
    }

    // 主窗关闭: 一并关闭出怪详情窗
    if (g_sub_spawn.win != nullptr)
    {
        glfwMakeContextCurrent(g_sub_spawn.win);
        ImGui::SetCurrentContext(g_sub_spawn.ctx);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(g_sub_spawn.ctx);
        glfwDestroyWindow(g_sub_spawn.win);
        g_sub_spawn.win = nullptr;
        g_sub_spawn.ctx = nullptr;
    }
    // 主窗关闭: 一并关闭植物攻速窗
    if (g_sub_plant.win != nullptr)
    {
        glfwMakeContextCurrent(g_sub_plant.win);
        ImGui::SetCurrentContext(g_sub_plant.ctx);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(g_sub_plant.ctx);
        glfwDestroyWindow(g_sub_plant.win);
        g_sub_plant.win = nullptr;
        g_sub_plant.ctx = nullptr;
    }    // 主窗关闭: 一并关闭所有子窗 (已退出渲染循环, 此时销毁是安全的)
    if (g_sub_speed.win != nullptr)
    {
        glfwMakeContextCurrent(g_sub_speed.win);
        ImGui::SetCurrentContext(g_sub_speed.ctx);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(g_sub_speed.ctx);
        glfwDestroyWindow(g_sub_speed.win);
        g_sub_speed.win = nullptr;
        g_sub_speed.ctx = nullptr;
    }
    glfwMakeContextCurrent(window);
    ImGui::SetCurrentContext(main_ctx);

    // 退出前: 先让注入的 DLL 卸钩子并自己退出 (否则钩子会留在游戏里, 一直钉着植物),
    // 再把打过的代码补丁全部还原
    g_pvz.UnloadHookDll();
    g_pvz.RestoreAllPatches();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(main_ctx);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
