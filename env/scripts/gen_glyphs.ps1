# ---------------------------------------------------------------------------
# 生成 tool-src/frontend/imgui_glyphs.h
#
# 原理: 扫描工具源码里"字符串常量"用到的全部字符 (中文 + 全角标点),
#       合并成紧凑的 ImGui 字形区间。字集精确 -> 图集小、内存低、绝不缺字。
#
# 为什么要扫字符串而不是扫全文: 源码里中文注释有几千字, 全扫会让图集膨胀
# 好几倍; 界面上真正会显示的只有字符串常量。
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File env\scripts\gen_glyphs.ps1
#   powershell ... -File env\scripts\gen_glyphs.ps1 -Check   (只对比不写文件)
#   powershell ... -File env\scripts\gen_glyphs.ps1 -Prune   (允许裁掉旧文件里
#                                                            不再用到的字)
#
# 注意 1: 界面新增中文后必须重新运行本脚本, 否则新字会显示成空白方块。
# 注意 2: 默认与旧文件取并集 (只增不减) —— 宁可贵几百个字形, 也绝不少字。
# 注意 3: 本脚本含中文, 必须保存为 UTF-8 带 BOM, 否则 Windows PowerShell 5.1
#         会按 ANSI 解码, 写出的头文件注释与字集会乱码。
# ---------------------------------------------------------------------------
param(
    [switch]$Check,          # 只统计/对比, 不覆盖 imgui_glyphs.h
    [switch]$Prune,          # 允许丢弃旧文件里本轮未用到的字符 (默认保留)
    [string]$OutFile         # 默认 tool-src\frontend\imgui_glyphs.h
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root      = Split-Path -Parent (Split-Path -Parent $ScriptDir)   # env\scripts -> 仓库根
if (-not $OutFile) {
    $OutFile = Join-Path $Root 'tool-src\frontend\imgui_glyphs.h'
}

# --- 扫描范围: 前端界面 + 核心层 (核心层的名字表会被界面直接显示) ---------
$files = @()
$files += Join-Path $Root 'tool-src\frontend\imgui_main.cpp'
$files += Get-ChildItem (Join-Path $Root 'tool-src\src')  -Filter *.cpp | ForEach-Object { $_.FullName }
$files += Get-ChildItem (Join-Path $Root 'tool-src\inc')  -Filter *.h   | ForEach-Object { $_.FullName }
$files  = $files | Where-Object { Test-Path $_ } | Sort-Object -Unique

$set = New-Object 'System.Collections.Generic.HashSet[int]'

foreach ($f in $files) {
    $text = [System.IO.File]::ReadAllText($f, [System.Text.Encoding]::UTF8)
    # 一次扫描 "注释 或 字符串", 只取字符串: 既排除注释里的中文,
    # 又不会被字符串内部的 // (例如 URL) 误伤。
    $rx = '("(?:\\.|[^"\\])*")|(/\*.*?\*/|//[^\r\n]*)'
    foreach ($m in [regex]::Matches($text, $rx, [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        if (-not $m.Groups[1].Success) { continue }
        $s = $m.Groups[1].Value
        for ($i = 0; $i -lt $s.Length; $i++) {
            $cp = [int][char]$s[$i]
            # 0x0020~0x00FF 由固定的"基本拉丁"区间覆盖, 这里只收 >= 0x0100;
            # 0xD800~0xDFFF 是 UTF-16 代理码位 (emoji 等非 BMP 字符), 不能进字库。
            if ($cp -ge 0x0100 -and -not ($cp -ge 0xD800 -and $cp -le 0xDFFF)) { [void]$set.Add($cp) }
        }
    }
}

# --- 旧文件 (若存在): 解析出字符集合, 默认并入本轮结果 -----------------------
$oldSet = New-Object 'System.Collections.Generic.HashSet[int]'
if (Test-Path $OutFile) {
    $old = [System.IO.File]::ReadAllText($OutFile, [System.Text.Encoding]::UTF8)
    foreach ($m in [regex]::Matches($old, '0x([0-9A-Fa-f]{4})\s*,\s*0x([0-9A-Fa-f]{4})')) {
        $a = [Convert]::ToInt32($m.Groups[1].Value, 16)
        $b = [Convert]::ToInt32($m.Groups[2].Value, 16)
        for ($c = $a; $c -le $b; $c++) { [void]$oldSet.Add($c) }
    }
}

$scanned = New-Object 'System.Collections.Generic.HashSet[int]'
foreach ($c in $set) { [void]$scanned.Add($c) }

if (-not $Prune) {
    # 只增不减: 旧文件里的字符全部保留 (多几百个字形对图集/内存影响极小),
    # 同样只并入 >= 0x0100 且非代理码位的部分, 避免与基本拉丁区间重叠。
    foreach ($c in $oldSet) {
        if ($c -ge 0x0100 -and -not ($c -ge 0xD800 -and $c -le 0xDFFF)) { [void]$set.Add($c) }
    }
}

# --- 合并连续码位为区间 -----------------------------------------------------
$cps = $set | Sort-Object
$ranges = @()
if ($cps.Count -gt 0) {
    $start = $cps[0]; $prev = $cps[0]
    for ($i = 1; $i -lt $cps.Count; $i++) {
        if ($cps[$i] -eq $prev + 1) { $prev = $cps[$i]; continue }
        $ranges += ,@($start, $prev)
        $start = $cps[$i]; $prev = $cps[$i]
    }
    $ranges += ,@($start, $prev)
}

Write-Host ("扫描 {0} 个文件 -> 字符串用字 {1} 个; 合并后共 {2} 个字符 / {3} 个区间" -f `
            $files.Count, $scanned.Count, $cps.Count, $ranges.Count)

# --- 对比旧文件 -------------------------------------------------------------
if ($oldSet.Count -gt 0) {
    # 基础拉丁区 0x0020~0x00FF 是固定写死的, 对比时排除, 只看 CJK/全角标点
    $notUsed = @($oldSet | Where-Object { $_ -ge 0x0100 -and -not $scanned.Contains($_) } | Sort-Object)
    $newCount = @($scanned | Where-Object { $_ -ge 0x0100 -and -not $oldSet.Contains($_) }).Count
    $oldCJK = @($oldSet | Where-Object { $_ -ge 0x0100 }).Count
    Write-Host ("旧文件 CJK/标点 {0} 个; 本次新增 {1} 个; 旧有但本轮源码未用到 {2} 个{3}" -f `
                $oldCJK, $newCount, $notUsed.Count, $(if ($Prune) { ' (将被裁掉)' } else { ' (已保留)' }))
    if ($notUsed.Count -gt 0) {
        $shown = (($notUsed | ForEach-Object { [char]$_ }) -join '')
        Write-Host ("未用到: {0}" -f $shown) -ForegroundColor DarkGray
    }
}

# --- 生成新文件 -------------------------------------------------------------
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('// 本文件由脚本自动生成 (env/scripts/gen_glyphs.ps1), 勿手改。')
[void]$sb.AppendLine('// 内容: PvZ Toolkit 界面实际用到的全部字符, 生成紧凑的 ImGui 字形区间。')
[void]$sb.AppendLine('// 好处: 字集精确 -> 图集小、内存低、且绝不缺字。')
[void]$sb.AppendLine('#pragma once')
[void]$sb.AppendLine('#include "imgui.h"')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('static const ImWchar kGlyphRanges[] = {')
[void]$sb.AppendLine('    0x0020, 0x00FF, // 基本拉丁')
foreach ($r in $ranges) {
    [void]$sb.AppendLine(('    0x{0:X4}, 0x{1:X4},' -f $r[0], $r[1]))
}
[void]$sb.AppendLine('    0,')
[void]$sb.AppendLine('};')

if ($Check) {
    Write-Host '(-Check) 未写入文件'
    exit 0
}

[System.IO.File]::WriteAllText($OutFile, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("已写入: {0}" -f $OutFile)
