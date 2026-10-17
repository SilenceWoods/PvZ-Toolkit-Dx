
#pragma once

#include <Windows.h>

#include <array>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace Pt
{

// 输出内存读写数据
#define _PZTK_MEMORY_OUTPUT

class Process
{
  public:
    Process();
    ~Process();

    // 根据窗口类名和标题打开进程
    bool OpenByWindow(const wchar_t *, const wchar_t *);

    // 进程可用性
    bool IsValid();

    // 读内存
    template <typename T>
    T ReadMemory(std::initializer_list<uintptr_t>);

    // 写内存
    template <typename T>
    void WriteMemory(T, std::initializer_list<uintptr_t>);

    // 读内存数组
    template <typename T, size_t size>
    std::array<T, size> ReadMemory(std::initializer_list<uintptr_t>);

    // 写内存数组
    template <typename T, size_t size>
    void WriteMemory(std::array<T, size>, std::initializer_list<uintptr_t>);

    // 批量读内存 (把整块对象数组一次取回, 避免逐字段 ReadProcessMemory)
    bool ReadBytes(void *buffer, uintptr_t addr, size_t size);

    // 批量写内存
    bool WriteBytes(const void *buffer, uintptr_t addr, size_t size);

    // -----------------------------------------------------------------------
    // 2026-10-07 新增: 字节特征扫描 (为"精确控制"打基础, 参考 AvZ 的做法)
    //
    // 有了它就不必给 14 个游戏版本各填一个代码地址: 按特征在目标进程里找地址,
    // 找到再打补丁 (见 PvZ::PatchBytes), 关闭时还原。
    //
    // 特征写法: "8B ?? 38 08 00 00" —— 两个十六进制字符 = 一个字节,
    //           "??" / "?" = 整字节通配, "4?" / "?8" = **半字节通配** (高低四位分开)。
    //           ★ 早先只支持整字节通配, 结果像 "FF 4? 58" 这种半字节特征被判成非法,
    //             直接返回空表 -> 扫描永远"0 处" (用户实测踩到过)。
    // 非法字符串返回空表。
    // -----------------------------------------------------------------------

    // 一个特征字节: value 已按 mask 掩好, mask 0 = 通配, 0xF0/0x0F = 半字节通配
    struct PatternByte
    {
        unsigned char value = 0;
        unsigned char mask = 0;
    };

    // 解析特征字符串
    static std::vector<PatternByte> ParsePattern(const char *text);

    // 在已读入的缓冲里扫描 (纯函数, 可离线测试; base 只用于换算绝对地址)
    static std::vector<uintptr_t> ScanBuffer(const std::vector<PatternByte> &pattern,
                                            const unsigned char *data, size_t size, uintptr_t base,
                                            size_t max_hits);

    // 在目标进程 [start, start + size) 里扫描 (分块读取, 块间重叠, 避免一次读太大)
    std::vector<uintptr_t> ScanPattern(const char *text, uintptr_t start, size_t size,
                                       size_t max_hits = 32);

    // 从函数体内的某个地址往回找函数入口 (常见序言: 55 8B EC / 8B FF 55 8B EC / 55 89 E5),
    // 最多回看 max_back 字节。纯函数版本可离线测试; 返回 0 表示没找到。
    static uintptr_t FindFunctionStartInBuffer(const unsigned char *data, size_t size, size_t offset,
                                               uintptr_t base, size_t max_back);

    // 同上, 但直接从目标进程读 (在 addr 之前 max_back 字节里找)
    uintptr_t FindFunctionStart(uintptr_t addr, size_t max_back = 0x400);

    // -----------------------------------------------------------------------
    // 最小 x86(32 位) 指令长度解码器 —— 用来验证"某个地址确实是一条指令的边界",
    // 从而在**没有标准序言**的发布版代码里也能可靠定位函数入口:
    //   只认编译器常生成的指令; 碰到不认识的返回 0 (调用方按"未知"跳过, 保守处理)。
    // 返回指令长度 (1~15), 0 = 未知/越界。
    // -----------------------------------------------------------------------
    static size_t X86Length(const unsigned char *p, size_t avail);

    // 从 start 开始按指令逐条前进, 判断能否**恰好**落在 target 上 (即 target 是指令边界)
    static bool X86Reaches(const unsigned char *p, size_t avail, size_t target_offset);

    // 收集代码里所有 `E8 rel32` (call) 的目标地址 —— 调用目标一定是函数入口, 是最可靠的锚点
    std::vector<uintptr_t> CollectCallTargets(uintptr_t start, size_t size, size_t max_targets = 40000);

    // 找函数入口: 先用序言/填充启发式, 再用"调用目标 + 解码验证"兜底。
    uintptr_t FindFunctionEntry(uintptr_t hit, size_t max_back = 0x2000);

  protected:
    HWND hwnd;     // 窗口句柄
    DWORD pid;     // 进程标识
    HANDLE handle; // 进程句柄

#if (defined _DEBUG) && (defined _PZTK_MEMORY_OUTPUT)
  private:
    std::string int_to_hex_string(unsigned int num)
    {
        std::stringstream sstream;
        sstream << "0x" << std::hex << num;
        return sstream.str();
    }

    // [[[0x6a9ec0] +0x768] +0x5560]
    std::string addr_list_to_string(std::initializer_list<uintptr_t> addr_list)
    {
        std::string str;
        for (auto it = addr_list.begin(); it != addr_list.end(); it++)
            if (it == addr_list.begin())
                str = "[" + int_to_hex_string(*it) + "]";
            else
                str = "[" + str + " +" + int_to_hex_string(*it) + "]";
        return str;
    }
#endif
};

// 读内存字符串
// 显式特化必须声明在命名空间作用域; MSVC 允许直接写在类里, GCC 不允许
template <>
std::string Process::ReadMemory<std::string>(std::initializer_list<uintptr_t>);

template <typename T>
T Process::ReadMemory(std::initializer_list<uintptr_t> addr)
{
    T result = T();

    if (!IsValid())
        return result;

    uintptr_t offset = 0;
    for (auto it = addr.begin(); it != addr.end(); it++)
    {
        if (it != addr.end() - 1)
        {
            unsigned long read_size = 0;
            int ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &offset, sizeof(offset), &read_size);
            if (ret == 0 || sizeof(offset) != read_size)
                return T();
        }
        else
        {
            unsigned long read_size = 0;
            int ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &result, sizeof(result), &read_size);
            if (ret == 0 || sizeof(result) != read_size)
                return T();
        }
    }

#if (defined _DEBUG) && (defined _PZTK_MEMORY_OUTPUT)
    std::cout << addr_list_to_string(addr) << " --> " << std::dec << result << " / " << std::hex << result << std::endl;
#endif

    return result;
}

template <>
inline std::string Process::ReadMemory<std::string>(std::initializer_list<uintptr_t> addr)
{
    std::string result = std::string();

    if (!IsValid())
        return result;

    uintptr_t offset = 0;
    for (auto it = addr.begin(); it != addr.end(); it++)
    {
        if (it != addr.end() - 1)
        {
            unsigned long read_size = 0;
            int ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &offset, sizeof(offset), &read_size);
            if (ret == 0 || sizeof(offset) != read_size)
                return std::string();
        }
        else
        {
            unsigned long read_size = 0;
            int ret = 0;
            char ch = 0;

        read_letter:
            ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &ch, sizeof(ch), &read_size);
            while (read_size == sizeof(ch) && ret != 0 && ch != 0)
            {
                result += ch;
                offset += sizeof(ch);
                goto read_letter;
            }
        }
    }

#if (defined _DEBUG) && (defined _PZTK_MEMORY_OUTPUT)
    std::cout << addr_list_to_string(addr) << " --> " << result << std::endl;
#endif

    return result;
}

template <typename T>
void Process::WriteMemory(T value, std::initializer_list<uintptr_t> addr)
{
    if (!IsValid())
        return;

    uintptr_t offset = 0;
    for (auto it = addr.begin(); it != addr.end(); it++)
    {
        if (it != addr.end() - 1)
        {
            unsigned long read_size = 0;
            int ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &offset, sizeof(offset), &read_size);
            if (ret == 0 || sizeof(offset) != read_size)
                return;
        }
        else
        {
            unsigned long write_size = 0;
            int ret = WriteProcessMemory(this->handle, (void *)(offset + *it), &value, sizeof(value), &write_size);
            if (ret == 0 || sizeof(value) != write_size)
                return;
        }
    }

#if (defined _DEBUG) && (defined _PZTK_MEMORY_OUTPUT)
    std::cout << addr_list_to_string(addr) << " <-- " << std::dec << value << " / " << std::hex << value << std::endl;
    // if (ReadMemory<T>(addr) != value)
    //     std::wcout << L"写内存出错!" << std::endl;
#endif
}

template <typename T, size_t size>
std::array<T, size> Process::ReadMemory(std::initializer_list<uintptr_t> addr)
{
    std::array<T, size> result = {T()};

    if (!IsValid())
        return result;

    T buff[size] = {0};
    uintptr_t offset = 0;
    for (auto it = addr.begin(); it != addr.end(); it++)
    {
        if (it != addr.end() - 1)
        {
            unsigned long read_size = 0;
            int ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &offset, sizeof(offset), &read_size);
            if (ret == 0 || sizeof(offset) != read_size)
                return std::array<T, size>{T()};
        }
        else
        {
            unsigned long read_size = 0;
            int ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &buff, sizeof(buff), &read_size);
            if (ret == 0 || sizeof(buff) != read_size)
                return std::array<T, size>{T()};
        }
    }
    for (size_t i = 0; i < size; i++)
        result[i] = buff[i];

#if (defined _DEBUG) && (defined _PZTK_MEMORY_OUTPUT)
    std::cout << addr_list_to_string(addr) << " --> ";
    for (size_t i = 0; i < size; i++)
        std::cout << std::hex << int(result[i]) << " ";
    std::cout << std::endl;
#endif

    return result;
}

template <typename T, size_t size>
void Process::WriteMemory(std::array<T, size> value, std::initializer_list<uintptr_t> addr)
{
    if (!IsValid())
        return;

    T buff[size] = {0};
    for (size_t i = 0; i < size; i++)
        buff[i] = value[i];
    uintptr_t offset = 0;
    for (auto it = addr.begin(); it != addr.end(); it++)
    {
        if (it != addr.end() - 1)
        {
            unsigned long read_size = 0;
            int ret = ReadProcessMemory(this->handle, (const void *)(offset + *it), &offset, sizeof(offset), &read_size);
            if (ret == 0 || sizeof(offset) != read_size)
                return;
        }
        else
        {
            unsigned long write_size = 0;
            int ret = WriteProcessMemory(this->handle, (void *)(offset + *it), &buff, sizeof(buff), &write_size);
            if (ret == 0 || sizeof(buff) != write_size)
                return;
        }
    }

#if (defined _DEBUG) && (defined _PZTK_MEMORY_OUTPUT)
    std::cout << addr_list_to_string(addr) << " <-- ";
    for (size_t i = 0; i < size; i++)
        std::cout << std::hex << int(value[i]) << " ";
    std::cout << std::endl;
    // if (ReadMemory<T, size>(addr) != value)
    //     std::wcout << L"写内存出错!" << std::endl;
#endif
}

} // namespace Pt
