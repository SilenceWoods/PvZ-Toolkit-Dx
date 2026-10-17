
#include "../inc/process.h"

#include <cstdint>

namespace Pt
{

Process::Process()
{
    this->hwnd = nullptr;
    this->pid = 0;
    this->handle = nullptr;
}

Process::~Process()
{
    if (IsValid())
        CloseHandle(this->handle);
}

bool Process::OpenByWindow(const wchar_t *class_name, const wchar_t *window_name)
{
    if (IsValid())
        CloseHandle(this->handle);

    this->hwnd = nullptr;
    this->pid = 0;
    this->handle = nullptr;

    this->hwnd = FindWindowW(class_name, window_name);
    if (this->hwnd != nullptr)
    {
        GetWindowThreadProcessId(this->hwnd, &(this->pid));
        if (this->pid != 0)
        {
            this->handle = OpenProcess(PROCESS_ALL_ACCESS, false, this->pid);
            if (this->handle != nullptr)
            {
                //
            }
        }
    }

    // assert(PROCESS_ALL_ACCESS == 0x001FFFFF);

#ifdef _DEBUG
    std::wcout << L"查找窗口: " << (class_name == nullptr ? L"nullptr" : class_name)               //
               << L" " << (window_name == nullptr ? L"nullptr" : window_name) << std::endl         //
               << L" -> " << this->hwnd << L" " << this->pid << L" " << this->handle << std::endl; //
#endif

    // 返回的是窗口有没有找到而不是进程有没有打开
    return this->hwnd != nullptr;
}

bool Process::IsValid()
{
    if (this->handle == nullptr)
        return false;

    DWORD exit_code;
    BOOL ret = GetExitCodeProcess(this->handle, &exit_code);
    bool valid = (ret != 0 && exit_code == STILL_ACTIVE);

#ifdef _DEBUG
    if (!valid)
        std::wcout << L"当前进程不可用." << std::endl;
#endif

    return valid;
}

bool Process::ReadBytes(void *buffer, uintptr_t addr, size_t size)
{
    if (!IsValid() || buffer == nullptr || size == 0)
        return false;

    SIZE_T read_size = 0;
    BOOL ret = ReadProcessMemory(this->handle, reinterpret_cast<const void *>(addr), buffer, size, &read_size);

    return ret != 0 && read_size == size;
}

bool Process::WriteBytes(const void *buffer, uintptr_t addr, size_t size)
{
    if (!IsValid() || buffer == nullptr || size == 0)
        return false;

    SIZE_T write_size = 0;
    BOOL ret = WriteProcessMemory(this->handle, reinterpret_cast<void *>(addr), buffer, size, &write_size);

    return ret != 0 && write_size == size;
}

// ---------------------------------------------------------------------------
// 字节特征扫描 (见 inc/process.h 的说明)
// ---------------------------------------------------------------------------
std::vector<Process::PatternByte> Process::ParsePattern(const char *text)
{
    std::vector<PatternByte> pattern;
    if (text == nullptr)
        return pattern;

    auto hex_value = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    auto is_wild = [](char c) { return c == '?' || c == '*'; };
    auto hex_value2 = [&](char c) { return hex_value(c); };

    const char *p = text;
    while (*p != 0)
    {
        if (*p == ' ' || *p == '\t' || *p == ',')
        {
            p++;
            continue;
        }

        PatternByte byte;
        const char hi_char = p[0];
        const char lo_char = p[1];

        // 先判"孤立通配": "8B ? 38" 里的单问号, 或者 "8B ?" 结尾。
        // 注意 "??" 不算孤立 (那是整字节通配, 要吃掉两个字符)。
        const bool lone_wild =
            is_wild(hi_char) && (lo_char == 0 || (!is_wild(lo_char) && hex_value2(lo_char) < 0));
        if (lone_wild)
        {
            byte.value = 0;
            byte.mask = 0x00;
            p += 1;
        }
        else if (is_wild(hi_char) && is_wild(lo_char)) // "??" / "*?"
        {
            byte.value = 0;
            byte.mask = 0x00;
            p += 2;
        }
        else if (is_wild(hi_char)) // "?8" 低半字节精确
        {
            const int lo = hex_value(lo_char);
            if (lo < 0)
                return std::vector<PatternByte>();
            byte.value = static_cast<unsigned char>(lo);
            byte.mask = 0x0f;
            p += 2;
        }
        else if (is_wild(lo_char)) // "4?" 高半字节精确
        {
            const int hi = hex_value(hi_char);
            if (hi < 0)
                return std::vector<PatternByte>();
            byte.value = static_cast<unsigned char>(hi << 4);
            byte.mask = 0xf0;
            p += 2;
        }
        else // 两个十六进制字符
        {
            const int hi = hex_value(hi_char);
            const int lo = hex_value(lo_char);
            if (hi < 0 || lo < 0)
                return std::vector<PatternByte>();
            byte.value = static_cast<unsigned char>(hi * 16 + lo);
            byte.mask = 0xff;
            p += 2;
        }
        pattern.push_back(byte);
    }
    return pattern;
}

std::vector<uintptr_t> Process::ScanBuffer(const std::vector<PatternByte> &pattern,
                                          const unsigned char *data, size_t size, uintptr_t base,
                                          size_t max_hits)
{
    std::vector<uintptr_t> hits;
    if (pattern.empty() || data == nullptr || size < pattern.size() || max_hits == 0)
        return hits;

    const size_t last = size - pattern.size();
    for (size_t i = 0; i <= last; i++)
    {
        bool ok = true;
        for (size_t j = 0; j < pattern.size(); j++)
        {
            const PatternByte &pb = pattern[j];
            if ((data[i + j] & pb.mask) != pb.value)
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            hits.push_back(base + i);
            if (hits.size() >= max_hits)
                break;
        }
    }
    return hits;
}

std::vector<uintptr_t> Process::ScanPattern(const char *text, uintptr_t start, size_t size, size_t max_hits)
{
    std::vector<uintptr_t> hits;
    const std::vector<PatternByte> pattern = ParsePattern(text);
    if (!IsValid() || pattern.empty() || size == 0)
        return hits;

    const size_t chunk = 1 * 1024 * 1024;         // 一次读 1 MB
    const size_t overlap = pattern.size() - 1;    // 块间重叠, 免得跨块的匹配漏掉
    std::vector<unsigned char> buffer(chunk + overlap);

    for (size_t done = 0; done < size;)
    {
        const size_t want = (size - done > chunk) ? chunk : (size - done);
        const size_t read_size = want + ((done + want < size) ? overlap : 0);
        if (!this->ReadBytes(buffer.data(), start + done, read_size))
            break;

        const std::vector<uintptr_t> part =
            ScanBuffer(pattern, buffer.data(), read_size, start + done, max_hits - hits.size());
        for (uintptr_t addr : part)
            hits.push_back(addr);
        if (hits.size() >= max_hits)
            break;

        done += want;
    }
    return hits;
}

uintptr_t Process::FindFunctionStartInBuffer(const unsigned char *data, size_t size, size_t offset,
                                             uintptr_t base, size_t max_back)
{
    if (data == nullptr || offset >= size)
        return 0;

    const size_t lowest = (offset > max_back) ? (offset - max_back) : 0;
    // 注意从 offset **本身**开始找 (i = offset + 1 再自减), 否则"地址正好是函数入口"会漏掉
    for (size_t i = offset + 1; i-- > lowest;)
    {
        // 8B FF 55 8B EC    mov edi, edi; push ebp; mov ebp, esp  (MSVC 热补丁序言) —— 先判长的
        if (data[i] == 0x8b && i + 4 < size && data[i + 1] == 0xff && data[i + 2] == 0x55 &&
            data[i + 3] == 0x8b && data[i + 4] == 0xec)
            return base + i;
        // 55 8B EC          push ebp; mov ebp, esp      (MSVC / MinGW 常见)
        // 前面如果是 8B FF, 那真正的入口还要往前 2 个字节 (热补丁序言)
        if (data[i] == 0x55 && i + 2 < size && data[i + 1] == 0x8b && data[i + 2] == 0xec)
        {
            if (i >= 2 && data[i - 2] == 0x8b && data[i - 1] == 0xff)
                return base + (i - 2);
            return base + i;
        }
        // 55 89 E5          push ebp; mov ebp, esp      (GCC)
        if (data[i] == 0x55 && i + 2 < size && data[i + 1] == 0x89 && data[i + 2] == 0xe5)
            return base + i;
    }

    // 发布版常见"帧指针省略(FPO)", 很多函数没有标准序言 —— 退一步用**代码填充边界**:
    // MSVC 把函数按 16 字节对齐, 函数之间通常用 CC(int3) 填充, 于是
    // "往回遇到的最后一段连续 CC 之后"就是函数入口。
    size_t i = offset;
    while (i > lowest)
    {
        if (data[i - 1] == 0xcc)
        {
            // i-1 是 CC; 往前跳过整段 CC, 落点就是新函数的第一条指令
            size_t j = i - 1;
            while (j > lowest && data[j - 1] == 0xcc)
                j--;
            if (j > lowest && i - j >= 2) // 至少 2 个 CC 才算填充 (单个 CC 可能只是 int3 断点)
                return base + i;
        }
        i--;
    }
    return 0;
}

uintptr_t Process::FindFunctionStart(uintptr_t addr, size_t max_back)
{
    if (!IsValid() || addr <= max_back)
        return 0;

    const uintptr_t start = addr - max_back;
    std::vector<unsigned char> buffer(max_back);
    if (!ReadBytes(buffer.data(), start, max_back))
        return 0;

    return FindFunctionStartInBuffer(buffer.data(), buffer.size(), static_cast<size_t>(addr - start),
                                     start, max_back);
}

// ---------------------------------------------------------------------------
// 最小 x86(32 位) 指令长度解码器
//   目的: 验证"地址 X 是一条指令的边界" —— 定位函数入口时, 只有"从入口逐条解码能
//   恰好走到命中点"的候选才是真入口 (发布版没有 55 8B EC 序言, 光靠字节猜不住)。
//   只覆盖编译器常生成的指令; 不认识的返回 0, 调用方保守跳过。
// ---------------------------------------------------------------------------
static size_t ModRMTotal(const unsigned char *p, size_t avail, size_t i, bool *ok)
{
    // 解析 ModRM (+SIB +disp), 返回从 i 开始到操作数结束的长度
    if (i >= avail)
    {
        *ok = false;
        return 0;
    }
    const unsigned char modrm = p[i++];
    const unsigned char mod = modrm >> 6;
    const unsigned char rm = modrm & 7;

    if (mod != 3 && rm == 4) // 有 SIB
    {
        if (i >= avail)
        {
            *ok = false;
            return 0;
        }
        const unsigned char sib = p[i++];
        if (mod == 0 && (sib & 7) == 5)
            i += 4; // [index*scale + disp32]
    }

    if (mod == 0 && rm == 5)
        i += 4; // disp32
    else if (mod == 1)
        i += 1; // disp8
    else if (mod == 2)
        i += 4; // disp32

    if (i > avail)
    {
        *ok = false;
        return 0;
    }
    return i;
}

size_t Process::X86Length(const unsigned char *p, size_t avail)
{
    if (p == nullptr || avail == 0)
        return 0;

    size_t i = 0;
    for (int guard = 0; guard < 8 && i < avail; guard++) // 前缀
    {
        const unsigned char b = p[i];
        if (b == 0x66 || b == 0x67 || b == 0xf0 || b == 0xf2 || b == 0xf3 || b == 0x2e || b == 0x36 ||
            b == 0x3e || b == 0x26 || b == 0x64 || b == 0x65)
            i++;
        else
            break;
    }
    if (i >= avail)
        return 0;

    const unsigned char op = p[i++];
    bool ok = true;
    bool need_modrm = false;
    int imm = 0; // 立即数长度

    if (op == 0x0f) // 双字节指令
    {
        if (i >= avail)
            return 0;
        const unsigned char op2 = p[i++];
        if (op2 >= 0x80 && op2 <= 0x8f)
            imm = 4; // jcc rel32
        else if (op2 == 0xb6 || op2 == 0xb7 || op2 == 0xbe || op2 == 0xbf || op2 == 0xaf || op2 == 0x1f ||
                 op2 == 0xb1 || op2 == 0xbc || op2 == 0xbd || op2 == 0x40 || op2 == 0x41 || op2 == 0x42 ||
                 op2 == 0x43 || op2 == 0x44 || op2 == 0x45 || op2 == 0x46 || op2 == 0x47 || op2 == 0x48 ||
                 op2 == 0x49 || op2 == 0x4a || op2 == 0x4b || op2 == 0x4c || op2 == 0x4d || op2 == 0x4e ||
                 op2 == 0x4f || op2 == 0x10 || op2 == 0x11 || op2 == 0x12 || op2 == 0x13 || op2 == 0x14 ||
                 op2 == 0x15 || op2 == 0x28 || op2 == 0x29 || op2 == 0x2a || op2 == 0x2b || op2 == 0x2c ||
                 op2 == 0x2d || op2 == 0x51 || op2 == 0x52 || op2 == 0x53 || op2 == 0x54 || op2 == 0x55 ||
                 op2 == 0x56 || op2 == 0x57 || op2 == 0x58 || op2 == 0x59 || op2 == 0x5a || op2 == 0x5b ||
                 op2 == 0x5c || op2 == 0x5d || op2 == 0x5e || op2 == 0x5f || op2 == 0xc0 || op2 == 0xc1 ||
                 op2 == 0xc2 || op2 == 0xc3 || op2 == 0xc4 || op2 == 0xc5 || op2 == 0xc6 || op2 == 0xc7 ||
                 op2 == 0xd0 || op2 == 0xd1 || op2 == 0xd2 || op2 == 0xd3 || op2 == 0xd4 || op2 == 0xd5 ||
                 op2 == 0xd6 || op2 == 0xd7 || op2 == 0xd8 || op2 == 0xd9 || op2 == 0xda || op2 == 0xdb ||
                 op2 == 0xdc || op2 == 0xdd || op2 == 0xde || op2 == 0xdf || op2 == 0xe0 || op2 == 0xe1 ||
                 op2 == 0xe2 || op2 == 0xe3 || op2 == 0xe4 || op2 == 0xe5 || op2 == 0xe6 || op2 == 0xe7 ||
                 op2 == 0xe8 || op2 == 0xe9 || op2 == 0xea || op2 == 0xeb || op2 == 0xec || op2 == 0xed ||
                 op2 == 0xee || op2 == 0xef || op2 == 0xf0 || op2 == 0xf1 || op2 == 0xf2 || op2 == 0xf3 ||
                 op2 == 0xf4 || op2 == 0xf5 || op2 == 0xf6 || op2 == 0xf7 || op2 == 0xf8 || op2 == 0xf9 ||
                 op2 == 0xfa || op2 == 0xfb || op2 == 0xfc || op2 == 0xfd || op2 == 0xfe || op2 == 0xff ||
                 op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03 || op2 == 0x04 || op2 == 0x05 ||
                 op2 == 0x06 || op2 == 0x07 || op2 == 0x08 || op2 == 0x09 || op2 == 0x0a)
            need_modrm = (op2 != 0x05 && op2 != 0x06 && op2 != 0x07 && op2 != 0x0b);
        else
            return 0;
    }
    else if (op >= 0x00 && op <= 0x3b)
    {
        // add/or/adc/sbb/and/sub/xor/cmp: (op&7)==4 -> AL,imm8 ; ==5 -> eAX,imm32 ; 其余 ModRM
        const unsigned char low = op & 7;
        if (low == 4)
            imm = 1;
        else if (low == 5)
            imm = 4;
        else
            need_modrm = true;
    }
    else if (op >= 0x40 && op <= 0x5f)
    {
        // inc/dec/push/pop reg: 单字节
    }
    else if (op == 0x68)
        imm = 4;
    else if (op == 0x69)
    {
        need_modrm = true;
        imm = 4;
    }
    else if (op == 0x6a)
        imm = 1;
    else if (op == 0x6b)
    {
        need_modrm = true;
        imm = 1;
    }
    else if (op >= 0x70 && op <= 0x7f)
        imm = 1;
    else if (op == 0x80 || op == 0x82 || op == 0x83)
    {
        need_modrm = true;
        imm = 1;
    }
    else if (op == 0x81)
    {
        need_modrm = true;
        imm = 4;
    }
    else if ((op >= 0x84 && op <= 0x8f))
        need_modrm = true;
    else if (op >= 0x90 && op <= 0x9f)
    {
        // nop/xchg/cwde/cdq/wait/pushf/popf/sahf/lahf: 单字节
    }
    else if (op >= 0xa0 && op <= 0xa3)
        imm = 4;
    else if (op >= 0xa4 && op <= 0xa7)
    {
    }
    else if (op == 0xa8)
        imm = 1;
    else if (op == 0xa9)
        imm = 4;
    else if (op >= 0xaa && op <= 0xaf)
    {
    }
    else if (op >= 0xb0 && op <= 0xb7)
        imm = 1;
    else if (op >= 0xb8 && op <= 0xbf)
        imm = 4;
    else if (op == 0xc0 || op == 0xc1)
    {
        need_modrm = true;
        imm = 1;
    }
    else if (op == 0xc2 || op == 0xca)
        imm = 2;
    else if (op == 0xc3 || op == 0xc9 || op == 0xcb || op == 0xcc || op == 0xce || op == 0xcf || op == 0xf4 ||
             op == 0xf5 || op == 0xf8 || op == 0xf9 || op == 0xfa || op == 0xfb || op == 0xfc || op == 0xfd ||
             op == 0x90)
    {
    }
    else if (op == 0xc6)
    {
        need_modrm = true;
        imm = 1;
    }
    else if (op == 0xc7)
    {
        need_modrm = true;
        imm = 4;
    }
    else if (op == 0xc8)
        imm = 3;
    else if (op == 0xcd)
        imm = 1;
    else if (op == 0xd0 || op == 0xd1 || op == 0xd2 || op == 0xd3)
        need_modrm = true;
    else if (op >= 0xd8 && op <= 0xdf)
        need_modrm = true;
    else if (op == 0xe0 || op == 0xe1 || op == 0xe2 || op == 0xe3)
        imm = 1;
    else if (op == 0xe4 || op == 0xe6)
        imm = 1;
    else if (op == 0xe5 || op == 0xe7)
        imm = 4;
    else if (op == 0xe8 || op == 0xe9)
        imm = 4;
    else if (op == 0xea)
        imm = 6;
    else if (op == 0xeb)
        imm = 1;
    else if (op == 0xec || op == 0xed || op == 0xee || op == 0xef)
    {
    }
    else if (op == 0xf6)
    {
        // test r/m8, imm8 只在 /0 与 /1 有立即数
        if (i >= avail)
            return 0;
        const unsigned char reg = (p[i] >> 3) & 7;
        need_modrm = true;
        if (reg <= 1)
            imm = 1;
    }
    else if (op == 0xf7)
    {
        if (i >= avail)
            return 0;
        const unsigned char reg = (p[i] >> 3) & 7;
        need_modrm = true;
        if (reg <= 1)
            imm = 4;
    }
    else if (op == 0xfe || op == 0xff)
        need_modrm = true;
    else
        return 0; // 不认识 -> 未知

    if (need_modrm)
        i = ModRMTotal(p, avail, i, &ok);
    if (!ok)
        return 0;
    i += static_cast<size_t>(imm);
    if (i > avail || i == 0 || i > 15)
        return 0;
    return i;
}

bool Process::X86Reaches(const unsigned char *p, size_t avail, size_t target_offset)
{
    if (p == nullptr || target_offset > avail)
        return false;
    size_t i = 0;
    while (i < target_offset)
    {
        const size_t len = X86Length(p + i, avail - i);
        if (len == 0)
            return false;
        i += len;
    }
    return i == target_offset;
}

std::vector<uintptr_t> Process::CollectCallTargets(uintptr_t start, size_t size, size_t max_targets)
{
    std::vector<uintptr_t> targets;
    const std::vector<PatternByte> call_pattern = ParsePattern("E8 ?? ?? ?? ??");
    if (!IsValid() || call_pattern.empty() || size == 0)
        return targets;

    const size_t chunk = 1 * 1024 * 1024;
    const size_t overlap = call_pattern.size() - 1;
    std::vector<unsigned char> buffer(chunk + overlap);

    for (size_t done = 0; done < size && targets.size() < max_targets;)
    {
        const size_t want = (size - done > chunk) ? chunk : (size - done);
        const size_t read_size = want + ((done + want < size) ? overlap : 0);
        if (!ReadBytes(buffer.data(), start + done, read_size))
            break;

        const std::vector<uintptr_t> hits =
            ScanBuffer(call_pattern, buffer.data(), read_size, start + done, max_targets - targets.size());
        for (uintptr_t addr : hits)
        {
            int32_t rel = 0;
            if (!ReadBytes(&rel, addr + 1, sizeof(rel)))
                continue;
            targets.push_back(static_cast<uintptr_t>(static_cast<int64_t>(addr) + 5 + rel));
        }
        done += want;
    }
    return targets;
}

// "候选入口前面看起来像上一个函数的结尾吗" —— 用来在多个"能解码到命中点"的候选里
// 挑出真正的函数开头 (前一个函数一定以 ret / int3 填充结尾)
static bool LooksLikeFunctionEnd(const unsigned char *p, size_t size, size_t idx)
{
    if (idx == 0)
        return true; // 节的开头
    const unsigned char b1 = p[idx - 1];
    if (b1 == 0xc3 || b1 == 0xcb || b1 == 0xcc) // ret / retf / int3
        return true;
    if (idx >= 3 && p[idx - 3] == 0xc2) // ret imm16
        return true;
    if (idx >= 2 && p[idx - 2] == 0xc9 && b1 == 0xc3) // leave; ret
        return true;
    if (idx >= 2 && (p[idx - 2] == 0x5d || p[idx - 2] == 0x5b || p[idx - 2] == 0x5e || p[idx - 2] == 0x5f) &&
        b1 == 0xc3) // pop reg; ret
        return true;
    (void)size;
    return false;
}

uintptr_t Process::FindFunctionEntry(uintptr_t hit, size_t max_back)
{
    // 1) 先试序言/填充启发式
    const uintptr_t by_prologue = FindFunctionStart(hit, max_back);
    if (by_prologue != 0)
        return by_prologue;

    // 2) 用"调用目标 + 解码验证"兜底: 调用目标一定落在函数入口上。
    //    多个候选都能解码走到命中点时, 优先选"前面是上一个函数结尾(ret/int3/填充)"的,
    //    并且取**最早**的那个 —— 那才是函数开头, 而不是函数内部的某个跳转目标。
    if (!IsValid() || hit <= max_back)
        return 0;

    const uintptr_t low = hit - max_back;
    const size_t span = max_back + 16;
    std::vector<unsigned char> code(span);
    if (!ReadBytes(code.data(), low, span))
        return 0;

    const std::vector<uintptr_t> targets = CollectCallTargets(low, span, 20000);
    const size_t off_hit = static_cast<size_t>(hit - low);
    if (off_hit >= code.size())
        return 0;

    uintptr_t best_boundary = 0;
    uintptr_t best_any = 0;
    for (uintptr_t t : targets)
    {
        if (t < low || t > hit)
            continue;
        const size_t off_t = static_cast<size_t>(t - low);
        if (off_t >= off_hit)
            continue;
        if (!X86Reaches(code.data() + off_t, code.size() - off_t, off_hit - off_t))
            continue;
        if (t > best_any)
            best_any = t;
        if (LooksLikeFunctionEnd(code.data(), code.size(), off_t))
        {
            if (best_boundary == 0 || t < best_boundary) // 取最早的那个
                best_boundary = t;
        }
    }
    if (best_boundary != 0)
        return best_boundary;
    return best_any;
}

} // namespace Pt