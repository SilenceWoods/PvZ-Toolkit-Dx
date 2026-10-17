
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace Pt
{

#define PVZ_NOT_FOUND 0
#define PVZ_OPEN_ERROR -1
#define PVZ_UNSUPPORTED 1

#ifdef _PVZ_BETA_LEAK_SUPPORT
#define PVZ_BETA_0_1_1_1014_EN 901
#define PVZ_BETA_0_9_9_1029_EN 902
#endif

#define PVZ_1_0_0_1051_EN 1001
#define PVZ_1_2_0_1065_EN 1002

#define PVZ_1_0_4_7924_ES 1003
#define PVZ_1_0_7_3556_ES 1004
#define PVZ_1_0_7_3467_RU 1005

#define PVZ_GOTY_1_2_0_1073_EN 2001
#define PVZ_GOTY_1_2_0_1096_EN 2002
#define PVZ_GOTY_1_2_0_1093_DE_ES_FR_IT 2003

#define PVZ_GOTY_1_1_0_1056_ZH 2004
#define PVZ_GOTY_1_1_0_1056_JA 2005
#define PVZ_GOTY_1_1_0_1056_ZH_2012_06 2006
#define PVZ_GOTY_1_1_0_1056_ZH_2012_07 2007

// 单独的一个 hack
// 包括 类型, 数目, 地址, 修改值, 原始值
// 一个功能的实现可能需要多个 hack
// 地址为 0 / -1 时忽略
template <typename T, size_t size>
struct HACK
{
    uintptr_t mem_addr;
    std::array<T, size> hack_value;
    std::array<T, size> reset_value;
};

// 用来保存不同版本的内存基址数据
struct PVZ_DATA
{
    uintptr_t path;
    uintptr_t lawn;

    uintptr_t frame_duration;

    uintptr_t board;

    uintptr_t zombie;
    uintptr_t zombie_status;
    uintptr_t zombie_dead;
    uintptr_t zombie_count_max;

    uintptr_t plant;
    uintptr_t plant_row;
    uintptr_t plant_type;
    uintptr_t plant_col;
    uintptr_t plant_imitater;
    uintptr_t plant_dead;
    uintptr_t plant_squished;
    uintptr_t plant_asleep;
    uintptr_t plant_count_max;
    uintptr_t plant_next_pos;

    uintptr_t lawn_mower;
    uintptr_t lawn_mower_dead;
    uintptr_t lawn_mower_count_max;
    uintptr_t lawn_mower_count;

    uintptr_t grid_item;
    uintptr_t grid_item_type;
    uintptr_t grid_item_col;
    uintptr_t grid_item_row;
    uintptr_t grid_item_dead;
    uintptr_t grid_item_count_max;

    uintptr_t cursor;
    uintptr_t cursor_grab;

    uintptr_t slot;
    uintptr_t slot_count;
    uintptr_t slot_seed_cd_past;
    uintptr_t slot_seed_cd_total;
    uintptr_t slot_seed_type;
    uintptr_t slot_seed_type_im;

    uintptr_t cut_scene;

    uintptr_t challenge;
    uintptr_t endless_rounds;

    uintptr_t game_paused;
    uintptr_t block_type;
    uintptr_t row_type;
    uintptr_t ice_trail_cd;
    uintptr_t spawn_list;
    uintptr_t spawn_type;
    uintptr_t scene;
    uintptr_t adventure_level;
    uintptr_t sun;
    uintptr_t game_clock;
    uintptr_t debug_mode;
    uintptr_t particle_systems_addr;

    uintptr_t game_selector;

    uintptr_t tod_mode;

    uintptr_t game_mode;
    uintptr_t game_ui;

    uintptr_t free_planting;

    uintptr_t anim;
    uintptr_t unnamed;
    uintptr_t particle_system;
    uintptr_t particle_system_type;
    uintptr_t particle_system_dead;
    uintptr_t particle_system_count_max;

    uintptr_t user_data;
    uintptr_t level;
    uintptr_t money;
    uintptr_t playthrough;
    uintptr_t mini_games;
    uintptr_t tree_height;
    uintptr_t twiddydinky;

    uintptr_t music;

    HACK<uint8_t, 1> block_main_loop;

    HACK<uint8_t, 1> unlock_sun_limit;
    HACK<uint8_t, 1> auto_collected;
    HACK<uint8_t, 1> not_drop_loot;

    HACK<uint8_t, 1> fertilizer_unlimited;
    HACK<uint8_t, 1> bug_spray_unlimited;
    std::vector<HACK<uint8_t, 1>> chocolate_unlimited;
    HACK<uint8_t, 1> tree_food_unlimited;

    HACK<uint8_t, 1> placed_anywhere;
    HACK<uint8_t, 1> placed_anywhere_preview;
    HACK<uint8_t, 1> placed_anywhere_iz;
    std::vector<HACK<uint8_t, 1>> fast_belt;
    HACK<uint8_t, 1> lock_shovel;

    HACK<uint16_t, 1> rake_unlimited;
    HACK<uint8_t, 1> init_lawn_mowers;
    HACK<uint32_t, 1> lawn_mower_initialize;

    HACK<uint8_t, 3> plant_immune_eat;
    HACK<uint8_t, 1> plant_immune_radius;
    HACK<uint8_t, 1> plant_immune_jalapeno;
    HACK<uint8_t, 3> plant_immune_projectile;
    HACK<uint8_t, 3> plant_immune_lob_motion;
    HACK<uint8_t, 1> plant_immune_square;
    HACK<uint8_t, 1> plant_immune_row_area;
    HACK<uint8_t, 1> plant_immune_spike_rock;
    HACK<uint8_t, 3> plant_immune_squish;

    HACK<uint8_t, 3> _plant_immune_eat;
    HACK<uint8_t, 3> _plant_immune_projectile;
    HACK<uint8_t, 3> _plant_immune_lob_motion;
    HACK<uint8_t, 1> _plant_immune_row_area;

    HACK<uint32_t, 1> zombie_immune_body_damage;
    HACK<uint8_t, 2> zombie_immune_helm_damage;
    HACK<uint32_t, 1> zombie_immune_shield_damage;
    HACK<uint8_t, 1> zombie_immune_burn_crumble;
    HACK<uint8_t, 1> zombie_immune_radius;
    HACK<uint8_t, 1> zombie_immune_burn_row;
    HACK<uint8_t, 1> zombie_immune_chomper;
    HACK<uint8_t, 1> zombie_immune_mind_controll;
    HACK<uint8_t, 2> zombie_immune_blow_away;
    HACK<uint8_t, 1> zombie_immune_splash;
    HACK<uint8_t, 1> zombie_immune_lawn_mower;

    HACK<uint32_t, 1> _zombie_immune_body_damage;
    HACK<uint8_t, 2> _zombie_immune_helm_damage;
    HACK<uint32_t, 1> _zombie_immune_shield_damage;
    HACK<uint8_t, 1> _zombie_immune_burn_crumble;

    HACK<uint8_t, 1> reload_instantly;
    std::vector<HACK<uint8_t, 1>> grow_up_quickly;
    std::vector<HACK<uint8_t, 1>> no_cooldown;
    HACK<uint8_t, 1> mushrooms_awake;
    HACK<uint8_t, 1> stop_spawning;
    std::vector<HACK<uint8_t, 1>> stop_zombies;
    HACK<uint8_t, 1> lock_butter;
    HACK<uint8_t, 1> no_crater;
    std::vector<HACK<uint8_t, 1>> no_ice_trail;
    std::vector<HACK<uint8_t, 1>> zombie_not_explode;

    HACK<uint8_t, 1> hack_street_zombies;

    HACK<uint16_t, 1> no_fog;
    HACK<uint32_t, 1> see_vase;
    HACK<uint16_t, 1> background_running;
    HACK<uint8_t, 1> disable_delete_userdata;
    HACK<uint32_t, 1> disable_save_userdata;
    HACK<uint8_t, 3> unlock_limbo_page;

    uintptr_t call_sync_profile;

    uintptr_t call_fade_out_level;

    uintptr_t call_wisdom_tree;

    uintptr_t call_put_plant;
    uintptr_t call_put_plant_imitater;
    uintptr_t call_put_plant_iz_style;
    uintptr_t call_put_zombie;
    uintptr_t call_put_zombie_in_row;
    uintptr_t call_put_grave;
    uintptr_t call_put_ladder;
    uintptr_t call_put_rake;
    uintptr_t call_put_rake_row;
    uintptr_t call_put_rake_col;

    uintptr_t call_start_lawn_mower;
    uintptr_t call_delete_lawn_mower;
    uintptr_t call_restore_lawn_mower;

    uintptr_t call_delete_plant;
    uintptr_t call_delete_grid_item;

    uintptr_t call_set_plant_sleeping;

    uintptr_t call_puzzle_next_stage_clear;
    uintptr_t call_pick_background;
    uintptr_t call_delete_particle_system;

    uintptr_t call_pick_zombie_waves;
    uintptr_t call_remove_cutscene_zombies;
    uintptr_t call_place_street_zombies;

    uintptr_t call_play_music;
};

// ---------------------------------------------------------------------------
// Zombie / Plant 对象内部的字段偏移
//
// 这些没有放进 PVZ_DATA, 因为它们不随版本变化:
// 12 个官方原版 (含各 GOTY) 的 Zombie / Plant 元素布局完全一致 ——
// 各版本里 zombie_dead 都是 0xec、plant_dead 都是 0x141,
// 而下面这些字段都在这些锚点之前, 所以偏移必然相同。
// 两个泄露的测试版布局不同 (zombie_dead=0xf0, plant_dead=0x44),
// 相关功能对它们不生效, 见 PvZ::RandomFixSupported()。
//
// 偏移取自游戏本体结构 (与反编译源码 Zombie.h / Plant.h 的注释互相印证):
//   Zombie::mZombieType +0x24   Zombie::mVelX +0x34 (移速)
//   Plant::mLaunchCounter +0x58 mLaunchRate +0x5c mShootingCounter +0x90
// ---------------------------------------------------------------------------
constexpr uintptr_t ZOMBIE_TYPE_OFFSET = 0x24;
constexpr uintptr_t ZOMBIE_SPEED_OFFSET = 0x34;
// Zombie 与 Plant 都继承 GameObject, 布局同源: 类型字段都在 +0x24,
// 相位 / 列都在 +0x28, 基类里的 mRow 都在 +0x1C (与 data().plant_row 同值)。
constexpr uintptr_t ZOMBIE_ROW_OFFSET = 0x1c;   // GameObject::mRow
constexpr uintptr_t ZOMBIE_POS_X_OFFSET = 0x2c; // Zombie::mPosX

// 用途见 PvZ::FixRandomValues 的注释:
// 僵尸的实际位移取自动画轨道速度, 而它正比于 mVelX, 但游戏只在
// UpdateAnimSpeed() 里由 mVelX 反推动画速率。把 mChilledCounter 置 1,
// 游戏下一帧 UpdatePlaying 会把它减到 0 并顺手调用 UpdateAnimSpeed()。
constexpr uintptr_t ZOMBIE_CHILLED_COUNTER_OFFSET = 0xac;

// ---------------------------------------------------------------------------
// 2026-10-07 新增功能要用的字段 (原来自反编译源码 Zombie.h 的偏移注释,
// 且都在上面说的"各正式版布局一致"的那一段里, 所以直接写成常量、不进版本表):
//   Zombie::mIceTrapCounter  +0xB4  (寒冰菇冻结; IsImmobilizied() 的总开关)
//   Zombie::mMindControlled  +0xB8  (魅惑)
//   Zombie::mBodyHealth      +0xC8 / mBodyMaxHealth +0xCC  (血量倍数)
// 判"僵尸进家"用的是工具早就在读的 Zombie::mPosX (+0x2C) —— 注意**不要**用
// GameObject::mX: 反编译的 GameObject.h 没给这个成员的偏移注释, 之前按 +0x08 猜的
// 结果是读到垃圾值, 僵尸一出生就被判"到家"而秒杀 (用户实测过)。
// ---------------------------------------------------------------------------
constexpr uintptr_t ZOMBIE_ICE_TRAP_OFFSET = 0xb4;
constexpr uintptr_t ZOMBIE_MIND_CONTROLLED_OFFSET = 0xb8;
constexpr uintptr_t ZOMBIE_BODY_HEALTH_OFFSET = 0xc8;
constexpr uintptr_t ZOMBIE_BODY_MAX_HEALTH_OFFSET = 0xcc;
constexpr uintptr_t ZOMBIE_SUMMON_COUNTER_OFFSET = 0x114; // 舞王的召唤/抬手倒计时

// LawnApp 的字段在各版本里是**整体平移**的 (我核对过 game_mode/game_ui/free_planting/
// music/user_data/tod_mode 六个字段: 只有 0x7f8 / 0x7f8+0x120 / 0x7f8+0x120+4 三种形态),
// 所以任何 LawnApp 字段都能用"已知的 game_mode + 固定差值"算出来, 不必补 14 张表:
//   LawnApp::mAppCounter    +0x838  (女仆秘籍要读写的全局帧计数器)
//   LawnApp::mMustacheMode  +0x8B8 … mSukhbirMode +0x8BE  (智慧树指令的 7 个标志位)
// ---------------------------------------------------------------------------
constexpr uintptr_t LAWNAPP_APPCOUNTER_DELTA = 0x838 - 0x7f8;
constexpr uintptr_t LAWNAPP_CHEAT_MODE_DELTA = 0x8b8 - 0x7f8;

// 智慧树三条受门禁的指令 (pinata / dance / daisies) 检查的是
// PlayerInfo::mChallengeRecords[GAMEMODE_TREE_OF_WISDOM - GAMEMODE_SURVIVAL_NORMAL_STAGE_1],
// **不是**工具"智慧树高度"写的那个 tree_height —— 这就是以前这两条点了没反应的根因。
// 反编译 PlayerInfo.h: mLevel +0x4C, mChallengeRecords +0x58 (GOTY), 相差 0xC;
// 工具表里的 data().level 也正是 0x24 / 0x24+0x28 两种形态, 所以这个相对差可以通用。
//   GAMEMODE_SURVIVAL_NORMAL_STAGE_1 = 1, GAMEMODE_TREE_OF_WISDOM = 50 → 下标 49
constexpr uintptr_t PLAYERINFO_RECORDS_DELTA = 0x58 - 0x4c;
constexpr int WISDOM_RECORD_INDEX = 49;

constexpr uintptr_t PLANT_LAUNCH_COUNTER_OFFSET = 0x58;
constexpr uintptr_t PLANT_LAUNCH_RATE_OFFSET = 0x5c;
constexpr uintptr_t PLANT_SHOOTING_COUNTER_OFFSET = 0x90;

// 解谜模式"我是僵尸"的 GameMode 取值区间 (GAMEMODE_PUZZLE_I_ZOMBIE_1 .. _ENDLESS)
constexpr int GAME_MODE_IZOMBIE_FIRST = 61;
constexpr int GAME_MODE_IZOMBIE_LAST = 70;

class Data
{
  public:
    Data();
    ~Data();

#ifdef _PVZ_BETA_LEAK_SUPPORT
    // 是否为测试版
    bool isBETA();
#endif

    // 是否为年度版
    bool isGOTY();

    // 根据版本号获取数据
    PVZ_DATA data();

  protected:
    // 查找结果
    int find_result;

    // 不同版本的内存数据
#ifdef _PVZ_BETA_LEAK_SUPPORT
    PVZ_DATA data_beta_0_1_1_1014_en;
    PVZ_DATA data_beta_0_9_9_1029_en;
#endif
    PVZ_DATA data_1_0_0_1051_en;
    PVZ_DATA data_1_2_0_1065_en;
    PVZ_DATA data_1_0_4_7924_es;
    PVZ_DATA data_1_0_7_3556_es;
    PVZ_DATA data_1_0_7_3467_ru;
    PVZ_DATA data_goty_1_2_0_1073_en;
    PVZ_DATA data_goty_1_2_0_1096_en;
    PVZ_DATA data_goty_1_2_0_1093_de_es_fr_it;
    PVZ_DATA data_goty_1_1_0_1056_zh;
    PVZ_DATA data_goty_1_1_0_1056_ja;
    PVZ_DATA data_goty_1_1_0_1056_zh_2012_06;
    PVZ_DATA data_goty_1_1_0_1056_zh_2012_07;

    // 将版本和对应数据联系起来的键值对
    std::map<int, PVZ_DATA> ver_map;
};

} // namespace Pt
