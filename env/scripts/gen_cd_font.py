#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""Generate tool-src/hook/cd_font.h -- the offline grayscale glyph table used by the
card-cooldown digits drawn *inside* the game (no game font API involved).

WHY THIS EXISTS
---------------
The cooldown readout used to be hand-made 7-segment bars (v29) and then a
7x9 run-length bitmap blitted as one filled rectangle per *horizontal run*
(v30).  The user rejected both looks on the real machine:

  v29: "好丑啊, 而且看不清, 还挡住了阳光数"
  v30: "效果还是不好, 这种方块数字不太好看"   (粗黑边 + 白色方块感)

We cannot use the game's own font API (TodDrawString needs a stack-built
SexyString == std::string; this DLL is built with -nostdlib and has no operator
new => Access Violation).  So the glyphs are rasterised HERE, offline, and
shipped as a tiny C table; the DLL only blits them through the already-verified
Graphics::SetColor / Graphics::FillRect call convention.

WHAT IS STORED (v31)
--------------------
For every character of `0-9 . -` one canvas of CANVAS_W x CANVAS_H **8-bit
grayscale pixels** (0 = transparent, 1..255 = the ink to write).  No run-length
encoding, no 7-level quantisation: the DLL walks the canvas pixel by pixel and
paints one rectangle per pixel (aggregated to 2x2 blocks when the per-frame
rectangle budget is tight).

Two things changed versus v30 and both matter for the look:

  * the canvas grew from 7x9 to **10x14** (the largest cell that still lets
    `35.0` -- three digits plus the decimal point with 2px gaps -- fit the 50px
    card: 4*10 + 4*2 = 48 <= 48 usable).  A bigger canvas means real
    antialiasing coverage instead of a nearly binary 1px stroke;
  * the stored value is the **blended white ink over the darkened card face**
    the game itself produces for a cooling packet
    (`SeedPacket.cpp:529` -> `Color(64,64,64)` => GRAY_BG = 64):
        gray = GRAY_BG + coverage * (255 - GRAY_BG) / 255
    Full coverage is pure white (255), partial coverage fades towards the
    darkened card.  ★ v36: this stored value is used as the **coverage record**,
    not as the colour to write -- the runtime inverts the very same formula
    (`CdGlyphCoverage`) and paints WHITE with `A = coverage`, so the blend is
    done **by the game** on whatever the card face happens to be (see the v35
    paragraph below for why the opaque grey had to go).

USAGE
-----
    python env/scripts/gen_cd_font.py
    python env/scripts/gen_cd_font.py --font C:/Windows/Fonts/tahoma.ttf --out tool-src/hook/cd_font.h
    python env/scripts/gen_cd_font.py --preview                      # ASCII art of every glyph
    python env/scripts/gen_cd_font.py --preview-png backup/shots/v36_glyph_preview.png --preview-scale 8
    python env/scripts/gen_cd_font.py --preview-png before.png --preview-ink-mode opaque --preview-ink-floor 160 --preview-ink-threshold 32
    python env/scripts/gen_cd_font.py --check                        # verify, do not write

★ v35 (the ink-floor era) -- and why v36 removed it
    v35 clamped every emitted grey up to `kCdFontInkFloor` (160) and dropped
    blocks fainter than `kCdFontInkMinCover` (32).  The user's real-machine
    verdict on v35: those artefacts are **not black dots but holes** -- the
    glyphs were hollowed out / the strokes broke into regular gaps.  Two causes,
    both fixed in v36:
      * the "drop faint blocks" gate IS a "skip pixels below some grey" path --
        a hole generator by construction.  It happened to drop nothing for the
        current table (darkest ink 76 > 32), i.e. it was luck; v36 deleted it
        (`kCdFontInkMinCover = 0` + `static_assert`, and recttest now asserts
        "drawn superset of ink" for 1x1/2x2 x 12 chars x both ink modes);
      * the clamp compressed the whole 76..159 range (**217 of 747 ink pixels =
        29%**) onto the single grey 160, so the antialiasing was gone and the
        result depended on an *assumed* card face (<= 128).  Any face pixel at or
        above 160 makes such a block equal to the background (= a hole) or darker
        (= a dot).
    v36 therefore blends with alpha: `out = face + (255 - face) * coverage/255`
    -- never darker than the face on ANY face (no dots), and coverage > 0 always
    yields alpha > 0 (no holes).  Fallback for a machine where alpha turns out
    not to be honoured: `PZTK_CD_NOALPHA=1` (opaque + clamp up, still drawing
    every inked pixel).

Exit code 0 = header written (or --check passed), 1 = error.
Run it after changing the font / size and re-run the offline unit tests
(tool-src/misc/recttest.cpp) -- they assert the table's invariants.

Requires Pillow (bundled interpreter has it):
    <dsh>\dependencies\python\python.exe env/scripts/gen_cd_font.py
"""

import argparse
import os
import sys
from datetime import date

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover - environment hint only
    sys.stderr.write("ERROR: Pillow is required (pip install Pillow).\n")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Glyph cell geometry -- MUST match tool-src/hook/cd_font.h and the cooldown
# geometry in tool-src/hook/hook_ctl.h (the cell is exactly this size; the
# runtime never scales the glyph).
# ---------------------------------------------------------------------------
CANVAS_W = 10
CANVAS_H = 14

CHARS = "0123456789.-"
DIGITS = "0123456789"
DEFAULT_FONT = r"C:\Windows\Fonts\tahoma.ttf"
DEFAULT_OUT = os.path.join("tool-src", "hook", "cd_font.h")

# Coverage -> ink.  Anything below COVER_THRESHOLD is pure transparent (it would
# cost a rectangle and be invisible anyway).
COVER_THRESHOLD = 16
# The colour the game darkens a cooling card with (SeedPacket.cpp:529
# `aPlantG.SetColor(Color(64, 64, 64, 255))`).  Partial coverage fades towards
# it, which is what makes the antialiased edge look like a real font.
GRAY_BG = 64

GLYPH_NAMES = {
    ".": "DOT",
    "-": "MINUS",
}

# ---------------------------------------------------------------------------
# The simulated card, mirrored from tool-src/hook/hook_ctl.h.  Only used for the
# PNG preview (the runtime geometry is the C++ pure functions).
#
# v36: the runtime paints **white with A = coverage** by default, so the preview
# has to COMPOSITE that over the card face for real:
#     out = face + (255 - face) * alpha / 255      (per channel, rounded)
# `--preview-ink-mode opaque` reproduces the v35 behaviour instead (clamp every
# grey up to INK_FLOOR and drop blocks fainter than INK_MIN_COVER) -- that is the
# "before" sheet of the report.
#
# The two preview card faces (dark / bright):
#   * CARD_DARK   = the cooling card face as measured on the user's own in-game
#                   screenshot (backup/shots/v33_cd_band_cmp.png, 5x zoom):
#                   (58, 62, 56) -- the card art multiplied by the game's own
#                   darkener Color(64,64,64)/255.  A re-measurement of the same
#                   screenshot around the digit band (card rows 20..26 / 41..53
#                   are face-only, no ink) gives 40..64, i.e. the same family.
#   * CARD_BRIGHT = the *revealed* face once the darkening band has retracted
#                   (SeedPacket.cpp:525-537 clips that band to a shrinking
#                   height, so from ~37% remaining on it no longer covers the
#                   digit band).  The colorize pass there is
#                   `Color(128,128,128)` + SetColorizeImages(true)
#                   (SeedPacket.cpp:274-283, and `aGrayness = 128` at
#                   653-656) => art x 128/255, i.e. **128** for pure-white art.
#                   ★ v35 treated that 128 as "the brightest face that can
#                   appear"; v36 does not *assume* anything about the face --
#                   that is the whole point of the alpha blend.
#   * The report also prints statistics for the mathematically worst face (160:
#     exactly the v35 ink grey) because that is where the opaque path turns into
#     "glyph == background" = the holes the user reported.
# ---------------------------------------------------------------------------
CARD_W, CARD_H = 50, 70                 # SeedPacket::SeedPacket mWidth/mHeight
SUN_COST_TOP = 54                       # 卡面阳光费用区上沿 (禁画区, 只画个示意)
DIGIT_TOP = 27                          # v34 起: 数字组 = 卡顶 27..40 (居中 31 + kCdTextDropY 3)
CELL_GAP = 2                            # kCdDigitGap
# ★ v35 的两个颜色闸门 —— **只在 --preview-ink-mode opaque 下模拟** (v36 运行期已删掉)
#   必须与 hook_ctl.h 的 kCdFontInkFloor / kCdFontInkMinCover 一致:
#   v36 的 kCdFontInkMinCover 恒为 0 (没有任何"低于某灰度就不画"的路径),
#   下面这个 32 只是用来**复现 v35 的 bug** 做对照的。
INK_FLOOR = 160                         # v35 墨色下限 (v36 只在不透明回退路径里用)
INK_MIN_COVER = 32                      # v35 的降权阈值 (v36 已删除 ⇒ 恒 0)
INK_FLOOR_V36 = 0                       # v36: 不存在降权阈值

CARD_DARK = (58, 62, 56)                # 实机量到的冷却卡面 (原图 x 64/255)
CARD_BRIGHT = (128, 128, 118)           # 最亮卡面 = 原图 x 128/255 (买不起那个状态)
CARD_FLOOR = (160, 160, 160)            # = v35 的墨色下限本身 (不透明画法在这里变成"洞")
SUN_COST = (30, 30, 30)                 # 卡上的阳光费用区 (禁画区, 只画个示意)


def quantise(value):
    """0..255 coverage -> stored 8-bit gray to write (0 = transparent).

    Monotone by construction: a brighter coverage never yields a darker pixel.
    """
    if value < COVER_THRESHOLD:
        return 0
    gray = GRAY_BG + (value * (255 - GRAY_BG) + 127) // 255
    if gray > 255:
        gray = 255
    return gray


def glyph_symbol(ch):
    if ch.isdigit():
        return "G" + ch
    if ch in GLYPH_NAMES:
        return GLYPH_NAMES[ch]
    raise ValueError("no symbol name for %r" % ch)


def ink_box(font, ch):
    """True (unclipped) ink box of `ch` drawn with the baseline anchor at (0,0)."""
    big = 4 * max(CANVAS_W, CANVAS_H)
    img = Image.new("L", (big, big), 0)
    draw = ImageDraw.Draw(img)
    draw.text((big // 2, big // 2), ch, font=font, fill=255, anchor="ls")
    box = img.getbbox()
    if box is None:
        return None
    return (box[0] - big // 2, box[1] - big // 2, box[2] - big // 2, box[3] - big // 2)


def union_ink_box(font, chars):
    left = top = 1 << 30
    right = bottom = -(1 << 30)
    for ch in chars:
        box = ink_box(font, ch)
        if box is None:
            return None
        left = min(left, box[0])
        right = max(right, box[2])
        top = min(top, box[1])
        bottom = max(bottom, box[3])
    return left, top, right, bottom


def baseline_for(font):
    """Baseline offset that centres the **digits** vertically inside the canvas.

    It must be ONE value for every glyph of the table: centring each glyph on
    its own ink box would drop the decimal point into the middle of the cell
    (its ink box is tiny and sits on the baseline).  So the digits decide, and
    `.` / `-` use the same baseline.

    Why the "ls" (left/baseline) anchor: the default "la" anchor's top edge is
    undefined for glyphs that do not reach the ascent (the period), which is how
    the first v30 table ended up placing the dot wrong.
    """
    box = union_ink_box(font, DIGITS)
    if box is None:
        raise ValueError("no ink for the digit set")
    _, top, _, bottom = box
    ink_h = bottom - top
    if ink_h > CANVAS_H:
        raise ValueError("digit ink is %dpx tall, canvas is %dpx" % (ink_h, CANVAS_H))
    top_offset = (CANVAS_H - ink_h) // 2
    return top_offset - top


def render_cell(font, ch, baseline):
    """Rasterise one character into the CANVAS_W x CANVAS_H cell.

    Horizontal placement: the **ink** (not the advance) is centred in the cell.
    Centring the *advance* is not enough -- Tahoma's advance at 11px is exactly
    6.0px, so ``int((7-6)/2) = 0`` and the whole glyph inked over columns 0..4,
    i.e. it looked off-centre and its antialiased left edge sat on the cell
    border.  Centring the ink puts a 5px wide digit on columns 1..5 and a 4px
    wide one on 1..4 (Pillow rounds the layout to integers, so this is
    reproducible on any build).  Guarded so a glyph can never poke out of the
    cell because of the centring itself.
    """
    img = Image.new("L", (CANVAS_W, CANVAS_H), 0)
    draw = ImageDraw.Draw(img)
    advance = font.getlength(ch)
    box = ink_box(font, ch)
    x = (CANVAS_W - advance) / 2.0
    if box is not None:
        ink_left, _, ink_right, _ = box
        ink_w = ink_right - ink_left
        x = (CANVAS_W - ink_w) / 2.0 - ink_left
        if x < -ink_left:                        # never to the left of the cell
            x = -ink_left
        if x + ink_right > CANVAS_W:             # never past the right border
            x = CANVAS_W - ink_right
    draw.text((x, baseline), ch, font=font, fill=255, anchor="ls")
    return img


def rasterise(font, ch, baseline):
    """The stored canvas: CANVAS_H rows of CANVAS_W 8-bit gray values."""
    px = render_cell(font, ch, baseline).load()
    return [[quantise(px[x, y]) for x in range(CANVAS_W)] for y in range(CANVAS_H)]


def fits(font):
    """Does this pixel size still fit the cell (ink + advance)?"""
    box = union_ink_box(font, DIGITS)
    if box is None:
        return False
    left, top, right, bottom = box
    if bottom - top > CANVAS_H:                              # ink taller than cell
        return False
    baseline = baseline_for(font)
    if baseline + top < 0 or baseline + bottom > CANVAS_H:   # vertically outside
        return False
    if right - left > CANVAS_W:                              # ink wider than cell
        return False
    for ch in DIGITS:                                        # advance must fit
        if font.getlength(ch) > CANVAS_W + 0.5:
            return False
    if font.getlength("-") > CANVAS_W + 0.5:
        return False
    return True


def auto_size(font_path, size=None):
    """Largest TrueType pixel size whose glyphs still fit the cell."""
    if size:
        try:
            return size, ImageFont.truetype(font_path, size)
        except OSError as exc:
            sys.stderr.write("ERROR: cannot load font size %d: %s\n" % (size, exc))
            sys.exit(1)
    best = None
    for candidate in range(6, 40):
        try:
            font = ImageFont.truetype(font_path, candidate)
        except OSError:
            break
        if fits(font):
            best = candidate
    if best is None:
        sys.stderr.write("ERROR: no usable pixel size found for %s\n" % font_path)
        sys.exit(1)
    return best, ImageFont.truetype(font_path, best)


def preview(table):
    """ASCII art of every glyph (so a wrong-looking glyph is visible without a build)."""
    ramp = " .:-=+*#@"
    lines = []
    for ch in table["order"]:
        px = table["px"][ch]
        lines.append("  '%s'  (%d ink pixels)" % (ch, table["ink"][ch]))
        for row in px:
            art = "".join(ramp[min(len(ramp) - 1, v * len(ramp) // 256)] if v else "." for v in row)
            lines.append("    |%s|" % art)
    return lines


def build(font_path, chars, size=None):
    size, font = auto_size(font_path, size)
    baseline = baseline_for(font)
    table = {
        "font_path": font_path,
        "font_name": os.path.basename(font_path),
        "font_size": size,
        "baseline": baseline,
        "order": list(chars),
        "px": {},
        "ink": {},
        "total_ink": 0,
        "max_ink": 0,
        "canvas_bytes": CANVAS_W * CANVAS_H * len(chars),
        "color": GRAY_BG,
    }
    for ch in chars:
        px = rasterise(font, ch, baseline)
        ink = sum(1 for row in px for v in row if v)
        if ink <= 0:
            sys.stderr.write("ERROR: glyph %r has no ink at size %d\n" % (ch, size))
            sys.exit(1)
        table["px"][ch] = px
        table["ink"][ch] = ink
        table["total_ink"] += ink
        table["max_ink"] = max(table["max_ink"], ink)
    return table


def render_header(table):
    out = []
    w = out.append
    w("// ---------------------------------------------------------------------------")
    w("// cd_font.h -- 卡片冷却数字的**离线灰度字模表** (逐像素 8 位灰度)")
    w("//")
    w("//   ★★ 这个文件是**生成的**, 不要手改 ★★")
    w("//     生成器: env/scripts/gen_cd_font.py")
    w("//     字体:   %s  (像素字号 %d, 基线 %d)"
      % (table["font_name"], table["font_size"], table["baseline"]))
    w("//     命令:   python env/scripts/gen_cd_font.py --font %s"
      % table["font_path"].replace("\\", "/"))
    w("//     生成日: %s" % date.today().isoformat())
    w("//")
    w("//   为什么不用游戏自带的字体 API (TodDrawString): 它要在栈上构造 SexyString")
    w("//   (= 本工程的 std::string), 而本 DLL 是 -nostdlib + 自备极简 CRT、**没有")
    w("//   operator new** => 堆操作 = Access Violation / 堆损坏 = 崩游戏。")
    w("//   所以字形在**离线**光栅化, 出厂只带这张表; 运行期只用已验证的")
    w("//   Graphics::SetColor + Graphics::FillRect 把每个像素画成一个矩形。")
    w("//")
    w("//   表结构: 每个字符一块 CANVAS_W x CANVAS_H 的**逐像素 8 位灰度**画布")
    w("//   (行主序, 0 = 透明不画)。这个灰度是**覆盖度的记录**, 不是最终颜色:")
    w("//     灰度 = %d + 覆盖度 x (255 - %d) / 255" % (table["color"], table["color"]))
    w("//   其中 %d 就是游戏自己给冷却中的卡面叠的暗色 (SeedPacket.cpp:529 的"
      % table["color"])
    w("//   `aPlantG.SetColor(Color(64, 64, 64, 255))`) —— 于是「白字渐渐融进变暗的卡面」")
    w("//   就是这套数值的物理含义。")
    w("//")
    w("//   ★★ v36 (2026-10-12) 这个灰度**怎么用**: 反解出覆盖度, 画成「白字 + A = 覆盖度」★★")
    w("//     运行期 (hook_ctl.h 的 `CdGlyphCoverage()` / `CdGlyphInk()`) 做两件事:")
    w("//       ① 覆盖度 = (灰度 - %d) x 255 / (255 - %d)  (本文件 quantise() 的反函数)" % (table["color"], table["color"]))
    w("//          —— 灰度 == 0 ⇒ 覆盖度 0 (不画); 灰度 > 0 ⇒ 覆盖度 **>= 1** (绝不会变成全透明);")
    w("//       ② 画出来的颜色 = **白 (255,255,255) + A = 覆盖度** (默认), 或者")
    w("//          **不透明灰 max(灰度, 160) + A = 255** (回退: 环境变量 PZTK_CD_NOALPHA=1)。")
    w("//     ⇒ 游戏按 SRCALPHA/INVSRCALPHA 把它叠在**任何**卡面上:")
    w("//          输出 = 卡面 + (255 - 卡面) x A / 255")
    w("//        ① 永远不比卡面更暗 ⇒ **不可能出现黑点**;")
    w("//        ② A 只在覆盖度 == 0 时才是 0 ⇒ **有墨必有画, 不可能出现空洞**;")
    w("//        ③ A 与覆盖率成正比 ⇒ 抗锯齿梯度完整保留 (暗卡面 58~62 上它与本表的")
    w("//           定标值只差 <= 2 级 ⇒ 观感就是 v31 那一版, 而那一版用户是接受的)。")
    w("//")
    w("//   ★★ v35 (2026-10-12) 那一版为什么被推翻 (留档, 别再改回去) ★★")
    w("//     v35 用「不透明灰 + 抬到下限 160 + 丢掉比 32 更淡的块」:")
    w("//       ① 「丢掉比 32 更淡的块」= 一条**低于某灰度就跳过不画**的路径 —— 天生的挖洞")
    w("//          路径。它当时恰好没丢现有字模 (表里最暗 = 76 > 32), 纯属**运气**;")
    w("//       ② 「抬到 160」把 76..159 这 **217 个像素 (占 747 的 29%)** 全压成同一个灰度")
    w("//          ⇒ 抗锯齿没了, 而且结果**完全押在「卡面 <= 128」这个推算值上**。")
    w("//     用户实机看到的就是这个的后果 —— 原话: 那些东西**不是黑点, 是「洞」**")
    w("//     (字形被挖空 / 笔画断成规则缺口), 因为卡面对 160 的那一片像素正好与底色同值。")
    w("//     ⇒ v36: 上面的 ① 整条删除 (`kCdFontInkMinCover = 0` + `static_assert` +")
    w("//       recttest 的「已画 ⊇ 墨迹」断言), ② 只留在不透明回退路径里。")
    w("//     ★ 本表的数值**一个字节都没变**: 它仍然是「覆盖率 → 该写多少灰度」的唯一记录。")
    w("//")
    w('//   ★★ v31 为什么不再用「同灰度水平行程 (run-length)」来省矩形 ★★')
    w("//      v30 把每行的同灰度像素并成一根 1px 高的行程, 但 7x9 的画布上抗锯齿只有")
    w("//      7 级、笔画几乎全是满覆盖 ⇒ 每个字形看起来就是**方块**; 再叠一层右下偏移")
    w("//      1px 的暗色 (kCdFontShadowGray=8) ⇒ 白块外面一圈**粗黑边**")
    w('//      (用户实机原话: 「这种方块数字不太好看」)。v31 = 逐像素灰度 + 10x14 画布,')
    w("//      并把阴影整层**删掉** (字本来就画在变暗的卡面上, 不需要描边)。")
    w("//")
    w("//   规模: %d 个字符 / %d 个墨迹像素 / %d 字节数据 (整个画布都存下来)。"
      % (len(table["order"]), table["total_ink"], table["canvas_bytes"]))
    w("// ---------------------------------------------------------------------------")
    w("")
    w("#ifndef PZTK_CD_FONT_H")
    w("#define PZTK_CD_FONT_H")
    w("")
    w("#include <stdint.h>")
    w("")
    w("namespace PtHook")
    w("{")
    w("// 字模画布尺寸 (= 冷却数字的**单个字符包围盒**尺寸; 见 hook_ctl.h 的 v31 几何节)")
    w("//   ★ 必须与 CdTextGeomMake 反算出的 (dw, dh) 一致 —— 字模**不缩放**,")
    w("//     直接按 1:1 画进那个格子; 不一致时 CdGlyphBlocks 返回 0 (这张卡不画),")
    w("//     并由 recttest 断言「正常卡面反算出来的格子就是这个尺寸」。")
    w("constexpr int32_t kCdFontCanvasW = %d;" % CANVAS_W)
    w("constexpr int32_t kCdFontCanvasH = %d;" % CANVAS_H)
    w("// 生成时用的像素字号 (换成别的字号必须重新生成, 并重跑离线单测)")
    w("constexpr int32_t kCdFontPixelSize = %d;" % table["font_size"])
    w("// 覆盖率->灰度 的混合底 (= 游戏给冷却卡面叠的暗色 Color(64,64,64))")
    w("//   灰度 = kCdFontGrayBg + 覆盖度 x (255 - kCdFontGrayBg) / 255")
    w("constexpr int32_t kCdFontGrayBg = %d;" % table["color"])
    w("constexpr int32_t kCdFontGrayMax = 255;")
    w("// 字符集: **顺序与下面每张表的顺序一一对应** (数字 0..9, 小数点, 减号)")
    w('constexpr char kCdFontChars[] = "%s";' % "".join(table["order"]))
    w("constexpr int32_t kCdFontCharCount = %d;" % len(table["order"]))
    w("constexpr int32_t kCdFontDataBytes = %d; // cd_glyph_px 的字节数 (离线断言用)"
      % table["canvas_bytes"])
    w("// 每个字符的墨迹像素数 (离线断言用: 解码出来的块数必须与它一致)")
    w("constexpr int32_t kCdFontInkPixels[%d] = {%s};"
      % (len(table["order"]), ", ".join(str(table["ink"][ch]) for ch in table["order"])))
    w("// 单字符最坏墨迹像素数 (= 预算估算里「最胖的一个字符」)")
    w("constexpr int32_t kCdFontMaxInkPixels = %d;" % table["max_ink"])
    w("")
    w("// 逐像素灰度画布 (行主序): 第 gi 个字符 = cd_glyph_px[gi * W * H + y * W + x]")
    w("//   0 = 透明 (不画); 1..255 = 直接写进画面的灰度 (255 = 纯白)")
    w("constexpr uint8_t cd_glyph_px[] = {")
    for ch in table["order"]:
        w("    // '%s'  (%d ink pixels)" % (ch, table["ink"][ch]))
        for row in table["px"][ch]:
            w("    %s," % ", ".join("%3d" % v for v in row))
        w("")
    w("};")
    w("")
    w("// 字符 -> 表下标 (-1 = 表里没有这个字符)。运行期不查表也能用:")
    w("//   数字 '0'..'9' 直接是下标 0..9, '.' = 10, '-' = 11 (见 kCdFontChars 的顺序)。")
    w("constexpr int32_t CdFontIndexOf(char c)")
    w("{")
    w("    for (int32_t i = 0; i < kCdFontCharCount; i++)")
    w("        if (kCdFontChars[i] == c)")
    w("            return i;")
    w("    return -1;")
    w("}")
    w("")
    w("} // namespace PtHook")
    w("")
    w("#endif // PZTK_CD_FONT_H")
    w("")
    return "\n".join(out)


# ---------------------------------------------------------------------------
# PNG preview: draw exactly what the DLL will draw (one rectangle per pixel,
# "max" aggregation for the 2x2 ladder step) onto a simulated card face, then
# blow it up so the user can judge the glyphs without starting the game.
#
# ★ v36: two ink models, mirroring hook_ctl.h's CdGlyphInk():
#   * "alpha"  (default, = CDINK_ALPHA): RGB = 255 (white), A = coverage
#     => the preview must COMPOSITE: out = face + (255 - face) * A / 255.
#   * "opaque" (fallback, = CDINK_OPAQUE / what v35 shipped): RGB = max(grey,
#     floor), A = 255 => out = RGB, painted straight over the face.
# ---------------------------------------------------------------------------
def glyph_blocks(grid, step, mode="alpha", floor=INK_FLOOR, threshold=INK_FLOOR_V36):
    """Mirror of the C++ CdGlyphBlocks(): fixed-block aggregation + ink colour.

    Returns [(x, y, w, h, gray, alpha)] with x/y relative to the cell.  The rules
    are exactly the shipped ones:
      * the canvas is cut into `step` x `step` blocks (clipped at the border);
      * a block's value is the MAX of its pixels (monotone: white stays white);
      * value == 0 (all transparent) => the block is **not drawn**;
        ★ v36: that is the ONLY reason not to draw -- there is no threshold any
        more (`threshold` exists purely to reproduce the v35 bug in "opaque" mode,
        and the v36 default is 0 = no drop path);
      * alpha mode  => (255, coverage(value));
        opaque mode => (max(value, floor), 255).
    """
    out = []
    for by in range(0, CANVAS_H, step):
        bh = min(step, CANVAS_H - by)
        for bx in range(0, CANVAS_W, step):
            bw = min(step, CANVAS_W - bx)
            gray = max(grid[y][x] for y in range(by, by + bh) for x in range(bx, bx + bw))
            if gray == 0:
                continue
            if threshold and gray < threshold:
                continue          # ★ v35 的"低于某灰度就跳过" —— v36 默认 threshold = 0
            if mode == "opaque":
                out.append((bx, by, bw, bh, max(gray, floor), 255))
            else:
                out.append((bx, by, bw, bh, 255, coverage_of(gray)))
    return out


def coverage_of(gray):
    """Mirror of the C++ CdGlyphCoverage(): stored grey -> coverage (0..255).

    Inverse of `quantise()`: `gray = GRAY_BG + cov * (255-GRAY_BG)/255`.
    gray == 0 => 0; any gray > 0 => >= 1 (an inked pixel can never become fully
    transparent -- that is the "no holes" half of the v36 invariant).
    """
    if gray == 0:
        return 0
    if gray <= GRAY_BG:
        return 1
    a = (gray - GRAY_BG) * 255 // (255 - GRAY_BG)
    return max(1, min(255, a))


def display_cells(sample):
    """Mirror of the C++ CdDisplayChar(): 最高位..个位, 小数点, 十分位."""
    digits = sample.replace(".", "")
    ndig = len(digits)
    chars = []
    for cell in range(ndig + 1):
        if cell == ndig - 1:
            chars.append(".")
        elif cell < ndig - 1:
            chars.append(digits[cell])
        else:
            chars.append(digits[ndig - 1])
    return chars


def blend(face, gray, alpha):
    """The blend the game does for us: white ink at `alpha` over `face`.

    alpha == 255 => exactly `gray` (opaque); otherwise per-channel
    `face + (255 - face) * alpha / 255`.  Never darker than `face`.
    """
    if alpha >= 255:
        return (gray, gray, gray)
    out = []
    for c in face:
        out.append(c + ((255 - c) * alpha + 127) // 255)
    return tuple(out)


def compose_strip(table, sample, step, card_rgb=CARD_DARK, mode="alpha", floor=INK_FLOOR,
                  threshold=INK_FLOOR_V36):
    """Return a (CARD_W, CARD_H) RGB canvas with the digit strip painted."""
    chars = display_cells(sample)
    cells = len(chars)
    total_w = cells * CANVAS_W + cells * CELL_GAP
    x0 = (CARD_W - total_w) // 2
    y0 = DIGIT_TOP                     # v34 起数字组 = 卡顶 27..40 (不再内缩 1px 居中)
    canvas = Image.new("RGB", (CARD_W, CARD_H), card_rgb)
    # the game draws its own sun cost from card top + 54 (we never touch it)
    for y in range(SUN_COST_TOP, CARD_H):
        for x in range(CARD_W):
            canvas.putpixel((x, y), SUN_COST)
    for cell, ch in enumerate(chars):
        cell_x = x0 + cell * (CANVAS_W + CELL_GAP)
        for (bx, by, bw, bh, gray, alpha) in glyph_blocks(table["px"][ch], step, mode, floor,
                                                          threshold):
            for y in range(by, by + bh):
                for x in range(bx, bx + bw):
                    face = canvas.getpixel((cell_x + x, y0 + y))
                    canvas.putpixel((cell_x + x, y0 + y), blend(face, gray, alpha))
    return canvas


def preview_png(table, out_path, scale=8, samples=("35.0", "2.4", "12.4", "99.9"),
                mode="alpha", floor=INK_FLOOR, threshold=INK_FLOOR_V36):
    """Compose the **4-row** comparison sheet and write it as a PNG.

    The four rows are exactly the ones the v36 report needs:
        ① 1x1 on the cooling (dark) face     ③ 2x2 on the cooling (dark) face
        ② 1x1 on the REVEALED (bright) face  ④ 2x2 on the revealed face
    With the default "alpha" ink model every row is free of both holes and black
    dots; with `mode="opaque"` + floor/threshold the same sheet reproduces the
    v35 look (that is `backup/shots/v36_glyph_before.png`).
    """
    label_font = ImageFont.load_default()
    rows = [
        ("1) 1x1 pixel rects / %dx zoom / COOLING (dark) face %s" % (scale, CARD_DARK), 1, CARD_DARK),
        ("2) 1x1 pixel rects / %dx zoom / REVEALED (brightest) face %s" % (scale, CARD_BRIGHT), 1,
         CARD_BRIGHT),
        ("3) 2x2 max-aggregated rects / %dx zoom / cooling (dark) face" % scale, 2, CARD_DARK),
        ("4) 2x2 max-aggregated rects / %dx zoom / revealed (brightest) face" % scale, 2,
         CARD_BRIGHT),
    ]
    zoom_w = CARD_W * scale
    zoom_h = CARD_H * scale
    pad = 12
    label_h = 14
    width = len(samples) * (zoom_w + pad) + pad
    sheet = Image.new("RGB", (width, len(rows) * (label_h + zoom_h + pad)), (24, 26, 24))
    draw = ImageDraw.Draw(sheet)
    y = 0
    for (title, step, face) in rows:
        draw.text((pad, y + 2), "%s  [ink %s]" % (title, mode), font=label_font, fill=(210, 210, 210))
        y += label_h
        x = pad
        for s in samples:
            img = compose_strip(table, s, step, face, mode, floor, threshold)
            img = img.resize((zoom_w, zoom_h), Image.NEAREST)
            sheet.paste(img, (x, y))
            x += img.width + pad
        y += zoom_h + pad
    sheet.save(out_path)
    return sheet.size


def preview_stats(table, mode, floor=INK_FLOOR, threshold=INK_FLOOR_V36,
                  faces=(("dark", CARD_DARK), ("bright", CARD_BRIGHT), ("floor160", CARD_FLOOR))):
    """Quantitative "no holes / no black dots" evidence for the report.

    For every face, every character and both ladder steps we count the emitted
    blocks and classify each one against the face it lands on:
      * darker   : the painted pixel ends up DARKER than the face  -> "black dot"
      * equal    : ... exactly the same value as the face          -> invisible = "hole"
      * faint    : ... at most FAINT_EPS brighter than the face     -> effectively invisible
      * ink      : ... clearly visible
    A glyph pixel can only be "equal/faint" if the ink model does not blend --
    with the v36 alpha model the darkest ink over any face is strictly >= the
    face, and `equal` can only happen for the very faintest coverage on a
    near-white face.
    """
    faint_eps = 4
    lines = []
    for name, face in faces:
        stats = {}
        for step in (1, 2):
            darker = equal = faint = ink = blocks = 0
            for ch in table["order"]:
                for (bx, by, bw, bh, gray, alpha) in glyph_blocks(table["px"][ch], step, mode,
                                                                  floor, threshold):
                    for y in range(by, by + bh):
                        for x in range(bx, bx + bw):
                            out = blend(face, gray, alpha)
                            delta = min(out[c] - face[c] for c in range(3))
                            if alpha >= 255:
                                delta = gray - max(face)
                            if delta < 0:
                                darker += 1
                            elif delta == 0:
                                equal += 1
                            elif delta <= faint_eps:
                                faint += 1
                            else:
                                ink += 1
                    blocks += 1
            stats[step] = (blocks, darker, equal, faint, ink)
        lines.append(
            "  face %-9s %s : 1x1 块 %3d -> 更暗 %4d / 同底色 %4d / 极淡 %4d / 清楚 %4d | "
            "2x2 块 %3d -> 更暗 %4d / 同底色 %4d / 极淡 %4d / 清楚 %4d"
            % (name, face, stats[1][0], stats[1][1], stats[1][2], stats[1][3], stats[1][4],
               stats[2][0], stats[2][1], stats[2][2], stats[2][3], stats[2][4]))
    return lines


def main(argv=None):
    ap = argparse.ArgumentParser(description="generate cd_font.h (offline grayscale glyphs)")
    ap.add_argument("--font", default=DEFAULT_FONT, help="TrueType/OpenType file")
    ap.add_argument("--font-size", type=int, default=None,
                    help="pixel size (default: largest that still fits the cell)")
    ap.add_argument("--chars", default=CHARS, help="characters to rasterise")
    ap.add_argument("--out", default=DEFAULT_OUT, help="output header path")
    ap.add_argument("--check", action="store_true", help="verify only, do not write")
    ap.add_argument("--preview", action="store_true", help="also print ASCII art")
    ap.add_argument("--preview-png", default=None, help="write a magnified preview PNG here")
    ap.add_argument("--preview-scale", type=int, default=8, help="zoom factor for --preview-png")
    ap.add_argument("--preview-stats", action="store_true",
                    help="print the per-face / per-step hole-vs-dot statistics")
    # ★ v36: the ink model.  The runtime values live in hook_ctl.h
    #   (CdInkMode / kCdFontInkFloor / kCdFontInkMinCover); `--preview-ink-mode
    #   opaque` + `--preview-ink-floor 160 --preview-ink-threshold 32` reproduces
    #   exactly what v35 shipped (that is the "before" sheet of the report).
    ap.add_argument("--preview-ink-mode", choices=("alpha", "opaque"), default="alpha",
                    help="alpha = v36 default (white + A=coverage); opaque = v35 look")
    ap.add_argument("--preview-ink-floor", type=int, default=INK_FLOOR,
                    help="[opaque mode only] clamp the ink grey up to this (kCdFontInkFloor)")
    ap.add_argument("--preview-ink-threshold", type=int, default=INK_FLOOR_V36,
                    help="[opaque mode only] drop blocks fainter than this (v35 had 32; "
                         "v36 has NO such path, so the default is 0)")
    args = ap.parse_args(argv)

    if not os.path.exists(args.font):
        sys.stderr.write("ERROR: font not found: %s\n" % args.font)
        return 1

    table = build(args.font, args.chars, args.font_size)
    sys.stdout.write(
        "font=%s size=%d baseline=%d chars=%d canvas=%dx%d ink=%d max_ink=%d bytes=%d\n"
        % (table["font_name"], table["font_size"], table["baseline"], len(table["order"]),
           CANVAS_W, CANVAS_H, table["total_ink"], table["max_ink"], table["canvas_bytes"]))
    for ch in table["order"]:
        sys.stdout.write("  '%s' ink=%d\n" % (ch, table["ink"][ch]))
    if args.preview:
        sys.stdout.write("\n".join(preview(table)) + "\n")

    text = render_header(table)
    if args.check:
        if not os.path.exists(args.out):
            sys.stderr.write("ERROR: --check but %s does not exist\n" % args.out)
            return 1
        with open(args.out, "r", encoding="utf-8") as fh:
            existing = fh.read()

        # ignore the date/command lines, they legitimately differ between runs
        def body(t):
            return [ln for ln in t.splitlines()
                    if not ln.startswith("//     生成日:")
                    and not ln.startswith("//     命令:")]

        if body(existing) != body(text):
            sys.stderr.write("MISMATCH: %s is not what the generator produces\n" % args.out)
            return 1
        sys.stdout.write("OK: %s matches the generator output\n" % args.out)
    else:
        with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        sys.stdout.write("wrote %s (%d bytes)\n" % (args.out, os.path.getsize(args.out)))

    if args.preview_png:
        size = preview_png(table, args.preview_png, args.preview_scale,
                           mode=args.preview_ink_mode,
                           floor=args.preview_ink_floor,
                           threshold=args.preview_ink_threshold)
        sys.stdout.write("wrote %s (%dx%d, scale %d, ink mode %s, floor %d, threshold %d)\n"
                         % (args.preview_png, size[0], size[1], args.preview_scale,
                            args.preview_ink_mode, args.preview_ink_floor,
                            args.preview_ink_threshold))
    if args.preview_stats or args.preview_png:
        # ── 逐格统计 (报告里的"无洞 / 无黑点"证据) ──
        for mode in ("alpha", "opaque"):
            sys.stdout.write("ink mode %s:\n" % mode)
            thr = args.preview_ink_threshold if mode == "opaque" else INK_FLOOR_V36
            flr = args.preview_ink_floor if mode == "opaque" else INK_FLOOR
            for line in preview_stats(table, mode, flr, thr):
                sys.stdout.write(line + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
