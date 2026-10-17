
#pragma once

#include "../hook/hook_ctl.h"

#include <Windows.h>

#include <array>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "code.h"
#include "data.h"
#include "lineup.h"
#include "process.h"

namespace Pt
{

typedef void (*cb_func)(void *, int);

// ---------------------------------------------------------------------------
// 僵尸移速分类
//
// 游戏里僵尸的移速 mVelX 由 Zombie::PickRandomSpeed() 决定, 而它在每次
// Zombie::StartWalkAnim() 时都会被调用, 所以取值完全由"类型 + 相位"决定。
//
// 分类原则:
//   * 游戏里是随机区间的 -> 列成可调整项, 每项可单独启用并固定;
//   * 游戏里是写死常量的 -> 列在表尾做说明, 不参与调整;
//   * 普僵 / 路障 / 铁桶 / 铁门 / 鸭子 / 各种植物僵尸 (只有一种状态, 区间相同)
//     合并成 ZSC_NORMAL;
//   * 有两种以上状态的 (撑杆/读报/雪橇/海豚/气球/矿工/跳跳/扶梯) 各状态分行;
//   * 橄榄球 / 潜水 / 小丑原本同区间, 按需求拆成独立的行。
//   * 潜水僵尸的"游泳"段是**固定 0.20** (入水那一刻被写死), 与岸上的随机区间不同,
//     所以单独列成表尾的 ZSC_SNORKEL_SWIM, 避免把水里的速度也按岸上值改掉。
//
// 判定顺序必须与 PickRandomSpeed 的 if-else 链一致: 先匹配先生效。
// 位置固定或速度由跳跃距离决定的状态返回 ZSC_SKIP。
// ---------------------------------------------------------------------------
enum ZombieSpeedClass
{
    // ---- 可调整 (游戏里是随机区间), 数组顺序即界面显示顺序 ----
    ZSC_NORMAL = 0,    // 普僵/路障/铁桶/铁门/鸭子/植物僵尸(倭瓜头除外)
    ZSC_POLE_VAULTING, // 撑杆(持杆)
    ZSC_POLE_AFTER,    // 撑杆(脱杆)
    ZSC_NEWSPAPER_READ,// 读报(看报)
    ZSC_NEWSPAPER_MAD, // 读报(暴怒)
    ZSC_FOOTBALL,      // 橄榄球
    ZSC_SNORKEL,       // 潜水（岸上） —— 水里那一段是固定 0.20, 见下面的 ZSC_SNORKEL_SWIM
    ZSC_ZAMBONI,       // 冰车
    ZSC_BOBSLED_OFF,   // 雪橇小队(下车)
    ZSC_DOLPHIN_RIDE,  // 海豚骑士(骑行)
    ZSC_DOLPHIN_SWIM,  // 海豚骑士(游泳)
    ZSC_JACK,          // 小丑
    ZSC_BALLOON,       // 气球(飞行 / 行走)
    ZSC_DIGGER_TUNNEL, // 矿工(未出土)
    ZSC_LADDER_CARRY,  // 扶梯(持梯)
    ZSC_LADDER_AFTER,  // 扶梯(脱梯)
    ZSC_CATAPULT,      // 投篮车 (只有一种状态; 投篮不影响速度值)
    ZSC_GARGANTUAR,    // 巨人 / 红眼
    ZSC_IMP_NORMAL,    // 小鬼(一般)
    ZSC_SQUASH_HEAD,   // 倭瓜头
    ZSC_ADJUSTABLE_COUNT,

    // ---- 游戏固定值, 只做说明, 不参与调整 ----
    ZSC_DANCER = ZSC_ADJUSTABLE_COUNT, // 舞王 / 伴舞 / 旗帜 / 跳跳(全程固定 0.45, 见下)
    ZSC_YETI,                          // 雪人(普通 / 暴走)
    ZSC_IMP_IZ,                        // 小鬼(IZ)
    ZSC_BOBSLED_ON,                    // 雪橇小队(乘车)
    ZSC_DIGGER_WALK,                   // 矿工(出土后, 一般 / IZ)
    ZSC_SNORKEL_SWIM,                  // 潜水（游泳）: 入水那一刻被写死 0.20
    ZSC_COUNT
};

// 不统计也不调整
#define ZSC_SKIP (-1)

// 每个分类的名称和游戏内合法区间
struct ZombieSpeedClassInfo
{
    const char *name_zh;
    const char *name_en;
    float min;           // 合法区间下限
    float max;           // 合法区间上限
    float default_value; // 游戏原本的取值 (固定值本身就是它, 随机区间取其中一个合法端点)
    // 固定值说明区用的自定义文本 (例如"0.12 / 0.23"); 为空则按 default_value 显示
    const char *fixed_text = nullptr;
};

extern const ZombieSpeedClassInfo zombie_speed_classes[ZSC_COUNT];

// 一个僵尸槽位的当前状态, 用于"僵尸移速"列表
struct ZombieSpeedInfo
{
    int index;    // 僵尸编号 (对象数组下标)
    int type;     // 僵尸种类
    int klass;    // 所属速度分类
    float speed;  // 当前实际移速
    bool pinned;  // 该槽位的移速是否正被固定
};

// 根据僵尸种类 / 阶段判断速度分类 (复刻 PickRandomSpeed 的分支顺序)
int ZombieSpeedClassOf(int zombie_type, int zombie_phase, bool iz_level);
// 僵尸 / 植物的紧凑种类名 (二字或简称), 供界面表格使用
// 名字表本体 (定义在 names.cpp)
extern const char *zombies_zh[];
extern const char *zombies[];
extern const char *plants_zh[];
extern const char *plants[];

const char *ZombieTypeName(int type);
const char *PlantTypeName(int type);

// ---------------------------------------------------------------------------
// 植物攻速分类
//
// 植物的攻速就是"两发之间的间隔帧数"。游戏在 Plant::UpdateShooter() 里这样重置:
//     mLaunchCounter = mLaunchRate - Sexy::Rand(15);
// 其中 mLaunchRate 是定义表里写死的常量 (150 / 200 / 300 / 2500), 属于固定值,
// 不去改它; 真正随机的是每次射击后减掉的那个 Rand(15), 它让间隔在
// [mLaunchRate-14, mLaunchRate] 之间抖动 —— 这就是要固定的随机因素。
// 做法: 把 mLaunchCounter 固定成用户设定的间隔, 消除每发抖动。
//
// 界面上按"每秒攻击次数"显示: 次数 = 100 / 间隔帧数 (游戏内部 100 帧每秒)。
//
// 地刺和大嘴花不走 mLaunchRate, 攻击节奏由动画倒计时写死 (地刺 100 帧一轮、
// 大嘴花咬 70 帧 + 咀嚼 4000 帧), 没有可长期写入的字段, 所以只列出来说明。
// ---------------------------------------------------------------------------
enum PlantSpeedClass
{
    PSC_PEA = 0,     // 豌豆 / 寒冰 / 三线 / 机枪  (除双发、裂荚)
    PSC_REPEATER,    // 双发 / 裂荚  (有"补发检查点", 反应时间上限比单发类低)
    PSC_CATTAIL,     // 猫尾草  (补发检查点是 50, 上限更低)
    PSC_MISC,        // 大喷菇 / 仙人掌
    PSC_STARFRUIT,   // 杨桃  (只参与攻击间隔; 索敌太特殊, 首发延迟不支持)
    PSC_SPORE,       // 小喷菇 / 海蘑菇 / 胆小菇
    PSC_GLOOM,       // 忧郁菇
    PSC_PULT,        // 卷心菜 / 玉米 / 西瓜 / 冰瓜
    PSC_SUN,         // 向日葵 / 双子向日葵 等的产速
    PSC_ADJUSTABLE_COUNT,

    // 下面两项不可调整, 只做说明
    PSC_SPIKEWEED = PSC_ADJUSTABLE_COUNT, // 地刺 / 钢地刺
    PSC_CHOMPER,                          // 大嘴花
    PSC_COUNT
};

struct PlantSpeedClassInfo
{
    const char *name_zh;
    const char *name_en;
    int counter_min; // 游戏随机出的最短间隔 (帧); 0 表示该项不参与调整
    int counter_max; // 游戏随机出的最长间隔 (帧)
    int counter_default;
    // 首发延迟 (前摇) 的区间, 单位为帧。
    // 游戏在 PlantInitialize() 里掷出第一次攻击的等待时间:
    //     发射类   mLaunchCounter = RandRangeInt(0, mLaunchRate)
    //     生产类   mLaunchCounter = RandRangeInt(300, mLaunchRate / 2)
    // 所以这里就是各自的合法区间, 0/0 表示该类不参与。
    int first_min;
    int first_max;
};

extern const PlantSpeedClassInfo plant_speed_classes[PSC_COUNT];

// 首发延迟的诊断显示: 植物窗口的"诊断"列、写入日志面板、以及
// pvztoolkit_windup_log.txt。0 = 隐藏 (默认), 1 = 显示。
// 第 5 步临时开回 1 用于排查"子弹错步"。
#ifndef PTK_WINDUP_DEBUG
#define PTK_WINDUP_DEBUG 0
#endif

// 一次首发延迟写入的记录 (诊断用)
struct WindupWriteLog
{
    int index;     // 植物编号
    int seed_type; // 种类
    int reason;    // 1 新种下 2 进射程 3 重试 4 改设置
    int before;    // 写入前的 mLaunchCounter
    int after;     // 写入的值
};

// 首发延迟诊断日志的每槽位状态 (写 txt 用)
struct WindupDumpState
{
    int arm_frame = -1;   // 上一次"进射程"写入的帧号
    int first_fire = -1;  // 之后第一次射击的帧号
    int volley = 0;       // 首轮弹数
    int written = 0;      // 写进去的值
    bool reported = true; // 这一次是否已经结算过
};

// 一个植物槽位的当前状态, 用于"植物攻速"列表
struct PlantSpeedInfo
{
    int index;     // 植物编号 (对象数组下标)
    int seed_type; // 植物种类
    int klass;     // 所属攻速分类
    int interval;  // 当前攻击间隔 (帧); 越小越快
    bool pinned;   // 该槽位是否正被固定
    // 诊断用: 直接显示内存里的真实状态
    int launch;    // 当前 mLaunchCounter
    int shooting;  // 当前 mShootingCounter
    int wrote;     // 本帧我们写入的值; -1 表示没写
    int reason;    // 写入原因: 1 新种下 2 进射程 3 重试 4 改设置
};

int PlantSpeedClassOf(int seed_type);

class PvZ : public Process, public Code, public Data
{
  public:
    PvZ();
    ~PvZ();

    // 安全地注入
    void asm_code_inject();

    // 应用 hack
    template <typename T, size_t size>
    void enable_hack(HACK<T, size>, bool);

    // 应用 hacks
    template <typename T, size_t size>
    void enable_hack(std::vector<HACK<T, size>>, bool);

#ifdef _DEBUG
    template <typename T, size_t size>
    void check_hack(HACK<T, size>);

    template <typename T, size_t size>
    void check_hack(std::vector<HACK<T, size>>);

    void check_all_hacks();
#endif

    // 设置查找游戏的回调函数
    void callback(cb_func, void *);

    // 查找植物大战僵尸, 找到了支持的版本返回真
    bool FindPvZ();

    // 游戏是否正常开启
    // 每次修改前都要检查
    bool GameOn();
    // 冰车无痕当前用的哪种方式 (界面提示用: 真无痕 / 退回旧补丁 / 已关闭)
    const std::string &IceTrailMsg() const { return this->ice_trail_msg; }
    // 游戏进程号 —— 用来识别"游戏关掉又重开" (pid 一变就是新的一局, 需要重新回显/自愈)
    uint32_t GamePid() const { return static_cast<uint32_t>(this->pid); }

    // ★★ 2026-10-13 (v40): 「碰撞箱」那条功能**整条已删除** ★★
    //   用户拍板 (原话意思): **越改越乱、不实用、测试用不到、常规关卡里看着难看** ⇒
    //   界面入口、工具侧采集/下发链 (采集/下发/绘制状态/几何诊断这几个成员函数与它们
    //   专用的结构体 `HitboxRect` / `ZombieHitRect` / `ZombieAttackRect` / `BobsledBox` /
    //   `BobsledMergeOut` 以及那几个纯函数)、DLL 侧专用钩点/跳板、以及"透明叠加窗"
    //   降级路径**全部删掉**。
    //   ★ **保留** (其它功能在用, 本轮一个字节都没动):
    //     * `ZombieBodyX()` / `PlantAttackX()` —— 出怪、移速、首发延迟的**攻击窗口判定**在用;
    //     * `ZombieSpeedClassOf()` 与 `ZombieType` / `ZombiePhase` 两张数值表 —— 移速/出怪/
    //       魅惑/冻结在用;
    //     * 卡片冷却数字整条链 (它用的是**另一个**钩点 `0x4172E7`)。

    // ---- 卡片冷却时间显示 (2026-10-10, kVersion 25) ----
    //   同样由 DLL 在**游戏自己那一帧**里画, 但钩点要比卡槽**更晚** (文字必须盖在卡面上):
    //   用 `Board::DrawGameObjects` 渲染列表最后一条 case 的那条 `call DrawFadeOut`。
    //   工具只负责定位 + 下发地址 + 开关, 数据 (冷却计数) 由 DLL 从 Board/SeedBank 自己读。
    //   返回 false = 钩点没定位到 / 没有共享通道 ⇒ 界面显示"未生效"。
    //   ★★ v37: 多一个 `mode` 入参 (PtHook::CdDrawMode: 0=关闭 1=矩形 2=游戏字体) ★★
    //     它每帧被**原样**写进 `Control::cd_mode`, 由 DLL 回读 (`cd_seen_mode`) 并比对;
    //     地址/布局自检不过时 DLL 会**自己降级成矩形**并把原因写回, 所以这里只负责
    //     "把用户的选择如实送到", 不做任何猜测。
    bool PublishCooldown(bool on, uint32_t mode);
    // DLL 侧冷却钩子是否已挂上
    bool CooldownHookOk();
    // DLL 侧冷却绘制状态文字 (tooltip / 诊断)
    std::string CooldownDrawStatus();
    // ★ v27: 一行版诊断 (**常驻显示在界面上**, 不必开 PZTK_DEV) ——
    //   "本帧画了 N 张 | 冷却中 M 张 | 跳过: xxx"。用户把这一行发回来就能定位
    //   "界面说已生效、卡面却什么都没有" 这类问题。
    std::string CooldownDrawBrief();
    // ★ v29: **一行版的"数字几何"** (也常驻显示) —— "单个数字 8x12 段粗 2 | 底衬 34x14"。
    //   为什么要单独常驻: 用户 v28 的投诉就是"**好丑 / 看不清**", 那是**尺寸**问题;
    //   而按项目惯例 (HANDOVER §8.1) "重要的状态别只挂 tooltip" —— 用户不会去悬停找。
    //   有了这一行, "跑的是不是 v29 的几何" 一眼可见 (v28 会打出 9x14 / 段粗 3 / 底衬 42x20)。
    std::string CooldownDrawGeom();

    // ★★ v38: 一行"注入/绘制**中断原因**" ★★
    //   用户实机反馈 "注入不稳定, 偶尔会中断, 拖主界面也会中断" ⇒ 必须能一眼区分:
    //     "钩点还在但本帧没画" vs "钩点被撤 / 从未注入" vs "补丁打不上 / 地址没下发"。
    //   判断在共享头纯函数 `PtHook::CdInterruptKindOf()` 里 (离线单测钉住),
    //   中文文案在 pvz.cpp 里 (本头文件里的中文不会进字形表)。
    //   `injected_ever` 用成员 `hook_seen_alive` —— 它只记"本游戏进程里 DLL 报过 hook_ok"。
    std::string HookInterruptText();

    // 游戏路径
    std::string GamePath();

    // 游戏模式
    int GameMode();

    // 游戏界面
    int GameUI();

    // 游戏场地
    int GetScene();
    void SetScene(int, bool);

    // 场地行数
    int GetRowCount();

  protected:
    // 回调函数指针和窗口指针
    cb_func cb_find_result;
    void *window;

    // "固定随机数值" 用的状态
    std::map<int, int> plant_original_launch_rate; // 植物种类 -> 原始 mLaunchRate
    std::vector<int> plant_last_launch_counter;    // 每个植物槽位上次的 mLaunchCounter
    std::vector<int> plant_last_interval;          // 每个植物槽位最近一次的攻击间隔 (帧)
    std::vector<unsigned char> plant_was_dead;     // 每个槽位上次是否为空 (用于识别新种下的植物)
    std::vector<int> plant_last_seed;              // 每个槽位上次的植物种类 (换种类也算新种下)
    std::vector<int> plant_last_shooting;          // 每个槽位上次的 mShootingCounter
    // 每个槽位上次写进去的值 (-1 = 没写过), 用于校验写入是否被游戏覆盖
    std::vector<int> windup_written;
    // 每个槽位本周期内是否开过火 (裂荚前头在 == 25 检查点开火, 不能只看归零那一下)
    std::vector<unsigned char> plant_fired_cycle;
    // 分类级的"进射程"待写入与"整类是否都安全"标记。
    // 边沿判定必须按分类做: "僵尸进射程"本来就是整类共享的外部事件, 若逐棵判断
    // 安全条件, 不安全的那些会被推迟到后面的帧才写, 于是同类植物从此错开相位
    // (实测 #3 比其它五棵晚 11 帧, 之后再也对不齐)。
    std::array<bool, PSC_COUNT> windup_pending;
    std::array<bool, PSC_COUNT> windup_pending_prev; // 上一拍的待写入标记
    std::array<bool, PSC_COUNT> windup_blocked;      // 本拍发现不安全 -> 下一拍整类再等
    std::array<bool, PSC_COUNT> windup_blocked_prev;
    // 每个槽位上一次写入所在的帧号 (用于下一帧回读校验, -1000 = 无)
    std::vector<int> windup_arm_tick;
    std::vector<unsigned char> plant_had_target;   // 每个槽位上一帧射程内是否有僵尸
    // 每棵植物独立的首发延迟状态: 1 = IDLE(钉住), 0 = ACTIVE(脱手)
    std::vector<unsigned char> plant_windup_idle;
    // 连续多少帧没有目标 (去抖: 目标消失要连续若干帧才算真的离开)
    std::vector<int> windup_no_target;
    // 首发延迟: 记录上一次应用到场上的值, 设置一变就重写一次 (老植物也要能改)
    std::array<int, PSC_COUNT> windup_applied;
    std::array<bool, PSC_COUNT> windup_reapply;
    // 最近若干次首发延迟写入 (最新的在前), 用于诊断
    std::vector<WindupWriteLog> windup_log;

    // ---- 2026-10-07 新增功能的用户选择与状态 ----
    bool hack_kill_home = false;             // 无视僵尸进家 (阈值见 pvz.cpp 的常量)
    float hack_zombie_hp_mul = 1.0f;         // 僵尸血量倍数
    int hack_maid_mode = 0;                  // 女仆秘籍模式 (0 关 1 保持前进 2 跳舞 3 召唤伴舞)
    bool hack_maid_patched = false;          // 是否用的是"代码补丁"模式 (否则退回钉时钟)
    bool hack_no_ice_trail = false;          // 冰车无痕 (真无痕: 把游戏写冰痕计时那条指令改成写 0)
    uintptr_t ice_timer_store_addr = 0;      // 被打补丁的"立即数地址" (用于还原)
    std::string ice_trail_msg;               // 冰车无痕当前用的哪种方式 (界面提示用)
    int ice_timer_max_seen = 0;              // 打补丁后观察到的 mIceTimer 最大值 (验证补丁是否命中)
    uintptr_t hack_maid_site = 0;            // 补丁点 (GetDancerFrame 里读时钟的指令)
    // ---- 多倍出怪 (点数 ×N) 的状态 ----
    bool hack_wave_mul = false;              // 是否已打上"预算 ×N"补丁
    int hack_wave_mul_n = 1;                 // 当前倍率 (1..24; 1 = 等于关闭)
    uintptr_t wave_mul_site = 0;             // 被打补丁的 6 字节起点 (补满循环入口)
    uintptr_t wave_mul_cave = 0;             // 代码洞地址 (只分配不释放, 见 pvz.cpp)
    uint32_t wave_mul_pid = 0;               // 打补丁时游戏的 pid (游戏重开后地址失效, 必须重来)
    std::string wave_mul_msg;                // 多倍出怪的一行状态 (界面提示用)
    // ---- 高级暂停 的状态 ----
    std::string adv_pause_msg;                // 界面提示用的一行状态
    uintptr_t adv_pause_page = 0;            // 代码洞页 (标志字节 + 两个桩; 只分配不释放)
    uint32_t adv_pause_pid = 0;              // 打补丁时游戏的 pid (重开后地址失效, 必须重来)
    bool hack_adv_pause = false;             // 是否已装上高级暂停的两处补丁
    uintptr_t adv_pause_call_site = 0;       // 空格分支里的 call DoPauseDialog (被打成 jmp 代码洞1)
    uintptr_t adv_pause_world_site = 0;      // Board::Update 世界段起点 (被打成 jmp 代码洞2)
    // 扫出"补满循环入口"那 6 字节的地址 (0 = 未命中/本版不支持)
    uintptr_t FindWaveMulSite();
    // 智慧树受门禁的指令: 临时把"智慧树模式最高纪录"顶上去, 发完按键再还原
    bool hack_wisdom_record_dirty = false;
    int hack_wisdom_record_old = 0;
    int hack_wisdom_record_restore = 0;      // 倒计时 (帧), 减到 0 就还原
    std::vector<unsigned char> zombie_hp_scaled; // 每个槽位是否已按倍数改过
    std::vector<float> zombie_hp_base;           // 每个槽位原始 mBodyMaxHealth

    // 冻结/魅惑共用的遍历 (freeze/mind 至少要有一个为真), 返回处理了几只
    int ZombieApply(bool freeze, bool mind);

    // 代码补丁记录 (打补丁前备份, 用于还原)
    struct CodePatchRecord
    {
        uintptr_t addr = 0;
        std::vector<unsigned char> original;
        std::vector<unsigned char> patched;
    };
    std::vector<CodePatchRecord> code_patches;

    // 植物字段探测的状态
    std::vector<unsigned char> probe_a, probe_b;
    uintptr_t probe_addr = 0;
    int probe_slot = 0;
    int probe_countdown = 0;
    bool probe_a_valid = false;
    std::string probe_report;

    // 方案 B: 注入 DLL 的状态
    HANDLE hook_map = nullptr;             // 共享内存句柄 (工具侧)
    PtHook::Control *hook_ctl = nullptr;   // 映射到本进程的控制块
    bool hook_injected = false;
    // ★★ v38: 这个游戏进程里 DLL **曾经**报过 hook_ok / 被调用过吗 ★★
    //   只用来回答"现在是'从未注入'还是'钩点被撤了'" (前者是刚启动, 后者才是断链)。
    //   在 FrameHacks 里每帧看一眼; 游戏断开/换进程时由 MarkHookDetached 清零。
    bool hook_seen_alive = false;
    // ★ v28: 工具侧**最后一次真正下发**给 DLL 的冷却开关状态 (PublishCooldown 的入参)。
    //   诊断行拿它显示"工具=开/关" —— 它是"我们到底写了什么"的真值, 和勾选框、和
    //   写进控制块的 cd_on 三者永远一致 (勾选框 -> 每帧 PublishCooldown -> 这里 + 控制块)。
    bool cd_published_on = false;
    // ★ v37: 工具侧**最后一次真正下发**的模式 (PublishCooldown 的第二个入参)。
    //   与 cd_published_on 同一套理由: 诊断行要显示"我们到底写了哪个模式",
    //   而 DLL 回读的那一份 (`cd_seen_mode`) 与它不一致时就是铁证。
    uint32_t cd_published_mode = 0;
    uint32_t hook_remote_module = 0;       // 目标进程里 DLL 的 HMODULE
    uintptr_t shooter_addr = 0;            // Plant::UpdateShooter 地址
    bool shooter_used_hit = false;         // true = 挂的是命中点本身 (没找到可信入口)
    std::string hook_dll_path_a;           // DLL 的窄字符路径 (状态行显示/探测用)
    std::string LastHookDllPathA() const { return this->hook_dll_path_a; }
    // 事件日志的落盘状态
    std::string hook_log_path_a;           // 日志文件路径 (窄字符)
    std::FILE *hook_log_file = nullptr;    // 常开, 退出时关
    uint32_t hook_log_first_tick = 0;      // 首条条目的 QPC, 用来算相对时间
    uint32_t hook_log_prev_tick = 0;       // 上一条条目的 QPC (算间隔用)
    uint64_t hook_log_qpc_freq = 0;        // QPC 频率
    std::wstring hook_dll_path;
    // 首发延迟诊断日志 (txt)
    std::FILE *windup_dump = nullptr;
    int windup_tick = 0;                              // 帧计数 (植物窗口 100Hz 时约等于游戏帧)
    std::vector<WindupDumpState> windup_dump_state;   // 每槽位
    bool windup_dump_enabled = true;                  // 是否写 txt
    int plant_sample_ticks = 2;                    // 每个刷新周期约等于几帧 (实测平滑)
    std::vector<unsigned char> zombie_chill_pending; // 僵尸槽位的 "强制重算动画速率" 倒计时
    std::vector<unsigned char> object_buffer;      // 对象数组的批量读缓冲

    // 各僵尸移速分类实际观测到的区间 (未固定时从游戏随机值学习)
    std::array<float, ZSC_COUNT> zombie_observed_min;
    std::array<float, ZSC_COUNT> zombie_observed_max;

  public:
    // 以下是修改功能

    // 解锁游戏
    void UnlockTrophy();

    // 直接过关
    void DirectWin(bool);

    // 去除上限
    void UnlockSunLimit(bool);

    // 修改阳光
    void SetSun(int);

    // 修改金币
    void SetMoney(int);

    // 自动收集
    void AutoCollected(bool);

    // 不掉战利品
    void NotDropLoot(bool);

    // 花肥无限
    void FertilizerUnlimited(bool);

    // 杀虫剂无限
    void BugSprayUnlimited(bool);

    // 巧克力无限
    void ChocolateUnlimited(bool);

    // 树肥无限
    void TreeFoodUnlimited(bool);

    // 智慧树高度
    void SetTreeHeight(int);

    // ---------------------------------------------------------------------
    // 2026-10-07 新增 (见 CHANGELOG): 僵尸批量操作 + 女仆秘籍 + 智慧树指令
    //   前端每帧调用一次 FrameHacks(); 上面这些 Set* 只记录用户的选择。
    //   全部只用内存字段 (零代码补丁), 具体偏移见 inc/data.h 的说明。
    // ---------------------------------------------------------------------

    // 冻结 / 魅惑 "当前场上" 的僵尸 (按一下生效一次; 之后新刷出的僵尸不受影响)
    //   返回这次处理了几只, 供界面显示
    int ZombieFreezeNow();
    int ZombieMindControlNow();

    // 无视僵尸进家: 僵尸走到 mPosX <= 阈值 就写 phase=3 (游戏自己走 DieWithLoot)
    void SetZombieKillAtHome(bool on);

    // 僵尸血量倍数 (1.0 = 原版; 新出现的僵尸按倍数改 mBodyMaxHealth/mBodyHealth)
    void SetZombieHealthMul(float mul);

    // 女仆秘籍: 0 关闭 / 1 保持前进 / 2 跳舞 / 3 召唤伴舞 (用 mPaused 控制舞王相位)
    void SetMaidMode(int mode);

    // 每帧调用 (前端主循环)
    void FrameHacks();

    // 智慧树指令 (7 条): 发按键序列给游戏窗口, 与玩家手敲完全等价
    static int WisdomCodeCount();
    static const char *WisdomCodeText(int index);  // 要敲的原文, 例如 "mustache"
    static const char *WisdomCodeLabel(int index); // 括号里的说明
    bool SendWisdomCode(int index);
    bool WisdomCodeEnabled(int index); // 读游戏里对应的标志位 (用于勾选框回显)

    // 诊断: 一行文字, 显示与 LawnApp 偏移有关的关键读数 (女仆秘籍 / 智慧树指令不生效时用)
    std::string DebugLine();

    // -----------------------------------------------------------------------
    // 2026-10-07 新增: 特征扫描 + 可还原的代码补丁 (精确控制的基础设施, 参考 AvZ)
    //   思路: 不按版本填死地址, 而是按字节特征在游戏主模块的可执行节里找地址;
    //         打补丁前先备份原始字节, 可逐条/全部还原。
    // -----------------------------------------------------------------------

    // 扫描游戏主模块 (PvZ 非 ASLR, 基址 0x400000) 的所有可执行节
    std::vector<uintptr_t> ScanGamePattern(const char *text, size_t max_hits = 16);

    // 诊断: 扫女仆秘籍那条"读 mAppCounter"的指令特征 (含版本相关的偏移), 返回可读结果
    std::string ScanMaidSignature();

    // 诊断: 扫首发延迟相关特征 (递减/比较 Plant::mLaunchCounter 的地方 + 所在函数入口)
    std::string ScanWindupSignature();

    // 女仆秘籍: 找 GetDancerFrame 里读 mAppCounter 的那条指令 (0 = 没找到/有歧义)
    uintptr_t FindMaidPatchSite();

    // 自愈: 如果游戏里**已经**是打过补丁的状态 (工具被强杀/退出时没还原), 认出来并恢复状态。
    // 返回识别到的模式 (0 = 没打补丁), 同时把补丁点与"原始字节"准备好, 以便随时还原。
    int DetectMaidPatch();

    // 从游戏 exe 文件里读某个虚拟地址处的**原始字节** (用于找回被覆盖前的指令)
    bool ReadOriginalBytesFromFile(uintptr_t va, std::vector<unsigned char> &out, size_t size);

    // -----------------------------------------------------------------------
    // 方案 B: 注入我们自己的小 DLL (pztk_hook.dll), 在游戏自己的帧里执行逻辑。
    //   为什么: 外部进程只能 ~60 Hz 写内存, 而游戏 100 Hz —— "必须在某一帧内落值"
    //   的功能 (首发延迟) 会概率性失效。DLL 用 MinHook 挂在 Plant::UpdateShooter 上。
    //   地址/开关/参数都通过共享内存 (hook/hook_ctl.h) 由工具下发。
    // -----------------------------------------------------------------------

    // 定位 Plant::UpdateShooter: 用香蒲检查点的唯一特征定位命中点, 再找函数入口
    // (可与反编译注释的 0x45F8A0 对照)
    uintptr_t FindUpdateShooter();

    // 建立/关闭共享内存通道 (注入之前必须先建)
    bool OpenHookChannel();
    void CloseHookChannel();

    // 清理上一次残留的 DLL (工具被强杀后, 旧 DLL 可能还挂在游戏里)
    void CleanupStaleHookDll();

    // 注入 DLL 并挂上 hook_addr 处的钩子; dll_path 是 pztk_hook.dll 的完整路径
    bool InjectHookDll(const std::wstring &dll_path, uintptr_t hook_addr);

    // 请 DLL 卸钩子并把它从游戏进程里卸载 (可重复调用)
    bool UnloadHookDll();

    bool HookInjected() const { return this->hook_injected; }
    // 钩子**此刻**是否真的在工作 (DLL 报告的 hook_ok) —— 判断该怎么走、界面显示什么都用它。
    // 只看工具自己的 hook_injected 不够: DLL 万一中途挂了/被卸载, 那个标志还会是 true,
    // 于是老路径被屏蔽、新路径又不干活 -> 首发延迟就"失效"了。
    bool HookAlive() const
    {
        return this->hook_injected && this->hook_ctl != nullptr && this->hook_ctl->hook_ok != 0;
    }
    // 游戏断开时调用: 清掉"已注入"状态 (DLL 会靠**工具进程的存活**自己卸钩子)
    void MarkHookDetached()
    {
        this->hook_injected = false;
        this->hook_remote_module = 0;
        // ★ v38: 游戏断开 = 换进程/游戏退出了 ⇒ "曾经报过 hook_ok" 这条记忆也要清,
        //   否则下一次连上会把"还没注入"误报成"钩点被撤"。
        this->hook_seen_alive = false;
    }
    uintptr_t ShooterAddress() const { return this->shooter_addr; }
    bool ShooterUsedHit() const { return this->shooter_used_hit; }
    unsigned int HookCallCount() const { return this->hook_ctl == nullptr ? 0 : this->hook_ctl->hook_calls; }
    unsigned int HookBadSelfCount() const { return this->hook_ctl == nullptr ? 0 : this->hook_ctl->bad_self; }

    // 帧级钉值: 让注入的 DLL 在游戏自己的帧里把 mLaunchCounter 钉到 value (on=false 则关闭)
    void SetHookPin(bool on, int seed, int value);

    // 方案 B: 帧级首发延迟 —— 把既有的"按分类首发延迟"设置下发给 DLL,
    // 由 DLL 在帧内判断"本排有没有僵尸"再决定钉/放 (完全帧级精确)
    void SetWindupHook(bool on, const std::array<float, PSC_COUNT> &sec,
                       const std::array<bool, PSC_COUNT> &en);

    // ---- 事件日志 (排查"发射不整齐"这类问题用) ----
    // DLL 只在状态跳变 (僵尸进排 / 放手 / 钉住) 时产生条目, 工具每帧取走并写进文本文件。
    void SetHookLog(bool on);
    void DrainHookLog();                     // 每帧调用
    const std::string &HookLogPathA() const { return this->hook_log_path_a; }
    // 仅供离线测试/诊断: 直接拿到共享控制块
    PtHook::Control *RawHookCtl() { return this->hook_ctl; }

    // 诊断行: 钩子状态 / 调用次数 / 最近看到的计数器与植物种类
    std::string HookStatus();

    // 女仆秘籍: 0 = 还原; 1/2/3 = 打补丁 (AvZ 的 90 B8 imm32 做法)。返回 0 成功, -1 退回钉时钟模式
    int ApplyMaidPatch(int mode);
    bool MaidPatched() { return this->hack_maid_patched; }

    // 植物字段探测: 记下场上第一棵活植物的整块结构, 60 帧后对比, 报出"值发生变化的偏移"。
    // 用来确定**这个游戏版本**里 mLaunchCounter 到底在哪个偏移 (首发延迟特征扫描的依据)。
    void ProbePlantFieldsBegin();
    void ProbeTick();
    std::string ProbeReport() { return this->probe_report; }

    // 打补丁 (写之前先备份原始字节; 同地址重复打补丁会覆盖记录而不是叠加)
    bool PatchBytes(uintptr_t addr, const std::vector<unsigned char> &bytes);

    // 还原某地址 / 全部还原; 返回是否成功
    bool RestorePatch(uintptr_t addr);
    void RestoreAllPatches();
    int PatchCount();
    bool HasPatchAt(uintptr_t addr);

    // 免费用卡
    void FreePlanting(bool);

    // 随意放置
    void PlacedAnywhere(bool);

    // 紧密传送
    void FastBelt(bool);

    // 连续铲子
    void LockShovel(bool);

    // 混乱关卡
    void MixMode(int, int);

    // 无尽轮数
    void EndlessRounds(int);

    // 生成植物
    void asm_put_plant(int, int, int, bool, bool);
    void PutPlant(int, int, int, bool);

    // 生成僵尸
    void asm_put_zombie(int, int, int);
    void PutZombie(int, int, int);

    // 生成墓碑
    void asm_put_grave(int, int);
    void PutGrave(int, int);

    // 生成梯子和智能搭梯
    void asm_put_ladder(int, int);
    void PutLadder(int, int);
    void AutoLadder(bool);

    // 生成钉耙
    void asm_put_rake(int, int);
    void PutRake(int, int);

    // 启动/删除/恢复小推车
    void SetLawnMowers(int);

    // 清除所有植物
    void ClearAllPlants();

    // 杀死所有僵尸
    void KillAllZombies();

    // 清理场地物品
    void ClearGridItems(std::vector<int>);

    // 植物无敌
    void PlantInvincible(bool);

    // 植物脆弱
    void PlantWeak(bool);

    // 僵尸无敌
    void ZombieInvincible(bool);

    // 僵尸虚弱
    void ZombieWeak(bool);

    // 立刻装填
    void ReloadInstantly(bool);

    // 蘑菇睡醒
    void MushroomsAwake(bool);

    // 暂停刷怪
    void StopSpawning(bool);

    // 停滞不前
    void StopZombies(bool);

    // 锁定黄油
    void LockButter(bool);

    // 不留弹坑
    void NoCrater(bool);

    // ---- 高级暂停 (2026-10-09) ----
    // 勾选后**不主动暂停游戏**: 按空格不再弹"暂停对话框", 而是只把**世界更新**冻住
    // (僵尸/植物/子弹/刷怪/雾/关卡结束), 并且**完全不碰 Board::mPaused** ——
    // 所以鼠标与种植预览照常更新、选卡/种植/铲子/工具全都能用, 音乐不停、也不弹框。
    // 再按一次空格恢复。触发点就是游戏自带暂停的那条路 (空格), 详见 pvz.cpp 的实现注释。
    void SetAdvPause(bool on);
    const std::string &AdvPauseMsg() const { return this->adv_pause_msg; }
    // 游戏里当前是否处于"高级暂停中" (读游戏进程里的标志字节; 未安装时返回 false)
    bool AdvPauseActive();
    // 两处补丁是否已装上
    bool AdvPausePatched() const { return this->hack_adv_pause; }

    // 不磨冰道
    void NoIceTrail(bool);

    // ---- 多倍出怪 (出怪点数 ×N) ----
    // 让**游戏自己的**出怪分配拿到 N 倍的"僵尸点数"预算: 挑哪种僵尸、权重表、
    // 首现波限制、无尽权重曲线、50 只/波上限全部照游戏原样跑, 只有预算被放大。
    // 锚点 = Board::PickZombieWaves 里"补满循环"入口那 6 字节 (每波恰好执行一次),
    // 改写成 jmp 到我们分配的代码洞做 `预算 *= N`, 详见 pvz.cpp 的实现注释。
    //   on = false 只还原补丁; n 限 1..24 (n == 1 等价于关闭)
    void SetWaveMul(bool on, int n);
    // 当前状态一行字 (界面提示用): 已生效 / 未命中锚点 / 已关闭 / 未连接
    const std::string &WaveMulMsg() const { return this->wave_mul_msg; }
    // 锚点是否已找到 (没找到 = 本版不支持, 界面要如实提示)
    bool WaveMulSupported();
    // 被打补丁的地址 (0 = 没打; 调试/日志用)
    uintptr_t WaveMulSite() const { return this->wave_mul_site; }

    // 小丑(辣椒)不爆
    void ZombieNotExplode(bool);

    // 获取卡槽某格卡片, 模仿者额外加 48
    int GetSlotSeed(int);

    // 修改卡槽某格卡片
    //   ★ 2026-10-14: 这条路径**在选卡界面会被游戏覆盖** —— `SeedChooserScreen::CloseSeedChooser()`
    //     在玩家按 "Let's Rock" 那一刻按**选卡结果**逐槽 `SetPacketType()` 覆盖 `Board::mSeedBank`。
    //     所以现在分两条路 (见下面的"延迟写入"): 战斗中直接写; **选卡界面记下来**,
    //     等战斗正式开始 (`mGameScene == 3` = SCENE_PLAYING) 那一刻**写一次**。
    void SetSlotSeed(int, int, bool);

    // 「换卡片」延迟写入的待写台账 (每格一项; 同一格再设一次就覆盖, 不会重复写)
    int PendingSlotSeedCount() const { return static_cast<int>(this->pending_slot_seeds.size()); }
    // 最近一次"换卡片"的结果文字 (界面 tooltip / DEV 行用)
    const std::string &SlotSeedMsg() const { return this->slot_seed_msg; }

    // -----------------------------------------------------------------------
    // ★★ 2026-10-14 新增: 「取消必选卡片」—— 冒险模式**二周目** Crazy Dave 预选 ★★
    //
    //   【游戏机制】(逐条经反汇编 + 反编译源码核实; 完整证据链见 HANDOVER §4.14)
    //     唯一入口在 `SeedChooserScreen` 构造函数末尾:
    //         if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
    //             CrazyDavePickSeeds();                      // VA 0x483F70 (EN 1.0.0.1051)
    //     判定指令在 VA 0x483F26: `83 78 2C 00 7E 06` = `cmp [eax+0x2C],0` (mFinishedAdventure,
    //     也就是工具版本表里的 `playthrough`) + `jle +6`; 后面紧跟 `55 E8 …` =
    //     `push ebp; call CrazyDavePickSeeds`。抽中的 3 种写进
    //     `SeedChooserScreen::mChosenSeeds[49]` (+0xA4, 步长 0x3C) 的 `+0x38 = mCrazyDavePicked`,
    //     而且**只在 UI 层**拦鼠标 (MouseDown 只放蜂鸣器 + tooltip 闪红); Board 层的
    //     `mSeedBank` 是 `CloseSeedChooser()` 按选卡结果逐槽覆盖的 ⇒ 没有任何强写。
    //
    //   【两套机制】(用户要求两套都做, 可同时启用; 效果必须立刻正确)
    //     机制 A (字节补丁): 把那条 `7E`(jle) 改成 `EB`(jmp) ⇒ 无条件跳过 CrazyDavePickSeeds()
    //         ⇒ 在**别的界面**勾上, 进关卡时根本不抽卡。
    //         签名 `83 78 ?? 00 7E 06 55 E8` 全 exe 唯一 (`??` 兼容 GOTY 的偏移);
    //         打前两步自检: ① 8 字节形状; ② `call` 目标函数体里确实有"把
    //         mCrazyDavePicked 置 1"的写入 (`88 4E 38` 形态)。任何一步不过 ⇒
    //         **拒绝打补丁**并如实报"未生效(不支持此版本)" (宁可功能不可用, 绝不改坏游戏)。
    //         用现成的 `PatchBytes`/`RestorePatch`: 可逆、退出/断开即还原。
    //     机制 B (运行时清标志): 选卡界面**真开着**时把 `mChosenSeeds[i].mCrazyDavePicked`
    //         清 0 ⇒ 那 3 张**立刻**能点下来。先做布局自检 (`mSeedType == i` 全过) 再写;
    //         幂等 (只写当前非 0 的); 每关都重来 (选卡界面每关重建 ⇒ 不以"指针变了"当触发)。
    // -----------------------------------------------------------------------

    // 机制 A 的自检结论 (界面/DEV 行按它说话)
    enum DavePickVerdict
    {
        DPV_NONE = 0, // 还没查过
        DPV_OK,       // 形状 + call 目标自检全部通过 ⇒ 可以打补丁
        DPV_SHORT,    // 命中处读不到那 12 字节
        DPV_SHAPE,    // 形状不是 `83 78 ?? 00 7E 06 55 E8`
        DPV_CALL,     // call rel32 算出来的目标不合法 (不在游戏模块的可执行节里)
        DPV_NO_WRITE, // call 目标函数体里找不到"置 mCrazyDavePicked=1"的写入
        DPV_PATCH,    // 自检过了但补丁写入/回读失败
        DPV_NOT_FOUND,// 签名 0 处命中 (本版不支持)
        DPV_AMBIGUOUS,// 签名多处命中 (有歧义, 不敢动)
    };

    // 命中处解出来的地址与立即数 (纯数据; 由 DavePickCheckShape 填)
    struct DavePickSite
    {
        uintptr_t site = 0;        // 签名起点 (那条 `cmp [...],0`)
        uintptr_t patch_addr = 0;  // 要改的那 1 字节 (= site + 4, 原 `7E`)
        uintptr_t call_target = 0; // `call CrazyDavePickSeeds` 的目标
        uint32_t cmp_offset = 0;   // 签名里 `??` 那个立即数 (PlayerInfo 字段偏移)
    };

    // 机制 B 的写入计划 (纯函数产出)
    struct DavePickClearPlan
    {
        bool layout_ok = false;             // 49 项 mSeedType == i 全部成立
        int bad_index = -1;                 // 第一个不符的项 (-1 = 全部成立)
        int picked = 0;                     // 当前 mCrazyDavePicked != 0 的项数 (要清几张)
        std::vector<uint32_t> zero_offsets; // 要写 0 的字节偏移 (相对 mChosenSeeds 起点)
    };

    // ---- 纯函数 (全部与进程无关, 便于离线断言; 见 misc/recttest.cpp) ----
    // ① 形状自检: code[hit_off .. hit_off+8) 必须是 `83 78 ?? 00 7E 06 55 E8`
    static DavePickVerdict DavePickCheckShape(const unsigned char *code, size_t size,
                                             uintptr_t base, size_t hit_off, DavePickSite &out,
                                             const char **why);
    // ② "把 mCrazyDavePicked 置 1" 的写入形状 (返回指令长度, 0 = 不是)
    static size_t DavePickPickedWriteLen(const unsigned char *p, size_t avail);
    // ③ 在 call 目标函数体里找那条写入 (out_off = 相对函数入口的偏移)
    static bool DavePickHasPickedWrite(const unsigned char *body, size_t len, size_t *out_off);
    // ④ 机制 B 的布局自检 + 写入计划 (chosen = mChosenSeeds 数组的原始字节)
    static DavePickClearPlan DavePickPlanClear(const unsigned char *chosen, size_t size);
    // ⑤ "换卡片"的延迟写入判据: 给定当前 `mGameScene` / 本关是否已写 / 待写格数 ⇒ 该不该写
    static bool SlotSeedShouldApply(int ui_now, bool wrote_this_battle, int pending_count);

    // 机制 A: 在游戏模块里扫签名 + 两步自检 (**只读, 不写内存**)。out 里是解出的地址。
    DavePickVerdict FindDavePickSite(DavePickSite &out, size_t *out_body_off = nullptr);
    // 机制 B: 选卡界面是不是**真开着** (指针非空 + `mMouseVisible` + `mGameScene == 2`)
    bool SeedChooserOpen(uintptr_t *out_screen = nullptr);
    // 用户开关: on 立刻生效 (打/还原机制 A 的补丁; 机制 B 由 TickNoDavePick 每帧做)
    void SetNoDavePick(bool on);
    // 每帧: 机制 B + 状态行刷新 (由 FrameHacks 调用; 没开开关就立刻返回)
    void TickNoDavePick();
    // 一行状态 (界面显示): 已生效 / 未生效(不支持此版本) / 等待选卡界面 / 未连接游戏 / 已关闭
    const std::string &DavePickMsg() const { return this->no_dave_pick_msg; }
    // DEV 行 (只在 PZTK_DEV=1 显示): 签名/补丁/自检证据, 不占普通界面
    std::string DavePickDevLine();
    // 机制 A 是否已生效 (自检通过且补丁已打上)
    bool DavePickPatched() const { return this->dave_pick_patched; }

  protected:
    // ---- 「取消必选卡片」的状态 ----
    bool hack_no_dave_pick = false;      // 用户勾选
    bool dave_pick_patched = false;      // 机制 A 的补丁是否已打上
    uintptr_t dave_pick_site = 0;        // 被打补丁的地址 (= 签名起点 + 4, 0 = 没打)
    uintptr_t dave_pick_target = 0;      // 自检通过的 call 目标 (CrazyDavePickSeeds 入口)
    uint32_t dave_pick_cmp_offset = 0;   // 自检通过时签名里那个立即数 (PlayerInfo 偏移)
    uint32_t dave_pick_pid = 0;          // 打补丁时游戏的 pid (游戏重开后地址失效)
    int dave_pick_verdict = 0;           // DavePickVerdict
    size_t dave_pick_body_off = 0;       // "置 1" 写入在 call 目标函数体里的偏移
    bool dave_pick_body_off_valid = false;
    bool dave_pick_b_layout_ok = false;  // 最近一次机制 B 的布局自检结论
    int dave_pick_b_bad_index = -1;      // 布局自检第一个不符的项 (-1 = 通过)
    int dave_pick_b_cleared = 0;         // 最近一帧机制 B 清了几张
    bool dave_pick_b_active = false;     // 最近一帧有没有真的在选卡界面上动过手
    std::string no_dave_pick_msg;        // 一行状态 (界面显示)

    // ---- 「换卡片」延迟写入的状态 ----
    struct PendingSlotSeed
    {
        int index = 0;
        int type = 0;
        bool imitater = false;
    };
    std::vector<PendingSlotSeed> pending_slot_seeds; // 选卡界面里被指定过的格
    int slot_seed_last_ui = -1;                      // 上一帧的 mGameScene (抓 LEVEL_INTRO->PLAYING)
    std::string slot_seed_msg;                       // 最近一次"换卡片"的结果文字

    // 真正写 mSeedBank 的那一步 (战斗中 / 战斗正式开始那一刻; 选卡界面走"记下来")
    void WriteSlotSeedNow(int index, int type, bool imitater);
    // 选卡界面里记一笔 (同一格覆盖)
    void RememberPendingSlotSeed(int index, int type, bool imitater);
    // 每帧: 到"战斗正式开始"那一刻把台账写一次 (幂等: 每关只写一次)
    void TickPendingSlotSeeds();
    // 状态行刷新 (机制 A/B 的结论 -> 一句人话)
    void RefreshDavePickMsg();

  public:
    // 水路种睡莲
    void LilyPadOnPool(int, int);

    // 屋顶放花盆
    void FlowerPotOnRoof(int, int);

    // 窗口截图
    void Screenshot();

    // 获取代码
    Lineup GetLineup();

    // 布置阵型
    void SetLineup(Lineup);

    // 根据出怪种类生成出怪列表
    void generate_spawn_list();

    // 更新选卡界面的出怪预览
    void update_spawn_preview();

    // 获取刷怪列表
    std::array<int, 1000> GetSpawnList();

    // 修改刷怪列表
    void SetSpawnList(std::array<int, 1000>);

    // 内置函数生成出怪列表
    void InternalSpawn(std::array<bool, 33>);

    // 自定义填充出怪列表
    void CustomizeSpawn(std::array<bool, 33>, bool, bool, int);

    // 修改背景音乐
    void SetMusic(int);

    // 清除浓雾
    void NoFog(bool);

    // 罐子透视
    void SeeVase(bool);

    // 后台运行
    void BackgroundRunning(bool);

    // 存档只读
    void UserdataReadonly(bool);

    // 内置调试模式
    void DebugMode(int);

    // 帧生成时间
    int GetFrameDuration();
    void SetFrameDuration(int);

    // 显示隐藏关卡
    void UnlockLimboPage(bool);

    // 以下为 "固定随机数值" 功能 --------------------------------------------

    // 该功能依赖 Zombie / Plant 的对象布局, 泄露的测试版不支持
    bool RandomFixSupported();

    // 每个刷新周期调用一次:
    //   1. 读回僵尸对象数组, 按分类把 mVelX 固定成用户设定的值
    //   2. 读回植物对象数组, 按分类把 mLaunchCounter 固定成用户设定的攻击间隔
    //      (只固定随机抖动, 不改写 mLaunchRate), 并输出两张列表
    // 最近若干次首发延迟写入 (诊断用, 最新的在前)
    const std::vector<WindupWriteLog> &WindupLog() const { return windup_log; }

    bool FixRandomValues(const std::array<float, ZSC_COUNT> &zombie_value,                         const std::array<bool, ZSC_COUNT> &zombie_enabled,  //
                         const std::array<float, PSC_COUNT> &plant_interval, // 秒
                         const std::array<bool, PSC_COUNT> &plant_enabled,   //
                         const std::array<float, PSC_COUNT> &windup_sec,     // 首发延迟(秒)
                         const std::array<bool, PSC_COUNT> &windup_enabled,  //
                         std::vector<ZombieSpeedInfo> &out_zombie,           //
                         std::vector<PlantSpeedInfo> &out_plant);

    // 把被改写过的植物 mLaunchRate 还原成游戏原值
    void RestorePlantLaunchRate();

    // 取某个分类从游戏实际随机值里观测到的速度区间。
    // 静态分类表来自反编译源码, 与官方原版的常量可能有出入 (例如普通僵尸
    // 实测能到 0.36 以上, 而源码写的是 0.32), 所以这里用实际观测值来校正。
    // 只有该分类没有被固定时才观测得到有意义的区间。返回 false 表示还没观测到。
    bool ObservedSpeedRange(int klass, float &out_min, float &out_max) const;

    // 清空观测到的区间 (界面上的"恢复默认值"会调用)
    void ResetObservedSpeedRange();
};

template <typename T, size_t size>
void PvZ::enable_hack(HACK<T, size> hack, bool on)
{
    if (hack.mem_addr == 0x00000000 || hack.mem_addr == 0xffffffff)
        return;

    if (on)
        WriteMemory(std::array<T, size>(hack.hack_value), {hack.mem_addr});
    else
        WriteMemory(std::array<T, size>(hack.reset_value), {hack.mem_addr});
}

template <typename T, size_t size>
void PvZ::enable_hack(std::vector<HACK<T, size>> hacks, bool on)
{
    for (size_t i = 0; i < hacks.size(); i++)
    {
        if (hacks[i].mem_addr == 0x00000000 || hacks[i].mem_addr == 0xffffffff)
            continue;

        if (on)
            WriteMemory(std::array<T, size>(hacks[i].hack_value), {hacks[i].mem_addr});
        else
            WriteMemory(std::array<T, size>(hacks[i].reset_value), {hacks[i].mem_addr});
    }
}

#ifdef _DEBUG

template <typename T, size_t size>
void PvZ::check_hack(HACK<T, size> hack)
{
    bool ok = true;

    if (hack.mem_addr == 0x00000000 || hack.mem_addr == 0xffffffff)
        return;

    auto read_value = ReadMemory<T, size>({hack.mem_addr});
    if (read_value != hack.reset_value)
        ok = false;

    if (!ok)
        std::cout << "Hack Error: " << hack.mem_addr << std::endl;
}

template <typename T, size_t size>
void PvZ::check_hack(std::vector<HACK<T, size>> hacks)
{
    bool ok = true;

    for (size_t i = 0; i < hacks.size(); i++)
    {
        if (hacks[i].mem_addr == 0x00000000 || hacks[i].mem_addr == 0xffffffff)
            continue;

        auto read_value = ReadMemory<T, size>({hacks[i].mem_addr});
        if (read_value != hacks[i].reset_value)
            ok = false;

        if (!ok)
            std::cout << "Hack Error: " << hacks[i].mem_addr << std::endl;
    }
}

#endif

} // namespace Pt
