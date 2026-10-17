// ---------------------------------------------------------------------------
// pztk_hook.dll —— 注入到游戏进程里的小 DLL (方案 B)
//
// 为什么需要它: 我们的工具是**外部进程**, 只能 ~60 Hz 写内存, 而游戏是 100 Hz;
// "必须在某一帧内落值"的功能 (典型是首发延迟) 因此会概率性失效。
// AvZ 的做法是注入 DLL、在游戏自己的帧里执行 —— 这里走同一条路线, 但保持外部工具
// 掌控一切: 地址/开关/参数都通过共享内存 (hook_ctl.h) 由工具下发。
//
// 第一步只做**只读观测**: 在 Plant::UpdateShooter 上挂一个钩子, 记录调用次数、
// 看到的 mLaunchCounter / mSeedType、植物指针, 写回控制块给工具显示 ——
// 用来确认"钩子确实挂上且每帧在被调用", 不改变任何游戏行为。
// ---------------------------------------------------------------------------
#include <Windows.h>
#include <tlhelp32.h> // CreateToolhelp32Snapshot / Thread32First (写热代码前挂起其它线程)
#include <string.h>   // memcpy (自己的极简 CRT 提供实现, 见 hook_crt.cpp)

#include "hook_ctl.h"
// ★ 卡片冷却跳板的**唯一**一份汇编码 (出厂 DLL 与离线自测共用, 见 cd_thunk.h 的栈算术)
#include "cd_thunk.h"
#include "minhook/MinHook.h"

using PtHook::Control;

static HANDLE g_channel = nullptr;
static Control *g_ctl = nullptr;
static void *g_target = nullptr;
static volatile LONG g_unloading = 0;
// ---------------------------------------------------------------------------
// 方案 B: 帧级首发延迟 —— 连"本排有没有僵尸"这个决策也在游戏帧内做
//
//   为什么需要: 只在帧内"落值"(方案 A) 还是要靠工具 60 Hz 判断"僵尸进射程",
//   那一步最多晚 ~1.7 游戏帧。把判断也搬进来, 首发弹的时机就完全由游戏帧决定。
//
//   实现: 维护一张"哪些排有僵尸"的位图 (32 位), 用 QPC 限流每 ~4ms 重建一次
//   (僵尸进出某一排是**慢变化**, 4ms 的陈旧度远小于一帧的判定误差; 而且不用每棵植物
//    都去扫一遍僵尸数组, 那样内存访问量会爆炸)。
// ---------------------------------------------------------------------------
static uint32_t g_row_mask = 0;      // 本帧的"哪些排有僵尸"
static uint32_t g_row_mask_prev = 0; // 上一帧的, 用来识别"僵尸刚进这排"
static int32_t g_row_x_lo[32];       // 每排僵尸碰撞箱左边缘最小值 (用于攻击窗口判定)
static int32_t g_row_x_hi[32];       // 每排僵尸碰撞箱右边缘最大值
static uint32_t g_row_mask_tick = 0;
static uint32_t g_qpc_period = 0; // ~4ms 对应的 QPC tick 数
static HMODULE g_module = nullptr;
static int32_t g_last_slot = -1;  // 上一次钩到的植物槽位 (回绕 = 新的一帧)
// 每棵植物"上一帧/本帧是否被钉住" (按槽位) —— 只在**状态跳变**时记日志, 否则每帧每棵都记, 量太大
static unsigned char g_pinned_prev[256] = {0};
static unsigned char g_pinned_cur[256] = {0};
static uint32_t g_frame_id = 0;      // 游戏自己的帧号 (0 = 还没读到)
static int32_t g_last_game_frame = -1;

// ★★ v38: 心跳超时**取消了** ★★
//   v37 这里有一个 `kHeartbeatTimeoutTicks` (= 工具心跳 2 秒没变就卸载), 它的初值是 0、
//   只在 `MaybeRefreshRowMask()` 里才被改成 2 秒 —— 而那个函数只有"开了帧级首发延迟且
//   命中了设定植物"时才会跑 ⇒ 多数会话里它就是 **0 tick**: 监视线程每 20ms 轮询一次,
//   只要**任何一次**没看到心跳变化, 就立刻卸钩子 + FreeLibraryAndExitThread。
//   而**拖动修改器主窗口**时 Windows 的模态移动循环跑在 DefWindowProc 里
//   (= 在我们 `glfwPollEvents()` 的 DispatchMessage 内部), 工具主循环整段停住 ⇒ 心跳必停
//   ⇒ 必然卸载 —— 这就是用户报的"一拖就中断 / 偶尔会中断"。
//   现在卸载判据只有两条 (见共享头 `CdUnloadDecision`): 工具明确要求 (`want_unload`),
//   或者工具**进程**真的没了 (`tool_pid` 的进程句柄被 signal)。心跳字段只作诊断。
static uint32_t QpcTick(); // 前置声明 (EmitLog 要用)

static uint32_t QpcTick()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<uint32_t>(now.QuadPart);
}

// 记一条事件日志 (只在跳变时调用, 所以量很小)
//   判定输入 (列/px/窗口/僵尸 x 范围) 由调用方先写进 g_ev_*, 这里原样带上 —— 出问题时
//   一条日志就能看出是"列错了""窗口错了"还是"僵尸 x 错了"。
static int32_t g_ev_col = 0, g_ev_px = 0, g_ev_xlo = 0, g_ev_xhi = 0, g_ev_wlo = 0, g_ev_whi = 0;
static uint32_t g_ev_win_ok = 0;
static unsigned char g_enemy_prev[256] = {0};
static unsigned char g_enemy_cur[256] = {0};

static void EmitLog(Control *ctl, uint32_t event, int32_t slot, int32_t row, int32_t seed, int32_t counter,
                    int32_t target)
{
    if (ctl->log_enable == 0 || slot < 0 || slot >= 256)
        return;
    const uint32_t w = ctl->log_write;
    PtHook::LogEntry &e = ctl->log_ring[w % PtHook::kLogRingSize];
    e.tick = QpcTick();
    e.event = event;
    e.slot = slot;
    e.row = row;
    e.seed = seed;
    e.counter = counter;
    e.target = target;
    e.mask = g_row_mask;
    e.frame = g_frame_id;
    e.col = g_ev_col;
    e.px = g_ev_px;
    e.x_lo = g_ev_xlo;
    e.x_hi = g_ev_xhi;
    e.win_lo = g_ev_wlo;
    e.win_hi = g_ev_whi;
    e.win_ok = g_ev_win_ok;
    ctl->log_write = w + 1; // 最后才递增, 工具读不到半截条目
}

// 读游戏自己的帧号 (mAppCounter)。返回 0 = 还没读到 (地址未知/不可读),
// 这时退回"植物槽位回绕"的老办法 (可能漏帧, 仅兜底)。
static uint32_t ReadGameFrame(Control *ctl)
{
    if (ctl->app_counter_addr == 0)
        return 0;
    if (IsBadReadPtr(reinterpret_cast<const void *>(static_cast<uintptr_t>(ctl->app_counter_addr)), 4))
        return 0;
    return static_cast<uint32_t>(
        *reinterpret_cast<volatile const int32_t *>(static_cast<uintptr_t>(ctl->app_counter_addr)));
}

// 扫一遍僵尸数组: 返回"哪些排有僵尸"的位图, 同时给出每排僵尸的 x 范围 (用于攻击窗口判定)
static uint32_t ScanRowMask(Control *ctl, int32_t *row_x_lo, int32_t *row_x_hi)
{
    const uint32_t base = ctl->z_base, size = ctl->z_size, count = ctl->z_count;
    const uint32_t off_dead = ctl->z_off_dead, off_row = ctl->z_off_row, off_status = ctl->z_off_status;
    const uint32_t off_x = ctl->z_off_x;
    const uint32_t off_type = ctl->z_off_type;
    // 几何不合法时记下具体哪一项, 状态行直接显示 (省得瞎猜)
    uint32_t bad = 0;
    if (base == 0)
        bad = 1;
    else if (size < 0x100 || size > 0x1000)
        bad = 2;
    else if (count == 0 || count > 4096)
        bad = 3;
    else if (off_row >= size)
        bad = 4;
    if (bad != 0)
    {
        ctl->z_geom_bad = bad;
        ctl->z_scan = 1;
        return g_row_mask;
    }
    // ★ 只扫"确实可读"的那一段, 而且**绝不整段放弃**:
    //   早先用 IsBadReadPtr 检查整个数组, 只要有未提交的页就整段放弃 -> 一个僵尸都看不到
    //   -> 所有植物被判"射程内没僵尸" -> 全部钉死不打 (用户实测就是这个现象)。
    //   现在用 VirtualQuery 逐段确认可读范围, 只扫可读前缀。
    uint32_t limit = count;
    {
        const uintptr_t start = static_cast<uintptr_t>(base);
        const uintptr_t end = start + static_cast<uintptr_t>(size) * count;
        uintptr_t addr = start;
        uint32_t usable = 0;
        while (addr < end)
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0)
                break;
            if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
                break;
            const uintptr_t chunk_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            const uintptr_t stop = (chunk_end < end) ? chunk_end : end;
            usable = static_cast<uint32_t>((stop - start) / size);
            addr = stop;
            if (stop >= end)
                break;
        }
        if (usable == 0)
        {
            ctl->z_scan = 2;
            return g_row_mask;
        }
        if (usable < count)
            limit = usable;
    }
    ctl->z_scan = 0;
    ctl->z_geom_bad = 0;

    uint32_t mask = 0;
    uint32_t live = 0;
    for (uint32_t i = 0; i < limit; i++)
    {
        const char *z = reinterpret_cast<const char *>(static_cast<uintptr_t>(base) + size * i);
        // ★ 与工具核心的判定保持一致: "消失" 之外, **死亡/烧焦/被压过** 的阶段也不算占排。
        //   早先只判 z_off_dead, 于是正在播放死亡动画的僵尸仍算占排 -> 植物钉住很慢 (用户实测)。
        if (*reinterpret_cast<volatile const unsigned char *>(z + off_dead) != 0)
            continue;
        if (off_status != 0 && off_status < size)
        {
            const int32_t phase = *reinterpret_cast<volatile const int32_t *>(z + off_status);
            if (phase == 1 || phase == 2 || phase == 3)
                continue;
        }
        const int32_t row = *reinterpret_cast<volatile const int32_t *>(z + off_row);
        if (row >= 0 && row < 32)
        {
            live++;
            mask |= (1u << row);
            // ★ 僵尸 mPosX 是 **float** (核心就是 ReadMemory<float> 读的) —— 早先按 int32 读,
            //   得到 1140129752 这种天文数字 -> 与攻击窗口永不重叠 -> 全部植物被钉死 (踩过)。
            const float zxf = (off_x != 0 && off_x < size)
                                  ? *reinterpret_cast<volatile const float *>(z + off_x)
                                  : -1.0f;
            // 与核心一致: 还没走进草坪的僵尸 (x 不在 -1000..800) 先不算
            if (zxf > -1000.0f && zxf < 800.0f)
            {
                // ★ 碰撞箱按**僵尸种类**算 (与核心 ZombieBodyX 完全一致):
                //   默认 [x+36, x+78] —— 僵尸身体在 mPosX 右侧。早先用 ±40 近似会整体偏左 ~40px,
                //   对忧郁菇 (240 宽窗口) 这种窄射程植物误差致命。
                int32_t rx = 36, rw = 42;
                if (off_type != 0 && off_type < size)
                {
                    const int32_t t = *reinterpret_cast<volatile const int32_t *>(z + off_type);
                    if (t >= 0 && t < 64 && ctl->z_body_rw[t] != 0)
                    {
                        rx = ctl->z_body_rx[t];
                        rw = ctl->z_body_rw[t];
                    }
                }
                const int32_t xl = static_cast<int32_t>(zxf) + rx;
                const int32_t xh = xl + rw;
                if (xl < row_x_lo[row])
                    row_x_lo[row] = xl;
                if (xh > row_x_hi[row])
                    row_x_hi[row] = xh;
            }
            // x 不合理就不写 (保持"未扫到"状态) -> 判定会自动退回"只看本排", 绝不钉死植物
        }
    }
    ctl->z_live = live;
    return mask;
}

// 每帧刷新一次位图 —— 关键点: **同一帧内所有植物看到的是同一份位图**,
// 这样"僵尸刚进这排"的瞬间会被同排所有植物在同一帧看到, 于是它们能在同一帧一起上膛
// (否则有的植物当帧看到"空"、有的看到"有", 就会错开, 发射不整齐)。
// 帧边界靠"植物槽位回绕"识别 (游戏按槽位顺序遍历植物); 另加 4ms 最小间隔兜底。
static void MaybeRefreshRowMask(Control *ctl, const void *self)
{
    LARGE_INTEGER freq;
    if (g_qpc_period == 0)
    {
        QueryPerformanceFrequency(&freq);
        const uint32_t f = static_cast<uint32_t>(freq.QuadPart);
        g_qpc_period = (f / 250 != 0) ? (f / 250) : 1; // 1/250 秒
        // ★ v38: 这里原来还给心跳超时赋值 (f*2) —— 已删. 卸载判据与心跳无关了, 见 QpcTick 上面那段.
    }

    int32_t slot = -1;
    if (ctl->plant_base != 0 && ctl->plant_size >= 0x100)
    {
        const uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(self));
        if (p >= ctl->plant_base && ((p - ctl->plant_base) % ctl->plant_size) == 0)
            slot = static_cast<int32_t>((p - ctl->plant_base) / ctl->plant_size);
    }

    const uint32_t tick = QpcTick();

    // ★ 帧边界: 优先用**游戏自己的帧号** (mAppCounter)。读不到才退回"槽位回绕"兜底。
    const uint32_t gframe = ReadGameFrame(ctl);
    bool frame_changed = false;
    bool via_heuristic = false;
    if (gframe != 0)
    {
        g_frame_id = gframe;
        if (g_last_game_frame < 0 || static_cast<int32_t>(gframe) != g_last_game_frame)
            frame_changed = true;
    }
    else if (slot >= 0 && g_last_slot >= 0 && slot <= g_last_slot)
    {
        frame_changed = true;
        via_heuristic = true;
    }

    // ★★ 帧一变就必须锁存。曾经的写法是先把 g_last_game_frame 更新掉、再用 4ms 门节流 ——
    //    门一挡, 这次帧变化就被"吃掉"了, 锁存永久丢失: g_row_mask_prev / g_pinned_prev 滞后一帧,
    //    于是可能漏掉"僵尸刚进排"那一帧 -> 同排植物不再对齐, 日志里表现为"连着两帧各钉一轮"。
    //    所以: 4ms 门只留给兜底路径 (槽位回绕可能一帧内触发多次), 帧号路径绝不节流。
    if (frame_changed &&
        (!via_heuristic || static_cast<uint32_t>(tick - g_row_mask_tick) >= g_qpc_period))
    {
        if (gframe != 0)
            g_last_game_frame = static_cast<int32_t>(gframe);
        g_row_mask_prev = g_row_mask;
        for (int r = 0; r < 32; r++)
        {
            g_row_x_lo[r] = 0x7fffffff;
            g_row_x_hi[r] = -0x7fffffff;
        }
        g_row_mask = ScanRowMask(ctl, g_row_x_lo, g_row_x_hi);
        g_row_mask_tick = tick;
        // 新的一帧: 把"本帧钉住状态"快照成"上帧状态", 供跳变检测 (日志 + 只写一次)
        for (int i = 0; i < 256; i++)
        {
            g_pinned_prev[i] = g_pinned_cur[i];
            g_enemy_prev[i] = g_enemy_cur[i];
        }
    }
    if (slot >= 0)
        g_last_slot = slot;
}


// 裸汇编跳板用的 C 助手 (见文件末尾的 HookThunk)
extern "C" void HookObserveAndPin(void *self);
//
// ★ 为什么必须用裸汇编跳板, 而不是直接把 C 函数当钩子:
//   1) MinHook 会把钩子点前面的指令搬到"蹦床"里执行, 再跳回原函数继续。
//      如果钩子点是**函数入口**, 那里紧接着就是序言 (push esi/edi/ebx 保存**调用者的**寄存器),
//      而 C 钩子在调用蹦床前已经用过这些寄存器 -> 序言保存到的是垃圾, 函数返回时把调用者的
//      ESI/EDI 覆盖成垃圾 -> 调用方随后访问就崩 (实测 EIP=入口+9 的 Access Violation)。
//      挂在**函数内部**的命中点就不会: 那时序言早已保存完毕。
//   2) 命中点处的植物指针放在 **ESI** (指令是 `cmp [esi+58h],50`), 不是 thiscall 的 ECX。
//      所以跳板里从 ESI 取指针, 传给这个 C 函数。
// ---------------------------------------------------------------------------
extern "C" void *g_hook_trampoline = nullptr; // MinHook 给的"原指令"入口 (蹦床)

extern "C" void HookObserveAndPin(void *self)
{
    Control *ctl = g_ctl;
    if (ctl == nullptr || self == nullptr || g_unloading != 0)
        return;

    // ★★ 强校验: self 必须落在植物数组里 (基址 + 对齐), 否则一律不碰。
    bool is_plant = false;
    const uint32_t size = ctl->plant_size;
    const uint32_t base = ctl->plant_base;
    const uint32_t count = ctl->plant_count;
    if (base != 0 && size >= 0x100 && count != 0 && count < 4096)
    {
        const uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(self));
        if (p >= base && p < base + size * count && ((p - base) % size) == 0)
            is_plant = true;
    }
    if (!is_plant)
    {
        ctl->bad_self++;
        return;
    }

    char *plant = reinterpret_cast<char *>(self);
    const int32_t counter =
        *reinterpret_cast<volatile const int32_t *>(plant + PtHook::kLaunchCounterOffset);
    const int32_t seed = *reinterpret_cast<volatile const int32_t *>(plant + PtHook::kSeedTypeOffset);

    ctl->hook_calls++;
    ctl->dll_alive++;
    ctl->last_plant = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(self));
    ctl->last_counter = counter;
    ctl->last_seed = seed;

    const int32_t prev = ctl->prev_counter;
    if (prev >= 0 && counter > prev + 2)
        ctl->jump_count++;
    ctl->prev_counter = counter;
    // 注意: min_counter **只在钉住期间**更新 (见下面两个钉值分支)。
    // 早先放在这里全局统计, 结果是"放开后计数器正常走到 0"也会把它拉低,
    // 指标就没意义了 (用户实测时被误导过)。

    // 钉值: 每帧把计数器写回目标值 (钩子点在递减之后, 等于"计数器永不前进")
    if (ctl->pin_mode == 1 && ctl->pin_enable != 0 &&
        (ctl->pin_seed == PtHook::kPinAllSeeds || ctl->pin_seed == seed))
    {
        const int32_t want = ctl->pin_value;
        if (want >= 0)
        {
            if (ctl->min_counter < 0 || counter < ctl->min_counter)
                ctl->min_counter = counter; // 钉住期间的最小值 (钉得牢就停在 want-1 附近)
            if (counter != want)
            {
                *reinterpret_cast<volatile int32_t *>(plant + PtHook::kLaunchCounterOffset) = want;
                ctl->pinned_count++;
            }
        }
    }

    // ---- 方案 B: 帧级首发延迟 (决策也在帧内) ----
    //   ① 本排为空            -> 钉住计数器 (不开火, 等僵尸);
    //   ② 僵尸**刚进**这一排  -> 把计数器对齐到设定值 (同排所有植物都在同一帧看到这个瞬间
    //                            -> 一起上膛、一起倒计时 -> 整齐发射);
    //   ③ 这一排一直被占着    -> 不干预, 让它按自己的节奏打;
    //   ④ **有子弹待出膛时 (mShootingCounter != 0) 一律不写** —— 老版源码里这条是硬要求:
    //      FindTargetAndFire 只是设了 mShootingCounter, 子弹要等它减到 0 才真正 Fire();
    //      动画期间改写 mLaunchCounter, 那颗子弹照旧出来, 新周期的检查点又排一颗
    //      -> 两发挤在一起 ("子弹错步", 实测 2~4 帧) 或直接吞弹。
    if (ctl->windup_hook != 0 && ctl->p_off_seed < 0x1000 && ctl->p_off_row < 0x1000)
    {
        const int32_t seed_here = *reinterpret_cast<volatile const int32_t *>(plant + ctl->p_off_seed);
        if (seed_here >= 0 && seed_here < 48)
        {
            const int32_t want = ctl->pin_by_seed[seed_here];
            if (want >= 0)
            {
                const int32_t row = *reinterpret_cast<volatile const int32_t *>(plant + ctl->p_off_row);
                if (row >= 0 && row < 32)
                {
                    MaybeRefreshRowMask(ctl, self);
                    ctl->zombie_rows = g_row_mask;
                    // ★ 索敌范围按植物种类: 三线射手打三排、忧郁菇 3x3(±1 行)、香蒲全场制导,
                    //   其余只看本排。工具按种子下发 pin_row_dev (0 / 1 / 7)。
                    //   范围算错了就会提前或延后脱手 (用户实测: 这三棵需要各自的索敌)。
                    // ★ 索敌范围完全照游戏的 FindTargetZombie / 核心的 PlantHasTarget:
                    //   行: 三线射手与忧郁菇 ±1 行, 香蒲任意行, 其余本排;
                    //   窗口: 用该植物的攻击矩形 (相对 mX) 与僵尸碰撞箱求重叠 —— 这一条对
                    //   忧郁菇/大喷菇这类**短射程**植物是必需的: 只看行会把"上下排很远的僵尸"
                    //   误判成进射程, 于是提前脱手, 等真僵尸靠近时已经没有延迟了 (用户实测)。
                    const int32_t dev = (ctl->pin_row_dev[seed_here] > 0) ? ctl->pin_row_dev[seed_here] : 0;
                    const int32_t col = (ctl->p_off_col != 0 && ctl->p_off_col < ctl->plant_size)
                                            ? *reinterpret_cast<volatile const int32_t *>(plant +
                                                                                           ctl->p_off_col)
                                            : -1;
                    const int32_t px = col * 80 + 40; // 与核心 PlantHasTarget 的公式一致
                    const int32_t win_lo = px + ctl->pin_win_lo[seed_here];
                    const int32_t win_hi = px + ctl->pin_win_hi[seed_here];

                    // ★★ 失败即退回: 只有"窗口数据齐全"时才用窗口判定, 否则一律退回"只看本排"。
                    //    上一版就是因为窗口数据缺失时算出空窗口 -> 判成"没僵尸" -> 所有植物被钉死。
                    //    宁可少一点精度, 也绝不能把植物钉住不打。
                    const bool win_ok = (col >= 0) &&
                                        (ctl->pin_win_hi[seed_here] > ctl->pin_win_lo[seed_here]) &&
                                        (ctl->z_off_x != 0 && ctl->z_off_x < ctl->z_size);

                    bool occupied = false;
                    bool occupied_prev = false;
                    for (int32_t r = row - dev; r <= row + dev; r++)
                    {
                        if (r < 0 || r >= 32)
                            continue;
                        // 这一帧某个排有僵尸: 若 x 范围没扫到 (扫描提前返回/偏移缺失), 当作"在射程内"
                        const bool has_now = (g_row_mask & (1u << r)) != 0;
                        const bool has_prev = (g_row_mask_prev & (1u << r)) != 0;
                        const bool x_unknown = (g_row_x_lo[r] > g_row_x_hi[r]);
                        if (has_now && (!win_ok || x_unknown ||
                                        (g_row_x_lo[r] < win_hi && g_row_x_hi[r] > win_lo)))
                            occupied = true;
                        if (has_prev && (!win_ok || x_unknown ||
                                         (g_row_x_lo[r] < win_hi && g_row_x_hi[r] > win_lo)))
                            occupied_prev = true;
                    }
                    // 身后窗口 (目前只有裂荚射手有): [0, px+16], 只看本行
                    const int32_t back_hi = ctl->pin_back_hi[seed_here];
                    if (back_hi > 0 && row >= 0 && row < 32)
                    {
                        const int32_t back_right = px + back_hi;
                        const bool x_unknown_row = (g_row_x_lo[row] > g_row_x_hi[row]);
                        if ((g_row_mask & (1u << row)) &&
                            (!win_ok || x_unknown_row ||
                             (g_row_x_lo[row] < back_right && g_row_x_hi[row] > 0)))
                            occupied = true;
                        if ((g_row_mask_prev & (1u << row)) &&
                            (!win_ok || x_unknown_row ||
                             (g_row_x_lo[row] < back_right && g_row_x_hi[row] > 0)))
                            occupied_prev = true;
                    }
                    // 诊断: 把这一帧的判定依据留给状态行 (用户一看就知道是不是窗口算错了)
                    ctl->win_probe_col = col;
                    ctl->win_probe_px = px;
                    ctl->win_probe_lo = win_lo;
                    ctl->win_probe_hi = win_hi;
                    ctl->win_probe_xlo = (row >= 0 && row < 32) ? g_row_x_lo[row] : 0;
                    ctl->win_probe_xhi = (row >= 0 && row < 32) ? g_row_x_hi[row] : 0;
                    ctl->win_probe_ok = win_ok ? 1 : 0;

                    // 这一帧日志要带的判定输入
                    g_ev_col = col;
                    g_ev_px = px;
                    g_ev_wlo = win_lo;
                    g_ev_whi = win_hi;
                    g_ev_win_ok = win_ok ? 1 : 0;
                    g_ev_xlo = (row >= 0 && row < 32) ? g_row_x_lo[row] : 0;
                    g_ev_xhi = (row >= 0 && row < 32) ? g_row_x_hi[row] : 0;

                    // ★ 铁证检测: 这一排**确实有僵尸**(只看行), 但我们仍在钉住 -> 判定出错了
                    const bool pin_now = (!occupied || !occupied_prev);
                    const int32_t slot = (ctl->plant_size >= 0x100)
                                             ? static_cast<int32_t>((static_cast<uint32_t>(
                                                        reinterpret_cast<uintptr_t>(self)) -
                                                                    ctl->plant_base) /
                                                                   ctl->plant_size)
                                             : -1;
                    bool row_has_enemy = false;
                    for (int32_t r = row - dev; r <= row + dev; r++)
                        if (r >= 0 && r < 32 && (g_row_mask & (1u << r)))
                            row_has_enemy = true;

                    // ---- 事件日志: 只记跳变 (僵尸进排 / 放手 / 钉住), 带植物编号 ----
                    if (occupied && !occupied_prev)
                        EmitLog(ctl, PtHook::LOG_ENTER, slot, row, seed_here, counter, want);
                    if (slot >= 0 && slot < 256)
                    {
                        const bool was_pinned = (g_pinned_prev[slot] != 0);
                        if (pin_now && !was_pinned)
                            EmitLog(ctl, PtHook::LOG_PIN, slot, row, seed_here, counter, want);
                        else if (!pin_now && was_pinned)
                            EmitLog(ctl, PtHook::LOG_RELEASE, slot, row, seed_here, counter, want);
                        g_pinned_cur[slot] = pin_now ? 1 : 0;

                        // 铁证: 本排有僵尸却还被钉住 —— 只在跳变时记一条, 免得刷屏
                        const bool pe = pin_now && row_has_enemy;
                        if (pe)
                            ctl->pinned_enemy++;
                        if (pe && g_enemy_prev[slot] == 0)
                            EmitLog(ctl, PtHook::LOG_PIN_ENEMY, slot, row, seed_here, counter, want);
                        g_enemy_cur[slot] = pe ? 1 : 0;
                    }

                    if (pin_now)
                    {
                        // 安全条件: 有子弹待出膛时不写 (见上面第 ④ 条)
                        const bool shooting_busy =
                            (ctl->p_off_shooting != 0 && ctl->p_off_shooting < ctl->plant_size &&
                             *reinterpret_cast<volatile const int32_t *>(plant + ctl->p_off_shooting) != 0);
                        if (ctl->min_counter < 0 || counter < ctl->min_counter)
                            ctl->min_counter = counter;
                        if (!shooting_busy)
                        {
                            ctl->pinned_last = counter; // 只在"这一帧我们动手了"时更新
                            ctl->pin_target = want;
                            if (counter != want)
                            {
                                *reinterpret_cast<volatile int32_t *>(plant +
                                                                       PtHook::kLaunchCounterOffset) = want;
                                ctl->pinned_count++;
                            }
                        }
                    }
                }
            }
        }
    }
}

// 裸汇编跳板: 保存全部寄存器 -> 从 ESI 取植物指针调 C 函数 -> 恢复全部寄存器 ->
// 跳到蹦床执行被搬走的原指令 (再自动跳回原函数)。
// ★ 注意: i386 Windows 的 C 符号有前导下划线, 汇编里必须写 _HookObserveAndPin /
//   _g_hook_trampoline, 否则链接报 undefined reference (踩过)。
__attribute__((naked)) static void HookThunk()
{
    __asm__ __volatile__(
        "pushal\n\t"
        "pushl %esi\n\t"                    // 命中点: 植物指针在 ESI
        "call _HookObserveAndPin\n\t"
        "addl $4, %esp\n\t"
        "popal\n\t"
        "jmp *_g_hook_trampoline\n\t");
}

static void RemoveHook()
{
    if (g_target == nullptr)
        return;
    MH_DisableHook(g_target);
    MH_RemoveHook(g_target);
    g_target = nullptr;
    g_hook_trampoline = nullptr;
    if (g_ctl != nullptr)
        g_ctl->hook_ok = 0;
}

// ---------------------------------------------------------------------------
// 游戏绘制 API 的调用 (这套二进制不按 MSVC 约定, 全是反汇编核对的):
//   Graphics::SetColor(const Color&):  ECX = Graphics*, EAX = &Color
//   Graphics::DrawRect(const Rect&) / Graphics::FillRect(const Rect&):
//                                      EAX = &Rect, Graphics* 压栈, 自己 ret 4
//   ★★ v40: 这两个小函数原来和"碰撞箱"那一段写在一起。碰撞箱整条链删掉之后,
//     它们是**冷却数字的矩形路径**在用的公共设施 (`GfxRectInk` / `GfxFillRectRaw`
//     都直接调它们) ⇒ **原样保留, 一个字节都没改**。
// ---------------------------------------------------------------------------
static void GfxSetColor(void *g, const uint32_t *color, uint32_t fn)
{
    __asm__ __volatile__("movl %2, %%ecx\n\t"
                         "movl %1, %%eax\n\t"
                         "call *%0\n\t" ::"r"(fn),
                         "r"(color), "r"(g)
                         : "eax", "ecx", "edx", "memory");
}

static void GfxDrawRect(void *g, const int32_t *rc, uint32_t fn)
{
    __asm__ __volatile__("pushl %2\n\t"
                         "movl %1, %%eax\n\t"
                         "call *%0\n\t" ::"r"(fn),
                         "r"(rc), "r"(g)
                         : "eax", "ecx", "edx", "memory");
}

// ---------------------------------------------------------------------------
// 在"其它线程已挂起"的保护下改写游戏代码 (打补丁与还原都走这里)
//
//   ★★ 钩点是"每帧都会执行"的**热点**(渲染循环里的那条 call):
//     直接写 5 个字节, 有极小概率正好撞上渲染线程在取这条指令 ⇒ 跳到垃圾地址, 游戏当场崩。
//     MinHook 装钩子时同样会挂起其它线程, 这里照做: 写之前把所有**别的**线程挂起, 写完再恢复。
//     (自己这个线程不挂 —— 我们只写内存、不去抢游戏的锁, 所以不会死锁。)
//   ★★ v40: 这一组原来是"碰撞箱绘制补丁"与"冷却数字补丁"共用。碰撞箱删掉后只剩冷却数字在用
//     ⇒ **原样保留, 一个字节都没改** (它是"写热点前先挂起线程"这条硬规矩的唯一落点)。
// ---------------------------------------------------------------------------
static int SuspendOtherThreads(HANDLE *handles, int max_handles)
{
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    const DWORD self_pid = GetCurrentProcessId();
    const DWORD self_tid = GetCurrentThreadId();
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    int n = 0;
    if (Thread32First(snap, &te) != 0)
    {
        do
        {
            if (te.th32OwnerProcessID != self_pid || te.th32ThreadID == self_tid)
                continue;
            if (n >= max_handles)
                break;
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
            if (h == nullptr)
                continue;
            if (SuspendThread(h) != static_cast<DWORD>(-1))
                handles[n++] = h;
            else
                CloseHandle(h);
        } while (Thread32Next(snap, &te) != 0);
    }
    CloseHandle(snap);
    return n;
}

static void ResumeOtherThreads(HANDLE *handles, int n)
{
    for (int i = 0; i < n; i++)
    {
        ResumeThread(handles[i]);
        CloseHandle(handles[i]);
    }
}

static bool WriteCodeSafe(uint32_t addr, const unsigned char *bytes, int len){
    HANDLE handles[256];
    const int n = SuspendOtherThreads(handles, 256);
    DWORD old_protect = 0;
    bool ok = false;
    if (VirtualProtect(reinterpret_cast<void *>(addr), static_cast<SIZE_T>(len),
                       PAGE_EXECUTE_READWRITE, &old_protect) != 0)
    {
        memcpy(reinterpret_cast<void *>(addr), bytes, static_cast<size_t>(len));
        VirtualProtect(reinterpret_cast<void *>(addr), static_cast<SIZE_T>(len), old_protect,
                       &old_protect);
        ok = true;
    }
    ResumeOtherThreads(handles, n);
    return ok;
}

// ---------------------------------------------------------------------------
// 卡片冷却时间显示 (2026-10-10 新增; v25)
//
//   目标: 正在冷却的种子卡上、**盖在卡面之上**显示剩余秒数 (一位小数, 如 2.4)。
//   约束: 只能用**游戏自己的绘制** (不用工具那个透明叠加窗), 且**绝不允许崩游戏**。
//
//   ★ 钩点选择 (EN 1.0.0.1051, objdump 反汇编实测):
//       0x4172E7: e8 b4 2d 00 00   call 0x41A0A0   (Board::DrawUIBottom)
//     它在 `Board::DrawGameObjects` 渲染循环里, 属于 **RENDER_ITEM_BOTTOM_UI** 那条 case
//     —— 也就是**种子卡槽所在的那条 case** (Board::DrawUIBottom 内部 `mSeedBank->Draw(g)`)。
//     为什么必须在这儿: 卡槽的 z = RENDER_LAYER_UI_BOTTOM(100000)+1 = 100001, 而金币栏是
//     600000、顶栏 700000、屏幕淡出 900000 —— 渲染列表按 z 升序画, 所以当年那条
//     `call DrawUICoinBank`(0x4172BD, 属于金币栏那条 case) 其实**在卡槽之后**, 在那儿画
//     冷却文字会被卡面盖住。反过来, 把钩点放在**这条 case 的原调用之后**, 文字就必然
//     盖在卡面之上, 又不会被随后画的金币栏/顶栏挡到 (卡片在底部, 那两个都不在那儿),
//     而且仍然被屏幕淡出盖住。这条 case 之后照样画的只有铲子 (x≈706 起), 与 10 张卡的
//     最右端 (x≈580) 不重叠。
//   ★ 跳板进入时的现场 (调用点 0x4172E0..0x4172E7 保证): 栈 = [返回地址 0x4172EC]
//     [Board* this], caller 已经压好了参数; **EDI = Graphics* g**。栈算术、
//     三条硬规矩(不许在重放前 push / 重放后 Board 在 [esp+40] / 结尾 ret 4)
//     全部写在 cd_thunk.h 里 —— 那是唯一一份汇编码, 离线自测跑的就是它。
//     ★ 上一版把 board 读成 [esp+36] (其实是进入时的返回地址), 而更早一版在重放前
//       做了 pushfl/pushal ⇒ 被调者从 [esp+4] 读到 pushal 存下的寄存器而不是 Board,
//       在 DrawUIBottom+0xB 崩 (现场 EIP=0x41A0AB)。两处都已改正并有单测钉住。
//   ★ 为什么文字**不用**游戏字体 (`TodDrawString` / `Sexy::_Font::StringWidth`):
//     它要在栈上构造 `SexyString` (= 本工程的 `std::string`), 而这个 DLL 是
//     `-nostdlib` + 自己的极简 CRT, **没有 `operator new`** ⇒ 任何堆操作就是
//     Access Violation / 堆损坏 = 崩游戏。**评估结论: 这条路不做, 也没有安全变体**
//     (v30 的完整理由见下面 v30 那一段与 HANDOVER §4.6).
//   ★ v25..v29 走的是"七段数字"; v30 起换成**离线灰度字模** (见下面 v30 那一段) ——
//     始终只依赖游戏 Graphics 的矩形 API (SetColor / DrawRect / FillRect),
//     这三个地址的工具侧字节自检 + 回读自检都在。
// ---------------------------------------------------------------------------
static uint32_t g_cd_site = 0;  // 钩点地址 (工具下发)
// Graphics::FillRect(const Rect&) —— **已核对可用**的那个地址 (0 = 拿不到, 降级成 DrawRect 描边)。
//   ★ v27 之前这个变量**从来没有被赋值过** (工具侧算出了地址却没下发), 而它又写在绘制闸门里
//     ⇒ 每次都在闸门处 return ⇒ "界面已生效、卡面一片空白"。现在由 ResolveFillRect() 从
//     Control::gfx_fill_rect_addr 取, 并且**只在拿到合法地址时**才用它做实心填充。
static uint32_t g_cd_fill = 0;
static unsigned char g_cd_saved[5] = {0};
static volatile LONG g_cd_patched = 0;
static volatile LONG g_cd_fail_perm = 0; // 1 = 判定为永久失败, 不再重试 (地址变了会清, 见监视线程)
static uint32_t g_cd_fail_addr = 0;      // ★ v38: "永久失败"是针对哪个地址判的 (地址变了要重试一次)

extern "C" void *g_cd_uib_ptr = nullptr; // 供汇编重放原调用 (Board::DrawUIBottom)
// 暂存"原调用的返回值" (原因见 cd_thunk.h: popal 会把 EAX 恢复成进入时的值)
extern "C" uintptr_t g_cd_retval = 0;

// 跳板: 钩点 = Board::DrawUIBottom 的调用点 (种子卡槽那一条 case 的原调用)
//   ★★ 顺序是**先重放原调用, 再画数字** (与 P2"先重放再画"那套思路一致, 但方向反过来):
//     冷却文字要"盖在卡面之上", 所以必须**等卡槽画完**才画 ⇒ 先重放、后画。
//
//   ★★ 栈算术与全部踩过的坑都写在 cd_thunk.h 里 (那是**唯一**一份汇编, 离线自测
//      include 同一个头 ⇒ 自测跑的就是下面这段代码)。三条要点复述:
//        ① 重放之前**不许 push**: 被调者从固定的 [esp+4] 读 `this`,
//           pushfl/pushal 会把它盖掉 ⇒ 被调者拿垃圾当 Board(上一版崩溃现场
//           EIP=0x41A0AB = DrawUIBottom+0xB 就是这么来的);
//        ② 重放之后 Board 在 [esp+40] (pushfl 4 + pushal 32 + 返回地址 4);
//           传给绘制函数时, 在"又压了 g 和 board"的栈上, 它是 [esp+52];
//        ③ 结尾 `ret 4`: 与原被调者行为一致 (与这条 case 的 caller 的栈平衡完全一致)。
__attribute__((naked)) static void CooldownThunk()
{
    PZTK_CD_THUNK_BODY;
}

// 画一个矩形 —— 游戏 Graphics 的调用约定 (Graphics::SetColor 用 ECX=g / EAX=&Color;
//   DrawRect 与 FillRect 用 EAX=&Rect 且 g 压栈, 自己 ret 4)。
//   失败(地址为 0)时**什么都不做**: 宁可看不见文字, 也绝不乱调地址。
//   为什么要自己写 FillRect 的调用: 共享头里只下发了 DrawRect 的地址, 而"数字底衬"需要
//   实心块 —— FillRect 的入口形状(前 3 字节 `8B 48 0C`)与 DrawRect(`8B 48 0C`)一样,
//   工具侧**用同一套字节自检**确认过才下发 (见 pvz.cpp 的 PublishCooldown)。
static void GfxFillRectRaw(void *g, const int32_t *rc, uint32_t fn, const uint32_t *color)
{
    if (g == nullptr || fn == 0)
        return;
    GfxSetColor(g, color, static_cast<uint32_t>(g_ctl ? g_ctl->gfx_set_color_addr : 0));
    __asm__ __volatile__("pushl %2\n\t"
                         "movl %1, %%eax\n\t"
                         "call *%0\n\t" ::"r"(fn),
                         "r"(rc), "r"(g)
                         : "eax", "ecx", "edx", "memory");
}

// 黑色 —— fill = 用 FillRect 实心画, 否则用 DrawRect 描边
//   ★★ FillRect 不可用时**必须降级成描边, 不能什么都不做** ★★
//      v26 的写法是 `if (fill) { GfxFillRectRaw(...); return; }`, 而 g_cd_fill 恒为 0
//      ⇒ GfxFillRectRaw 直接 return ⇒ 底衬与**数字的每一段**都是空操作 = 整帧不可见。
//      这就是"工具显示已生效、卡面却一片空白"的第二个成因。
//   ★ v31: 黑色底衬**不再画** (它是"粗黑边"的一半来源) ⇒ 本函数在当前代码里**没有调用点**;
//     留着是因为它同时是"FillRect 不可用时降级成描边"这条硬规矩的最简示例 + 红探针的模板。
static void GfxRectBlack(void *g, const int32_t *rc, bool fill)
{
    if (g == nullptr || g_ctl == nullptr)
        return;
    uint32_t color[4];
    color[0] = 0; // 纯黑
    color[1] = 0;
    color[2] = 0;
    color[3] = 0xFF; // ★ alpha 必须是 0xff: 游戏的 DrawRect/FillRect 遇到 mAlpha==0 直接返回
    if (fill && g_cd_fill != 0)
    {
        GfxFillRectRaw(g, rc, g_cd_fill, color);
        return;
    }
    const uint32_t fn = static_cast<uint32_t>(g_ctl->gfx_draw_rect_addr);
    if (fn == 0)
        return; // ★ 地址为 0 时**必须**在调用前返回 (原来的写法会 call *0 = 当场崩)
    GfxSetColor(g, color, static_cast<uint32_t>(g_ctl->gfx_set_color_addr));
    GfxDrawRect(g, rc, fn);
}

// 白色 (数字) —— fill = 实心段, 否则描边
//   ★ v30: 字模走 `GfxRectInk(..., 0xFF, alpha, ...)` (每个块自带灰度 + 覆盖度), 所以本函数
//     在当前代码里**没有调用点**; 留着是因为"纯白矩形"是这套调用约定的最简示例,
//     下次要在卡面上补个白块时直接可用 (它也一样在地址为 0 时安全返回)。
static void GfxRectWhite(void *g, const int32_t *rc, bool fill)
{
    if (g == nullptr || g_ctl == nullptr)
        return;
    uint32_t color[4];
    color[0] = 0xFF;
    color[1] = 0xFF;
    color[2] = 0xFF;
    color[3] = 0xFF;
    if (fill && g_cd_fill != 0)
    {
        GfxFillRectRaw(g, rc, g_cd_fill, color);
        return;
    }
    const uint32_t fn = static_cast<uint32_t>(g_ctl->gfx_draw_rect_addr);
    if (fn == 0)
        return;
    GfxSetColor(g, color, static_cast<uint32_t>(g_ctl->gfx_set_color_addr));
    GfxDrawRect(g, rc, fn);
}

// 红色 (二分定位探针: 覆盖整张卡面的大红块) —— 见 Control::cd_red_probe
static void GfxRectRed(void *g, const int32_t *rc, bool fill)
{
    if (g == nullptr || g_ctl == nullptr)
        return;
    uint32_t color[4];
    color[0] = 0xFF;
    color[1] = 0x20;
    color[2] = 0x20;
    color[3] = 0xFF;
    if (fill && g_cd_fill != 0)
    {
        GfxFillRectRaw(g, rc, g_cd_fill, color);
        return;
    }
    const uint32_t fn = static_cast<uint32_t>(g_ctl->gfx_draw_rect_addr);
    if (fn == 0)
        return;
    GfxSetColor(g, color, static_cast<uint32_t>(g_ctl->gfx_set_color_addr));
    GfxDrawRect(g, rc, fn);
}

// ---------------------------------------------------------------------------
// ★★ v31: 冷却数字 = "离线灰度字模 + **逐像素**画矩形 (预算紧张时 2x2 聚合)" ★★
//
//   实机反馈 (用户, v30): 秒数确实画在卡面上了 ✓, 但外观仍不合格 ✗ ——
//     **"效果还是不好, 这种方块数字不太好看"** (截图里是**粗黑边 + 白色方块**感的数字)。
//
//   ★ 为什么 v30 必然是"方块": 它把每一行的**同灰度水平行程**并成一根 1px 高的实心条,
//     而 7x9 的画布上抗锯齿几乎不存在 (笔画全是满覆盖)、灰度阶梯最高只有 208 (根本没有白),
//     再叠一层右下偏移 1px 的**暗色阴影** ⇒ 白块外面一圈**粗黑边**。
//     ⇒ 观感就是"方块", 而且底色再亮一点就糊成一片。
//   ★ v31 的做法:
//     ① **逐像素**: 每个字模像素 = 一个 1x1 矩形, 颜色 = 该像素的 8 位灰度
//        (0 = 透明; 灰度 = 白字在**游戏自己叠的暗色**上的混合结果, 见 cd_font.h 表头);
//     ② **10x14** 画布 (Tahoma 19px) ⇒ 真的抗锯齿 (每像素一个覆盖度), 高 14px 落在参考图的
//        12~16px 区间里; 10 是这张 50px 卡面上"4 个字符格子 + 2px 缝隙"能放下的最大宽度
//        (12x16 在几何上放不下 —— 见 hook_ctl.h 的 v31 几何节);
//     ③ **阴影层与黑色底衬全部删掉** (它们就是那圈黑边的来源); 字画在变暗的卡面上即可。
//   ★ 性能 (用户要求"绝不能因为画字卡住游戏")
//     矩形数 = SetColor + FillRect 的调用次数。逐像素比 v30 的"逐行程"**更贵**, 所以每帧
//     有一个**硬预算** kCdRectBudget (800, 与 v30 相同: 那个数已经在用户实机上跑过、无卡顿反馈),
//     降级阶梯由共享头纯函数 CdPickStep 给出, 顺序不能反, 且**每一步都先问够不够**:
//       ① 1x1 逐像素够画 ⇒ 画 (最清晰);
//       ② 不够 ⇒ **这一张卡**改成 2x2 聚合 (取块内最大灰度 ⇒ 仍然单调、不会把白画成灰);
//       ③ 2x2 也不够 ⇒ 这张卡整张不画, 记 CDSKIP_BIT_BUDGET。
//     ⇒ 本帧矩形数**永远 <= 预算** (离线单测用同一个 CdPickStep 断言三种预算情形)。
//
//   为什么不用游戏自带字体 API (TodDrawString): 它要在栈上构造 SexyString
//   (= 本工程的 std::string), 而本 DLL 是 -nostdlib + 自备极简 CRT、**没有 operator
//   new** ⇒ 任何堆操作 = Access Violation / 堆损坏 = 崩游戏。**结论: 不做**
//   (评估细节见 HANDOVER §4.6.1 / CHANGELOG 顶部)。
//
//   字模从哪来: env/scripts/gen_cd_font.py (Pillow) 离线渲染 Tahoma 19px -> 10x14 的
//   **逐像素 8 位灰度**画布 (灰度 = 白字混合在游戏自己叠的暗色 Color(64,64,64) 上)
//   -> tool-src/hook/cd_font.h。
//
//   ★★ v36: 颜色改成 **alpha 混合** —— 表里的灰度先反解成**覆盖度**, 再画成
//   "白字 + A = 覆盖度" ★★
//     为什么: v35 的"不透明灰 + 抬到下限 160"把 76..159 这段 (29% 的墨迹像素) 压成
//     同一个灰, 抗锯齿没了; 更致命的是它**押注在"卡面 <= 128"这个推算值上** ——
//     实机反馈 (用户: 那些不是黑点, 是**洞** / 笔画断成规则缺口) 说明这个押注不成立。
//     alpha 画法与卡面亮度**无关**:
//         输出 = 卡面 + (255 - 卡面) x A / 255
//     ① 永远不比卡面更暗 ⇒ **不可能有黑点**; ② A 只在覆盖度 0 时才是 0 ⇒ **有墨必有画,
//     不可能有洞**; ③ A 与覆盖率成正比 ⇒ 抗锯齿梯度完整保留 (暗卡面上等于 v31 那套定标值)。
//     ★ 回退开关: 环境变量 `PZTK_CD_NOALPHA=1` (DLL 加载时读一次) ⇒ 不透明 + 抬下限,
//       且**照样不丢任何像素** (那条"低于某灰度就不画"的路径在 v36 已被彻底删除)。
//
//   本文件里只做三件事:
//     ① CdGlyphBlocks() (共享头纯函数) 把字模展开成矩形块 —— "画什么"只有这一份实现;
//     ② 把每个块用**已实机验证过**的 Graphics::SetColor + FillRect 画出来
//        (v36 起同一个 Color 结构的 **A 取覆盖度**, 不新增任何 API/参数);
//     ③ 档位 (1x1 / 2x2) 由共享头纯函数 CdPickStep 按预算决定 —— **没有阴影层**。
// ---------------------------------------------------------------------------

// 画一个字模矩形块 (1x1 或 2x2)。
//   fill  = 用 FillRect 实心画, 否则 DrawRect 描边 (拿不到 FillRect 时的降级, 与 v27 的规矩一致)。
//   last_ink = "Graphics 里现在装的是哪个颜色" 的缓存 (-1 = 未知), 见下。
//     ★ 为什么值得缓存: 一个 1x1 的矩形要配一次 SetColor, 而同一个笔画里相邻像素常常同色;
//       FillRect 才是花钱的那个 (要么是一次 D3D DrawPrimitive, 要么是 DDImage 的一次 surface 锁),
//       SetColor 只是 `mov [ecx+0x30], edx` —— 跳过重复的 SetColor 是纯收益, 不改任何像素。
//     ★ v36: 颜色现在是 **(gray, alpha) 一对**, 所以缓存也必须缓存这一对 ——
//       以前只缓存 gray 就够了 (alpha 恒 0xff); 现在同一个 gray 配不同 alpha 是两个颜色,
//       只比 gray 会把 alpha 画错 (字形会整块变实/变透)。缓存键 = (gray << 8) | alpha。
//     ★ 任何"别的颜色"绘制 (红探针) 之后必须把缓存置回 -1。
static void GfxRectInk(void *g, const int32_t *rc, uint32_t gray, uint32_t alpha, bool fill,
                       int32_t &last_ink)
{
    if (g == nullptr || g_ctl == nullptr)
        return;
    uint32_t color[4];
    color[0] = gray;
    color[1] = gray;
    color[2] = gray;
    // ★★ v36: alpha 不再是常量 ★★
    //   默认 (alpha 档) = 该像素的覆盖度 (1..255) ⇒ 白字按 alpha 叠在卡面上, 永远不会
    //   比卡面更暗 (不可能有黑点), 覆盖度 > 0 就一定画 (不可能有洞);
    //   回退 (PZTK_CD_NOALPHA=1) = 255 ⇒ 不透明 + 抬下限 (老路径)。
    //   ★ 绝不能是 0: 游戏里 `Color` 的 mAlpha==0 会被当成"不画", 那不是我们要的"淡"。
    color[3] = (alpha == 0) ? 0xFF : alpha;
    const int32_t key = static_cast<int32_t>((gray << 8) | (color[3] & 0xFF));
    if (fill && g_cd_fill != 0)
    {
        if (last_ink != key)
        {
            GfxSetColor(g, color, static_cast<uint32_t>(g_ctl->gfx_set_color_addr));
            last_ink = key;
        }
        GfxFillRectRaw(g, rc, g_cd_fill, color);
        return;
    }
    const uint32_t fn = static_cast<uint32_t>(g_ctl->gfx_draw_rect_addr);
    if (fn == 0)
        return; // ★ 地址为 0 时**必须**在调用前返回 (否则 call *0 = 当场崩)
    if (last_ink != key)
    {
        GfxSetColor(g, color, static_cast<uint32_t>(g_ctl->gfx_set_color_addr));
        last_ink = key;
    }
    GfxDrawRect(g, rc, fn);
}

// ★★ v36: 墨色模式 (alpha 混合 / 不透明回退) ★★
//   默认 = `PtHook::CDINK_ALPHA` (白字 + A = 覆盖度)。环境变量 `PZTK_CD_NOALPHA=1`
//   (DLL 加载时读一次 ⇒ 改完要**重开游戏**) 切到 `PtHook::CDINK_OPAQUE`:
//   A=0xff + 灰度抬到 kCdFontInkFloor, **照画不跳像素**。
//   为什么要这个开关: alpha 到底有没有被游戏的 FillRect 真的混合, 只有实机能验;
//   万一 alpha 被忽略 (表现: 数字变成**实心白块**), 用户必须能立刻切回旧路径。
//   ★ 只用 kernel32 的 GetEnvironmentVariableA (不碰 CRT, 不做堆分配)。
static int32_t g_cd_ink_mode = PtHook::CDINK_ALPHA;

static void ReadInkModeFromEnv()
{
    char buf[8];
    const DWORD n = GetEnvironmentVariableA("PZTK_CD_NOALPHA", buf, sizeof(buf));
    g_cd_ink_mode = PtHook::CDINK_ALPHA;
    if (n > 0 && n < sizeof(buf) && buf[0] == '1')
        g_cd_ink_mode = PtHook::CDINK_OPAQUE;
}

// 把一个字符的字模按 `step` 档位画出来 (step = 1: 逐像素; step = 2: 2x2 聚合)。
//   格子位置由调用方按 CdCellRect 算好 (共享头纯函数), 这里只负责画。
//   返回真的下发了多少个矩形 (0 = 字模/格子尺寸对不上, 或调用方给的档位不合法)。
static int32_t DrawGlyphCell(void *g, int x0, int y0, int cw, int ch, char c, int32_t step,
                             int32_t &last_ink)
{
    if (cw != PtHook::kCdFontCanvasW || ch != PtHook::kCdFontCanvasH)
        return 0; // 字模不缩放 (见共享头 CdGlyphBlocks 的说明)
    PtHook::CdBlock blocks[PtHook::kCdGlyphMaxBlocks];
    const int32_t n = PtHook::CdGlyphBlocks(c, cw, ch, step, blocks, PtHook::kCdGlyphMaxBlocks,
                                            g_cd_ink_mode);
    int32_t drawn = 0;
    int32_t rc[4];
    for (int32_t i = 0; i < n; i++)
    {
        rc[0] = x0 + blocks[i].x;
        rc[1] = y0 + blocks[i].y;
        rc[2] = blocks[i].w;
        rc[3] = blocks[i].h;
        GfxRectInk(g, rc, blocks[i].gray, blocks[i].alpha, true, last_ink);
        drawn++;
    }
    return drawn;
}

// ---------------------------------------------------------------------------
// 问系统要"这段内存到底能安全读多少字节" (0 = 一个字节都不该读)
//
//   为什么不用 `IsBadReadPtr`: 它靠"故意触发异常再吞掉"来判断可读性, 在多线程
//   进程里是与系统竞争的经典误用 (MSDN 明确建议不要用; 目标内存的属主可能在
//   两次访问之间改保护属性/提交状态), 而且它自己也可能把异常处理栈搞坏。
//   跳板跑在游戏自己的渲染线程里, 一旦在这儿出问题就是**崩游戏** —— 用户明确
//   要求绝不允许。所以统一改用 VirtualQuery 逐段确认: 只看已提交、无 GUARD、
//   非 NOACCESS 的页, 返回可读前缀长度。
//   ★ 与 §ScanRowMask 里"绝不整段放弃、只扫可读前缀"是同一套做法。
// ---------------------------------------------------------------------------
static uint32_t ReadableBytes(uint32_t addr, uint32_t want)
{
    if (addr == 0 || want == 0)
        return 0;
    const uintptr_t start = static_cast<uintptr_t>(addr);
    const uintptr_t end = start + static_cast<uintptr_t>(want);
    uintptr_t cur = start;
    uint32_t got = 0;
    while (cur < end)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<LPCVOID>(cur), &mbi, sizeof(mbi)) == 0)
            break;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
            break;
        const uintptr_t chunk_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        const uintptr_t stop = (chunk_end < end) ? chunk_end : end;
        got = static_cast<uint32_t>(stop - start);
        if (stop >= end)
            break;
        cur = stop;
    }
    return got;
}

// ---------------------------------------------------------------------------
// FillRect 地址的"二次独立核对" (工具侧查过一次, DLL 自己再查一次)
//
//   ★ 为什么 DLL 还要再查: 工具侧只核对入口前 3 字节 `8B 48 0C` —— 万一那个常量在别的
//     构建里指到了别的函数上, 我们会拿"EAX=&Rect, g 压栈, 自己 ret 4"的约定去调一个
//     **约定不同**的函数 ⇒ 栈错位 = 崩游戏。这里在**第一次使用前**用实时字节再确认一次,
//     并用 VirtualQuery 确认那几字节可读。不通过就**永远不用 FillRect** (返回 0 ⇒ 绘制
//     自动降级成 DrawRect 描边, 功能照旧可见)。
//   ★ 只在"工具下发的值变了"时重查 (缓存), 所以热路径上只有一次整数比较。
// ---------------------------------------------------------------------------
static uint32_t g_cd_fill_seen = 0; // 上一次核对过的"工具下发值"

static uint32_t ResolveFillRect(Control *ctl)
{
    const uint32_t pub = ctl->gfx_fill_rect_addr;
    if (pub == 0)
    {
        g_cd_fill = 0;
        g_cd_fill_seen = 0;
        return 0;
    }
    if (pub == g_cd_fill_seen)
        return g_cd_fill;
    g_cd_fill_seen = pub;
    g_cd_fill = 0;
    unsigned char b[3] = {0, 0, 0};
    if (ReadableBytes(pub, 3) == 3)
    {
        memcpy(b, reinterpret_cast<const void *>(static_cast<uintptr_t>(pub)), 3);
        // Graphics::FillRect(const Rect&) 的入口前 3 字节: `8B 48 0C` = mov ecx,[eax+0xC]
        //   (与 DrawRect 的入口同形, 反汇编核对过)
        if (b[0] == 0x8B && b[1] == 0x48 && b[2] == 0x0C)
            g_cd_fill = pub;
    }
    return g_cd_fill;
}

// ---------------------------------------------------------------------------
// ★★ v37: "原生方式" —— 用**游戏自己的字体**画冷却数字 ★★
//
//   用户拍板的目标: 与游戏 UI 里那个 `150` 一样 —— 真抗锯齿 / 真混合 / 逐像素原生。
//   为什么矩形拼字做不到: 每张卡约 240 次 `SetColor`+`FillRect`, 墨色必须**假定**卡面
//   底色 (v35 押"<=128"、v36 改成 alpha) ⇒ "黑点"与"洞"是**固有**的, 只能缓解不能消除;
//   而游戏自己的字体是 ImageFont (DDS 图集 + 逐像素 alpha), 由游戏的 alpha blit 画出来,
//   与底色**天然无关**。
//
//   ★★ 全部 ABI 依据在共享头 hook_ctl.h 的"v37 原生字体路径"那一节里逐条列出,
//      每一条都带出厂 exe 的字节 (函数地址 / 调用约定 / `SexyString` 的 SSO 布局 /
//      字体全局槽的取法 / 我们为什么**不**调 TodDrawString)。这里只做三件事:
//        ① 从工具下发的**全局槽地址**现读 `FONT_CONTINUUMBOLD14` 这个 `_Font*`;
//        ② 从**字体对象自己的虚表**里现读 DrawString (+0x28) / StringWidth (+0x1C),
//           并用出厂字节指纹核对入口形状与结尾的 `ret 0x18` (= 调用约定本身) ——
//           任何一条不符就**立刻降级成矩形路径** (绝不"先试一次看看");
//        ③ 在**栈上**构造一个 SSO 的 `SexyString` (容量 15 < 16 ⇒ 零堆分配、析构不 free),
//           用已证明的 `__thiscall` 调 DrawString 把数字写在可画区正中。
//
//   ★★ 这一节里绝不做的事 (每一条都会"直接崩游戏而且捕获不到") ★★
//     * 不调 `TodDrawString` (0x511CE0): 非虚函数 + LTCG 私有寄存器约定, 字节上无法安全复现;
//       它只是 Translate + StringWidth + DrawString, 而我们的串没有 `[...]` 标记 ⇒ 等价。
//     * 不做任何堆分配: 字符串走 SSO (<= 15 字节); 这一轮**一次**堆分配都不需要。
//     * 不释放/改写任何游戏对象: 只**读**全局槽与字体对象, 只写自己的栈。
//     * 自检不过就**不画** (降级), 绝不带着"可能不对的约定"去 call。
// ---------------------------------------------------------------------------
static uint32_t g_font_slot_pub = 0;   // 上一次核对过的"工具下发的全局槽地址"
static void *g_font_ptr = nullptr;     // 自检**通过**的 `_Font*` (空 = 不能用原生路径)
static uint32_t g_font_bad_ptr = 0;    // 自检**失败**过的字体指针 (同一个指针不重复查)
static uint32_t g_font_draw_fn = 0;    // 虚表 +0x28 (只用于诊断上报)
static uint32_t g_font_sw_fn = 0;      // 虚表 +0x1C (只用于诊断上报)
static int32_t g_font_ascent = 0;      // `_Font` +0x04
static int32_t g_font_height = 0;      // `_Font` +0x0C
static uint32_t g_font_state = PtHook::CDNAT_NO_ADDR;
static bool g_font_slot_readable = false;
static int64_t g_qpc_freq = 0;         // QueryPerformanceFrequency (只问一次)

// QPC: 只用来量"我们这一段绘制本身"的耗时 (不量游戏总帧率 —— 那个没有可比性)
static int64_t QpcFreqCached()
{
    if (g_qpc_freq == 0)
    {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        g_qpc_freq = (f.QuadPart > 0) ? f.QuadPart : 1;
    }
    return g_qpc_freq;
}

static int64_t QpcTicks()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

// QPC 差值 -> µs (只量"我们这一段绘制本身"; 换算全在 32 位里做, 见共享头 CdMulDivU32)。
//   ★ 为什么不直接算 64 位除法: 本 DLL 是 -nostdlib, 没有 __udivdi3 ⇒ 链接会失败。
static int32_t QpcDeltaToUs(int64_t delta)
{
    if (delta <= 0)
        return 0;
    const int64_t freq = QpcFreqCached();
    if (freq <= 0 || freq > 0xFFFFFFFFLL)
        return 0;
    if (delta > 0xFFFFFFFFLL)
        return PtHook::kCdUsPerFrameMax;
    return PtHook::CdQpcToUs(static_cast<uint32_t>(delta), static_cast<uint32_t>(freq));
}

// 逐字节比较 (带可读性确认)。返回真 = 目标可读**且**逐字节等于 want。
//   ★ 为什么不用 memcmp: 目标在游戏进程里, 可能整页不可读 —— 先 VirtualQuery 再比。
static bool BytesEqual(uint32_t addr, const uint8_t *want, int32_t n)
{
    if (addr == 0 || want == nullptr || n <= 0)
        return false;
    const uint32_t need = static_cast<uint32_t>(n);
    if (ReadableBytes(addr, need) < need)
        return false;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(static_cast<uintptr_t>(addr));
    for (int32_t i = 0; i < n; i++)
    {
        if (p[i] != want[i])
            return false;
    }
    return true;
}

// 读一个游戏进程里的 32 位值 (不可读时返回 0 并由调用方当作"自检不过")
static uint32_t ReadU32(uint32_t addr)
{
    if (addr == 0 || ReadableBytes(addr, 4) < 4)
        return 0;
    return *reinterpret_cast<volatile const uint32_t *>(static_cast<uintptr_t>(addr));
}

static void ForgetGameFont(uint32_t state)
{
    g_font_ptr = nullptr;
    g_font_draw_fn = 0;
    g_font_sw_fn = 0;
    g_font_ascent = 0;
    g_font_height = 0;
    g_font_state = state;
}

// 取本帧可用的 `_Font*` (0 = 不能用原生路径, 原因写进 state_out)
//   ★ 缓存策略: 只在"下发的全局槽地址变了 / 槽里的字体指针变了"时才重跑自检
//     (热路径上只剩一次整数比较 + 一次读); 自检失败过的指针**不重复查** (避免每帧
//     都做几次 VirtualQuery)。
//   ★ 每帧仍然**现读**槽里的指针 ⇒ 游戏换关/重载资源时立刻跟上, 绝不缓存游戏对象。
//   ★★ v38: 两条判据都收敛成共享头纯函数 (`CdFontSlotProbeDue` / `CdFontSelfCheckNeeded`)
//     —— 因为 v37 那两处**会粘住**: ① "槽不可读"只算一次就永久缓存 (这一局再也用不上
//     原生方式); ② "坏指针不重复查"必须保留, 但**换了新对象必须重查** (否则换关后
//     永远禁用原生方式)。判据进共享头 ⇒ 离线单测能直接钉住"失败不粘滞"。
static uint32_t ResolveGameFont(Control *ctl, uint32_t &state_out)
{
    const uint32_t pub = ctl->gfx_font_global_addr;
    if (pub == 0)
    {
        g_font_slot_pub = 0;
        g_font_bad_ptr = 0;
        g_font_slot_readable = false;
        ForgetGameFont(PtHook::CDNAT_NO_ADDR);
        state_out = g_font_state;
        return 0;
    }
    if (pub != g_font_slot_pub)
    {
        g_font_slot_pub = pub;
        g_font_bad_ptr = 0;
        g_font_slot_readable = (ReadableBytes(pub, 4) >= 4);
        ForgetGameFont(PtHook::CDNAT_FONT_NULL);
    }
    // ★★ v38: 探测失败**绝不缓存** ★★ (用户硬要求: "任何一帧失败都不许影响下一帧")
    //   v37 只在"下发的槽地址变了"时才算一次 `ReadableBytes` —— 那一次要是失败
    //   (工具刚下发、页面还没提交), `g_font_slot_readable` 就**永远**是 false,
    //   这一局再也用不上原生方式。现在改成: 上一次没读成功 ⇒ 下一帧**必须重试**
    //   (判据 `CdFontSlotProbeDue` 在共享头里, 离线单测钉住)。
    //   代价: 一个失败的帧多做一次 VirtualQuery —— 可以忽略; 成功之后仍然走缓存。
    else if (PtHook::CdFontSlotProbeDue(false, g_font_slot_readable))
    {
        g_font_slot_readable = (ReadableBytes(pub, 4) >= 4);
    }
    if (!g_font_slot_readable)
    {
        // ★ 这里**故意**不缓存成"永久失败": 下一帧还会重试 (见上面那一段)
        state_out = PtHook::CDNAT_FONT_NULL;
        return 0;
    }
    const uint32_t font = *reinterpret_cast<volatile const uint32_t *>(static_cast<uintptr_t>(pub));
    if (font == 0)
    {
        ForgetGameFont(PtHook::CDNAT_FONT_NULL); // 游戏还没把字体塞进全局槽
        state_out = g_font_state;
        return 0;
    }
    // ★★ v38: 缓存判据收敛到共享头纯函数 (v37 是散在这里的四个 if) ★★
    //   `g_font_ptr` / `g_font_bad_ptr` 都是 `_Font*` (0 = 没缓存) ⇒ 传地址值就行。
    const uint32_t font_cached = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_font_ptr));
    if (!PtHook::CdFontSelfCheckNeeded(pub, g_font_slot_pub, font, font_cached, g_font_bad_ptr))
    {
        // 命中缓存 (同一个已经核对过的对象) / 同一个坏对象不重复查
        if (font_cached != 0 && font_cached == font)
            return font;
        state_out = g_font_state;
        return 0;
    }

    // ---- 全套自检 (只在上面两个"变了"的条件成立时才跑) ----
    ForgetGameFont(PtHook::CDNAT_FONT_BAD);
    const uint32_t vtbl = ReadU32(font);
    if (vtbl == 0 || ReadableBytes(vtbl, static_cast<uint32_t>(PtHook::kFntVTableReadNeed)) <
                         static_cast<uint32_t>(PtHook::kFntVTableReadNeed))
    {
        g_font_bad_ptr = font;
        state_out = g_font_state;
        return 0;
    }
    // ① 先证明"这确实是一张 `_Font` 的虚表": 槽 +0x04 必须是 GetAscent
    //    (`8B 41 04 C3`), 顺带把 mAscent 的偏移钉住。不是就算了 (宁可不用新方式)。
    const uint32_t get_ascent = ReadU32(vtbl + static_cast<uint32_t>(PtHook::kFntSlotGetAscent));
    if (!BytesEqual(get_ascent, PtHook::kFntAscentProbe, 4))
    {
        g_font_bad_ptr = font;
        state_out = g_font_state;
        return 0;
    }
    // ② DrawString (+0x28): 入口形状 + 结尾的 `C2 18 00`。
    //    ★★ 这一条检查的就是**调用约定**: `ret 0x18` = 被调者自己清 6 个 dword。
    //       如果这里不符而我们照旧用"被调者清栈"的方式去调, 栈会立刻错位 ⇒ 崩游戏。
    const uint32_t draw = ReadU32(vtbl + static_cast<uint32_t>(PtHook::kFntSlotDrawString));
    if (!BytesEqual(draw, PtHook::kFntDrawProbe, PtHook::kFntDrawProbeLen))
    {
        ForgetGameFont(PtHook::CDNAT_VTABLE_BAD);
        g_font_bad_ptr = font;
        state_out = g_font_state;
        return 0;
    }
    if (!BytesEqual(draw + static_cast<uint32_t>(PtHook::kFntDrawRetOffset), PtHook::kFntDrawRet, 3))
    {
        ForgetGameFont(PtHook::CDNAT_ABI_BAD);
        g_font_bad_ptr = font;
        state_out = g_font_state;
        return 0;
    }
    // ③ StringWidth (+0x1C): 入口形状 + **`SexyString` 的 SSO 布局指纹**
    //    (`cmp [obj+0x18],0x10` + `+0x14` 当长度 + `+0x04` 当缓冲)。
    //    ★ 这一条是"我们在栈上按那份布局造对象"的**运行期**依据: 布局变了就降级。
    const uint32_t sw = ReadU32(vtbl + static_cast<uint32_t>(PtHook::kFntSlotStringWidth));
    if (!BytesEqual(sw, PtHook::kFntSwProbe, PtHook::kFntSwProbeLen))
    {
        ForgetGameFont(PtHook::CDNAT_VTABLE_BAD);
        g_font_bad_ptr = font;
        state_out = g_font_state;
        return 0;
    }
    if (!BytesEqual(sw + static_cast<uint32_t>(PtHook::kFntSwLayoutOffset), PtHook::kFntSwLayout,
                    PtHook::kFntSwLayoutLen))
    {
        ForgetGameFont(PtHook::CDNAT_LAYOUT_BAD);
        g_font_bad_ptr = font;
        state_out = g_font_state;
        return 0;
    }
    // ④ 度量 (只读两个 int)
    const int32_t ascent =
        static_cast<int32_t>(ReadU32(font + static_cast<uint32_t>(PtHook::kFntOffAscent)));
    const int32_t height =
        static_cast<int32_t>(ReadU32(font + static_cast<uint32_t>(PtHook::kFntOffHeight)));
    if (ascent <= 0 || height <= 0 || height > 64 || ascent > height)
    {
        g_font_bad_ptr = font;
        state_out = g_font_state; // 仍是 FONT_BAD
        return 0;
    }
    g_font_ptr = reinterpret_cast<void *>(static_cast<uintptr_t>(font));
    g_font_draw_fn = draw;
    g_font_sw_fn = sw;
    g_font_ascent = ascent;
    g_font_height = height;
    g_font_state = PtHook::CDNAT_OK;
    state_out = g_font_state;
    return font;
}

// `_Font::DrawString` 的调用: `__thiscall`, ECX = `_Font*`, 6 个栈参,
//   **被调者自己清 6 个 dword** (出厂字节 `C2 18 00`)。
//   ★ 因此这里 call 之后**绝不能** `add esp,0x18` —— 那会把 esp 抬高一截,
//     调用者的局部量/返回地址全部错位 (这是"调用约定搞错"最典型的一种崩法)。
//   ★ 为什么用裸汇编而不是 `__thiscall` 函数指针: 与工程里既有的 SetColor/DrawRect/
//     FillRect 保持同一套写法, 也避免编译器替我们插入它以为需要的栈调整。
//   ★ 先把参数一次性压完, 再把函数入口与 this 装进 edx/ecx, 最后 call —— 这样
//     "取参"与"call"之间没有会被覆盖的基址寄存器。
struct FontDrawArgs
{
    void *font;         // +0x00  `_Font*` (进 ECX)
    void *g;            // +0x04  Graphics*
    int32_t x;          // +0x08
    int32_t y;          // +0x0C
    const void *str;    // +0x10  &SexyString
    const void *color;  // +0x14  &Color
    const void *clip;   // +0x18  &Rect (Graphics::mClipRect)
    uint32_t fn;        // +0x1C  从虚表 +0x28 现读的入口
};

static void FontDrawStringRaw(const FontDrawArgs &a)
{
    __asm__ __volatile__("pushl %%esi\n\t"
                         "movl  %0, %%esi\n\t"
                         "pushl 24(%%esi)\n\t"
                         "pushl 20(%%esi)\n\t"
                         "pushl 16(%%esi)\n\t"
                         "pushl 12(%%esi)\n\t"
                         "pushl  8(%%esi)\n\t"
                         "pushl  4(%%esi)\n\t"
                         "movl  28(%%esi), %%edx\n\t"
                         "movl   0(%%esi), %%ecx\n\t"
                         "call  *%%edx\n\t"
                         "popl  %%esi\n\t" ::"r"(&a)
                         : "eax", "ecx", "edx", "memory", "cc");
}

// `_Font::StringWidth` 的调用: `__thiscall`, ECX = `_Font*`, 1 个栈参,
//   **被调者清 1 个 dword** (`ret 4`; 两处出厂调用点之后都没有 `add esp,4`)。
struct FontWidthArgs
{
    void *font;        // +0x00
    const void *str;   // +0x04
    uint32_t fn;       // +0x08
};

static int32_t FontStringWidthRaw(const FontWidthArgs &a)
{
    int32_t w = 0;
    __asm__ __volatile__("pushl %%esi\n\t"
                         "movl  %1, %%esi\n\t"
                         "pushl 4(%%esi)\n\t"
                         "movl  8(%%esi), %%edx\n\t"
                         "movl  0(%%esi), %%ecx\n\t"
                         "call  *%%edx\n\t"
                         "popl  %%esi\n\t"
                         "movl  %%eax, %0\n\t"
                         : "=m"(w)
                         : "r"(&a)
                         : "eax", "ecx", "edx", "memory", "cc");
    return w;
}

// 把"剩余秒数"拼成定长 ASCII 串 —— ★★ v38: 顺序与矩形路径**同源** ★★
//
//   ★★ 这里就是用户实机报的"小数点往右移了一位"的根因 ★★
//     `chars_hi[]` 的约定 (由本文件第一趟填充、由共享头 CdDisplayChar 消费) 是:
//         chars_hi[0..ndig-1] = **最高位 .. 十分位** (共 ndig 个字符, 十分位就在最后一格),
//         小数点插在第 ndig-1 格**之前** ⇒ 整串一共 ndig+1 个字符。
//     而 v37 的这里按另一套约定写 ("chars_hi = 最高位..个位", 再补 `.` 和 frac):
//         循环抄完 ndig 个 (末尾那个**已经是十分位**), 又补 `.` 与 frac
//         ⇒ `35.0` 变 `350.0`、`12.4` 变 `124.4` —— 小数点整整右移一格, 末尾还多一个字符。
//     ⇒ 现在**不再自己拼**: 逐格调用共享头的 `CdTextCharsBuild` (它内部就是 CdDisplayChar)
//       ⇒ 两条路的字符顺序**只有一份**实现, 不可能再分叉 (离线单测逐组合断言)。
//
//   `frac` 只用来做一次**交叉核对**: 顺序函数放进末位的那个字符必须就是十分位
//     (两处说法不一致就返回 0 ⇒ 调用方拒绝这一帧, 绝不画半个数)。
//   返回串长 (= ndig + 1, 含小数点); 0 = 输入不合法。
static int32_t BuildCdText(char *out, int32_t cap, const char *chars_hi, int32_t ndig, int32_t frac)
{
    if (out == nullptr || chars_hi == nullptr || cap < 8)
        return 0;
    if (ndig < 1 || ndig > 3 || frac < 0 || frac > 9)
        return 0;
    const int32_t n = PtHook::CdTextCharsBuild(ndig, chars_hi, out, cap);
    if (n <= 0)
        return 0;
    // 交叉核对: 末位必须是十分位, 且必须与调用方给的 frac 一致
    if (n != ndig + 1 || out[n - 1] != static_cast<char>('0' + frac))
        return 0;
    // 再核对一次"小数点下标 == 个位那一格的右边" (与矩形路径的 CdDotRect 同一格)
    if (out[PtHook::CdDotIndex(ndig)] != '.')
        return 0;
    return n;
}

// 在游戏自己的绘制流水线里画冷却数字 (由跳板调用; g = Graphics*, board = Board*,
//   replay_result = 原调用 (DrawUIBottom) 的返回值)
//
//   ★★ 这道函数是"整个绘制段的唯一闸门": 跳板的重放调用已经做完了, 这里**任何**
//      早退都只意味着"少画几个数字", 绝不影响重放 —— 用户要求"内层一旦自检不过就
//      只跳过绘制"。所以下面每一条校验都是"直接 return", 没有一个 abort/写游戏内存。
extern "C" void CooldownDrawInGame(void *g, void *board, void *replay_result)
{
    (void)replay_result; // 原样转交只为离线单测能断言"它被当成返回值、g 来自 EDI"
    Control *ctl = g_ctl;
    if (ctl == nullptr || g == nullptr || board == nullptr || g_unloading != 0)
        return;
    // ---- ★★ v28: 先把"我这一帧读到的开关"原样回写给工具 ★★ ----
    //   这是跨进程开关的**硬规矩第三条** (见 hook_ctl.h 的 CdWiringHealth 上面那段):
    //   DLL 必须让工具看得见"自己实际读到的是什么"。v27 没有这个回读, 于是
    //   "工具勾着、DLL 读到 0" 只能靠用户肉眼比对勾选框和诊断文案去猜 —— 猜不出来。
    //   ★ 必须在**任何早退之前**、并且**只读这一次**: 后面所有判定 (含闸门) 都用
    //     `seen_on` 这一份, 绝不重读 ⇒ "跳过原因"必然基于同一帧、同一份状态。
    const uint32_t seen_on = ctl->cd_on;
    ctl->cd_seen_on = seen_on;
    // ★★ v37: 模式也按同一套规矩"现读一次 + 原样回读" ★★
    //   为什么要回读而不是各读各的: 模式决定**画法** —— 工具写 2(游戏字体)、DLL 读到 1
    //   (矩形) 时画的完全是另一种东西 (也正是用户要对比的两种效果) ⇒ 必须让"两侧不一致"
    //   在界面上**看得见** (由共享头 CdModeHealth 判, 文案只有工具侧那一份)。
    //   ★ 模式未知 (不在 0..2) ⇒ **强制走矩形路径** (硬规矩), 并且在诊断里写明原因。
    const uint32_t seen_mode_raw = ctl->cd_mode;
    ctl->cd_seen_mode = seen_mode_raw;
    const uint32_t seen_mode =
        PtHook::CdModeValid(seen_mode_raw) ? seen_mode_raw : static_cast<uint32_t>(PtHook::CDMODE_RECT);
    ctl->cd_calls++;
    ctl->cd_cards_last = 0;
    // ★ 诊断字段一律**先清零**: 整帧闸门早退时不能留着上一帧的数字 ——
    //   否则界面会同时显示"跳过: 卡数组不可读"和一组看起来正常的数字, 自相矛盾。
    //   判据永远是 `cd_skip_mask` + `cd_cards_cooling/drawn`, 探针只用来补细节。
    ctl->cd_cards_cooling = 0;
    ctl->cd_skip_mask = 0;
    ctl->cd_skip_card = -1;
    ctl->cd_probe_bank = 0;
    ctl->cd_probe_count = 0;
    ctl->cd_probe_x = 0;
    ctl->cd_probe_y = 0;
    ctl->cd_probe_w = 0;
    ctl->cd_probe_h = 0;
    ctl->cd_probe_counter = 0;
    ctl->cd_probe_total = 0;
    ctl->cd_probe_trans_x = 0;
    ctl->cd_probe_trans_y = 0;
    ctl->cd_probe_fill = 0;
    // ★ v37: 原生路径的探针/耗时同样**先清零** —— 而且必须与 cd_cards_last 放在**同一处**
    //   (前面那批一起清), 否则整帧闸门早退时这些字段会**留着上一帧的值**, 界面就会
    //   同时显示"没画"和"原生耗时 123us"这种自相矛盾的东西 (v29/v30 都踩过这个坑)。
    //   ★ 初值必须是 `CDNAT_MODE_OFF` 而不是 `CDNAT_OK`: 闸门早退 (比如开关关着) 时我们
    //     **根本没试过**原生路径, 报"本帧就是它画的"就是假话。
    ctl->cd_native_state = PtHook::CDNAT_MODE_OFF;
    ctl->cd_native_font = 0;
    ctl->cd_native_draw_fn = 0;
    ctl->cd_native_sw_fn = 0;
    ctl->cd_native_w = 0;
    ctl->cd_native_h = 0;
    ctl->cd_native_x = 0;
    ctl->cd_native_y = 0;
    ctl->cd_native_ink_w = 0;
    ctl->cd_native_ink_h = 0;
    ctl->cd_native_scale_x = 0;
    ctl->cd_native_scale_y = 0;
    ctl->cd_native_us = 0;
    ctl->cd_rect_us = 0;

    if (seen_on == 0)
    {
        ctl->cd_skip_mask = PtHook::CDSKIP_BIT_OFF;
        return;
    }
    if (ctl->gfx_set_color_addr == 0 || ctl->gfx_draw_rect_addr == 0)
    {
        // ★★ 这里**故意不含 FillRect** (v27 的一条硬规矩) ★★
        //   v26 写成 `... || g_cd_fill == 0`, 而工具侧从没下发过 FillRect (Control 里
        //   没有那个字段) ⇒ 恒为 0 ⇒ 本函数每次都在这里 return ⇒ 卡面一片空白,
        //   而界面还显示"已生效" (那只说明钩点自检过了)。缺实心填充就降级成描边。
        ctl->cd_skip_mask = PtHook::CDSKIP_BIT_NO_ADDR;
        return;
    }

    // FillRect: 先做一次"下发的值变了才重查"的独立核对 (拿不到就降级, **不是**前置条件)
    const uint32_t fill_addr = ResolveFillRect(ctl);
    ctl->cd_probe_fill = fill_addr;
    ctl->cd_probe_trans_x = 0;
    ctl->cd_probe_trans_y = 0;

    // ---- 整帧闸门 (共享头里的纯函数 CdFrameGate ⇒ 离线单测覆盖的就是它) ----
    //   参数表里**没有** fill: 实心填充缺席只影响"好不好看", 不影响"画不画"。
    const uint32_t board_addr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(board));
    const int32_t board_ok =
        (ReadableBytes(board_addr, PtHook::kOffBoardSeedBank + 4) >= PtHook::kOffBoardSeedBank + 4) ? 1 : 0;
    const uint32_t bank =
        board_ok ? *reinterpret_cast<volatile const uint32_t *>(reinterpret_cast<const char *>(board) +
                                                                PtHook::kOffBoardSeedBank)
                 : 0u;
    // 整个卡数组 (含最后一张卡的最后一个字段) 必须都确认可读, 才敢逐卡读。
    //   ★ 上限 = 卡数组起点(0x28) + 步长(0x50) x 10 + 最后一张卡末尾(0x4C) = 0x394
    const uint32_t bank_need =
        static_cast<uint32_t>(PtHook::kOffBankPackets + PtHook::kOffPacketStride * PtHook::kMaxSeedPackets +
                              PtHook::kOffPktTimesUsed);
    const int32_t bank_ok = (bank >= 0x10000u && ReadableBytes(bank, bank_need) >= bank_need) ? 1 : 0;
    const int32_t count =
        bank_ok ? *reinterpret_cast<volatile const int32_t *>(
                      reinterpret_cast<const char *>(static_cast<uintptr_t>(bank)) + PtHook::kOffBankPacketCount)
                : 0;

    const uint32_t gate = PtHook::CdFrameGate(seen_on != 0 ? 1 : 0, ctl->gfx_set_color_addr,
                                              ctl->gfx_draw_rect_addr, board_ok, bank, bank_ok, count);
    if (gate != PtHook::CDSKIP_NONE)
    {
        ctl->cd_skip_mask = gate;
        return;
    }

    // ---- 只读探针 (界面上直接看得见; 出问题时不用再让用户猜) ----
    //   ★ v29: 几何探针也要先清零 —— 本帧一张都没画时不能留着上一帧的尺寸,
    //     否则诊断行会同时说"没画"和"数字 8x12", 自相矛盾。
    ctl->cd_probe_digit_w = 0;
    ctl->cd_probe_digit_h = 0;
    ctl->cd_probe_stroke = 0;
    ctl->cd_probe_panel_w = 0;
    ctl->cd_probe_panel_h = 0;
    //   ★ v30: 矩形用量探针同样先清零 (本帧没画就不能留着上一帧的矩形数)
    ctl->cd_rects_card = 0;
    ctl->cd_rects_last = 0;
    ctl->cd_rect_budget = PtHook::kCdRectBudget;
    //   ★ v31: 聚合档位探针 (1 = 1x1 逐像素 / 2 = 2x2 聚合; 0 = 本帧没画)
    ctl->cd_rects_step = 0;
    const char *sb = reinterpret_cast<const char *>(static_cast<uintptr_t>(bank));
    ctl->cd_probe_bank = static_cast<int32_t>(bank);
    ctl->cd_probe_count = count;
    // Graphics 的平移量 (GraphicsState +0x08/+0x0C) —— **只做诊断, 不参与坐标计算**:
    //   见 hook_ctl.h 里 SeedPacketRect 的说明 (FillRect/DrawRect 内部自己会加一次平移,
    //   游戏自己的 DrawSeedPacket 也是在同一个"已含平移"的空间里画的; 再加一次 = 双重平移
    //   = 数字整体跑出卡面, 这正是"画了但看不见"最典型的成因)。
    if (ReadableBytes(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g)), PtHook::kOffGfxNeed) >=
        PtHook::kOffGfxNeed)
    {
        const char *gp = reinterpret_cast<const char *>(g);
        ctl->cd_probe_trans_x =
            static_cast<int32_t>(*reinterpret_cast<volatile const float *>(gp + PtHook::kOffGfxTransX));
        ctl->cd_probe_trans_y =
            static_cast<int32_t>(*reinterpret_cast<volatile const float *>(gp + PtHook::kOffGfxTransY));
        // ★★ v41: 把绘制时 Graphics 的**缩放**也报上去 (x1000 的定点数, 只读诊断) ★★
        //   为什么必须报: `Graphics::DrawImage` 在 `mScaleX/mScaleY != 1` 时会**把字形按缩放画**
        //   (目标矩形 = ceil(srcW*mScaleX) x ceil(srcH*mScaleY), 位置还要过 mScaleOrig* 重映射)。
        //   ⇒ 实机上只要这个值不是 1000, "数字看着比离线预览小/偏"就有了直接证据;
        //     本 DLL 只读它、不改绘制方式 (改绘制方式需要实机结论, 见 HANDOVER 的硬规矩)。
        ctl->cd_native_scale_x = static_cast<int32_t>(
            *reinterpret_cast<volatile const float *>(gp + PtHook::kOffGfxScaleX) * 1000.0f);
        ctl->cd_native_scale_y = static_cast<int32_t>(
            *reinterpret_cast<volatile const float *>(gp + PtHook::kOffGfxScaleY) * 1000.0f);
    }

    const int32_t bank_x = *reinterpret_cast<volatile const int32_t *>(sb + PtHook::kOffGoX);
    const int32_t bank_y = *reinterpret_cast<volatile const int32_t *>(sb + PtHook::kOffGoY);
    const char *packets = sb + PtHook::kOffBankPackets;

    // 探针: 记下**第一张冷却卡**的原始 tick 与卡面矩形 (放不下/算不出的原因一眼能看出来)
    bool probe_taken = false;
    uint32_t drawn = 0;
    uint32_t cooling = 0;
    uint32_t skip_mask = 0;
    int32_t skip_card = -1;
    // ★★ v31: **两趟绘制** (先给每张可画的卡定档位, 再画) ★★
    //   为什么要两趟: 见共享头 CdAllocSteps 的说明 —— 逐张贪心 ("能 1x1 就 1x1") 会把
    //   预算吃光, 剩下的卡连最省的 2x2 都放不下 (10 张最坏卡时 7 张一个数字都不显示)。
    //   ⇒ 第一趟只做**读值 + 闸门 + 算两档矩形数**, 把可画的卡放进栈上的 jobs[];
    //     第二趟先用 CdAllocSteps 统一分配档位, 再真正绘制。
    //   ★ 全部在栈上 (这个 DLL 没有堆): kMaxSeedPackets = 10 张卡 x 一个小结构。
    struct CdJob
    {
        PtHook::CardRect card;
        PtHook::CdTextGeom geom;
        char chars_hi[3];
        int32_t frac;       // ★ v37: 十分位 (原生路径要把整串拼成 "35.0" 再交给游戏字体)
        int32_t n1;         // 1x1 逐像素要多少矩形
        int32_t n2;         // 2x2 聚合要多少矩形
        int32_t card_index; // 卡号 (只用于诊断 "(卡#N)")
    };
    CdJob jobs[PtHook::kMaxSeedPackets];
    int32_t job_n1[PtHook::kMaxSeedPackets];
    int32_t job_n2[PtHook::kMaxSeedPackets];
    int32_t job_steps[PtHook::kMaxSeedPackets];
    int32_t njob = 0;
    for (int32_t i = 0; i < count; i++)
    {
        const char *pk = packets + PtHook::kOffPacketStride * i;
        const int32_t ptype = *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffPktType);
        const int32_t active = *reinterpret_cast<volatile const unsigned char *>(pk + PtHook::kOffPktActive);
        const int32_t refreshing =
            *reinterpret_cast<volatile const unsigned char *>(pk + PtHook::kOffPktRefreshing);
        const int32_t counter =
            *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffPktRefreshCounter);
        const int32_t total = *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffPktRefreshTime);

        // ★ 单张卡的闸门也是共享头里的纯函数 (边界语义 = game-src SeedPacket::Update/Draw)
        const uint32_t card_gate = PtHook::CdCardGate(ptype, active, refreshing, counter, total);
        if (card_gate != PtHook::CDSKIP_NONE)
        {
            skip_mask |= card_gate;
            // ★ v29: `cd_skip_card` 只记**异常**原因的那张卡 (空槽/已就绪/不在冷却 是"正常",
            //   原来也会占掉这个卡号 ⇒ 诊断里的 "(卡#N)" 会指着一张根本没有冷却的卡, 更难看懂)
            if (skip_card < 0 && (card_gate & ~PtHook::kCdSkipNormalBits) != 0)
                skip_card = i;
            continue;
        }
        cooling++;

        // ---- 卡面矩形: ★ 照抄游戏自己的算式 (共享头 SeedPacketRect) ★ ----
        //   游戏: SeedBank::BeginDraw → Translate(bank.mX, mY); SeedPacket::BeginDraw →
        //         Translate(packet.mX, mY); SeedPacket::Draw → DrawSeedPacket(g, mOffsetX, 0)
        //   ⇒ 同一个 Graphics 空间里: x = bank.mX + packet.mX + mOffsetX, y = bank.mY + packet.mY
        //     (与 Board.cpp:3741 给 ToolTip 用的公式逐项相同 —— 那个坐标就是卡在屏幕上的位置)
        const int32_t off_x = *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffPktOffsetX);
        const int32_t pkt_x = *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffGoX);
        const int32_t pkt_y = *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffGoY);
        int32_t card_w = *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffGoWidth);
        int32_t card_h = *reinterpret_cast<volatile const int32_t *>(pk + PtHook::kOffGoHeight);
        // 几何读到异常值时**退回构造函数的常量** (SeedPacket::SeedPacket 里 mWidth=50/mHeight=70),
        //   而不是直接跳过这张卡 —— 卡面尺寸本来就是编译期常量, 读坏了不该让功能整体消失。
        if (card_w < 20 || card_w > 200 || card_h < 20 || card_h > 200)
        {
            card_w = 50; // SEED_PACKET_WIDTH
            card_h = 70;
            skip_mask |= PtHook::CDSKIP_BIT_GEOM; // 记一笔 (仍然按常量画, 只是报告里看得见)
            if (skip_card < 0)
                skip_card = i;
        }
        const PtHook::CardRect card =
            PtHook::SeedPacketRect(bank_x, bank_y, pkt_x, pkt_y, off_x, card_w, card_h);

        // 分解成数字: tenths = 124 -> 高位 1 + 低位 2 + 一位小数 -> 显示 "1.2"
        //   ★ digits[] 一律按**低位在前**存 (v30 起只用来判位数, 字模走 chars_hi[])
        const int32_t tenths = PtHook::CooldownTenths(counter, total);
        const int32_t whole = tenths / 10;
        const int32_t frac = tenths % 10;
        int32_t digits[3] = {0, 0, 0};
        int32_t ndig = 0;
        digits[ndig++] = frac;      // 小数点右边那位
        digits[ndig++] = whole % 10; // 个位 (whole<10 时这一位就是整数部分)
        if (whole >= 10)
            digits[ndig++] = whole / 10; // 十位 (只有 >=10 秒才占一格, 免得显示 "01.2")
        // ★ v30: 字模是按**字符**查表的, 所以再展开成"从左到右"的字符序列
        //   (chars_hi[0] = 最高位; 小数点**不在这里** —— 它由 CdDisplayChar 插在中间)
        char chars_hi[3] = {'0', '0', '0'};
        for (int32_t k = 0; k < ndig; k++)
            chars_hi[k] = static_cast<char>('0' + digits[ndig - 1 - k]);

        // ---- ★★ v29 几何 + v31 字模: 全部交给共享头的纯函数 ★★ ----
        //   硬规矩与证据链见 hook_ctl.h 的"v29 几何节"与 v31 的字模节; 第一趟只做三件事:
        //     ① 用**可画区** (标题条以下、游戏自己的阳光费用之上) 反算字符宽高;
        //     ② 位置闸门: 数字与底衬**外框**都必须落在卡面内, 且**绝不许**碰阳光费用
        //        与阳光计数器;
        //     ③ 算清这张卡在两档下各要多少矩形 (n1/n2) 并排队 —— 档位由 CdAllocSteps 统一分配。
        //   ★ 用户实机投诉的三条都在这里被钉死:
        //     "方块数字/粗黑边" ⇒ v31 逐像素灰度字模 + 10x14 画布 + 删掉阴影与底衬;
        //     "挡住阳光数"     ⇒ v29 的可画区 + 闸门 (数字只在标题条与费用区之间);
        //     "显示成 350."    ⇒ v31 用 CdDisplayChar 把小数点放到个位与十分位之间;
        //     "数字要上下居中" ⇒ v32 的 CdTextRect 以**可画区的垂直中心**为基准;
        //     "还要再往上"     ⇒ v33 把可画区上边界 kCdTitleBandH 从 30 收到 **9**
        //                        (实机卡片截图逐行量出来的植物图案区上沿, 见 hook_ctl.h)。
        //     "还要再下移 3px" ⇒ v34 在居中的算式上加一条**实机目测校准**偏移
        //                        `kCdTextDropY = 3` (用户自己数过像素), 数字组 24..37 → **27..40**;
        //                        禁画区边界 (9 / 54)、字号、矩形数、预算阶梯**全都没动**。
        const PtHook::CardRect digit_area = PtHook::CdDigitArea(card);
        PtHook::CdTextGeom geom;
        if (!PtHook::CdTextGeomMake(digit_area.w, digit_area.h, ndig, geom))
        {
            // 反算不出最小字号 (卡太窄/太矮) ⇒ 这张卡不画 (宁缺勿滥, 绝不溢出卡面)
            skip_mask |= PtHook::CDSKIP_BIT_PANEL;
            if (skip_card < 0)
                skip_card = i;
            continue;
        }

        const uint32_t place = PtHook::CdPlaceGate(card, geom, bank_x, bank_y);
        if (place != PtHook::CDSKIP_NONE)
        {
            // ★ "绝不盖住阳光数": 与**卡上的阳光费用**(COST) 或**阳光计数器**(SUN) 相交
            //   ⇒ 跳过这张卡; 原因位由 DLL 写回、工具翻成中文 (见 pvz.cpp 的 CdSkipText)
            //   —— 用户实机投诉的"挡住了阳光数"从此不可能再发生。
            skip_mask |= place;
            if (skip_card < 0)
                skip_card = i;
            continue;
        }

        // 两档各要多少矩形 (字模解不出来 ⇒ n1/n2 = 0 ⇒ 这张卡不进队列, 记 PANEL)
        const int32_t n1 = PtHook::CdTextBlockCount(geom, chars_hi, PtHook::CDSTEP_BLOCK1);
        const int32_t n2 = PtHook::CdTextBlockCount(geom, chars_hi, PtHook::CDSTEP_BLOCK2);
        if (n1 <= 0 || n2 <= 0)
        {
            // 字模表里没有这个字符 / 格子尺寸与字模画布不一致 ⇒ 这张卡不画
            //   (正常卡面永远是 10x14 = 字模画布; 不一致说明几何被改坏了)
            skip_mask |= PtHook::CDSKIP_BIT_PANEL;
            if (skip_card < 0)
                skip_card = i;
            continue;
        }
        if (njob < PtHook::kMaxSeedPackets)
        {
            CdJob &job = jobs[njob];
            job.card = card;
            job.geom = geom;
            job.chars_hi[0] = chars_hi[0];
            job.chars_hi[1] = chars_hi[1];
            job.chars_hi[2] = chars_hi[2];
            job.frac = frac;
            job.n1 = n1;
            job.n2 = n2;
            job.card_index = i;
            job_n1[njob] = n1;
            job_n2[njob] = n2;
            job_steps[njob] = PtHook::CDSTEP_SKIP;
            njob++;
        }

        // 探针: 第一张"本来可以画"的卡 (档位还没定, 但几何/读数已经可以报了)
        if (!probe_taken)
        {
            ctl->cd_probe_x = card.x;
            ctl->cd_probe_y = card.y;
            ctl->cd_probe_w = card.w;
            ctl->cd_probe_h = card.h;
            ctl->cd_probe_counter = counter;
            ctl->cd_probe_total = total;
            const PtHook::CardRect panel = PtHook::CdPanelRect(geom, card);
            ctl->cd_probe_digit_w = geom.dw;
            ctl->cd_probe_digit_h = geom.dh;
            ctl->cd_probe_stroke = geom.stroke;
            ctl->cd_probe_panel_w = panel.w;
            ctl->cd_probe_panel_h = panel.h;
            probe_taken = true;
        }
    }

    // ---- 第二趟: 绘制 (v37 起三选一) ----
    //   ★★ 三种模式的分派 (用户拍板的硬规矩) ★★
    //     CDMODE_RECT   (1) ⇒ 走下面那条**一个字节都没改**的矩形路径 (老方式, 默认);
    //     CDMODE_NATIVE (2) ⇒ 先走原生路径 (游戏字体); **任何自检不过就整帧降级**
    //                          到矩形路径, 并把原因写进 cd_native_state;
    //     其它值 (模式未知) ⇒ 强制矩形路径。
    //   ★ 计时: 只量**我们这一段绘制本身** (QPC 差 -> µs), 两段各自一次, 互不叠加。
    uint32_t native_state = PtHook::CDNAT_MODE_OFF;
    int32_t native_us = 0;
    int32_t rect_us = 0;
    bool drew_native = false;
    const bool want_native = (seen_mode == static_cast<uint32_t>(PtHook::CDMODE_NATIVE));

    if (want_native)
    {
        uint32_t font_state = PtHook::CDNAT_NO_ADDR;
        const uint32_t font = ResolveGameFont(ctl, font_state);
        native_state = font_state;
        ctl->cd_native_font = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_font_ptr));
        ctl->cd_native_draw_fn = g_font_draw_fn;
        ctl->cd_native_sw_fn = g_font_sw_fn;
        ctl->cd_native_h = g_font_height; // 行高是**字体**的属性, 与这一帧有没有卡无关
        if (font != 0)
        {
            // ---- 第一步: **先把每一张卡都量好、过完闸门**, 全部通过才真的画 ----
            //   为什么要先量后画: "画了一半才发现某张放不下"会让整帧的降级变得**不可逆**
            //   (已经画上去的没法撤回) ⇒ 与工程里"每一步先问够不够"同一条规矩。
            struct NativePlan
            {
                PtHook::CardRect box; // ★ v41: 这块就是**墨迹盒** (真正会画上去的像素范围)
                int32_t pen_x;        // 传给 DrawString 的 theX
                int32_t cell_top;     // 字形格顶 ⇒ 传给 DrawString 的 theY = cell_top + mAscent
                int32_t n;
                char txt[8];
            };
            NativePlan plan[PtHook::kMaxSeedPackets];
            int32_t plan_n = 0;
            bool plan_ok = true;
            uint32_t plan_fail = PtHook::CDNAT_OK;
            for (int32_t j = 0; j < njob && plan_ok; j++)
            {
                char txt[8];
                const int32_t n =
                    BuildCdText(txt, 8, jobs[j].chars_hi, jobs[j].geom.ndig, jobs[j].frac);
                if (n <= 0)
                {
                    plan_ok = false;
                    plan_fail = PtHook::CDNAT_DRAW_ERR;
                    break;
                }
                // ★ 栈上的 SSO `SexyString` (容量 15 < 16 ⇒ 零堆分配; 见共享头的字节证明)
                PtHook::CdSsoString probe;
                if (!PtHook::CdSsoInit(probe, txt, n))
                {
                    // 只有 >= 16 字节才会走到这里, 而我们的串最长 4 字节 ⇒ **不可能**。
                    // 但规矩是"不过就降级", 不是"不可能就忽略"。
                    plan_ok = false;
                    plan_fail = PtHook::CDNAT_LAYOUT_BAD;
                    break;
                }
                FontWidthArgs wa;
                wa.font = g_font_ptr;
                wa.str = &probe;
                wa.fn = g_font_sw_fn;
                const int32_t tw = FontStringWidthRaw(wa);
                const int32_t th = g_font_height;
                // ---- ★★ v41: 按**实测的墨迹盒**居中 (不再拿步进盒/行高去居中) ★★ ----
                //   `CdNativeInkPlace` 是共享头里的纯函数 (DLL 与离线单测**同一份**实现):
                //     ink      = 墨迹盒 (tw + 6 宽 x 27 高; 四个边界是从出厂 pak 量出来的)
                //     pen_x    = 传给 DrawString 的 theX
                //     cell_top = 字形格顶 ⇒ theY = cell_top + mAscent
                //   ★ 为什么必须这样: 描边层让墨迹比步进盒宽 6px、行高(43)比字高(27)高 16px,
                //     拿步进盒/行高居中 ⇒ 观感就是"贴着左边 + 字号显得小" (v40 用户反馈)。
                PtHook::CardRect box;
                int32_t pen_x = 0;
                int32_t cell_top = 0;
                if (!PtHook::CdNativeInkPlace(jobs[j].card, tw, g_font_ascent, box, pen_x, cell_top))
                {
                    plan_ok = false;
                    plan_fail = PtHook::CDNAT_FIT_BAD;
                    break;
                }
                // 闸门判的就是墨迹盒本身 (kCdNativeMargin = 0 ⇒ 不再外扩猜余量)
                const uint32_t pg = PtHook::CdNativePlaceGate(jobs[j].card, box, bank_x, bank_y);
                if (pg != PtHook::CDSKIP_NONE)
                {
                    // 会压阳光费用/计数器 ⇒ **整帧**降级 (老方式仍能把数字画出来)
                    skip_mask |= pg;
                    if (skip_card < 0)
                        skip_card = jobs[j].card_index;
                    plan_ok = false;
                    plan_fail = PtHook::CDNAT_FIT_BAD;
                    break;
                }
                if (plan_n < PtHook::kMaxSeedPackets)
                {
                    NativePlan &p = plan[plan_n++];
                    p.box = box;
                    p.pen_x = pen_x;
                    p.cell_top = cell_top;
                    p.n = n;
                    for (int32_t k = 0; k < 8; k++)
                        p.txt[k] = (k < n) ? txt[k] : '\0';
                }
                // 诊断探针: 第一张卡真的量到的宽高 + 墨迹盒 + 落点 (用户要能对比"多大/在哪")
                if (j == 0)
                {
                    ctl->cd_native_w = tw;
                    ctl->cd_native_h = th;
                    ctl->cd_native_x = box.x; // ★ v41: 报的是**墨迹盒**左上角
                    ctl->cd_native_y = box.y;
                    ctl->cd_native_ink_w = box.w;
                    ctl->cd_native_ink_h = box.h;
                }
            }
            if (plan_ok)
            {
                // ---- 第二步: 真正画 (每张卡一个栈上的 SSO 字符串 + 一次 DrawString) ----
                const int64_t t0 = QpcTicks();
                int32_t rgba[4];
                rgba[0] = 255;
                rgba[1] = 255;
                rgba[2] = 255;
                rgba[3] = 255; // ★ alpha 必须是 255 (0 会被游戏当成"不画"); 白色 = 与游戏 UI 同款观感
                // Graphics::mClipRect 在 +0x20 (16 字节) —— 与游戏自己传的**同一个**裁剪矩形
                const void *clip = reinterpret_cast<const char *>(g) + PtHook::kGfxClipRectOff;
                for (int32_t j = 0; j < plan_n; j++)
                {
                    PtHook::CdSsoString str;
                    if (!PtHook::CdSsoInit(str, plan[j].txt, plan[j].n))
                        continue;
                    FontDrawArgs da;
                    da.font = g_font_ptr;
                    da.g = g;
                    // ★★ v41: 落点 = 由**墨迹盒**反推出来的绘制锚点 (不再是步进盒左上角) ★★
                    //   theX = pen_x        (墨迹左边界 = pen_x + kFontInkLeft)
                    //   theY = cell_top + mAscent  ⇒ 墨迹顶端 = cell_top + kFontInkTop
                    da.x = plan[j].pen_x;
                    da.y = plan[j].cell_top + g_font_ascent;
                    da.str = &str;
                    da.color = rgba;
                    da.clip = clip;
                    da.fn = g_font_draw_fn;
                    FontDrawStringRaw(da);
                    drawn++;
                }
                native_us = QpcDeltaToUs(QpcTicks() - t0);
                drew_native = true;
                native_state = PtHook::CDNAT_OK;
            }
            else
            {
                native_state = plan_fail; // ★ 自检/放置不过 ⇒ 下面自动走矩形路径
            }
        }
    }

    // ---- 矩形路径 (老方式; 默认 / 自动回退目标) ----
    //   ★★ v37 的一个硬规矩: 这一段**一个字节的行为都没改** ★★
    //     它是 v31..v36 已经在用户实机上跑过的那条路; 只在"模式 = 矩形"或"原生路径降级"
    //     时执行 (两者互斥 ⇒ 同一帧只有一段耗时非 0)。
    if (!drew_native)
    {
        const int64_t t0 = QpcTicks();
        const int32_t rects_used =
            PtHook::CdAllocSteps(job_n1, job_n2, njob, PtHook::kCdRectBudget, job_steps, njob);
        for (int32_t j = 0; j < njob; j++)
        {
            const CdJob &job = jobs[j];
            const int32_t step = job_steps[j];
            if (step == PtHook::CDSTEP_SKIP)
            {
                // 这张卡整张不画 (连最省的 2x2 都放不下) ⇒ 记预算原因位
                //   (宁可少一张, 也绝不让本帧矩形数越过预算)
                skip_mask |= PtHook::CDSKIP_BIT_BUDGET;
                if (skip_card < 0)
                    skip_card = job.card_index;
                continue;
            }
            const PtHook::CardRect &card = job.card;
            const PtHook::CdTextGeom &geom = job.geom;
            int32_t rc[4];
            // 0) ★ 二分定位探针 (只有工具侧 PZTK_CD_RED=1 时才画): 覆盖**整张卡面**的大红块。
            //    连它都看不见 ⇒ 坐标/时机/FillRect 的问题; 看得见红块但没数字 ⇒ 颜色/数字的问题。
            //    ★ 它在数字**之前**画, 所以画完必须把颜色缓存清掉 (颜色已经不是数字的了):
            //      下面 last_ink 从 -1 开始 = "Graphics 里装的不是我们要的颜色"。
            if (ctl->cd_red_probe != 0)
            {
                rc[0] = card.x;
                rc[1] = card.y;
                rc[2] = card.w;
                rc[3] = card.h;
                GfxRectRed(g, rc, true);
            }
            // 1) 数字本体: 第 0..ndig 格 (最高位..个位, 小数点, 十分位) —— 顺序由 CdDisplayChar 定
            //    ★ v31: **不画阴影、不画底衬**; 每个像素一个矩形 (预算紧张时 2x2 聚合)。
            //    ★ v36: 颜色 = **白字 + A = 该像素的覆盖度** (alpha 混合, 与卡面亮度无关);
            //      回退模式 (PZTK_CD_NOALPHA=1) 才回到"不透明灰 + 抬下限"。
            int32_t used_here = 0;
            int32_t last_ink = -1; // (gray, alpha) 颜色缓存 (见 GfxRectInk 的说明)
            for (int32_t cell = 0; cell < PtHook::CdCellCount(geom); cell++)
            {
                const char c = PtHook::CdDisplayChar(geom, job.chars_hi, cell);
                const PtHook::CardRect box = PtHook::CdCellRect(geom, card, cell);
                used_here += DrawGlyphCell(g, box.x, box.y, box.w, box.h, c, step, last_ink);
            }
            ctl->cd_rects_card = used_here;
            ctl->cd_rects_step = step;
            drawn++;
        }
        rect_us = QpcDeltaToUs(QpcTicks() - t0);
        ctl->cd_rects_last = rects_used;
        ctl->cd_rect_budget = PtHook::kCdRectBudget;
    }
    else
    {
        // 原生路径画完了 ⇒ 本帧没有下发任何矩形 (诊断行要能一眼看出这一点)
        ctl->cd_rects_card = 0;
        ctl->cd_rects_step = 0;
        ctl->cd_rects_last = 0;
        ctl->cd_rect_budget = PtHook::kCdRectBudget;
    }
    ctl->cd_native_state = native_state;
    ctl->cd_native_us = native_us;
    ctl->cd_rect_us = rect_us;
    ctl->cd_cards_last = drawn;
    ctl->cd_cards_cooling = cooling;
    // 一张都没画出来 ⇒ 把"跳过原因"记全; 画了卡就只报**异常**的那些位 (正常位不必吵)
    ctl->cd_skip_mask = (drawn == 0) ? skip_mask : (skip_mask & ~PtHook::kCdSkipNormalBits);
    ctl->cd_skip_card = skip_card;
}

// 打补丁: 把钩点那条 `call Board::DrawUIBottom` 换成 `call CooldownThunk` (等长 5 字节)
//   ★ 一条铁律: **必须是 call(0xE8), 不能是 jmp(0xE9)** ——
//     跳板要靠调用者压进来的返回地址 ret 回去; 换成 jmp 就没有返回地址, ret 会弹栈上垃圾。
static bool InstallCooldownPatch()
{
    Control *ctl = g_ctl;
    if (ctl == nullptr)
        return false;

    const uint32_t site = ctl->cd_call_addr;
    if (site == 0)
    {
        ctl->cd_hook_fail = PtHook::PATCHFAIL_NO_ADDR;
        return false;
    }
    // 打之前先核对原字节: 必须是一条 call (目标是谁由工具下发, 这里只要求形状 + 目标吻合)
    unsigned char cur[5];
    memcpy(cur, reinterpret_cast<const void *>(static_cast<uintptr_t>(site)), 5);
    if (cur[0] != 0xE8)
    {
        ctl->cd_hook_fail = PtHook::PATCHFAIL_BAD_BYTES;
        g_cd_fail_addr = site; // ★ v38: 记下是哪个地址判的"永久失败" (地址变了要重试一次)
        InterlockedExchange(&g_cd_fail_perm, 1);
        return false;
    }
    memcpy(g_cd_saved, cur, 5);
    const uint32_t rel = static_cast<uint32_t>(cur[1]) | (static_cast<uint32_t>(cur[2]) << 8) |
                         (static_cast<uint32_t>(cur[3]) << 16) | (static_cast<uint32_t>(cur[4]) << 24);
    const uint32_t callee = site + 5 + static_cast<uint32_t>(static_cast<int32_t>(rel));

    // ★ 这里**必须**确认被调者就是 Board::DrawUIBottom (光"是条 call"不够)。
    //   原因: 跳板的语义是"等这次调用把卡槽画完, 然后往上写字"。如果钩点其实落在另一条
    //   call 上 (例如紧跟其后的那条 `call DrawFog`), 我们会**在卡槽之前**画字 ⇒ 文字被卡面
    //   盖住 —— 功能静默失效, 而且很难查 (v25 真踩过这个坑)。所以这里额外要求
    //   **用实时字节现算出来的目标**正好等于 Board::DrawUIBottom (PtHook::kCooldownCalleeEN);
    //   不符就**一个字节都不写**。
    //   (工具下发前也做过同样的现算 + 比对; 两处**各自独立**核对, 与"工具/DLL 互查"的
    //    既有做法一致 —— 任一处不符, 工具不下发地址、DLL 报"未生效"。)
    if (callee != PtHook::kCooldownCalleeEN)
    {
        ctl->cd_hook_fail = PtHook::PATCHFAIL_BAD_BYTES;
        g_cd_fail_addr = site; // ★ v38: 记下是哪个地址判的"永久失败" (地址变了要重试一次)
        InterlockedExchange(&g_cd_fail_perm, 1);
        return false;
    }

    unsigned char patch[5];
    PtHook::EncodeCallPatch(site, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&CooldownThunk)), patch);
    // ★ 写的是"每帧都会执行"的热点 ⇒ 必须挂起其它线程后再写
    if (!WriteCodeSafe(site, patch, 5))
    {
        ctl->cd_hook_fail = PtHook::PATCHFAIL_PROTECT;
        return false;
    }
    // 回读自检: 不一致就立刻还原 (宁可不画, 也不能把游戏代码改坏)
    unsigned char chk[5];
    memcpy(chk, reinterpret_cast<const void *>(static_cast<uintptr_t>(site)), 5);
    bool ok = true;
    for (int i = 0; i < 5; i++)
        if (chk[i] != patch[i])
            ok = false;
    if (!ok)
    {
        WriteCodeSafe(site, g_cd_saved, 5);
        ctl->cd_hook_fail = PtHook::PATCHFAIL_VERIFY;
        g_cd_fail_addr = site; // ★ v38: 同上
        InterlockedExchange(&g_cd_fail_perm, 1);
        return false;
    }

    g_cd_site = site;
    g_cd_uib_ptr = reinterpret_cast<void *>(static_cast<uintptr_t>(callee)); // 重放原调用用**实时现算**的目标
    InterlockedExchange(&g_cd_patched, 1);
    ctl->cd_hook_ok = 1;
    ctl->cd_hook_fail = PtHook::PATCHFAIL_NONE;
    g_cd_fail_addr = 0; // ★ v38: 装上了 ⇒ 清掉"永久失败"记的地址
    return true;
}

// 还原冷却钩点 (★ DLL 卸载前**必须**做: 否则游戏会跳进已经卸载的 DLL 代码里)
static void RemoveCooldownPatch()
{
    if (g_cd_patched == 0 || g_cd_site == 0)
        return;
    InterlockedExchange(&g_cd_patched, 0);
    WriteCodeSafe(g_cd_site, g_cd_saved, 5);
    if (g_ctl != nullptr)
        g_ctl->cd_hook_ok = 0;
    g_cd_site = 0;
}

// ★ 关键: MinHook 装钩子时会**挂起其它线程**去改代码, 而 DllMain 里持有加载器锁 ——
//   在 DllMain 里调用 MH_EnableHook 会死锁 (第一次实测就撞上了: 工具显示"已注入",
//   但 hook_ok 一直是 0、调用 0 次, 因为 LoadLibraryW 其实卡住了)。
//   所以 DllMain 只做映射 + 建线程, 真正的装钩子放在这个线程里做。
static DWORD WINAPI InitThread(LPVOID)
{
    // 等 DllMain 返回、加载器锁释放
    Sleep(50);

    if (g_ctl == nullptr)
        return 0;

    // ★ v36: 读一次墨色模式的环境变量开关 (PZTK_CD_NOALPHA=1 ⇒ 不透明回退路径)。
    //   只在这里读一次 (与"每次绘制都读环境"相比省掉一个 kernel32 调用; 代价是改完
    //   环境变量要**重开游戏**才生效 —— 这条写进文档了)。
    ReadInkModeFromEnv();

    g_ctl->init_step = 1; // 准备初始化 MinHook
    if (MH_Initialize() != MH_OK)
    {
        g_ctl->hook_fail = PtHook::FAIL_INIT;
        return 0;
    }

    const uint32_t target = g_ctl->hook_addr;
    if (target == 0)
    {
        g_ctl->hook_fail = PtHook::FAIL_NO_ADDR;
        MH_Uninitialize();
        return 0;
    }

    g_ctl->init_step = 2; // 建钩子 (含指令搬移)
    if (MH_CreateHook(reinterpret_cast<LPVOID>(static_cast<uintptr_t>(target)),
                      reinterpret_cast<LPVOID>(&HookThunk),
                      reinterpret_cast<LPVOID *>(&g_hook_trampoline)) != MH_OK)
    {
        g_ctl->hook_fail = PtHook::FAIL_CREATE;
        MH_Uninitialize();
        return 0;
    }

    g_ctl->init_step = 3; // 启用钩子 (会短暂挂起其它线程)
    if (MH_EnableHook(reinterpret_cast<LPVOID>(static_cast<uintptr_t>(target))) != MH_OK)
    {
        g_ctl->hook_fail = PtHook::FAIL_ENABLE;
        g_hook_trampoline = nullptr;
        MH_Uninitialize();
        return 0;
    }

    g_target = reinterpret_cast<void *>(static_cast<uintptr_t>(target));
    g_ctl->init_step = 4; // 完成
    g_ctl->hook_ok = 1;
    g_ctl->hook_fail = PtHook::FAIL_NONE;
    g_ctl->dll_alive++;

    // ---- 卡片冷却数字的钩点 (打补丁; 工具可能还没下发地址, 那就留给下面的循环重试) ----
    //   ★★ v40: 这里原来还打一条"碰撞箱绘制钩子" (0x4172BD) —— 碰撞箱整条链已删除,
    //     现在这个函数只剩冷却数字这一条补丁。
    g_ctl->init_step = 5;
    InstallCooldownPatch();

    // 之后继续当"监视线程": 等工具要求卸载, 或者**工具进程真的没了**
    //   ★★ v38: 判据从"心跳新鲜度"换成"进程存活" ★★
    //     为什么必须换: 拖动工具主窗口时 Windows 的模态移动循环占住工具主循环 ⇒ 心跳停;
    //     旧代码据此把钩子全撤掉 ⇒ 用户报的"一拖就中断 / 偶尔会中断"(见 QpcTick 上面那段)。
    //     现在: `OpenProcess(SYNCHRONIZE)` 拿工具的进程句柄, 每轮 `WaitForSingleObject(h, 20)`
    //     **同时**当"睡 20ms"和"工具还在吗"用 —— 句柄被 signal 只可能是进程真的结束了
    //     (包括被强杀/崩溃), 与"刷新快慢"完全无关。
    //   ★ 拿不到句柄 (或工具还没写 tool_pid) ⇒ **一律当作还活着** (宁可不卸载, 也绝不误撤)。
    //   ★ 判据本身在共享头 `CdUnloadDecision` (纯函数, 离线单测钉住"心跳停多少次都不卸载")。
    HANDLE tool_proc = nullptr;
    uint32_t tool_pid_opened = 0;
    for (;;)
    {
        Control *ctl = g_ctl;
        if (ctl == nullptr)
            break;

        // ---- 工具进程句柄: 第一次拿到 tool_pid 时开一次, 之后一直持有 ----
        //   为什么要持有而不是每轮开关: 句柄本身把"那个进程对象"钉住 ⇒ 即使 PID 被系统
        //   复用给别的进程, 我们等到的仍然是**原来那个**工具的结束信号 (不会被复用骗过)。
        if (tool_proc == nullptr && ctl->tool_pid != 0 && ctl->tool_pid != tool_pid_opened)
        {
            tool_pid_opened = ctl->tool_pid;
            tool_proc = OpenProcess(SYNCHRONIZE, FALSE, tool_pid_opened);
            // 打不开 (权限/已退出) ⇒ 保持 nullptr = "当作还活着", 绝不因此卸载
        }

        bool tool_alive = true;
        if (tool_proc != nullptr)
            tool_alive = (WaitForSingleObject(tool_proc, 20) == WAIT_TIMEOUT); // 20ms = 本轮睡眠
        else
            Sleep(20);

        if (PtHook::CdUnloadDecision(tool_alive, ctl->want_unload != 0) != PtHook::CDUNLOAD_NONE)
            break;

        // 卡片冷却钩子: 工具是"每帧下发地址"的, 首次注入时可能还没写到, 这里补挂
        if (g_cd_patched == 0 && ctl->cd_call_addr != 0)
        {
            if (g_cd_fail_perm != 0 && g_cd_fail_addr != ctl->cd_call_addr)
                InterlockedExchange(&g_cd_fail_perm, 0);
            if (g_cd_fail_perm == 0)
                InstallCooldownPatch();
        }
    }
    if (tool_proc != nullptr)
        CloseHandle(tool_proc);
    InterlockedExchange(&g_unloading, 1);
    Sleep(60);
    RemoveCooldownPatch(); // ★ 必须在下钩子/卸载之前还原: 否则游戏会跳进已卸载的 DLL
    RemoveHook();
    MH_Uninitialize();
    if (g_ctl != nullptr)
    {
        g_ctl->hook_ok = 0;
        g_ctl->dll_build = 0;
    }
    // ★ 自己把 DLL 从游戏里卸掉 (工具不需要知道远程 HMODULE, 残留的旧 DLL 也能自己清干净)
    FreeLibraryAndExitThread(g_module, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        g_module = hModule; // 卸载时要用 (FreeLibraryAndExitThread)

        g_channel = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, PtHook::kMapName);
        if (g_channel == nullptr)
            return FALSE; // 工具必须先建好共享内存, 否则不加载 (避免"裸注入"留在游戏里)
        g_ctl = static_cast<Control *>(MapViewOfFile(g_channel, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Control)));
        if (g_ctl == nullptr)
        {
            CloseHandle(g_channel);
            g_channel = nullptr;
            return FALSE;
        }
        if (g_ctl->magic != PtHook::kMagic || g_ctl->version != PtHook::kVersion ||
            g_ctl->struct_size != sizeof(Control))
        {
            g_ctl->hook_fail = PtHook::FAIL_BAD_MAGIC;
            UnmapViewOfFile(g_ctl);
            g_ctl = nullptr;
            CloseHandle(g_channel);
            g_channel = nullptr;
            return FALSE;
        }

        // 一挂上就报告自己的构建号 —— 万一注入的是旧文件, 工具能立刻发现
        g_ctl->dll_build = PtHook::kDllBuild;

        // 装钩子交给独立线程 (见 InitThread 上面的说明)
        HANDLE th = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (th == nullptr)
        {
            g_ctl->hook_fail = PtHook::FAIL_INIT;
            return FALSE;
        }
        CloseHandle(th);
        return TRUE;
    }

    if (reason == DLL_PROCESS_DETACH)
    {
        InterlockedExchange(&g_unloading, 1);
        RemoveCooldownPatch(); // ★ 先还原游戏代码 (否则游戏会跳进已卸载的 DLL)
        RemoveHook();
        MH_Uninitialize();
        if (g_ctl != nullptr)
        {
            g_ctl->hook_ok = 0;
            UnmapViewOfFile(g_ctl);
            g_ctl = nullptr;
        }
        if (g_channel != nullptr)
        {
            CloseHandle(g_channel);
            g_channel = nullptr;
        }
    }
    return TRUE;
}

// 供工具查询 (可选)
extern "C" __declspec(dllexport) uint32_t PztkHookVersion()
{
    return PtHook::kVersion;
}
