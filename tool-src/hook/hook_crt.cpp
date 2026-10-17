// ---------------------------------------------------------------------------
// 注入 DLL 的"最小 CRT"
//
// 为什么需要: 本工具链 (临时 MinGW) 只带了 exe 用的 crt2.o, **没有 dllcrt2.o**,
// 所以 -shared 链接会报 "cannot find dllcrt2.o"。
// 而我们这个 DLL 的全部依赖只有: kernel32 的几个 API + memcpy/memset
// (MinHook 的内存是自己用 VirtualAlloc 管的, 不碰 malloc)。
// 于是干脆不链 CRT: 自己提供入口点与这两个函数。
// ---------------------------------------------------------------------------
#include <Windows.h>
#include <cstddef>

extern "C" int __stdcall DllMain(HINSTANCE, DWORD, LPVOID);

// 链接器入口 (-Wl,-e,_DllMainCRTStartup@12)
extern "C" BOOL __stdcall _DllMainCRTStartup(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    return DllMain(hinst, reason, reserved);
}

extern "C" void *memcpy(void *dst, const void *src, size_t size)
{
    unsigned char *d = static_cast<unsigned char *>(dst);
    const unsigned char *s = static_cast<const unsigned char *>(src);
    while (size-- != 0)
        *d++ = *s++;
    return dst;
}

extern "C" void *memset(void *dst, int value, size_t size)
{
    unsigned char *d = static_cast<unsigned char *>(dst);
    while (size-- != 0)
        *d++ = static_cast<unsigned char>(value);
    return dst;
}

// 某些优化路径会引用它; 定义成 0 表示"没有浮点运算"
extern "C" int _fltused = 0;
