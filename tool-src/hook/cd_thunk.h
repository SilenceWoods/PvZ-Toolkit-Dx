// ---------------------------------------------------------------------------
// 卡片冷却显示: 钩点跳板 (2026-10-11 重写; kVersion 26)
//
//   为什么单独放在一个头文件里: 跳板的**汇编码只有这一份**, 出厂 DLL 和离线自测
//   (tool-src/misc/recttest.cpp) 都 include 这个头 —— 自测跑的就是出厂跳板本身,
//   而不是"照着抄一遍的另一份"。这是修完那次崩溃后特意做的: 上一版跳板的栈算术
//   是错的, 而错误只存在于"没有单测覆盖的裸汇编"里。
//
// ---------------------------------------------------------------------------
// 调用点 (EN 1.0.0.1051, Board::DrawGameObjects 里 RENDER_ITEM_BOTTOM_UI 那条 case):
//
//   0x4172E0: 8B 55 08        mov  edx,[ebp+0x8]     ; Board* this
//   0x4172E3: 8B 7D 0C        mov  edi,[ebp+0xc]     ; Graphics* g
//   0x4172E6: 52              push edx               ; ★ caller 已经把 Board* 压好
//   0x4172E7: E8 B4 2D 00 00  call 0x41A0A0        ; ★ 被补丁的 5 字节
//   0x4172EC: ...                                    ; 原调用之后继续
//
//   Board::DrawUIBottom(Graphics* g) 是 **this 走栈** 的成员函数 (单参数, 被调者
//   `ret 4` 自己清参数) —— 这不是猜的, 与同一个函数里、**已经实机验证过**的
//   碰撞箱钩点 (0x4172BD `call DrawUICoinBank`) 是同一个形状: 两者都是
//   `mov edi,[ebp+0xc]` + `push Board` + `<callee>` + 被调者 `ret 4`。
//   本文件第 C 条把这条约定写成硬规矩, 并且用离线测试钉住。
//
// ---------------------------------------------------------------------------
// 进入跳板时的现场 (钩点把跳板当成"被调者"来 call):
//
//   [esp+0] = 返回地址 0x4172EC
//   [esp+4] = Board* this          ← caller 压的, **被调者就是从这里读 this**
//   EDI     = Graphics* g          ← 0x4172E3 那条 `mov edi,[ebp+0xc]`
//
// ---------------------------------------------------------------------------
// ★★ 栈算术 (硬规矩, 逐条算清楚) ★★
//
//   A) 被调者读 `this` 的地址是**死的**: `[esp+4]` (this 走栈, 不在 ECX)。
//      ⇒ 重放到那条 call 的那一刻, 栈顶必须是"返回地址, 下面紧跟 this"这个形状。
//      上一版 (v25) 在重放前做了 `pushf/pusha` (36 字节) 就直接 call, 被调者读到的
//      `[esp+4]` 就变成了 pushal 存下的某个寄存器值 ⇒ 它拿着垃圾当 Board 用, 第一条
//      用到 this 的指令就崩 —— 现场正是 `EIP = 0x41A0AB`
//      (= DrawUIBottom + 0xB, 序言刚过完、第一次按 this 取字段的地方)。
//
//   B) 重放之后要再次用到 Board*, 只能从**栈上**取, 且偏移必须现算。
//      下面是**修好之后**的真实布局 (PZTK_CD_THUNK_BODY 逐条对应, 单测逐字节钉住):
//        进入跳板                     esp = S      [S] = 返回地址, [S+4] = Board(caller 压的)
//        pushfl                       esp = S-4
//        pushal                       esp = S-36   保存的 8 个寄存器在 S-36..S-5
//        pushl 40(%esp)               esp = S-40   ★ 把 Board 复制到栈顶 (S-36+40 = S+4)
//        call *g_cd_uib_ptr           压返回地址后 esp = S-44
//                                     ⇒ 被调者看到 [esp] = 内部返回地址, [esp+4] = S-40 = Board ✓
//        被调者 ret 4                 esp = S-40   (清掉我们压的那份)
//        popal                        esp = S-4
//        popfl                        esp = S
//        ── 到这里栈已回到进入时的样子 ──
//        pushal                       esp = S-36
//        push _g_cd_retval            esp = S-40   (参数3 = 原调用的返回值, 从暂存取)
//        push board                   esp = S-44   (参数2)
//        push g (edi)                 esp = S-48   (参数1)
//        call CooldownDrawInGame      压返回地址后 esp = S-52
//        ★ 取 board 的偏移 = **40(%esp)** —— 这是用 gdb 在真实跳板上逐格扫出来的
//          (搜索栈上等于 Board 值的槽; 见 CHANGELOG v26 的排查记录)。
//          读法: board 就是"重放前我们压的那份", 它在 S-40; 而 draw 调用时的 esp=S-52,
//          两者只差 12 字节 —— 但这**不等于**偏移是 12: 那 12 字节里还夹着
//          ret $4 平掉的 4 字节。实测偏移是 **40**, 别再用纸笔推 (推错过两次)。
//          最终判据只有一条: 离线单测里"绘制回调拿到的 board == 假 Board"。
//        addl $12,%esp                esp = S-40
//        popal                        esp = S-4
//        ret $4                       esp = S+4   ★ 清掉 **caller** 压的那份 Board
//
//      ★ 上一版的两处错都在这张表上:
//        ① 重放前**没压** Board (以为"caller 压的那份还在 [esp+4]") —— 其实 pushal 已经
//           把它挤到 [esp+40] 去了, 被调者读到的是返回地址; 实测后果: 跳板结尾的
//           `ret 4` 清掉的是 caller 那份 ⇒ 栈整体少 4 字节 ⇒ `ret` 弹到 Board 指针上
//           当返回地址 ⇒ 跳到数据区执行 (离线单测里复现成 EIP=0xC0000005)。
//        ② 给绘制函数传的 board 写成 36(%esp)/52(%esp) —— 前者是"进入时的返回地址",
//           后者漏算了"给重放压的那份 Board"。正解是 **56(%esp)**。
//
//   C) 结尾的 ret 形式 = **ret 4**。
//      这条 call 的调用者 (DrawGameObjects) 只 push、**不事后 add esp** —— 它认定
//      被调者会清掉那个参数 (这正是 thiscall 的 `ret 4`)。所以跳板必须与"原被调者"
//      行为一致: 自己压的那份由被调者的 ret 4 清掉, **caller 压的那份由跳板的
//      ret 4 清掉**。同一个函数里 (0x4172BD 碰撞箱钩点) 已实机验证过这条,
//      两处形状完全相同。
//
//   D) 绘制段必须**整段可跳过**, 且绝不能影响重放本身。
//      重放调用是"冷路径"(它一跑, 卡槽就画完了, 后面该干嘛干嘛), 绘制是"热路径"
//      (读卡数组、算数字、画矩形)。两道闸门:
//        ① 编译期/链路期: 重放成功后直接 `testl %eax,%eax` —— g_cd_uib_ptr 为 0
//           就 `jz` 跳过整段绘制; 指针为 0 说明工具没下发地址, 那就只做透明重放;
//        ② 运行期: 绘制函数自己第一件事就是查开关/指针/长度 —— 任何一项不对
//           `ret` 回去, 一行游戏代码都不碰 (见 hook_main.cpp 的 CooldownDrawInGame)。
//
//   E) g 用**进入时保存的 EDI**, 不用被调者的返回值。
//      被调者的 EAX 是它自己的返回值, 与 g 无任何关系; 上一版 `movl %eax,%edi`
//      等于"把 callee 的返回值当 Graphics* 用"。这里改成 `movl %edi,%eax`:
//      EDI 在 popal 之后 = 进入时的 g, 然后 push 给 C 函数当第 1 个参数。
//
//   F) 寄存器全程 pushal/popfl (pushfl/popol 成对) 保存与恢复, 被调用者保存的
//      EDI/ESI/EBX 不会漏 —— 调用者 (DrawGameObjects) 的 EBP/EBX/ESI/EDI
//      在跳板返回后与进入前完全一致。
//
//   G) ★★ 重放时**必须自己再压一份 Board** (这一步是第二次修崩溃的关键) ★★
//      调用者 (DrawGameObjects) 压 Board 之后**不自己清** —— 它认定被调者会清
//      (这正是 thiscall 的 `ret 4`)。我们 call 的时候又会压一层返回地址, 被调者
//      读 this 用的是 [esp+4], 所以我们必须**自己压一份 Board** 放在那个位置:
//
//          进入跳板               esp = S        [S]=retaddr   [S+4]=Board(caller 压的)
//          pushfl                 esp = S-4
//          pushal                 esp = S-36
//          pushl 40(%esp)         esp = S-40      ← 这里 [esp] = Board (我们压的那份)
//          call *fn               压返回地址后 esp = S-44, 被调者看到
//                                                 [esp+4] = S-40 = ★ Board ✓
//          被调者 ret 4           esp = S-40     (清掉**我们**压的那份)
//          ...
//          跳板结尾 ret $4        esp = S+4      (清掉**caller**压的那份, 与原被调者等价)
//
//      ★ 少了这次 push 会怎样 (实测过): 被调者从 [esp+4] 读到的是**返回地址**
//        (一段代码地址) 而不是 Board, 而跳板结尾的 ret 4 清掉的是 caller 压的那份
//        => 栈整体少 4 字节 => `ret` 弹到 Board 指针上当返回地址 => 跳到数据上执行。
//        这与"重放前不许 push"并不矛盾: 不许的是**额外**的 pushf/pusha 那种
//        "把 caller 的参数挤走"的压栈; 这里压的**就是要给被调者当 this 的那一份**,
//        位置与数量都刚好 (call 的返回地址 + 我们压的 Board = 被调者看到的
//        [esp]=返回地址 / [esp+4]=this, 与原调用点**逐字节同形**)。
//
//      ── 所以"重放那次 call 的现场"与原调用点**完全一致**, 这也正是
//      (已实机验证的) 碰撞箱跳板 0x4172BD 的做法: 同一个函数、同一个形状。
//
// ---------------------------------------------------------------------------
// 本文件由 hook_main.cpp 与 misc/recttest.cpp **共同** include ⇒ 单测覆盖的就是出厂代码。
// ---------------------------------------------------------------------------
#pragma once

// 重放目标 (Board::DrawUIBottom 的实时地址; 由工具现算后下发, 0 = 不下发/自检不过)
extern "C" void *g_cd_uib_ptr;
// ★ 暂存"原调用的返回值"。
//   为什么必须存在一个变量里: 重放之后我们要 `popal` 把 8 个通用寄存器**原样还原**
//   (调用者 DrawGameObjects 的 EAX 有自己的语义, 不能留成被调者的返回值) —— 而 popal
//   会把 EAX 恢复成**进入跳板时**的值, 于是"被调者的返回值"当场就没了。
//   所以: 重放刚返回时立刻把 EAX 存进这个变量, popal 之后再取回来当第 3 个参数。
//   (上一版直接 `pushl %eax` —— 在 popal 之后, 压进去的是 caller 的旧 EAX, 实测
//    绘制回调收到的第 3 个参数恒为调用者的 EAX, 不是返回值。)
extern "C" uintptr_t g_cd_retval;
// 绘制回调: (Graphics* g, Board* board, void* replay_result)
//   第 3 个参数 = 原调用的返回值 (EAX)。生产代码不用它 (这是 DLL 的一个坑:
//   x86-pc 默认栈传参, 调用方 add esp 平栈, 多传一个 dword 是安全的);
//   离线单测用它来断言"被调者的返回值确实被当成返回值, 而 g 来自 EDI"。
extern "C" void CooldownDrawInGame(void *g, void *board, void *replay_result);

// ★ 跳板入口的字节特征 (给"从成品 DLL 里把跳板抠出来反汇编"的验证脚本用):
//   nop; nop; nop; nop  四条 nop 从设计上就是**特征位** —— 它们不改变任何行为,
//   但让验证脚本可以拿一段独一无二的字节序列去定位跳板 (避开"跳板里那条
//   call 的 rel32/绝对地址在文件里是重定位占位、每次构建都不同"这个麻烦),
//   而不用靠猜测。特征字节 = 90 90 90 90 后紧跟 9C 60 (pushfl; pushal)。
//
// ★ 把跳板写成宏 (而不是函数): 出厂 DLL 与离线自测各自展开同一段汇编码 —— 这样
//   自测不需要链接 hook_main.cpp, 又能保证"跑的就是同一段代码"。
//   (名字带上 PZTK_, 避免和测试里别的东西撞名。)
#define PZTK_CD_THUNK_BODY                                         \
    __asm__ __volatile__(                                          \
        ".byte 0x90, 0x90, 0x90, 0x90\n\t" /* 特征位: 4 条 nop  */ \
        "pushfl\n\t"                       /* 保存标志寄存器      */ \
        "pushal\n\t"                       /* 保存 8 个通用寄存器  */ \
        /* ---- 冷路径: 重放原调用 ---- */                         \
        "cmpl $0, _g_cd_uib_ptr\n\t"       /* 地址没下发? 跳过    */ \
        "je 1f\n\t"                                                \
        "pushl 40(%esp)\n\t"               /* ★ 见上面 G: 把 Board 放到被调者的 [esp+4] */ \
        "call *_g_cd_uib_ptr\n\t"          /* DrawUIBottom 自己 ret 4 (清掉我们压的这份) */ \
        "movl %eax, _g_cd_retval\n\t"      /* ★ 先把返回值存下来 (popal 会覆盖 EAX) */ \
        "1:\n\t"                                                   \
        "popal\n\t"                        /* 恢复全部寄存器 (EDI = 进入时的 g) */ \
        "popfl\n\t"                                                \
        /* ---- 热路径: 画冷却数字 (可整段跳过) ---- */             \
        "cmpl $0, _g_cd_uib_ptr\n\t"                               \
        "je 2f\n\t"                                                \
        "pushal\n\t"                                               \
        "pushl _g_cd_retval\n\t"           /* 参数3: 原调用返回值 (从暂存取) */ \
        "movl 40(%esp), %eax\n\t"          /* ★ 参数2 = Board (实测偏移, 见上面 B 表) */ \
        "pushl %eax\n\t"                                           \
        "movl %edi, %eax\n\t"              /* ★ 参数1 = g (进入时 EDI, 见 E) */ \
        "pushl %eax\n\t"                                           \
        "call _CooldownDrawInGame\n\t"                             \
        "addl $12, %esp\n\t"               /* 3 个参数, 自己平栈    */ \
        "popal\n\t"                                                \
        "2:\n\t"                                                   \
        "ret $4\n\t")                      /* ★ 清掉 **caller 压的那份 Board** [见 C/G] */
