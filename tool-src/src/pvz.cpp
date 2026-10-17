
#include "../inc/pvz.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib> // getenv (PZTK_CD_RED 二分定位探针)
#include <cstring>
#include <fstream>
#include <limits>

namespace Pt
{

// ★ 2026-10-13: `ZombieType` 的完整取值表在本文件中段 (见 `enum : int` 那张 ZOMBIE_* 表),
//   但 `FrameHacks()` 在它**前面**就要用到"舞王"那一个 —— 所以在这里前向声明一份。
//   真值 = 9 (game-src/ConstEnums.h 的 `enum ZombieType`, 从 `ZOMBIE_NORMAL = 0` 数:
//   NORMAL 0 / FLAG 1 / TRAFFIC_CONE 2 / POLEVAULTER 3 / PAIL 4 / NEWSPAPER 5 / DOOR 6 /
//   FOOTBALL 7 / DANCER 8 …).  ★ 旧代码这里**写死成 8** (= 橄榄球僵尸) ⇒ 女仆秘籍那条
//   "舞王召唤倒计时"路径从来没命中过任何僵尸。
enum : int
{
    ZOMBIE_DANCER_FWD = 9,
};
static_assert(ZOMBIE_DANCER_FWD == 9, "ZOMBIE_DANCER 的真值 = 9 (ConstEnums.h 的 enum ZombieType)");

PvZ::PvZ()
{
    this->cb_find_result = nullptr;
    this->window = nullptr;

    // 观测区间初始化为空
    for (int i = 0; i < ZSC_COUNT; i++)
    {
        this->zombie_observed_min[i] = std::numeric_limits<float>::max();
        this->zombie_observed_max[i] = std::numeric_limits<float>::lowest();
    }

    // 首发延迟还没有应用到场上
    for (int i = 0; i < PSC_COUNT; i++)
    {
        this->windup_applied[i] = -1;
        this->windup_reapply[i] = false;
    }

    // FindPvZ();
}

PvZ::~PvZ()
{
}

void PvZ::asm_code_inject()
{
    // if (GameOn()) // 其他地方预先判断了, 这里其实不用
    {
        enable_hack(data().block_main_loop, true);
        Sleep(GetFrameDuration() * 2);
        Code::asm_code_inject(this->handle);
        enable_hack(data().block_main_loop, false);
    }
}

#ifdef _DEBUG

void PvZ::check_all_hacks()
{
    check_hack<uint8_t, 1>(data().block_main_loop);

    check_hack<uint8_t, 1>(data().unlock_sun_limit);
    check_hack<uint8_t, 1>(data().auto_collected);
    check_hack<uint8_t, 1>(data().not_drop_loot);

    check_hack<uint8_t, 1>(data().fertilizer_unlimited);
    check_hack<uint8_t, 1>(data().bug_spray_unlimited);
    check_hack<uint8_t, 1>(data().chocolate_unlimited);
    check_hack<uint8_t, 1>(data().tree_food_unlimited);

    check_hack<uint8_t, 1>(data().placed_anywhere);
    check_hack<uint8_t, 1>(data().placed_anywhere_preview);
    check_hack<uint8_t, 1>(data().placed_anywhere_iz);
    check_hack<uint8_t, 1>(data().fast_belt);
    check_hack<uint8_t, 1>(data().lock_shovel);

    check_hack<uint16_t, 1>(data().rake_unlimited);
    check_hack<uint8_t, 1>(data().init_lawn_mowers);
    check_hack<uint32_t, 1>(data().lawn_mower_initialize);

    check_hack<uint8_t, 3>(data().plant_immune_eat);
    check_hack<uint8_t, 1>(data().plant_immune_radius);
    check_hack<uint8_t, 1>(data().plant_immune_jalapeno);
    check_hack<uint8_t, 3>(data().plant_immune_projectile);
    check_hack<uint8_t, 3>(data().plant_immune_lob_motion);
    check_hack<uint8_t, 1>(data().plant_immune_square);
    check_hack<uint8_t, 1>(data().plant_immune_row_area);
    check_hack<uint8_t, 1>(data().plant_immune_spike_rock);
    check_hack<uint8_t, 3>(data().plant_immune_squish);

    check_hack<uint8_t, 3>(data()._plant_immune_eat);
    check_hack<uint8_t, 3>(data()._plant_immune_projectile);
    check_hack<uint8_t, 3>(data()._plant_immune_lob_motion);
    check_hack<uint8_t, 1>(data()._plant_immune_row_area);

    check_hack<uint32_t, 1>(data().zombie_immune_body_damage);
    check_hack<uint8_t, 2>(data().zombie_immune_helm_damage);
    check_hack<uint32_t, 1>(data().zombie_immune_shield_damage);
    check_hack<uint8_t, 1>(data().zombie_immune_burn_crumble);
    check_hack<uint8_t, 1>(data().zombie_immune_radius);
    check_hack<uint8_t, 1>(data().zombie_immune_burn_row);
    check_hack<uint8_t, 1>(data().zombie_immune_chomper);
    check_hack<uint8_t, 1>(data().zombie_immune_mind_controll);
    check_hack<uint8_t, 2>(data().zombie_immune_blow_away);
    check_hack<uint8_t, 1>(data().zombie_immune_splash);
    check_hack<uint8_t, 1>(data().zombie_immune_lawn_mower);

    check_hack<uint32_t, 1>(data()._zombie_immune_body_damage);
    check_hack<uint8_t, 2>(data()._zombie_immune_helm_damage);
    check_hack<uint32_t, 1>(data()._zombie_immune_shield_damage);
    check_hack<uint8_t, 1>(data()._zombie_immune_burn_crumble);

    check_hack<uint8_t, 1>(data().reload_instantly);
    check_hack<uint8_t, 1>(data().grow_up_quickly);
    check_hack<uint8_t, 1>(data().mushrooms_awake);
    check_hack<uint8_t, 1>(data().stop_spawning);
    check_hack<uint8_t, 1>(data().stop_zombies);
    check_hack<uint8_t, 1>(data().lock_butter);
    check_hack<uint8_t, 1>(data().no_crater);
    check_hack<uint8_t, 1>(data().no_ice_trail);
    check_hack<uint8_t, 1>(data().zombie_not_explode);

    check_hack<uint8_t, 1>(data().hack_street_zombies);

    check_hack<uint16_t, 1>(data().no_fog);
    check_hack<uint32_t, 1>(data().see_vase);
    check_hack<uint16_t, 1>(data().background_running);
    check_hack<uint8_t, 1>(data().disable_delete_userdata);
    check_hack<uint32_t, 1>(data().disable_save_userdata);
    check_hack<uint8_t, 3>(data().unlock_limbo_page);
}

#endif

void PvZ::callback(cb_func func, void *win)
{
    this->cb_find_result = func;
    this->window = win;
}

bool PvZ::FindPvZ()
{
    this->find_result = PVZ_NOT_FOUND;

    std::vector<std::wstring> pvz_titles = {
#ifdef _PVZ_BETA_LEAK_SUPPORT
        L"Bloom & Doom BETA 0.1.1.1014",       //
        L"Plants vs. Zombies BETA 0.9.9.1029", //
#endif
        L"Plants vs. Zombies",                    //
        L"Plants vs. Zombies 1.2.0.1073",         //
        L"Plants vs. Zombies 1.2.0.1073 RELEASE", //
        L"Plants vs. Zombies GOTY",               //
        L"Pflanzen gegen Zombies 1.2.0.1093",     //
        L"Plantas contra Zombis 1.2.0.1093",      //
        L"Plantes contre Zombies 1.2.0.1093",     //
        L"Piante contro zombi 1.2.0.1093",        //
    };

    for (size_t i = 0; i < pvz_titles.size() + 1; i++)
    {
        bool found = false;

        // 优先查找已知标题, 兼顾其他改动了标题的版本
        if (i != pvz_titles.size())
            found = OpenByWindow(L"MainWindow", pvz_titles[i].c_str());
        else
            found = OpenByWindow(L"MainWindow", nullptr);

        // 注意分支完整
        if (found)
        {
            if (IsValid())
            {
                auto nth = 0x00400000 + ReadMemory<uintptr_t>({0x00400000 + 0x3c});
                auto lcd = 0x00400000 + ReadMemory<uintptr_t>({nth + 0xc8});
                std::string pdb;
                if (lcd != (0x00400000 + 0))
                    pdb = ReadMemory<std::string>({lcd + ReadMemory<uintptr_t>({lcd}) + 0x18});
                // std::cout << pdb << std::endl;
                auto none = std::string::npos;
                if (pdb.empty()                                                        //
                    || (pdb.find(".pdb") == none)                                      //
                    || (pdb.find("\\Lawn\\") == none && pdb.find("\\lawn\\") == none)) //
                {
                    // 找到的可能是其他宝开游戏
                    this->find_result = PVZ_NOT_FOUND;
                }
                else
                {
                    this->find_result = PVZ_UNSUPPORTED;
                }

                // version detection key value
                std::vector<std::tuple<unsigned int, int>> v = {
#ifdef _PVZ_BETA_LEAK_SUPPORT
                    {0x49359c21, PVZ_BETA_0_1_1_1014_EN}, //
                    {0x499a6204, PVZ_BETA_0_9_9_1029_EN}, //
#endif
                    {0x49ecf563, PVZ_1_0_0_1051_EN},               //
                    {0x4a37d6af, PVZ_1_2_0_1065_EN},               //
                    {0x4a5b7963, PVZ_1_0_4_7924_ES},               //
                    {0x4c237519, PVZ_1_0_7_3556_ES},               //
                    {0x4ce4c3d6, PVZ_1_0_7_3467_RU},               //
                    {0x4c2e3453, PVZ_GOTY_1_2_0_1073_EN},          //
                    {0x4d02b058, PVZ_GOTY_1_2_0_1096_EN},          //
                    {0x4ca31baa, PVZ_GOTY_1_2_0_1093_DE_ES_FR_IT}, //
                    {0x4c563de1, PVZ_GOTY_1_1_0_1056_ZH},          //
                    {0x4cc8e5f8, PVZ_GOTY_1_1_0_1056_JA},          //
                    {0x4fcd7be2, PVZ_GOTY_1_1_0_1056_ZH_2012_06},  //
                    {0x5003d437, PVZ_GOTY_1_1_0_1056_ZH_2012_07},  //
                };

                auto time_compiled = ReadMemory<unsigned int>({nth + 0x08});
                for (size_t j = 0; j < v.size(); j++)
                {
                    auto [time_date_stamp, version_name] = v[j];
                    if (time_compiled == time_date_stamp)
                    {
                        this->find_result = version_name;
                        break;
                    }
                }
            }
            else // 没权限拿不到进程句柄
            {
                this->find_result = PVZ_OPEN_ERROR;
            }
        }
        else
        {
            this->find_result = PVZ_NOT_FOUND;
        }

        // 没有找到窗口或者打开进程失败就继续找
        if (this->find_result == PVZ_NOT_FOUND || this->find_result == PVZ_OPEN_ERROR)
            continue;
        // 找到了支持或者不支持的版本则终止循环
        else
            break;
    }

    bool supported = this->find_result != PVZ_NOT_FOUND     //
                     && this->find_result != PVZ_OPEN_ERROR //
                     && this->find_result != PVZ_UNSUPPORTED;

    if (!supported)
    {
        if (IsValid())
            CloseHandle(this->handle);
        this->hwnd = nullptr;
        this->pid = 0;
        this->handle = nullptr;
    }

#ifdef _DEBUG
    if (supported)
    {
        check_all_hacks();
        WriteMemory<bool>(true, {data().lawn, data().tod_mode});
    }
#endif

    if (cb_find_result != nullptr && this->window != nullptr)
        cb_find_result(this->window, this->find_result);

    return supported;
}

bool PvZ::GameOn()
{
    bool on = this->find_result != PVZ_NOT_FOUND      //
              && this->find_result != PVZ_OPEN_ERROR  //
              && this->find_result != PVZ_UNSUPPORTED //
              && IsValid();

    if (on)
    {
#ifdef _DEBUG
        std::wcout << L"游戏已经打开, 可以修改." << std::endl;
#endif
    }
    else
    {
        on = FindPvZ();
#ifdef _DEBUG
        std::wcout << L"游戏没有正常打开, 重新查找: " << (on ? L"找到了" : L"没找到") << std::endl;
#endif
    }

    return on;
}

std::string PvZ::GamePath()
{
    return ReadMemory<std::string>({data().path});
}

int PvZ::GameMode()
{
    return ReadMemory<int>({data().lawn, data().game_mode});
}

int PvZ::GameUI()
{
    return ReadMemory<int>({data().lawn, data().game_ui});
}

int PvZ::GetScene()
{
    int scene = -1;

    int ui = GameUI();
    if (ui == 2 || ui == 3)
        scene = ReadMemory<int>({data().lawn, data().board, data().scene});

    return scene;
}

void PvZ::SetScene(int scene, bool reset)
{
    if (scene < 0 || scene > 5)
        return;

    if (!GameOn())
        return;

    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    auto has_lawn_mower = //
        ReadMemory<uint32_t>({data().lawn, data().board, data().lawn_mower_count}) > 0;

    int music_id = scene + 1;
    if (music_id == 6)
        music_id = 2;

    if (GetScene() == scene)
        goto reset_scene;

    if (has_lawn_mower)
        SetLawnMowers(1);

#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
    {
        unsigned int scene_id[6] = {1, 2, 3, 4, 5, 7};
        if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        {
            WriteMemory<uint8_t, 7>({0xb8, 0x03, 0x00, 0x00, 0x00, 0x90, 0x90}, {0x004103e1});
            WriteMemory<uint32_t>(scene_id[scene], {0x004103e1 + 1});
        }
        else if (this->find_result == PVZ_BETA_0_9_9_1029_EN)
        {
            WriteMemory<uint8_t, 7>({0xb8, 0x03, 0x00, 0x00, 0x00, 0x90, 0x90}, {0x00416e31});
            WriteMemory<uint32_t>(scene_id[scene], {0x00416e31 + 1});
        }
    }
#endif

    asm_init();
    asm_mov_exx_dword_ptr(Reg::ESI, data().lawn);
    asm_mov_exx_dword_ptr_exx_add(Reg::ESI, data().board);
    asm_add_list({0xc7, 0x86});  // mov [esi+0000554C],scene
    asm_add_dword(data().scene); //
    asm_add_dword(scene);        //
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
        asm_mov_exx_exx(Reg::ECX, Reg::ESI);
#endif
    asm_call(data().call_pick_background);
    asm_ret();
    asm_code_inject();

#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
    {
        if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
            // movzx eax,byte ptr [eax+00410A50]
            WriteMemory<uint8_t, 7>({0x0f, 0xb6, 0x80, 0x50, 0x0a, 0x41, 0x00}, {0x004103e1});
        else if (this->find_result == PVZ_BETA_0_9_9_1029_EN)
            // movzx eax,byte ptr [eax+00417480]
            WriteMemory<uint8_t, 7>({0x0f, 0xb6, 0x80, 0x80, 0x74, 0x41, 0x00}, {0x00416e31});
    }
#endif

    // 1.lawn 2.bare 3.pool
    // 0.none 1.land 2.water
    switch (scene)
    {
    case 0:
    case 1:
    case 4:
    case 5:
        WriteMemory<int, 6 * 9>({1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2,
                                 1, 1, 1, 1, 1, 2},
                                {data().lawn, data().board, data().block_type});
        WriteMemory<int, 6>({1, 1, 1, 1, 1, 0},
                            {data().lawn, data().board, data().row_type});
        break;
    case 2:
    case 3:
    default:
        WriteMemory<int, 6 * 9>({1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1,
                                 1, 1, 3, 3, 1, 1},
                                {data().lawn, data().board, data().block_type});
        WriteMemory<int, 6>({1, 1, 2, 2, 1, 1},
                            {data().lawn, data().board, data().row_type});
        break;
    }

    if (has_lawn_mower)
        SetLawnMowers(2);

    // 泳池和雾夜仍然保留水波光
    if (scene != 2 && scene != 3)
    {
        unsigned int particle_system_struct_size = 0x2c;
        asm_init();
        auto particle_system_offset = ReadMemory<uintptr_t>({data().lawn, data().anim, data().unnamed, data().particle_system});
        auto particle_system_count_max = ReadMemory<uintptr_t>({data().lawn, data().anim, data().unnamed, data().particle_system_count_max});
        for (size_t i = 0; i < particle_system_count_max; i++)
        {
            auto particle_system_type = ReadMemory<int>({particle_system_offset + data().particle_system_type + particle_system_struct_size * i});
            auto particle_system_dead = ReadMemory<bool>({particle_system_offset + data().particle_system_dead + particle_system_struct_size * i});
            if (!particle_system_dead && particle_system_type == 34)
            {
                uintptr_t addr = particle_system_offset + particle_system_struct_size * i;
#ifdef _PVZ_BETA_LEAK_SUPPORT
                if (isBETA())
                    asm_mov_exx(Reg::ECX, addr);
                else
#endif
                    asm_push_dword(addr);
                asm_call(data().call_delete_particle_system);
            }
        }
        asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
        asm_add_list({0xc7, 0x80});                  // mov [eax+00005620],00000000
        asm_add_dword(data().particle_systems_addr); //
        asm_add_dword(0);                            //
        asm_ret();
        asm_code_inject();
    }

    SetMusic(music_id);

reset_scene:
    if (reset)
    {
        asm_init();
        asm_mov_exx_dword_ptr(Reg::EDI, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EDI, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::EDI, data().challenge);
        asm_add_list({0xff, 0x8f}); // dec [edi+0000006c]
        asm_add_dword(data().endless_rounds);
        if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
            this->find_result == PVZ_GOTY_1_1_0_1056_JA)
            asm_push_exx(Reg::EDI);
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
            asm_mov_exx_exx(Reg::ECX, Reg::EDI);
#endif
        asm_call(data().call_puzzle_next_stage_clear);
        asm_ret();
        asm_code_inject();
    }
}

int PvZ::GetRowCount()
{
    int scene = GetScene();
    return (scene == 2 || scene == 3) ? 6 : 5;
}

// 以下是修改功能

void PvZ::UnlockTrophy()
{
    if (!GameOn())
        return;

    auto userdata = ReadMemory<uintptr_t>({data().lawn, data().user_data});
    if (userdata == 0) // 还没建立用户
        return;

    auto playthrough = userdata + data().playthrough;

    // Adventure
    int adventure_playthrough = ReadMemory<int>({playthrough});
    if (adventure_playthrough < 2)
        WriteMemory<int>(2, {playthrough});
    playthrough += 1 * sizeof(int);

    ///

    auto mini_games = userdata + data().mini_games;

    // Survival
    // Survival Hard
    WriteMemory<int, 5 + 5>({5, 5, 5, 5, 5, 10, 10, 10, 10, 10}, {mini_games});
    mini_games += (5 + 5) * sizeof(int);

    // Survival Endless
    mini_games += 5 * sizeof(int);

    // Mini-games
    WriteMemory<int, 20>({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {mini_games});
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        WriteMemory<int>(5, {mini_games + 18 * sizeof(int)});
    else if (this->find_result == PVZ_BETA_0_9_9_1029_EN)
        WriteMemory<int>(5, {mini_games + 15 * sizeof(int)});
#endif
    mini_games += 20 * sizeof(int);

    // Hidden Mini-games
    // Squirrel
    mini_games += (13 + 1) * sizeof(int);

    // Wisdom Tree (Height)
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result != PVZ_BETA_0_1_1_1014_EN)
#endif
        mini_games += 1 * sizeof(int);

    // Vasebreaker
    WriteMemory<int, 9>({1, 1, 1, 1, 1, 1, 1, 1, 1}, {mini_games});
    mini_games += 9 * sizeof(int);

    // Vasebreaker Endless
    mini_games += 1 * sizeof(int);

    // I, Zombie
    WriteMemory<int, 9>({1, 1, 1, 1, 1, 1, 1, 1, 1}, {mini_games});
    mini_games += 9 * sizeof(int);

    // I, Zombie Endless
    mini_games += 1 * sizeof(int);

    ////

    auto twiddydinky = userdata + data().twiddydinky;

    // Gatling Pea
    // Twin Sunflower
    // Gloom-shroom
    // Cattail
    // Winter Melon
    // Gold Magnet
    // Spikerock
    // Cob Cannon
    WriteMemory<int, 8>({1, 1, 1, 1, 1, 1, 1, 1}, {twiddydinky});
    twiddydinky += 8 * sizeof(int);

    // Imitater
    WriteMemory<int>(1, {twiddydinky});
    twiddydinky += 1 * sizeof(int);

    // unknown
    // Marigold Sprout x3
    // Golden Watering Can
    // Fertilizer
    // Bug Spray
    // Phonograph
    // Gardening Glove
    // Mushroom Garden
    // Wheel Barrow
    // Snail
    twiddydinky += (1 + 3 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1) * sizeof(int);

    // 6+4 seed slots
    WriteMemory<int>(4, {twiddydinky});
    twiddydinky += 1 * sizeof(int);

    // Pool Cleaner
    WriteMemory<int>(1, {twiddydinky});
    twiddydinky += 1 * sizeof(int);

    // Roof Cleaner
    WriteMemory<int>(1, {twiddydinky});
    twiddydinky += 1 * sizeof(int);

    // Garden Rake
    // Aquarium Garden
    // Chocolate
    twiddydinky += (1 + 1 + 1) * sizeof(int);

#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result != PVZ_BETA_0_1_1_1014_EN)
#endif
    {
        // Tree of Wisdom
        WriteMemory<int>(1, {twiddydinky});
        twiddydinky += 1 * sizeof(int);

        // Tree Food
        twiddydinky += 1 * sizeof(int);

        // Wall-nut First Aid
        WriteMemory<int>(1, {twiddydinky});
        twiddydinky += 1 * sizeof(int);
    }

    // 年度版成就
    if (isGOTY())
    {
        auto achievement = userdata + 0x24;
        if (this->find_result == PVZ_GOTY_1_2_0_1096_EN)
        {
            std::array<bool, 21> values{};
            values.fill(true);
            WriteMemory(values, {achievement});
        }
        else
        {
            std::array<bool, 20> values{};
            values.fill(true);
            WriteMemory(values, {achievement});
        }
    }

    // 刷新主界面
    if (adventure_playthrough == 0 && GameUI() == 1)
    {
        // TODO 只能在主界面
        asm_init();
        asm_push_byte(1); // 显示 Loading
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().game_selector);
        if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
            this->find_result == PVZ_GOTY_1_1_0_1056_JA)
            asm_mov_exx_exx(Reg::ESI, Reg::ECX);
        asm_call(data().call_sync_profile);
        asm_ret();
        asm_code_inject();
    }
}

void PvZ::DirectWin(bool brightest_cob_cannon)
{
    if (!GameOn())
        return;
    if (GameUI() != 3)
        return;

    int frame_time = GetFrameDuration();

    int mode = GameMode();
    bool light_cob = brightest_cob_cannon && 1 <= mode && mode <= 15;

    if (light_cob)
    {
        int game_clock = ReadMemory<int>({data().lawn, data().board, data().game_clock});
        int frame_to_wait = 75 - ((game_clock + 500) % 75);
        if (ReadMemory<bool>({data().lawn, data().board, data().game_paused}))
        {
            if (frame_to_wait != 0)
            {
                // 解除暂停
                PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 0);
                PostMessage(hwnd, WM_KEYUP, VK_SPACE, 0);
                Sleep(frame_to_wait * frame_time);
                // 暂停
                PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 0);
                PostMessage(hwnd, WM_KEYUP, VK_SPACE, 0);
            }
        }
        else
        {
            Sleep(frame_to_wait * frame_time);
            // 暂停
            PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 0);
            PostMessage(hwnd, WM_KEYUP, VK_SPACE, 0);
        }
        Sleep(frame_time);
    }

    if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
        this->find_result == PVZ_GOTY_1_1_0_1056_JA)
    {
        asm_init();
        asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
        asm_push_exx(Reg::EAX);
        asm_call(data().call_fade_out_level);
        asm_ret();
        asm_code_inject();
    }
    else
    {
        asm_init();
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_call(data().call_fade_out_level);
        asm_ret();
        asm_code_inject();
    }

    if (light_cob)
    {
        Sleep(frame_time);
        // 解除暂停
        PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 0);
        PostMessage(hwnd, WM_KEYUP, VK_SPACE, 0);
    }

    // #ifdef _DEBUG
    //     Sleep(frame_time * (500 + 10));
    //     std::cout << ReadMemory<int>({data().lawn, data().board, data().game_clock}) % 75 << std::endl;
    // #endif

    // unsigned int slot_seed_struct_size = 0x50;
    // std::vector<int> seed_types = {40, 41, 42, 43, 44, 45, 46, 47, 8, 48};
    // std::vector<int> seed_types_im = {-1, -1, -1, -1, -1, -1, -1, -1, -1, 8};
    // auto slots_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().slot});
    // for (size_t i = 0; i < 10; i++)
    // {
    //     WriteMemory<int>(seed_types[i], {slots_offset + data().slot_seed_type + i * slot_seed_struct_size});
    //     WriteMemory<int>(seed_types_im[i], {slots_offset + data().slot_seed_type_im + i * slot_seed_struct_size});
    // }
}

void PvZ::UnlockSunLimit(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().unlock_sun_limit, on);
}

void PvZ::SetSun(int sun)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    WriteMemory<int>(sun, {data().lawn, data().board, data().sun});
}

void PvZ::SetMoney(int money)
{
    if (!GameOn())
        return;

    if (ReadMemory<uintptr_t>({data().lawn, data().user_data}) != 0)
        WriteMemory<int>(money, {data().lawn, data().user_data, data().money});
}

void PvZ::AutoCollected(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().auto_collected, on);
}

void PvZ::NotDropLoot(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().not_drop_loot, on);
}

void PvZ::FertilizerUnlimited(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().fertilizer_unlimited, on);
    if (on && ReadMemory<uintptr_t>({data().lawn, data().user_data}) != 0)
        WriteMemory<int>(1000 + 20, {data().lawn, data().user_data, data().twiddydinky + 14 * 4});
}

void PvZ::BugSprayUnlimited(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().bug_spray_unlimited, on);
    if (on && ReadMemory<uintptr_t>({data().lawn, data().user_data}) != 0)
        WriteMemory<int>(1000 + 20, {data().lawn, data().user_data, data().twiddydinky + 15 * 4});
}

void PvZ::ChocolateUnlimited(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().chocolate_unlimited, on);
    if (on && ReadMemory<uintptr_t>({data().lawn, data().user_data}) != 0)
        WriteMemory<int>(1000 + 10, {data().lawn, data().user_data, data().twiddydinky + 26 * 4});
}

void PvZ::TreeFoodUnlimited(bool on)
{
    if (!GameOn())
        return;

#ifdef _PVZ_BETA_LEAK_SUPPORT
    // 早期测试版没有智慧树
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        return;
#endif

    enable_hack(data().tree_food_unlimited, on);
    if (on && ReadMemory<uintptr_t>({data().lawn, data().user_data}) != 0)
        WriteMemory<int>(1000 + 10, {data().lawn, data().user_data, data().twiddydinky + 28 * 4});
}

void PvZ::SetTreeHeight(int height)
{
    if (!GameOn())
        return;

    if (ReadMemory<uintptr_t>({data().lawn, data().user_data}) == 0)
        return;

#ifdef _PVZ_BETA_LEAK_SUPPORT
    // 早期测试版没有智慧树
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        return;
#endif

    if (GameMode() == 50) // Zen Garden
    {
        WriteMemory<int>(height - 1, {data().lawn, data().user_data, data().tree_height});

        if (isGOTY())
        {
            if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
                this->find_result == PVZ_GOTY_1_1_0_1056_JA)
            {
                asm_init();
                asm_mov_exx_dword_ptr(Reg::EBX, data().lawn);
                asm_mov_exx_dword_ptr_exx_add(Reg::EBX, data().board);
                asm_mov_exx_dword_ptr_exx_add(Reg::EBX, data().challenge);
                asm_push_exx(Reg::EBX);
                asm_call(data().call_wisdom_tree);
                asm_ret();
                asm_code_inject();
            }
            else if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH_2012_06 || //
                     this->find_result == PVZ_GOTY_1_1_0_1056_ZH_2012_07)
            {
                asm_init();
                asm_mov_exx_dword_ptr(Reg::EDI, data().lawn);
                asm_mov_exx_dword_ptr_exx_add(Reg::EDI, data().board);
                asm_mov_exx_dword_ptr_exx_add(Reg::EDI, data().challenge);
                asm_call(data().call_wisdom_tree);
                asm_ret();
                asm_code_inject();
            }
            else
            {
                asm_init();
                asm_mov_exx_dword_ptr(Reg::ESI, data().lawn);
                asm_mov_exx_dword_ptr_exx_add(Reg::ESI, data().board);
                asm_mov_exx_dword_ptr_exx_add(Reg::ESI, data().challenge);
                asm_call(data().call_wisdom_tree);
                asm_ret();
                asm_code_inject();
            }
        }
        else
        {
#ifdef _PVZ_BETA_LEAK_SUPPORT
            if (this->find_result == PVZ_BETA_0_9_9_1029_EN)
            {
                asm_init();
                asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
                asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
                asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().challenge);
                asm_call(data().call_wisdom_tree);
                asm_ret();
                asm_code_inject();
            }
            else
#endif
            {
                asm_init();
                asm_mov_exx(Reg::EDI, data().lawn);
                asm_call(data().call_wisdom_tree);
                asm_ret();
                asm_code_inject();
            }
        }
    }
    else
    {
        if (ReadMemory<uintptr_t>({data().lawn, data().user_data}) != 0)
            WriteMemory<int>(height, {data().lawn, data().user_data, data().tree_height});
    }
}

void PvZ::FreePlanting(bool on)
{
    if (!GameOn())
        return;

    WriteMemory<int>(static_cast<int>(on), {data().lawn, data().free_planting});

    if (GameUI() != 3)
        return;

    unsigned int slot_seed_struct_size = 0x50;

    auto slot_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().slot});
    auto slot_count = ReadMemory<uint32_t>({slot_offset + data().slot_count});
    for (size_t i = 0; i < slot_count; i++)
    {
        int slot_seed_cd_total = ReadMemory<int>({slot_offset + data().slot_seed_cd_total + slot_seed_struct_size * i});
        WriteMemory<int>(slot_seed_cd_total, {slot_offset + data().slot_seed_cd_past + slot_seed_struct_size * i});
    }
}

void PvZ::PlacedAnywhere(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().placed_anywhere, on);
    enable_hack(data().placed_anywhere_preview, on);
    enable_hack(data().placed_anywhere_iz, on);
}

void PvZ::FastBelt(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().fast_belt, on);
}

void PvZ::LockShovel(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().lock_shovel, on);

    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    auto cursor_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().cursor});
    if (on)
    {
        WriteMemory<int>(6, {cursor_offset + data().cursor_grab});
    }
    else
    {
        if (ReadMemory<int>({cursor_offset + data().cursor_grab}) == 6)
            WriteMemory<int>(0, {cursor_offset + data().cursor_grab});
    }
}

void PvZ::MixMode(int mode, int level)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    if (mode == 0) // 冒险模式
    {
        WriteMemory<int>(level, {data().lawn, data().user_data, data().level});
        WriteMemory<int>(level, {data().lawn, data().board, data().adventure_level});
    }
    WriteMemory<int>(mode, {data().lawn, data().game_mode});
}

void PvZ::EndlessRounds(int level)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    int mode = GameMode();
    if (mode == 60 || mode == 70 || (mode >= 11 && mode <= 15)) // 仅限无尽模式
    {
        auto indirect_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().challenge});
        WriteMemory<int>(level, {indirect_offset + data().endless_rounds});
    }
}

void PvZ::asm_put_plant(int row, int col, int type, bool imitater, bool iz_style)
{
    if (imitater)
    {
        asm_push_dword(type);
        asm_push_dword(48);
    }
    else
    {
        asm_push_dword(-1);
        asm_push_dword(type);
    }
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
    {
        asm_push_dword(row);
        asm_push_dword(col);
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
    }
    else
#endif
    {
        asm_mov_exx(Reg::EAX, row);
        asm_push_dword(col);
        asm_mov_exx_dword_ptr(Reg::EBP, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EBP, data().board);
        asm_push_exx(Reg::EBP);
    }
    asm_call(data().call_put_plant);

    // 多余的过程是为了让 eax 值为目标植物的地址以供后续的布阵函数使用

    if (imitater)
    {
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().plant);
        asm_mov_exx_dword_ptr(Reg::EBX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EBX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::EBX, data().plant_next_pos);
        asm_add_list(0x69, 0xdb, 0x4c, 0x01, 0x00, 0x00); // imul ebx,ebx,plant_struct_size
        asm_add_list(0x01, 0xd9);                         // add ecx,ebx
        asm_push_exx(Reg::ECX);
        asm_mov_exx_exx(Reg::ESI, Reg::EAX);
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
            asm_mov_exx_exx(Reg::ECX, Reg::EAX);
#endif
        asm_call(data().call_put_plant_imitater);
        asm_pop_exx(Reg::ECX);
        asm_mov_exx_exx(Reg::EAX, Reg::ECX);
    }

    if (iz_style)
    {
        asm_mov_exx_exx(Reg::ESI, Reg::EAX);
        asm_push_exx(Reg::EAX);
        asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().challenge);
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
            asm_mov_exx_exx(Reg::ECX, Reg::EAX);
#endif
        asm_call(data().call_put_plant_iz_style);
        asm_mov_exx_exx(Reg::EAX, Reg::ESI);
    }
}

void PvZ::PutPlant(int row, int col, int type, bool imitater)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    int row_count = GetRowCount();        // 行数
    int col_count = (type == 47 ? 8 : 9); // 玉米加农炮不种在九列
    int width = (type == 47 ? 2 : 1);     // 玉米加农炮宽度两列
    int mode = GameMode();
    bool iz_style = (mode >= 61 && mode <= 70);
    asm_init();
    if (row == -1 && col == -1)
        for (int r = 0; r < row_count; r++)
            for (int c = 0; c < col_count; c += width)
                asm_put_plant(r, c, type, imitater, iz_style);
    else if (row != -1 && col == -1)
        for (int c = 0; c < col_count; c += width)
            asm_put_plant(row, c, type, imitater, iz_style);
    else if (row == -1 && col != -1)
        for (int r = 0; r < row_count; r++)
            asm_put_plant(r, col, type, imitater, iz_style);
    else
        asm_put_plant(row, col, type, imitater, iz_style);
    asm_ret();
    asm_code_inject();
}

void PvZ::asm_put_zombie(int row, int col, int type)
{
    if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
        this->find_result == PVZ_GOTY_1_1_0_1056_JA)
    {
        asm_push_dword(type); // 0x6a byte(type)
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().challenge);
        asm_push_exx(Reg::ECX);
        asm_mov_exx(Reg::EAX, row);
        asm_mov_exx(Reg::ECX, col);
        asm_call(data().call_put_zombie);
    }
#ifdef _PVZ_BETA_LEAK_SUPPORT
    else if (isBETA())
    {
        asm_push_dword(row);
        asm_push_dword(col);
        asm_push_dword(type);
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().challenge);
        asm_call(data().call_put_zombie);
    }
#endif
    else
    {
        asm_push_dword(col);
        asm_push_dword(type);
        asm_mov_exx(Reg::EAX, row);
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().challenge);
        asm_call(data().call_put_zombie);
    }
}

void PvZ::PutZombie(int row, int col, int type)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    if (type == 25) // 僵王
    {
        asm_init();
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
        {
            asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
            asm_push_dword(0);
            asm_push_dword(0);
            asm_push_dword(25);
        }
        else
#endif
        {
            asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
            asm_push_dword(0);
            asm_push_dword(25);
        }
        asm_call(data().call_put_zombie_in_row);
        asm_ret();
        asm_code_inject();
        return;
    }
    int row_count = GetRowCount();
    int col_count = 9;
    asm_init();
    if (row == -1 && col == -1)
        for (int r = 0; r < row_count; r++)
            for (int c = 0; c < col_count; c++)
                asm_put_zombie(r, c, type);
    else if (row != -1 && col == -1)
        for (int c = 0; c < col_count; c++)
            asm_put_zombie(row, c, type);
    else if (row == -1 && col != -1)
        for (int r = 0; r < row_count; r++)
            asm_put_zombie(r, col, type);
    else
        asm_put_zombie(row, col, type);
    asm_ret();
    asm_code_inject();
}

void PvZ::asm_put_grave(int row, int col)
{
    if (isGOTY())
    {
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().challenge);
        asm_push_exx(Reg::ECX);
        asm_mov_exx(Reg::EDI, row);
        asm_mov_exx(Reg::EBX, col);
        asm_call(data().call_put_grave);
    }
#ifdef _PVZ_BETA_LEAK_SUPPORT
    else if (isBETA())
    {
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().challenge);
        asm_push_dword(row);
        asm_push_dword(col);
        asm_call(data().call_put_grave);
    }
#endif
    else
    {
        asm_mov_exx_dword_ptr(Reg::EDX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EDX, data().board);
        asm_mov_exx_dword_ptr_exx_add(Reg::EDX, data().challenge);
        asm_push_exx(Reg::EDX);
        asm_mov_exx(Reg::EDI, row);
        asm_mov_exx(Reg::EBX, col);
        asm_call(data().call_put_grave);
    }
}

void PvZ::PutGrave(int row, int col)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    int row_count = GetRowCount();
    int col_count = 9;
    asm_init();
    if (row == -1 && col == -1)
        for (int r = 0; r < row_count; r++)
            for (int c = 0; c < col_count; c++)
                asm_put_grave(r, c);
    else if (row != -1 && col == -1)
        for (int c = 0; c < col_count; c++)
            asm_put_grave(row, c);
    else if (row == -1 && col != -1)
        for (int r = 0; r < row_count; r++)
            asm_put_grave(r, col);
    else
        asm_put_grave(row, col);
    asm_ret();
    asm_code_inject();
}

void PvZ::asm_put_ladder(int row, int col)
{
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
    {
        asm_push_dword(row);
        asm_push_dword(col);
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        asm_call(data().call_put_ladder);
    }
    else
#endif
    {
        asm_mov_exx(Reg::EDI, row);
        asm_push_dword(col);
        asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
        asm_call(data().call_put_ladder);
    }
}

void PvZ::PutLadder(int row, int col)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    int row_count = GetRowCount();
    int col_count = 9;
    asm_init();
    if (row == -1 && col == -1)
        for (int r = 0; r < row_count; r++)
            for (int c = 0; c < col_count; c++)
                asm_put_ladder(r, c);
    else if (row != -1 && col == -1)
        for (int c = 0; c < col_count; c++)
            asm_put_ladder(row, c);
    else if (row == -1 && col != -1)
        for (int r = 0; r < row_count; r++)
            asm_put_ladder(r, col);
    else
        asm_put_ladder(row, col);
    asm_ret();
    asm_code_inject();
}

void PvZ::AutoLadder(bool imitater_pumpkin_only = true)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    ClearGridItems({3}); // 清空所有梯子

    unsigned int plant_struct_size = 0x14c;

    auto plant_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});
    auto plant_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});

    asm_init();
    for (size_t i = 0; i < plant_count_max; i++)
    {
        auto plant_dead = ReadMemory<bool>({plant_offset + data().plant_dead + plant_struct_size * i});
        auto plant_squished = ReadMemory<bool>({plant_offset + data().plant_squished + plant_struct_size * i});
        auto plant_type = ReadMemory<uint32_t>({plant_offset + data().plant_type + plant_struct_size * i});
        if (!plant_dead && !plant_squished && plant_type == 30) // 30 南瓜
        {
            auto plant_row = ReadMemory<uint32_t>({plant_offset + data().plant_row + plant_struct_size * i});
            auto plant_col = ReadMemory<uint32_t>({plant_offset + data().plant_col + plant_struct_size * i});
            auto plant_imitater = //
                ReadMemory<int>({plant_offset + data().plant_imitater + plant_struct_size * i}) == 48;
            // 1.草地 2.裸地 3.泳池
            auto block_type = ReadMemory<int>({data().lawn, data().board,                                 //
                                               data().block_type + 0x04 * plant_row + 0x18 * plant_col}); //
            if (plant_col != 0 && block_type == 1                                                         //
                && (!imitater_pumpkin_only || (imitater_pumpkin_only && plant_imitater)))                 //
            {
#ifdef _DEBUG
                std::wcout << L"搭梯: " << (plant_row + 1) << L" " << (plant_col + 1) << std::endl;
#endif
                asm_put_ladder(plant_row, plant_col);
            }
        }
    }
    asm_ret();
    asm_code_inject();
}

void PvZ::asm_put_rake(int row, int col)
{
    WriteMemory<int>(row, {data().call_put_rake_row + 1});
    WriteMemory<int>(col, {data().call_put_rake_col + 4});

    asm_init();
    if (isGOTY())
    {
        if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
            this->find_result == PVZ_GOTY_1_1_0_1056_JA)
        {
            asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
            asm_push_exx(Reg::ECX);
        }
        else
        {
            asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
        }
    }
    else
    {
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
        {
            asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        }
        else
#endif
        {
            asm_mov_exx_dword_ptr(Reg::EDX, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::EDX, data().board);
            asm_push_exx(Reg::EDX);
        }
    }
    asm_call(data().call_put_rake);
    asm_ret();
    asm_code_inject();
}

void PvZ::PutRake(int row, int col)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    std::array<uint8_t, 8> reset_code_rake_col = {0};
    std::array<uint8_t, 7> reset_code_rake_row = {0};
    std::array<uint8_t, 6> reset_code_rake_row_goty = {0};

    reset_code_rake_col = ReadMemory<uint8_t, 8>({data().call_put_rake_col});
    if (!isGOTY())
        reset_code_rake_row = ReadMemory<uint8_t, 7>({data().call_put_rake_row});
    else
        reset_code_rake_row_goty = ReadMemory<uint8_t, 6>({data().call_put_rake_row});

    if (!isGOTY())
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
            WriteMemory<uint8_t, 7>({0xbd, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90}, {data().call_put_rake_row});
        else
#endif
            WriteMemory<uint8_t, 7>({0xba, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90}, {data().call_put_rake_row});
    else
        WriteMemory<uint8_t, 6>({0xbf, 0x00, 0x00, 0x00, 0x00, 0x90}, {data().call_put_rake_row});

    enable_hack(data().rake_unlimited, true);

    int row_count = GetRowCount();
    int col_count = 9;
    if (row == -1 && col == -1)
        for (int r = 0; r < row_count; r++)
            for (int c = 0; c < col_count; c++)
                asm_put_rake(r, c);
    else if (row != -1 && col == -1)
        for (int c = 0; c < col_count; c++)
            asm_put_rake(row, c);
    else if (row == -1 && col != -1)
        for (int r = 0; r < row_count; r++)
            asm_put_rake(r, col);
    else
        asm_put_rake(row, col);

    enable_hack(data().rake_unlimited, false);

    WriteMemory<uint8_t, 8>(reset_code_rake_col, {data().call_put_rake_col});
    if (!isGOTY())
        WriteMemory<uint8_t, 7>(reset_code_rake_row, {data().call_put_rake_row});
    else
        WriteMemory<uint8_t, 6>(reset_code_rake_row_goty, {data().call_put_rake_row});
}

// 0.启动 1.删除 2.恢复
void PvZ::SetLawnMowers(int option)
{
    // #ifdef _DEBUG
    //     HACK<uint32_t, 1> test_hack = data().lawn_mower_initialize;
    //     assert(ReadMemory<uint32_t>({test_hack.mem_addr}) == test_hack.reset_value[0]);
    //     assert(ReadMemory<float>({test_hack.hack_value[0]}) == -21.00f);
    //     assert(ReadMemory<float>({test_hack.reset_value[0]}) == -160.00f);
    // #endif

    if (option != 0 && option != 1 && option != 2)
        return;

    if (!GameOn())
        return;
    if (GameUI() != 3)
        return;

    unsigned int lawn_mower_struct_size = 0x48;

    auto lawn_mower_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().lawn_mower_count_max});
    auto lawn_mower_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().lawn_mower});

    if (option == 2)
    {
        enable_hack(data().init_lawn_mowers, true);
        enable_hack(data().lawn_mower_initialize, true);
    }

    asm_init();
    for (size_t i = 0; i < lawn_mower_count_max; i++)
    {
        auto lawn_mower_dead = ReadMemory<bool>({lawn_mower_offset + data().lawn_mower_dead + lawn_mower_struct_size * i});
        if (!lawn_mower_dead)
        {
            uint32_t addr = lawn_mower_offset + lawn_mower_struct_size * i;
            if (option == 0)
            {
                if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
                    this->find_result == PVZ_GOTY_1_1_0_1056_JA)
                    asm_mov_exx(Reg::EBX, addr);
#ifdef _PVZ_BETA_LEAK_SUPPORT
                else if (isBETA())
                    asm_mov_exx(Reg::ECX, addr);
#endif
                else
                    asm_mov_exx(Reg::ESI, addr);
                asm_call(data().call_start_lawn_mower);
            }
            else
            {
#ifdef _PVZ_BETA_LEAK_SUPPORT
                if (isBETA())
                    asm_mov_exx(Reg::ECX, addr);
                else
#endif
                    asm_mov_exx(Reg::EAX, addr);
                asm_call(data().call_delete_lawn_mower);
            }
        }
    }
    if (option == 2)
    {
        asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
            asm_mov_exx_exx(Reg::ECX, Reg::EAX);
        else
#endif
            asm_push_exx(Reg::EAX);
        asm_call(data().call_restore_lawn_mower);
    }
    asm_ret();
    asm_code_inject();

    if (option == 2)
    {
        enable_hack(data().init_lawn_mowers, false);
        enable_hack(data().lawn_mower_initialize, false);
    }
}

void PvZ::ClearAllPlants()
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    unsigned int plant_struct_size = 0x14c;

    auto plant_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});
    auto plant_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});

    asm_init();
    for (size_t i = 0; i < plant_count_max; i++)
    {
        auto plant_dead = ReadMemory<bool>({plant_offset + data().plant_dead + plant_struct_size * i});
        auto plant_squished = ReadMemory<bool>({plant_offset + data().plant_squished + plant_struct_size * i});
        if (!plant_dead && !plant_squished)
        {
            uint32_t addr = plant_offset + plant_struct_size * i;
#ifdef _PVZ_BETA_LEAK_SUPPORT
            if (isBETA())
                asm_mov_exx(Reg::ECX, addr);
            else
#endif
                asm_push_dword(addr);
            asm_call(data().call_delete_plant);
        }
    }
    asm_ret();
    asm_code_inject();
}

void PvZ::KillAllZombies()
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    unsigned int zombie_struct_size = isGOTY() ? 0x168 : 0x15c;
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        zombie_struct_size = 0x160;
#endif

    auto zombie_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().zombie_count_max});
    auto zombie_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().zombie});
    for (size_t i = 0; i < zombie_count_max; i++)
    {
        if (!ReadMemory<bool>({zombie_offset + data().zombie_dead + i * zombie_struct_size}))     // 没有消失
            WriteMemory<int>(3, {zombie_offset + data().zombie_status + i * zombie_struct_size}); // 3 秒杀
    }
}

// 1 墓碑
// 2 弹坑
// 3 梯子
// 4/5 传送门
// 7 罐子
// 10 蜗牛
// 11 钉耙
// 12 脑子
void PvZ::ClearGridItems(std::vector<int> types)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    unsigned int grid_item_struct_size = 0xec;
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        grid_item_struct_size = 0x8c;
#endif

    auto grid_item_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().grid_item_count_max});
    auto grid_item_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().grid_item});

    asm_init();
    for (size_t i = 0; i < grid_item_count_max; i++)
    {
        auto grid_item_dead = ReadMemory<bool>({grid_item_offset + data().grid_item_dead + grid_item_struct_size * i});
        auto grid_item_type = ReadMemory<int>({grid_item_offset + data().grid_item_type + grid_item_struct_size * i});
        if (!grid_item_dead && std::find(types.begin(), types.end(), grid_item_type) != types.end())
        {
            int addr = grid_item_offset + grid_item_struct_size * i;
#ifdef _PVZ_BETA_LEAK_SUPPORT
            if (isBETA())
                asm_mov_exx(Reg::ECX, addr);
            else
#endif
                asm_mov_exx(Reg::ESI, addr);
            asm_call(data().call_delete_grid_item);
        }
    }
    asm_ret();
    asm_code_inject();
}

void PvZ::PlantInvincible(bool on)
{
    if (!GameOn())
        return;

#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
    {
        HACK<uint8_t, 5 + 3> plant_immune_eat = {0x0052130a,                                        //
                                                 {0xbd, 0x00, 0x00, 0x00, 0x00, 0x01, 0x6e, 0x48},  //
                                                 {0xbd, 0xfc, 0xff, 0xff, 0xff, 0x01, 0x6e, 0x48}}; //

        enable_hack(plant_immune_eat, on);
    }
    else if (this->find_result == PVZ_BETA_0_9_9_1029_EN)
    {
        HACK<uint8_t, 5 + 3> plant_immune_eat = {0x0052edf6,                                        //
                                                 {0xbd, 0x00, 0x00, 0x00, 0x00, 0x01, 0x6e, 0x48},  //
                                                 {0xbd, 0xfc, 0xff, 0xff, 0xff, 0x01, 0x6e, 0x48}}; //

        enable_hack(plant_immune_eat, on);
    }
#endif

    enable_hack(data().plant_immune_eat, on);
    enable_hack(data().plant_immune_radius, on);
    enable_hack(data().plant_immune_jalapeno, on);
    enable_hack(data().plant_immune_projectile, on);
    enable_hack(data().plant_immune_lob_motion, on);
    enable_hack(data().plant_immune_square, on);
    enable_hack(data().plant_immune_row_area, on);
    enable_hack(data().plant_immune_spike_rock, on);
    enable_hack(data().plant_immune_squish, on);
}

void PvZ::PlantWeak(bool on)
{
    if (!GameOn())
        return;

#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
    {
        HACK<uint8_t, 5 + 3> _plant_immune_eat = {0x0052130a,                                        //
                                                  {0xbd, 0x00, 0x00, 0x00, 0x00, 0x21, 0x6e, 0x48},  //
                                                  {0xbd, 0xfc, 0xff, 0xff, 0xff, 0x01, 0x6e, 0x48}}; //

        enable_hack(_plant_immune_eat, on);
    }
    else if (this->find_result == PVZ_BETA_0_9_9_1029_EN)
    {
        HACK<uint8_t, 5 + 3> _plant_immune_eat = {0x0052edf6,                                        //
                                                  {0xbd, 0x00, 0x00, 0x00, 0x00, 0x21, 0x6e, 0x48},  //
                                                  {0xbd, 0xfc, 0xff, 0xff, 0xff, 0x01, 0x6e, 0x48}}; //

        enable_hack(_plant_immune_eat, on);
    }
#endif

    enable_hack(data()._plant_immune_eat, on);
    enable_hack(data()._plant_immune_projectile, on);
    enable_hack(data()._plant_immune_lob_motion, on);
    enable_hack(data()._plant_immune_row_area, on);
}

void PvZ::ZombieInvincible(bool on)
{
    if (!GameOn())
        return;

#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
    {
        HACK<uint8_t, 4 + 6> zombie_immune_body_damage = {0x0051f084,                                                    //
                                                          {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90},  //
                                                          {0x8b, 0x44, 0x24, 0x18, 0x29, 0x86, 0xcc, 0x00, 0x00, 0x00}}; //

        enable_hack(zombie_immune_body_damage, on);
    }
#endif

    enable_hack(data().zombie_immune_body_damage, on);
    enable_hack(data().zombie_immune_helm_damage, on);
    enable_hack(data().zombie_immune_shield_damage, on);
    enable_hack(data().zombie_immune_burn_crumble, on);
    enable_hack(data().zombie_immune_radius, on);
    enable_hack(data().zombie_immune_burn_row, on);
    enable_hack(data().zombie_immune_chomper, on);
    enable_hack(data().zombie_immune_mind_controll, on);
    enable_hack(data().zombie_immune_blow_away, on);
    enable_hack(data().zombie_immune_splash, on);
    enable_hack(data().zombie_immune_lawn_mower, on);
}

void PvZ::ZombieWeak(bool on)
{
    if (!GameOn())
        return;

#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
    {
        HACK<uint8_t, 4 + 6> _zombie_immune_body_damage = {0x0051f084,                                                    //
                                                           {0xc7, 0x86, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  //
                                                           {0x8b, 0x44, 0x24, 0x18, 0x29, 0x86, 0xcc, 0x00, 0x00, 0x00}}; //

        enable_hack(_zombie_immune_body_damage, on);
    }
#endif

    enable_hack(data()._zombie_immune_body_damage, on);
    enable_hack(data()._zombie_immune_helm_damage, on);
    enable_hack(data()._zombie_immune_shield_damage, on);
    enable_hack(data()._zombie_immune_burn_crumble, on);
}

void PvZ::ReloadInstantly(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().reload_instantly, on);
    enable_hack(data().grow_up_quickly, on);
    enable_hack(data().no_cooldown, on);
}

void PvZ::MushroomsAwake(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().mushrooms_awake, on);

    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    unsigned int plant_struct_size = 0x14c;

    if (on)
    {
        auto plant_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});
        auto plant_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});
        asm_init();
        for (size_t i = 0; i < plant_count_max; i++)
        {
            auto plant_dead = ReadMemory<bool>({plant_offset + data().plant_dead + plant_struct_size * i});
            auto plant_squished = ReadMemory<bool>({plant_offset + data().plant_squished + plant_struct_size * i});
            auto plant_asleep = ReadMemory<bool>({plant_offset + data().plant_asleep + plant_struct_size * i});
            if (!plant_dead && !plant_squished && plant_asleep)
            {
                uint32_t addr = plant_offset + plant_struct_size * i;
                if (isGOTY())
                    asm_mov_exx(Reg::EDI, addr);
#ifdef _PVZ_BETA_LEAK_SUPPORT
                else if (isBETA())
                    asm_mov_exx(Reg::ECX, addr);
#endif
                else
                    asm_mov_exx(Reg::EAX, addr);
                asm_push_byte(0);
                asm_call(data().call_set_plant_sleeping);
            }
        }
        asm_ret();
        asm_code_inject();
    }
}

void PvZ::StopSpawning(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().stop_spawning, on);
}

void PvZ::StopZombies(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().stop_zombies, on);
}

// ---------------------------------------------------------------------------
// 2026-10-07 新增: 僵尸批量操作 (冻结 / 魅惑 / 家门口归天 / 血量倍数)
//                   + 女仆秘籍 (暂停相位控制)
//                   + 智慧树指令 (发按键)
//
// 全部只用内存字段, **不需要任何代码补丁**, 因此对 14 个游戏版本通用:
//   * 冻结: Zombie::mIceTrapCounter = 600 (Zombie.h +0xB4)。
//     游戏侧的总开关是 Zombie::IsImmobilizied() = mIceTrapCounter > 0 || mButteredCounter > 0
//     (Zombie.cpp:6624), 它为真时 UpdatePlaying 里整段"移动 / 啃植物 / 入屋 / 相位计数"
//     都不执行 (Zombie.cpp:4643-4651), Animate() 也提前返回 (4938)。
//     **关闭时必须写 1 而不是 0**: 写 0 只让逻辑恢复, 而动画速率还停在 ApplyAnimRate(0)
//     留下的 0, 僵尸会"站着不动又不结冰"; 写 1 让游戏下一帧自己减到 0 并走 RemoveIceTrap()
//     (Zombie.cpp:4611-4619)。
//   * 魅惑: Zombie::mMindControlled = 1 (Zombie.h +0xB8)。颜色/掉头向右/攻击非魅惑僵尸/
//     不再啃植物全部由这个字段驱动 (Zombie.cpp:5085 / 6491 / 6924 / 5180)。
//     ⚠ 游戏里 EffectedByDamage() 对魅惑僵尸返回 false (Zombie.cpp:8161-8178) ——
//       也就是"魅惑僵尸免疫植物伤害", 全场魅惑后自己的植物会打不动; 且它们会一路向右
//       走到 mPosX > 850 就 DieNoLoot() 消失 (CheckForBoardEdge, Zombie.cpp:4537)。
//       所以这个开关是**单向**的: 打开后只能靠结束关卡复原。
//   * 无视进家: 每帧判 GameObject::mX (+0x08) <= 阈值就写 mZombiePhase = 3
//     (PHASE_ZOMBIE_MOWERED)。这与工具现有"秒杀"完全同路 (KillAllZombies 也是写 3,
//     pvz.cpp:1578): 游戏会派发 UpdateMowered -> 没有碾压动画时立刻 DieWithLoot(),
//     死亡 / 掉落 / 清理都交给游戏自己做。不要写 mDead=1 (只 DataArrayFree, 可能留残影)。
//   * 血量倍数: 按槽位记下原始 mBodyMaxHealth (+0xCC), 把 max 与当前 (+0xC8) 一起写成
//     base * 倍数; 倍数改回 1.00 时还原。新出现的僵尸按"槽位由空变有"识别。
// ---------------------------------------------------------------------------
// 无视僵尸进家: 僵尸的 **mPosX** 小于等于这个值就"到家"。
// 用 mPosX (+0x2C) 而不是 GameObject::mX: 后者反编译没给偏移注释, 之前猜的 +0x08
// 读到的是垃圾值, 僵尸一出生就被判到家而秒杀。
// 数值: 小推车静止位约 -21、游戏失败线是 -100; 用户要求"再往左半格"(半格 = 40),
// 所以取 0 —— 僵尸正好走到草坪左边缘(家门口)才归天。
static const int kZombieHomeX = 0;

void PvZ::SetZombieKillAtHome(bool on)
{
    this->hack_kill_home = on;
}

// 冻结"当前场上"的僵尸: 写 mIceTrapCounter = 600 (寒冰菇那种冻结, 100 帧 = 1 秒,
// 所以约 6 秒后自然解冻)。按一下生效一次, 之后新刷出的僵尸不受影响。
// 关闭不用特殊处理 —— 游戏自己会把计数器减到 0 并走 RemoveIceTrap()。
int PvZ::ZombieFreezeNow()
{
    if (!GameOn() || !RandomFixSupported())
        return 0;
    const int ui = GameUI();
    if (ui != 2 && ui != 3)
        return 0;
    return this->ZombieApply(true, false);
}

// 魅惑"当前场上"的僵尸: 写 mMindControlled = 1。
// ⚠ 游戏里 EffectedByDamage() 对魅惑僵尸返回 false —— 魅惑僵尸免疫植物伤害;
//   它们还会一路向右走出屏幕 (mPosX > 850 时 DieNoLoot)。所以这是单向的。
int PvZ::ZombieMindControlNow()
{
    if (!GameOn() || !RandomFixSupported())
        return 0;
    const int ui = GameUI();
    if (ui != 2 && ui != 3)
        return 0;
    return this->ZombieApply(false, true);
}

// 两个"按一下生效"的函数共用的遍历: 返回处理了几只僵尸
int PvZ::ZombieApply(bool freeze, bool mind)
{
    unsigned int struct_size = isGOTY() ? 0x168 : 0x15c;
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        struct_size = 0x160;
#endif
    uint32_t count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().zombie_count_max});
    const uintptr_t base = ReadMemory<uintptr_t>({data().lawn, data().board, data().zombie});
    if (base == 0 || count_max == 0)
        return 0;
    if (count_max > 4096)
        count_max = 4096;

    int done = 0;
    for (uint32_t i = 0; i < count_max; i++)
    {
        const uintptr_t addr = base + struct_size * i;
        if (ReadMemory<unsigned char>({addr + data().zombie_dead}) != 0)
            continue;
        const int phase = ReadMemory<int>({addr + data().zombie_status});
        if (phase == 1 || phase == 2 || phase == 3) // 死亡 / 烧焦 / 被压过
            continue;

        if (freeze)
        {
            // 冻结 = 寒冰菇那种: 写 mIceTrapCounter 让 IsImmobilizied() 为真 (移动/啃植物/
            // 入屋/相位计数全停, 见 Zombie.cpp:4643), 100 帧 = 1 秒所以约 6 秒后自然解冻。
            const int frozen = 600;
            WriteBytes(&frozen, addr + ZOMBIE_ICE_TRAP_OFFSET, sizeof(frozen));

            // ★ 但动画不会自己停: 游戏的动画速率是在 UpdateAnimSpeed() 里设成 0 的
            // (Zombie.cpp:6695 `if (IsImmobilizied()) ApplyAnimRate(0.0f)`), 而它只在
            // 特定时机被调用 —— 其中一个时机是 chilled 计数器减到 0 那一帧
            // (Zombie.cpp:4620-4626)。所以把 mChilledCounter 写成 1: 游戏下一帧把它减到 0,
            // 顺手调用 UpdateAnimSpeed() -> 因为我们已经把 mIceTrapCounter 顶上去了,
            // 它会走 ApplyAnimRate(0) -> 动画才真的停住。
            // (这与工具"固定移速"里用过的 chilled=1 技巧同源。)
            const int chill_once = 1;
            WriteBytes(&chill_once, addr + ZOMBIE_CHILLED_COUNTER_OFFSET, sizeof(chill_once));
        }
        if (mind)
        {
            const unsigned char yes = 1;
            WriteBytes(&yes, addr + ZOMBIE_MIND_CONTROLLED_OFFSET, sizeof(yes));
        }
        done++;
    }
    return done;
}

void PvZ::SetZombieHealthMul(float mul)
{
    if (mul < 0.05f)
        mul = 0.05f;
    if (mul > 100.0f)
        mul = 100.0f;
    this->hack_zombie_hp_mul = mul;
}

void PvZ::SetMaidMode(int mode)
{
    if (mode < 0 || mode > 3)
        mode = 0;
    this->hack_maid_mode = mode;
}

// 女仆秘籍: 社区的四个操作 (PVZ Wiki / AvZ 的 Move / Dancing / CallPartner / Stop)。
// ★ 参考 AvZ 的实现 (AsmVsZombies-master/inc/avz_memory.h 的 AMaidCheats):
//   它把 Zombie::GetDancerFrame 里那条读 mAppCounter 的指令 (0x52DFC9, EN 版)
//   `8B 80 38 08 00 00` (mov eax, [eax+0x838]) 换成 `90 B8 <imm32>`, 也就是
//   **让这个函数直接返回一个常数**, 再由函数剩下的 `(值 % 460) / 20` 算出相位:
//       Move        imm32 = 0xE9  = 233 -> 233/20 = 11 -> PHASE_DANCER_DANCING_LEFT (一直前进)
//       Dancing     imm32 = 0x140 = 320 -> 16        -> PHASE_DANCER_RAISE_LEFT_1 (原地抬手)
//       CallPartner imm32 = 0xF0  = 240 -> 12        -> PHASE_DANCER_WALK_TO_RAISE (召唤判定帧)
//       Stop        还原成 8B 80 38 08 00 00
//   要点: 相位必须**恒定**, 游戏只在相位变化时才重播 reanim (Zombie.cpp:3058);
//   早先我按"相位跑到区间边缘就往前赶"的写法会让相位每轮变一次, 于是动画每轮重播一次,
//   看起来就是"鬼畜"。现在改成同样把相位**钉死**在目标帧上 —— 纯内存写, 不需要代码补丁:
//   每帧把 mAppCounter 写回"本轮的起点 + 目标帧 * 20", 于是 (值 % 460)/20 恒为常数。
//   副作用: mAppCounter 被钉住期间, 两处植物"上下浮动"动画 (Plant.cpp:3617/4074) 会停,
//   纯观感; AvZ 那种直接改代码的写法没有这个副作用 (但需要按版本找地址)。
static int MaidPinCounter(int mode, int counter)
{
    if (counter < 0)
        counter = -counter;
    const int cycle = 20 * 23;
    int frame = -1;
    switch (mode)
    {
    case 1: frame = 11; break; // 一直前进
    case 2: frame = 16; break; // 原地抬手 (不前进, 也不召唤)
    case 3: frame = 12; break; // 抬手帧, 每次都进召唤判定
    default: return counter;
    }
    return counter - (counter % cycle) + frame * 20;
}

// 僵尸碰撞箱表 (定义在文件后面, 要用 ZOMBIE_* 常量)
void FillZombieBodyTable(PtHook::Control *ctl);

void PvZ::FrameHacks()
{
    this->ProbeTick(); // 植物字段探测 (异步, 到期才出结果)

    // ★★ 2026-10-14 新增 (都必须在下面那些 `return` 之前 —— 它们要在任意界面跑) ★★
    //   ① 「换卡片」: 到"战斗正式开始"那一刻把选卡界面记下的卡槽写一次 (幂等)
    //   ② 「取消必选卡片」: 选卡界面开着时把那 3 张的 mCrazyDavePicked 清 0 (机制 B)
    this->TickPendingSlotSeeds();
    this->TickNoDavePick();

    // ---- 冰车无痕自检: 打完补丁后看看游戏是不是还在写 3000 ----
    //   "补丁点没命中" 的判断依据: mIceTimer 又被写成很大的值 (3000)。
    //   一旦发现: 立刻退回旧补丁 (确实不产生冰痕, 但冰车速度会受影响) 并在提示里说清,
    //   免得出现"看着是无痕、其实没生效"的情况。
    if (this->hack_no_ice_trail && this->ice_timer_store_addr != 0 && GameOn())
    {
        int mx = 0;
        for (int r = 0; r < 6; r++)
        {
            const int v = ReadMemory<int>({data().lawn, data().board, data().ice_trail_cd + r * 4});
            if (v > mx)
                mx = v;
        }
        if (mx > this->ice_timer_max_seen)
            this->ice_timer_max_seen = mx;
        if (mx > 100) // 游戏仍在写 3000 -> 补丁点没打中那句
        {
            this->RestorePatch(this->ice_timer_store_addr);
            this->ice_timer_store_addr = 0;
            enable_hack(data().no_ice_trail, true);
            this->ice_trail_msg = "补丁点未命中(游戏仍在写 3000), 已退回旧补丁: 冰车速度会受影响";
        }
    }

    // ---- 给注入的 DLL 刷新"植物数组几何" (它靠这个强校验 self 到底是不是一棵植物) ----
    if (this->hook_ctl != nullptr)
    {
        // ★ v38: 心跳**只作诊断**了 —— DLL 的卸载判据改成"工具进程是否存活"
        //   (见共享头 CdUnloadDecision)。为什么: 拖动本工具主窗口时 Windows 的模态移动
        //   循环会占住这个主循环 ⇒ 心跳停; v37 的 DLL 据此把钩子全撤掉 = 用户报的
        //   "拖修改器主界面也会中断 / 偶尔会中断"。现在心跳停多久都不会触发卸载。
        this->hook_ctl->tool_beat++;
        // "本游戏进程里 DLL 曾经真的在跑" —— 只用来把"从未注入"与"钩点被撤"分开
        if (this->hook_ctl->hook_ok != 0 || this->hook_ctl->hook_calls != 0)
            this->hook_seen_alive = true;
    }

    if (this->hook_ctl != nullptr && this->hook_injected && GameOn())
    {
        this->hook_ctl->plant_size = 0x14c; // 与核心遍历植物用的常量一致
        this->hook_ctl->plant_base =
            static_cast<uint32_t>(ReadMemory<uintptr_t>({data().lawn, data().board, data().plant}));
        this->hook_ctl->plant_count =
            ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});

        // 方案 B 用到的字段偏移 (版本相关, 每帧刷新最省事也最安全)
        this->hook_ctl->p_off_seed = static_cast<uint32_t>(data().plant_type);
        this->hook_ctl->p_off_row = static_cast<uint32_t>(data().plant_row);
        this->hook_ctl->p_off_shooting = static_cast<uint32_t>(PLANT_SHOOTING_COUNTER_OFFSET);
        this->hook_ctl->z_base =
            static_cast<uint32_t>(ReadMemory<uintptr_t>({data().lawn, data().board, data().zombie}));
        this->hook_ctl->z_size = isGOTY() ? 0x168 : 0x15c;
        this->hook_ctl->z_count =
            ReadMemory<uint32_t>({data().lawn, data().board, data().zombie_count_max});
        this->hook_ctl->z_off_dead = static_cast<uint32_t>(data().zombie_dead);
        this->hook_ctl->z_off_row = static_cast<uint32_t>(ZOMBIE_ROW_OFFSET);
        this->hook_ctl->z_off_status = static_cast<uint32_t>(data().zombie_status);
        this->hook_ctl->z_off_x = static_cast<uint32_t>(ZOMBIE_POS_X_OFFSET);
        this->hook_ctl->z_off_type = static_cast<uint32_t>(ZOMBIE_TYPE_OFFSET);
        FillZombieBodyTable(this->hook_ctl);
        this->hook_ctl->p_off_col = static_cast<uint32_t>(data().plant_col);

        // 游戏自己的帧计数器 (LawnApp::mAppCounter) 的**绝对地址**, DLL 用它判定帧边界。
        // 路径形式是 {lawn, 偏移} —— data().lawn 是 gLawnApp 指针变量的地址, 先解引用再加偏移。
        const uint32_t app = ReadMemory<uint32_t>({data().lawn});
        if (app != 0)
            this->hook_ctl->app_counter_addr =
                app + static_cast<uint32_t>(data().game_mode + LAWNAPP_APPCOUNTER_DELTA);
    }

    // ---- 女仆秘籍 (读时钟 + 按住舞王的召唤计数器) ----
    // 如果已经用"代码补丁"模式 (AvZ 的 90 B8 imm32), 相位本身就是常数,
    // 既不用钉时钟, 也不用管召唤计数器, 这里直接跳过。
    if (GameOn() && this->hack_maid_mode != 0 && !this->hack_maid_patched && GameUI() == 3)
    {
        // ★ 注意路径写法: data().lawn 是 **gLawnApp 指针变量的地址**, 不是 LawnApp 基址。
        //   必须用 {lawn, 偏移} 这种路径形式 (工具会先解引用), 不能自己是 lawn + 偏移 相加。
        const std::initializer_list<uintptr_t> app_path = {data().lawn,
                                                          data().game_mode + LAWNAPP_APPCOUNTER_DELTA};
        const int counter = ReadMemory<int>(app_path);
        const int pinned = MaidPinCounter(this->hack_maid_mode, counter);
        if (pinned != counter && counter > 0 && counter < 100000000)
            WriteMemory<int>(pinned, app_path);

        // 模式 1/2 = "不召唤伴舞": 把舞王的召唤计数器按住 (让它永远到不了 1)。
        // 模式 3 = 一直尝试召唤: 不管它, 让它自然走到 1。
        if (this->hack_maid_mode == 1 || this->hack_maid_mode == 2)
        {
            unsigned int struct_size = isGOTY() ? 0x168 : 0x15c;
#ifdef _PVZ_BETA_LEAK_SUPPORT
            if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
                struct_size = 0x160;
#endif
            const uint32_t count_max =
                ReadMemory<uint32_t>({data().lawn, data().board, data().zombie_count_max});
            const uintptr_t base = ReadMemory<uintptr_t>({data().lawn, data().board, data().zombie});
            if (base != 0 && count_max != 0)
            {
                const uint32_t n = count_max > 4096 ? 4096 : count_max;
                for (uint32_t i = 0; i < n; i++)
                {
                    const uintptr_t z = base + struct_size * i;
                    if (ReadMemory<unsigned char>({z + data().zombie_dead}) != 0)
                        continue;
                    // ★ 2026-10-13: 这里原来**写死成 8** —— 而 ZOMBIE_DANCER 的真值是 9
                    //   (ConstEnums.h 的 enum ZombieType 从 ZOMBIE_NORMAL = 0 数) ⇒ 这条
                    //   "女仆秘籍: 舞王召唤倒计时钉死"**从来没命中过任何僵尸**。改用上面的常量。
                    if (ReadMemory<int>({z + ZOMBIE_TYPE_OFFSET}) != ZOMBIE_DANCER_FWD)
                        continue;
                    const int hold = 100;
                    WriteBytes(&hold, z + ZOMBIE_SUMMON_COUNTER_OFFSET, sizeof(hold));
                }
            }
        }
    }

    // ---- 智慧树受门禁的指令: 发完按键后把纪录还原回去 ----
    if (this->hack_wisdom_record_dirty)
    {
        if (--this->hack_wisdom_record_restore <= 0)
        {
            WriteMemory<int>(this->hack_wisdom_record_old,
                             {data().lawn, data().user_data,
                              data().level + PLAYERINFO_RECORDS_DELTA + WISDOM_RECORD_INDEX * 4});
            this->hack_wisdom_record_dirty = false;
        }
    }

    // ---- 僵尸批量操作 ----
    if (!GameOn())
        return;
    const int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;
    if (!RandomFixSupported()) // 两个泄露的测试版布局不同, 不碰
        return;

    const bool hp_on = (this->hack_zombie_hp_mul != 1.0f);
    if (!this->hack_kill_home && !hp_on)
        return;

    // "无视僵尸进家" 只能在玩家防守的关卡里生效:
    //   我是僵尸 (61~70) 里僵尸是玩家自己的兵, 一到门口就归天等于当场自杀;
    //   禅境花园 / 智慧树没有僵尸, 也不用管。
    const int mode_now = GameMode();
    const bool kill_home_ok = this->hack_kill_home && ui == 3 &&
                              !(mode_now >= GAME_MODE_IZOMBIE_FIRST && mode_now <= GAME_MODE_IZOMBIE_LAST);

    unsigned int struct_size = isGOTY() ? 0x168 : 0x15c;
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        struct_size = 0x160;
#endif

    uint32_t count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().zombie_count_max});
    const uintptr_t base = ReadMemory<uintptr_t>({data().lawn, data().board, data().zombie});
    if (base == 0 || count_max == 0)
        return;
    if (count_max > 4096)
        count_max = 4096;

    if (this->zombie_hp_scaled.size() < count_max)
    {
        this->zombie_hp_scaled.resize(count_max, 0);
        this->zombie_hp_base.resize(count_max, 0.0f);
    }

    for (uint32_t i = 0; i < count_max; i++)
    {
        const uintptr_t addr = base + struct_size * i;
        const bool dead = ReadMemory<unsigned char>({addr + data().zombie_dead}) != 0;
        const int phase = ReadMemory<int>({addr + data().zombie_status});
        // 相位 1/2/3 = 死亡 / 烧焦 / 被小推车压过 (见文件后面 ZombiePhase 那组常量)
        const bool alive = !dead && phase != 1 && phase != 2 && phase != 3;
        if (!alive)
        {
            this->zombie_hp_scaled[i] = 0;
            continue;
        }

        // 血量倍数: 新僵尸 (槽位由空变有) 记下原始上限, 之后按倍数写 max + 当前
        if (hp_on)
        {
            if (!this->zombie_hp_scaled[i])
            {
                const float base_hp = ReadMemory<float>({addr + ZOMBIE_BODY_MAX_HEALTH_OFFSET});
                if (base_hp > 0.0f && base_hp < 1000000.0f)
                {
                    this->zombie_hp_base[i] = base_hp;
                    this->zombie_hp_scaled[i] = 1;
                }
            }
            if (this->zombie_hp_scaled[i])
            {
                const float want = this->zombie_hp_base[i] * this->hack_zombie_hp_mul;
                if (ReadMemory<float>({addr + ZOMBIE_BODY_MAX_HEALTH_OFFSET}) != want)
                {
                    WriteBytes(&want, addr + ZOMBIE_BODY_MAX_HEALTH_OFFSET, sizeof(want));
                    WriteBytes(&want, addr + ZOMBIE_BODY_HEALTH_OFFSET, sizeof(want));
                }
            }
        }
        else if (this->zombie_hp_scaled[i])
        {
            const float base_hp = this->zombie_hp_base[i];
            WriteBytes(&base_hp, addr + ZOMBIE_BODY_MAX_HEALTH_OFFSET, sizeof(base_hp));
            const float now_hp = ReadMemory<float>({addr + ZOMBIE_BODY_HEALTH_OFFSET});
            if (now_hp > base_hp)
                WriteBytes(&base_hp, addr + ZOMBIE_BODY_HEALTH_OFFSET, sizeof(base_hp));
            this->zombie_hp_scaled[i] = 0;
        }

        // 冻结 / 魅惑是"按一下生效一次"的独立函数 (ZombieFreezeNow / ZombieMindControlNow),
        // 这里只做每帧持续的两件事: 血量倍数 与 无视僵尸进家。

        // 无视僵尸进家: 用工具早就在读的 Zombie::mPosX (+0x2C)
        if (kill_home_ok)
        {
            const float mx = ReadMemory<float>({addr + ZOMBIE_POS_X_OFFSET});
            if (mx <= static_cast<float>(kZombieHomeX))
            {
                const int mowered = 3; // PHASE_ZOMBIE_MOWERED -> 游戏自己 DieWithLoot
                WriteBytes(&mowered, addr + data().zombie_status, sizeof(mowered));
            }
        }
    }
}

// 智慧树指令: 与玩家在主菜单/关卡里手敲完全一样 (游戏用 TypingCheck 累积 KeyCode,
// 见 LawnApp.cpp:1333-1341 与 Board::DoTypingCheck, Board.cpp:7864)。
// 发送后游戏里的标志位可以从 LawnApp +0x8B8.. 读回来做勾选框回显。
// 注意: 用的是 "trickedout" 而不是 "tricked out" —— 空格会被游戏当成暂停 (Board.cpp:7966)。
int PvZ::WisdomCodeCount()
{
    return 7;
}

const char *PvZ::WisdomCodeText(int index)
{
    static const char *kText[7] = {"mustache", "trickedout", "future", "pinata",
                                   "dance", "daisies", "sukhbir"};
    if (index < 0 || index >= 7)
        return "";
    return kText[index];
}

const char *PvZ::WisdomCodeLabel(int index)
{
    static const char *kLabel[7] = {
        "僵尸长胡子",
        "小推车变跑车",
        "僵尸戴墨镜",
        "僵尸掉糖果 (树高≥1000)",
        "僵尸边走边跳舞 (树高≥500)",
        "僵尸倒地变雏菊 (树高≥100)",
        "僵尸叫声变搞笑音效",
    };
    if (index < 0 || index >= 7)
        return "";
    return kLabel[index];
}

// 三条受门禁的指令各自需要的"智慧树模式最高纪录" (LawnApp::CanDoPinataMode 等, LawnApp.cpp:3449-3474)
static int WisdomCodeRequirement(int index)
{
    switch (index)
    {
    case 3: return 1000; // pinata
    case 4: return 500;  // dance
    case 5: return 100;  // daisies
    default: return 0;   // 其余四条没有门禁
    }
}

bool PvZ::SendWisdomCode(int index)
{
    if (!GameOn())
        return false;
    if (index < 0 || index >= 7)
        return false;
    if (this->hwnd == nullptr)
        return false;

    // 受门禁的三条: 游戏先查 PlayerInfo::mChallengeRecords[49] (智慧树模式的最高纪录),
    // **不是**工具"智慧树高度"写的 tree_height —— 所以直接把纪录临时顶到门禁值,
    // 发完按键后由 FrameHacks 还原 (用户存档里的那个数值不会被永久改掉)。
    const int need = WisdomCodeRequirement(index);
    if (need > 0)
    {
        const std::initializer_list<uintptr_t> rec_path = {
            data().lawn, data().user_data,
            data().level + PLAYERINFO_RECORDS_DELTA + WISDOM_RECORD_INDEX * 4};
        const int old = ReadMemory<int>(rec_path);
        if (old >= 0 && old < need)
        {
            WriteMemory<int>(need, rec_path);
            this->hack_wisdom_record_old = old;
            this->hack_wisdom_record_restore = 30; // 30 帧足够游戏处理完按键
            this->hack_wisdom_record_dirty = true;
        }
    }

    const char *text = WisdomCodeText(index);
    for (const char *p = text; *p != 0; p++)
    {
        // TypingCheck 吃的是 KeyCode (= 虚拟键码), 字母用大写
        const WPARAM vk = static_cast<WPARAM>(std::toupper(static_cast<unsigned char>(*p)));
        PostMessage(this->hwnd, WM_KEYDOWN, vk, 0);
        PostMessage(this->hwnd, WM_KEYUP, vk, 0);
    }
    return true;
}

bool PvZ::WisdomCodeEnabled(int index)
{
    if (!GameOn())
        return false;
    if (index < 0 || index >= 7)
        return false;
    return ReadMemory<unsigned char>(
               {data().lawn, data().game_mode + LAWNAPP_CHEAT_MODE_DELTA + index}) != 0;
}

// ---------------------------------------------------------------------------
// 特征扫描 + 可还原的代码补丁 (精确控制的基础设施)
//
// 为什么需要: 我们所有的"写字段"手段都受限于自己的循环频率 (约 60 Hz), 而游戏是 100 Hz,
// 于是"必须在某一帧内落值"的功能 (典型就是首发延迟) 会**概率性失效**。
// AvZ 的做法是注进游戏、在游戏自己的帧里执行; 我们走"外部进程 + 特征扫描 + 代码补丁"的
// 同一条技术路线: 不必给 14 个版本各填一个地址, 按特征定位即可。
// ---------------------------------------------------------------------------
std::vector<uintptr_t> PvZ::ScanGamePattern(const char *text, size_t max_hits)
{
    std::vector<uintptr_t> hits;
    if (!GameOn() || max_hits == 0)
        return hits;

    // PvZ 不是 ASLR, 主模块基址固定 0x400000。先读 DOS/PE 头拿节表, 只在**可执行节**里扫。
    const uintptr_t base = 0x400000;
    unsigned char dos[0x40] = {0};
    if (!ReadBytes(dos, base, sizeof(dos)) || dos[0] != 'M' || dos[1] != 'Z')
        return hits;

    uint32_t pe_off = 0;
    std::memcpy(&pe_off, dos + 0x3c, sizeof(pe_off));
    if (pe_off == 0 || pe_off > 0x1000)
        return hits;

    unsigned char pe[0x18] = {0};
    if (!ReadBytes(pe, base + pe_off, sizeof(pe)) || pe[0] != 'P' || pe[1] != 'E')
        return hits;

    uint16_t section_count = 0, opt_size = 0;
    std::memcpy(&section_count, pe + 0x06, sizeof(section_count));
    std::memcpy(&opt_size, pe + 0x14, sizeof(opt_size));
    if (section_count == 0 || section_count > 96 || opt_size > 0x400)
        return hits;

    const uintptr_t sec_table = base + pe_off + 0x18 + opt_size;
    for (uint16_t i = 0; i < section_count; i++)
    {
        unsigned char sh[40] = {0};
        if (!ReadBytes(sh, sec_table + 40 * i, sizeof(sh)))
            break;

        uint32_t vsize = 0, vaddr = 0, chars = 0;
        std::memcpy(&vsize, sh + 0x08, sizeof(vsize));
        std::memcpy(&vaddr, sh + 0x0c, sizeof(vaddr));
        std::memcpy(&chars, sh + 0x24, sizeof(chars));
        if ((chars & 0x20000000) == 0) // IMAGE_SCN_MEM_EXECUTE
            continue;
        if (vsize == 0 || vsize > 0x4000000)
            continue;

        const std::vector<uintptr_t> part =
            ScanPattern(text, base + vaddr, vsize, max_hits - hits.size());
        for (uintptr_t a : part)
            hits.push_back(a);
        if (hits.size() >= max_hits)
            break;
    }
    return hits;
}

std::string PvZ::ScanMaidSignature()
{
    if (!GameOn())
        return "未连接游戏";

    // Zombie::GetDancerFrame 里读 mAppCounter 的指令 = `mov r32, [r32 + mAppCounter偏移]`
    // (EN 1.0.0.1051 是 8B 80 38 08 00 00, 后面跟着 99 B9 CC 01 00 00 F7 F9 = cdq; mov ecx,460; idiv ecx)
    // 这里先用**宽特征**把所有"读这个偏移"的地方都列出来 (植物那两处也读它), 并把后面的字节打出来,
    // 方便确认哪一处才是 GetDancerFrame。
    const uint32_t off = static_cast<uint32_t>(data().game_mode + LAWNAPP_APPCOUNTER_DELTA);
    char pattern[64];
    std::snprintf(pattern, sizeof(pattern), "8B ?? %02X %02X %02X %02X", off & 0xff, (off >> 8) & 0xff,
                  (off >> 16) & 0xff, (off >> 24) & 0xff);

    const std::vector<uintptr_t> hits = ScanGamePattern(pattern, 8);
    std::string out = "女仆特征 [" + std::string(pattern) + "]: " +
                      std::to_string(hits.size()) + " 处";
    for (size_t i = 0; i < hits.size(); i++)
    {
        unsigned char tail[8] = {0};
        ReadBytes(tail, hits[i], sizeof(tail));
        char line[128];
        std::snprintf(line, sizeof(line), "  #%d %08X -> %02X %02X %02X %02X %02X %02X %02X %02X",
                      static_cast<int>(i + 1), static_cast<unsigned int>(hits[i]), tail[0], tail[1],
                      tail[2], tail[3], tail[4], tail[5], tail[6], tail[7]);
        out += "\n";
        out += line;
    }
    return out;
}

// 生成首发延迟的候选特征 (纯函数, 便于离线测试: 特征必须能解析, 而且能匹配手工汇编出的指令)
struct WindupPattern
{
    const char *name;
    std::string pattern;
};

static std::vector<WindupPattern> BuildWindupPatterns(uint32_t off)
{
    const unsigned char o0 = off & 0xff, o1 = (off >> 8) & 0xff, o2 = (off >> 16) & 0xff,
                        o3 = (off >> 24) & 0xff;
    char buf[64];
    std::vector<WindupPattern> out;

    auto add = [&](const char *name, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        out.push_back({name, std::string(buf)});
    };

    add("cmp [r+0x58],100 (生产类, disp8)", "83 7? %02X 64", o0);
    add("cmp [r+0x58],50 (香蒲, disp8)", "83 7? %02X 32", o0);
    add("cmp [r+0x58],25 (香蒲, disp8)", "83 7? %02X 19", o0);
    add("cmp [r+0x58],100 (生产类, disp32)", "83 B? %02X %02X %02X %02X 64", o0, o1, o2, o3);
    add("dec [r+0x58] (disp8)", "FF 4? %02X", o0);
    add("dec [r+0x58] (disp32)", "FF 8? %02X %02X %02X %02X", o0, o1, o2, o3);
    add("sub [r+0x58],1 (disp8)", "83 6? %02X 01", o0);
    add("读字段 mov r,[r+0x58] (disp8)", "8B 4? %02X", o0);
    add("写字段 mov [r+0x58],r (disp8)", "89 4? %02X", o0);
    return out;
}

// 首发延迟的候选特征: 游戏里递减/比较 Plant::mLaunchCounter 的地方
//   ★ 偏移小的成员, 编译器用 **disp8** 编码 (3 字节), 不是 disp32 —— 第一版我按 disp32 写,
//     结果 5 个特征全是 0 处 (用户实测)。两种编码都要覆盖:
//       dec [r+0x58]       FF 4? 58            (disp8) / FF 8? 58 00 00 00       (disp32)
//       sub [r+0x58], 1    83 6? 58 01         (disp8) / 83 A? 58 00 00 00 01    (disp32)
//       cmp [r+0x58], imm  83 7? 58 <imm>      (disp8) / 83 B? 58 00 00 00 <imm> (disp32)
//       读 / 写该字段       8B 4? 58 / 89 4? 58 (disp8) 及 disp32 形式
//   偏移 0x58 已由用户的"植物字段探测"实测确认 (值在 0~150 之间来回跳)。
std::string PvZ::ScanWindupSignature()
{
    if (!GameOn())
        return "未连接游戏";

    const std::vector<WindupPattern> items = BuildWindupPatterns(PLANT_LAUNCH_COUNTER_OFFSET);

    std::string out;
    for (const WindupPattern &item : items)
    {
        const std::vector<uintptr_t> hits = ScanGamePattern(item.pattern.c_str(), 4);
        char line[256];
        std::snprintf(line, sizeof(line), "%s [%s]: %d 处", item.name, item.pattern.c_str(),
                      static_cast<int>(hits.size()));
        out += line;
        for (size_t k = 0; k < hits.size(); k++)
        {
            const uintptr_t fn = FindFunctionEntry(hits[k]);
            char dump[3 * 16 + 1] = {0};
            unsigned char bytes[16] = {0};
            if (ReadBytes(bytes, hits[k], sizeof(bytes)))
            {
                for (int b = 0; b < 16; b++)
                    std::snprintf(dump + b * 3, 4, "%02X ", bytes[b]);
            }
            // UpdateShooter 在反编译源码里标了地址 (Plant.cpp:935 //0x45F8A0), 顺手对一下
            const char *ref = "";
            if (hits[k] >= 0x45F8A0 && hits[k] < 0x45F8A0 + 0x400)
                ref = (fn == 0x45F8A0) ? " [与反编译 0x45F8A0 一致]" : " [反编译参考 0x45F8A0]";
            std::snprintf(line, sizeof(line), "\n    #%d 命中 %08X  函数入口 %08X%s\n        %s",
                          static_cast<int>(k + 1), static_cast<unsigned int>(hits[k]),
                          static_cast<unsigned int>(fn), ref, dump);
            out += line;
        }
        out += "\n";
    }
    return out;
}

// 定位 Zombie::GetDancerFrame 里那条读 mAppCounter 的指令 (供打补丁)。
// 判别: 用版本相关的特征把"读 mAppCounter 的地方"都扫出来, 其中**后面紧跟 cdq(99)** 的那一处
// 就是 GetDancerFrame —— 用户实测的命中是:
//   #1 00465080 -> 8B 88 38 08 00 00 89 4C   (植物里的用法)
//   #2 00465CD4 -> 8B 80 38 08 00 00 8B 4B   (植物里的用法)
//   #3 0052DFC9 -> 8B 80 38 08 00 00 99 F7   ← cdq / idiv, 与 AvZ 的补丁点 0x52DFC9 完全一致
uintptr_t PvZ::FindMaidPatchSite()
{
    if (!GameOn())
        return 0;

    const uint32_t off = static_cast<uint32_t>(data().game_mode + LAWNAPP_APPCOUNTER_DELTA);
    char pattern[64];
    std::snprintf(pattern, sizeof(pattern), "8B ?? %02X %02X %02X %02X", off & 0xff, (off >> 8) & 0xff,
                  (off >> 16) & 0xff, (off >> 24) & 0xff);

    const std::vector<uintptr_t> hits = ScanGamePattern(pattern, 8);
    uintptr_t found = 0;
    for (uintptr_t addr : hits)
    {
        unsigned char head[7] = {0};
        if (!ReadBytes(head, addr, sizeof(head)))
            continue;
        if (head[0] != 0x8b || (head[1] & 0xc0) != 0x80) // mov r32, [r32+disp32]
            continue;
        uint32_t disp = 0;
        std::memcpy(&disp, head + 2, sizeof(disp));
        if (disp != off)
            continue;
        if (head[6] != 0x99) // 后面是 cdq 才是 GetDancerFrame
            continue;
        if (found != 0)
            return 0; // 竟然有多处 -> 不敢动, 交回"钉时钟"模式
        found = addr;
    }
    return found;
}

// 女仆秘籍: AvZ 原版做法 —— 把 GetDancerFrame 里读时钟的指令换成 `90 B8 <imm32>`
// (nop; mov eax, imm32), 由函数剩下的 (值 % 460)/20 得到常数相位:
//   Move = 233(0xE9) -> 11 (一直前进) / Dancing = 320(0x140) -> 16 (原地抬手)
//   CallPartner = 240(0xF0) -> 12 (召唤判定帧)
// 好处: **不需要冻结 mAppCounter**, 于是两处植物浮动动画照常 (比"钉时钟"干净)。
// 返回 0 = 用的是补丁模式; -1 = 没找到地址/校验不过 (调用方退回"钉时钟"模式)。
int PvZ::ApplyMaidPatch(int mode)
{
    if (!GameOn())
        return -1;

    // ★ 关键: 打过补丁之后, 那条指令已经变成 90 B8 <imm32>, **再扫原特征就扫不到了**。
    //   所以补丁点要记住 (hack_maid_site), 已经打好补丁时直接用记录, 绝不再扫。
    //   若记录是空的 (例如工具上次被强杀, 补丁还在游戏里), 用 DetectMaidPatch 自愈恢复现场。
    uintptr_t site = this->hack_maid_patched ? this->hack_maid_site : FindMaidPatchSite();
    if (site == 0 && FindMaidPatchSite() == 0)
    {
        const int detected = DetectMaidPatch(); // 认出"已经打过补丁"的现场并登记原始字节
        if (detected != 0)
            site = this->hack_maid_site;
    }

    if (mode == 0)
    {
        // 关闭: 一定要还原 —— 即使这次没扫到, 也要用记录里的地址还原, 不能把补丁留在游戏里
        if (this->hack_maid_patched && this->hack_maid_site != 0)
        {
            RestorePatch(this->hack_maid_site);
            this->hack_maid_patched = false;
            this->hack_maid_site = 0;
        }
        return 0;
    }

    if (site == 0)
        return -1; // 没找到补丁点 -> 调用方退回"钉时钟"模式

    uint32_t imm = 0;
    if (mode == 1)
        imm = 0xE9;
    else if (mode == 2)
        imm = 0x140;
    else if (mode == 3)
        imm = 0xF0;
    else
        return -1;

    const std::vector<unsigned char> bytes = {0x90, 0xB8, static_cast<unsigned char>(imm & 0xff),
                                              static_cast<unsigned char>((imm >> 8) & 0xff),
                                              static_cast<unsigned char>((imm >> 16) & 0xff),
                                              static_cast<unsigned char>((imm >> 24) & 0xff)};
    if (!PatchBytes(site, bytes))
        return -1;

    this->hack_maid_patched = true;
    this->hack_maid_site = site;
    return 0;
}

// 植物字段探测: 用来确定**这个版本**里 mLaunchCounter 的真实偏移。
// 做法: 记下场上第一棵活植物的整块结构 (0x14C 字节), 60 帧后再读一次, 报出所有"值变了"的偏移 ——
// 倒计时类字段 (mLaunchCounter) 必然在列表里, 固定 150 的通常是 mLaunchRate。
void PvZ::ProbePlantFieldsBegin()
{
    this->probe_a_valid = false;
    this->probe_report = "未连接游戏";
    if (!GameOn())
        return;

    const size_t struct_size = 0x14c;
    const uintptr_t base = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});
    uint32_t count = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});
    if (base == 0 || count == 0)
    {
        this->probe_report = "场上没有植物: 先种一棵再来探测";
        return;
    }
    if (count > 4096)
        count = 4096;

    for (uint32_t i = 0; i < count; i++)
    {
        const uintptr_t addr = base + struct_size * i;
        unsigned char dead = 0, squished = 0;
        ReadBytes(&dead, addr + data().plant_dead, 1);
        ReadBytes(&squished, addr + data().plant_squished, 1);
        if (dead != 0 || squished != 0)
            continue;

        this->probe_a.assign(struct_size, 0);
        if (!ReadBytes(this->probe_a.data(), addr, struct_size))
        {
            this->probe_report = "读取植物结构失败";
            return;
        }
        this->probe_addr = addr;
        this->probe_slot = static_cast<int>(i);
        this->probe_a_valid = true;
        this->probe_countdown = 60; // 约 1 秒 (前端约 60 Hz)
        this->probe_report = "采样中... (已记录第 " + std::to_string(i) + " 号植物, 60 帧后给出结果)";
        return;
    }
    this->probe_report = "场上没有活着的植物";
}

void PvZ::ProbeTick()
{
    if (!this->probe_a_valid || this->probe_countdown <= 0)
        return;
    if (--this->probe_countdown > 0)
        return;

    this->probe_b.assign(this->probe_a.size(), 0);
    if (!ReadBytes(this->probe_b.data(), this->probe_addr, this->probe_a.size()))
    {
        this->probe_report = "再次读取植物结构失败";
        this->probe_a_valid = false;
        return;
    }

    std::string out = "植物字段探测 (第 " + std::to_string(this->probe_slot) + " 号, 间隔 60 帧) 变化的偏移:\n";
    int shown = 0;
    for (size_t off = 0; off + 4 <= this->probe_a.size(); off += 4)
    {
        int va = 0, vb = 0;
        std::memcpy(&va, this->probe_a.data() + off, 4);
        std::memcpy(&vb, this->probe_b.data() + off, 4);
        if (va == vb)
            continue;
        char line[96];
        std::snprintf(line, sizeof(line), "  0x%02X: %d -> %d\n", static_cast<unsigned>(off), va, vb);
        out += line;
        if (++shown >= 18)
        {
            out += "  ...(其余省略)\n";
            break;
        }
    }
    if (shown == 0)
        out += "  (没有任何字段变化 —— 这棵植物当前完全静止?)\n";
    out += "其中数值在 0~150 之间来回跳的就是 mLaunchCounter; 固定 150 的是 mLaunchRate";
    this->probe_report = out;
    this->probe_a_valid = false;
}

// 从游戏 exe 文件里读某个虚拟地址处的**原始字节**。
// 用途: 工具被强杀/退出时没来得及还原, 下次启动时靠它把"被覆盖前的指令"找回来。
bool PvZ::ReadOriginalBytesFromFile(uintptr_t va, std::vector<unsigned char> &out, size_t size)
{
    out.clear();
    const std::string path = this->GamePath();
    if (path.empty())
        return false;

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    // DOS -> PE
    char dos[0x40] = {0};
    file.seekg(0, std::ios::beg);
    file.read(dos, sizeof(dos));
    if (!file || dos[0] != 'M' || dos[1] != 'Z')
        return false;

    uint32_t pe_off = 0;
    std::memcpy(&pe_off, dos + 0x3c, sizeof(pe_off));
    char pe[0x18] = {0};
    file.seekg(pe_off, std::ios::beg);
    file.read(pe, sizeof(pe));
    if (!file || pe[0] != 'P' || pe[1] != 'E')
        return false;

    uint16_t section_count = 0, opt_size = 0;
    std::memcpy(&section_count, pe + 0x06, sizeof(section_count));
    std::memcpy(&opt_size, pe + 0x14, sizeof(opt_size));
    if (section_count == 0 || section_count > 96 || opt_size > 0x400)
        return false;

    // 可选头里的 ImageBase (PE32 在 +0x34) 与实际基址一起用来算"相对虚拟地址"
    uint32_t image_base = 0;
    file.seekg(pe_off + 0x18 + 0x34, std::ios::beg);
    file.read(reinterpret_cast<char *>(&image_base), sizeof(image_base));
    if (!file || va < image_base)
        return false;
    const uint32_t rva = static_cast<uint32_t>(va - image_base);

    const std::streamoff sec_table = static_cast<std::streamoff>(pe_off) + 0x18 + opt_size;
    for (uint16_t i = 0; i < section_count; i++)
    {
        char sh[40] = {0};
        file.seekg(sec_table + 40 * i, std::ios::beg);
        file.read(sh, sizeof(sh));
        if (!file)
            return false;

        uint32_t vsize = 0, vaddr = 0, raw_size = 0, raw_ptr = 0;
        std::memcpy(&vsize, sh + 0x08, sizeof(vsize));
        std::memcpy(&vaddr, sh + 0x0c, sizeof(vaddr));
        std::memcpy(&raw_size, sh + 0x10, sizeof(raw_size));
        std::memcpy(&raw_ptr, sh + 0x14, sizeof(raw_ptr));
        if (rva < vaddr || rva >= vaddr + vsize)
            continue;
        const uint32_t delta = rva - vaddr;
        if (delta + size > raw_size)
            return false; // 落在未初始化区 / 超出文件内容

        out.assign(size, 0);
        file.seekg(static_cast<std::streamoff>(raw_ptr) + delta, std::ios::beg);
        file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(size));
        return static_cast<bool>(file);
    }
    return false;
}

// 自愈: 识别"游戏里已经是打过补丁状态"的现场。
// 我们的补丁长这样: 90 B8 <imm32>, 其中 imm32 ∈ {233(0xE9), 320(0x140), 240(0xF0)} ——
// 而且被替换的那条指令后面跟着 cdq(99)。找到就把补丁点/原始字节准备好, 并且把模式读出来。
int PvZ::DetectMaidPatch()
{
    if (!GameOn())
        return 0;

    struct Candidate
    {
        uint32_t imm;
        int mode;
    };
    const Candidate forms[3] = {{0x000000e9, 1}, {0x00000140, 2}, {0x000000f0, 3}};

    uintptr_t found = 0;
    int found_mode = 0;
    for (const Candidate &form : forms)
    {
        char pattern[32];
        std::snprintf(pattern, sizeof(pattern), "90 B8 %02X %02X %02X %02X 99", form.imm & 0xff,
                      (form.imm >> 8) & 0xff, (form.imm >> 16) & 0xff, (form.imm >> 24) & 0xff);
        const std::vector<uintptr_t> hits = ScanGamePattern(pattern, 4);
        if (hits.size() != 1)
            continue; // 0 处 = 这个模式没打; 多处 = 不敢动
        if (found != 0)
            return 0;
        found = hits[0];
        found_mode = form.mode;
    }
    if (found == 0)
        return 0;

    // 登记"原始字节 -> 当前字节"的补丁记录 (只记录, **不写内存**):
    // 原始字节从 exe 文件里取, 当前字节就是游戏里现在的内容。这样之后"关闭"能正确还原,
    // 换模式也能在保留原始字节的前提下直接覆盖。
    std::vector<unsigned char> original, current(6, 0);
    if (ReadBytes(current.data(), found, current.size()) &&
        ReadOriginalBytesFromFile(found, original, current.size()))
    {
        bool exists = false;
        for (CodePatchRecord &rec : this->code_patches)
        {
            if (rec.addr == found)
            {
                rec.original = original;
                rec.patched = current;
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            CodePatchRecord rec;
            rec.addr = found;
            rec.original = original;
            rec.patched = current;
            this->code_patches.push_back(rec);
        }
    }

    this->hack_maid_site = found;
    this->hack_maid_patched = true;
    return found_mode;
}

// ---------------------------------------------------------------------------
// 方案 B: 注入 DLL + 帧级钩子
// ---------------------------------------------------------------------------
uintptr_t PvZ::FindUpdateShooter()
{
    if (!GameOn())
        return 0;

    // Plant::UpdateShooter 里香蒲那两个检查点连在一起, 特征足够唯一:
    //   cmp [esi+58h],50 ; jne +13 ; cmp [esi+24h],0x2B(香蒲) ; jne +0D
    // (反编译 Plant.cpp:972; 该函数在源码里标了 //0x45F8A0)
    const std::vector<uintptr_t> hits = ScanGamePattern("83 7E 58 32 75 13 83 7E 24 2B 75 0D", 4);
    if (hits.empty())
        return 0;

    // ★ 结论: 必须挂**命中点本身** (0x45F91C 那一处), 不能挂函数入口。原因有两条, 都是实测踩出来的:
    //   1) 挂函数入口 = 紧挨着函数序言。MinHook 会把序言搬到蹦床执行, 而序言会 `push esi/edi/ebx`
    //      保存**调用者的**寄存器; 我们的 C 钩子在调用蹦床前已经用过这些寄存器 -> 序言存下垃圾,
    //      函数返回时把调用方的寄存器覆盖成垃圾 -> 调用方 (Plant::Update) 随后访问就崩
    //      (实测 EIP=入口+9 的 Access Violation, ECX=0)。
    //      挂在函数内部不会: 那时序言早已保存完毕。
    //   2) 命中点处的植物指针放在 **ESI** (指令就是 `cmp [esi+58h],50`), 而 thiscall 的入口在 ECX。
    //      DLL 的裸汇编跳板正好从 ESI 取指针, 所以这里必须用命中点。
    this->shooter_addr = hits[0];
    this->shooter_used_hit = true;
    return hits[0];
}

bool PvZ::OpenHookChannel()
{
    if (this->hook_ctl != nullptr)
        return true;

    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                    sizeof(PtHook::Control), PtHook::kMapName);
    if (map == nullptr)
        return false;

    auto *ctl = static_cast<PtHook::Control *>(
        MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PtHook::Control)));
    if (ctl == nullptr)
    {
        CloseHandle(map);
        return false;
    }

    std::memset(ctl, 0, sizeof(PtHook::Control));
    ctl->magic = PtHook::kMagic;
    ctl->version = PtHook::kVersion;
    ctl->struct_size = sizeof(PtHook::Control);
    // ★★ v38: 把本工具的**进程号**写进去 —— 这是 DLL 判断"工具还在不在"的唯一依据 ★★
    //   为什么必须在这里 (而不是每帧): 它必须**早于** LoadLibrary 注入那一刻就已经有效,
    //   否则 DLL 一挂上读到的 tool_pid 是 0 (那时我们只能"当作还活着", 虽然也不会误撤钩子,
    //   但就没有"工具被强杀 ⇒ DLL 自己清干净"这条能力了)。本函数在 InjectHookDll 的
    //   第一步就被调用 ⇒ 时序天然满足。
    //   DLL 那边用 OpenProcess(SYNCHRONIZE)+WaitForSingleObject 等它结束; 见共享头
    //   `CdUnloadDecision` 那一节 (v37 的"心跳 2 秒没变就卸钩子"就是"拖窗口就中断"的根因)。
    ctl->tool_pid = static_cast<uint32_t>(GetCurrentProcessId());

    this->hook_map = map;
    this->hook_ctl = ctl;
    return true;
}

// 启动时清理"上一次留下的 DLL": 工具被强杀时, 旧 DLL 可能还挂在游戏里。
// 它自己有心跳超时 (2 秒) 会退出, 但这里主动催一下, 免得用户刚开工具时游戏还被钉着。
void PvZ::CleanupStaleHookDll()
{
    if (this->hook_ctl == nullptr)
        return;
    if (this->hook_ctl->hook_ok == 0 && this->hook_ctl->dll_build == 0)
        return; // 没有残留

    this->hook_ctl->want_unload = 1;
    for (int i = 0; i < 150; i++) // 最多等 1.5 秒
    {
        if (this->hook_ctl->hook_ok == 0 && this->hook_ctl->dll_build == 0)
            break;
        Sleep(10);
    }
    this->hook_ctl->want_unload = 0;
    this->hook_ctl->hook_addr = 0;
    this->hook_ctl->pin_mode = 0;
    this->hook_ctl->pin_enable = 0;
    this->hook_ctl->windup_hook = 0;
    this->hook_injected = false;
    this->hook_remote_module = 0;
}

void PvZ::CloseHookChannel()
{
    if (this->hook_ctl != nullptr)
    {
        UnmapViewOfFile(this->hook_ctl);
        this->hook_ctl = nullptr;
    }
    if (this->hook_map != nullptr)
    {
        CloseHandle(this->hook_map);
        this->hook_map = nullptr;
    }
}

bool PvZ::InjectHookDll(const std::wstring &dll_path, uintptr_t hook_addr)
{
    if (!GameOn() || dll_path.empty() || hook_addr == 0)
        return false;
    if (!OpenHookChannel())
        return false;

    this->CleanupStaleHookDll(); // 先清掉上一次可能残留的 DLL (工具强杀过的情况)

    // ★ 先卸载上一次注入的 DLL:
    //   否则若是"游戏里已经加载着旧的 pztk_hook.dll", LoadLibraryW 会直接返回旧模块句柄,
    //   于是我们重新编译的 DLL 根本不会被加载 (看起来注入成功, 实际跑的是旧代码)。
    if (this->hook_injected)
        UnloadHookDll();

    this->hook_ctl->dll_build = 0; // 等 DLL 自己报构建号
    this->hook_ctl->hook_addr = static_cast<uint32_t>(hook_addr);
    this->hook_ctl->want_unload = 0;
    this->hook_ctl->want_hook = 1;
    this->hook_ctl->hook_ok = 0;
    this->hook_ctl->hook_fail = PtHook::FAIL_NONE;
    this->hook_ctl->hook_calls = 0;
    this->hook_ctl->pinned_count = 0;

    // 把 DLL 路径写进目标进程 (LoadLibraryW 需要 UTF-16; 我们的目录含中文, 不能用 A 版)
    const SIZE_T bytes = (dll_path.size() + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(this->handle, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote == nullptr)
        return false;
    if (!WriteProcessMemory(this->handle, remote, dll_path.c_str(), bytes, nullptr))
    {
        VirtualFreeEx(this->handle, remote, 0, MEM_RELEASE);
        return false;
    }

    // kernel32 在同一个 Windows 会话的 32 位进程里通常同址, 用本进程的 LoadLibraryW 地址即可
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        k32 == nullptr ? nullptr : GetProcAddress(k32, "LoadLibraryW"));
    if (load_library == nullptr)
    {
        VirtualFreeEx(this->handle, remote, 0, MEM_RELEASE);
        return false;
    }

    HANDLE thread = CreateRemoteThread(this->handle, nullptr, 0, load_library, remote, 0, nullptr);
    if (thread == nullptr)
    {
        VirtualFreeEx(this->handle, remote, 0, MEM_RELEASE);
        return false;
    }
    WaitForSingleObject(thread, 8000);
    DWORD module = 0;
    GetExitCodeThread(thread, &module);
    CloseHandle(thread);
    VirtualFreeEx(this->handle, remote, 0, MEM_RELEASE);

    // ★ 不能只看 LoadLibraryW 的返回值: 若 DllMain 卡住, GetExitCodeThread 会返回
    //   STILL_ACTIVE(0x103) 而不是 0, 会被误判成"注入成功"(第一次实测就是这样:
    //   显示"已注入"但 hook_ok=0、调用 0 次)。
    //   所以: 等 DLL 自己把 hook_ok / hook_fail 写出来才算成功。
    if (module == 0 || module == 0xFFFFFFFF || module == STILL_ACTIVE)
        return false;

    this->hook_remote_module = module;
    this->hook_dll_path = dll_path;
    {   // 记一份窄字符路径, 供状态行显示/探测
        const int need = WideCharToMultiByte(CP_ACP, 0, dll_path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (need > 1)
        {
            std::string narrow(static_cast<size_t>(need), '\0');
            WideCharToMultiByte(CP_ACP, 0, dll_path.c_str(), -1, narrow.data(), need, nullptr, nullptr);
            narrow.resize(static_cast<size_t>(need) - 1);
            this->hook_dll_path_a = narrow;
        }
    }
    this->hook_injected = true; // DLL 已被加载 (但钩子可能还在装)

    for (int i = 0; i < 300; i++) // 最多等 3 秒
    {
        if (this->hook_ctl->hook_ok != 0)
            return true;
        if (this->hook_ctl->hook_fail != PtHook::FAIL_NONE)
            break; // 装钩子失败 (原因码留给界面显示)
        Sleep(10);
    }

    // ★ 失败要把状态清掉: 否则"已注入"会一直是真的, 界面就不会再重试 (踩过)
    this->hook_injected = false;
    return false;
}

bool PvZ::UnloadHookDll()
{
    if (!GameOn() || this->hook_ctl == nullptr)
        return false;

    // 先请 DLL 自己卸钩子 (它的监视线程会处理), 等它把 hook_ok 清零
    this->hook_ctl->want_unload = 1;
    this->hook_ctl->want_hook = 0;
    for (int i = 0; i < 200 && this->hook_ctl->hook_ok != 0; i++)
        Sleep(10);
    Sleep(80); // 再留一点余量, 确保游戏线程已经离开我们的钩子代码

    bool ok = false;
    // 如果 DLL 已经自己卸掉了 (hook_ok/dll_build 都被清零), 就不要再远程 FreeLibrary ——
    // 那个句柄可能已经被系统复用, 再释放会误伤别的模块。
    const bool self_unloaded = (this->hook_ctl->hook_ok == 0 && this->hook_ctl->dll_build == 0);
    if (!self_unloaded && this->hook_remote_module != 0)
    {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        auto free_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            k32 == nullptr ? nullptr : GetProcAddress(k32, "FreeLibrary"));
        if (free_library != nullptr)
        {
            HANDLE thread = CreateRemoteThread(this->handle, nullptr, 0, free_library,
                                               reinterpret_cast<LPVOID>(static_cast<uintptr_t>(
                                                   this->hook_remote_module)),
                                               0, nullptr);
            if (thread != nullptr)
            {
                WaitForSingleObject(thread, 4000);
                CloseHandle(thread);
                ok = true;
            }
        }
    }

    this->hook_remote_module = 0;
    this->hook_injected = false;
    this->hook_ctl->want_unload = 0;
    return ok;
}

// (僵尸碰撞箱表的定义在文件后面, 因为要用到 ZOMBIE_* 常量)

void PvZ::SetWindupHook(bool on, const std::array<float, PSC_COUNT> &sec,
                        const std::array<bool, PSC_COUNT> &en)
{
    if (this->hook_ctl == nullptr)
        return;

    // 定义在文件后面 (分类表附近), 这里先声明
    int PlantWindupCheckpointOf(int seed_type);
    int PlantWindupRowDevOf(int seed_type);
    int PlantWindupBackHiOf(int seed_type);
    void PlantWindupWindowOf(int seed_type, int &lo, int &hi);

// 僵尸碰撞箱表 (定义在文件后面, 要用 ZOMBIE_* 常量)
void FillZombieBodyTable(PtHook::Control *ctl);

    this->hook_ctl->windup_hook = on ? 1 : 0;

    // 既有的首发延迟 UI 是"按速度分类"设置的, 这里映射到**每种植物**:
    //   秒 -> 帧 (游戏 100 Hz), 至少 1 帧; 不参与的分类给 -1 (DLL 就不碰它)。
    // ★ 双发 / 裂荚 / 猫尾草要**加上补发检查点** (老版源码 PlantWindupCheckpoint):
    //   它们靠"计数器路过某个值"补发第二颗 (双发/裂荚 26, 猫尾草 50), 所以钉住的值必须是
    //   "延迟 + 检查点" —— 否则计数器从延迟值直接走到 0, 永远路过不了检查点, 一轮少一颗。
    //   上限也照老版: 不超过植物自己的 mLaunchRate (表的 first_max 已经扣掉了检查点)。
    for (int seed = 0; seed < 48; seed++)
    {
        const int k = PlantSpeedClassOf(seed);
        if (k < 0 || k >= PSC_COUNT || !en[k])
        {
            this->hook_ctl->pin_by_seed[seed] = -1;
            continue;
        }
        int delay = static_cast<int>(sec[k] * 100.0f + 0.5f);
        if (delay < 1)
            delay = 1;
        if (delay > 800)
            delay = 800;

        int frames = delay + PlantWindupCheckpointOf(seed);
        const int launch_rate = 150; // 与分类表的 counter_default 一致 (mLaunchRate)
        if (frames > launch_rate)
            frames = launch_rate;
        this->hook_ctl->pin_by_seed[seed] = frames;

        // 索敌行范围 (行差): 0=只看本排; 1=本排±1 (忧郁菇 3x3 / 三线射手三排); 7=任意排 (香蒲)
        //   —— DLL 靠它判断"这棵植物眼里有没有僵尸", 必须和游戏真实索敌一致,
        //   否则会提前或延后脱手 (用户实测: 忧郁菇/香蒲/三线射手 需要各按自己的范围)。
        this->hook_ctl->pin_row_dev[seed] = PlantWindupRowDevOf(seed);
        // 攻击窗口 (相对植物 mX 的像素偏移): 忧郁菇/大喷菇/小喷菇这类短射程植物**必须**用它,
        //   否则"上下排远处的僵尸"会被误判成进射程 -> 提前脱手 (用户实测忧郁菇)。
        int win_lo = 0, win_hi = 0;
        PlantWindupWindowOf(seed, win_lo, win_hi);
        this->hook_ctl->pin_win_lo[seed] = win_lo;
        this->hook_ctl->pin_win_hi[seed] = win_hi;
        // 裂荚射手会打身后 (后头往左打): 身后窗口 [0, px+16], 只有本行。
        // 只打前方的植物这里给 0 —— 它们的窗口左边界已是 px+60, 身后自然不算进射程。
        this->hook_ctl->pin_back_hi[seed] = PlantWindupBackHiOf(seed);
    }
}

// ---------------------------------------------------------------------------
// 卡片冷却时间显示 (2026-10-10, kVersion 25): 定地址 + 下发开关
//
//   钩点 = `Board::DrawGameObjects` 渲染循环里 **RENDER_ITEM_BOTTOM_UI** 那条 case 的
//          `call Board::DrawUIBottom` (EN 1.0.0.1051 = **0x4172E7**, 原被调者 0x41A0A0)。
//
//   为什么不用当年那条"金币栏"钩点 0x4172BD: 那条 `call DrawUICoinBank` 属于
//   RENDER_ITEM_COIN_BANK (z=600000), 而种子卡属于 RENDER_ITEM_BOTTOM_UI, 它的 z 是
//   **RENDER_LAYER_UI_BOTTOM(100000) + 1 = 100001** —— 比金币栏**更早**画。所以那条点
//   在卡槽**之前**, 在那儿画冷却文字会被卡面盖住。
//   渲染循环按 z 升序画: UI_BOTTOM(100001, 卡槽在这条 case 里) < 金币栏(600000) < 顶栏
//   (700000, 含进度条) < 屏幕淡出(900000, 最后一条)。
//   ⇒ 把钩点放在 **RENDER_ITEM_BOTTOM_UI 那条 case 的原调用之后**, 于是:
//     * 文字必然盖在卡面之上 ✓ (卡槽就是这条 case 里画的)
//     * 之后画的金币栏/顶栏都不会盖到卡片区域 (卡片在底部 x≈79..759, y≈0..70;
//       金币栏在左下角, 进度条在顶栏) ✓
//     * 屏幕淡出仍然盖在文字之上 ✓ (不会浮在淡出/胜利画面上)
//   ★ 这条 case 之后还有**铲子**(同 case 内紧随卡槽) —— 铲子在 x≈706 起, 而 10 张卡
//     最宽到 x≈580 ⇒ 不重叠 (Board::DrawShovel / GetSeedBankExtraWidth 的坐标)。
//   ★ 我们**不重放**原调用、而是**在它之后**画, 所以钩点不能选"某条 call 之前"那种写法,
//     只能在原 call 的**调用点**上做手脚 —— 用的就是"5 字节等长 call 补丁":
//       0x4172E7: e8 b4 2d 00 00   call 0x41A0A0
//     把目标换成 DLL 里的跳板; 跳板的 `ret 4` 正好回到 0x4172EC, 也就是原调用之后的那条指令
//     (调用者压的那一份 Board 由调用者的 `add esp,4` 自己清 —— 原 callee 是 `ret 4`,
//      我们的跳板同样只清自己压的那一份, 栈平衡与原状完全一致)。
//
//   ★ 定位用**实时字节现算**, 不写死目标:
//     0x4172E7 那 5 字节必须是 `E8 rel32` 的 call; 目标地址由 rel32 现算后回给 DLL,
//     DLL 跳板重放的就是这个"现算出来的目标"。字节形状不符 ⇒ 一个地址都不下发,
//     DLL 侧看到 0 就一行游戏代码都不动 (只降级, 不崩游戏)。
//
//   ★★ v26 (2026-10-11) 加强: 光"是个 call"不够 —— 现算出来的目标**必须等于**
//      `Board::DrawUIBottom` (0x41A0A0)。v25 只查了首字节 0xE8, 而紧跟其后的
//      `call DrawFog` 同样以 0xE8 开头: 一旦选错钩点, 文字会画在卡槽之前
//      (= 被卡面盖住的静默失效), 极难排查。现在工具侧与 DLL 侧各自独立现算并
//      比对同一个常量 (PtHook::kCooldownCalleeEN), 任何一处不符就不下发/不写补丁。
// ---------------------------------------------------------------------------
static const uintptr_t kCooldownCallSiteEN = static_cast<uintptr_t>(PtHook::kCooldownSiteEN); // 0x4172E7
static const uintptr_t kGfxFillRectEN = 0x00586DC0;      // Graphics::FillRect(const Rect&)
// ★★ v40: 这两个地址原来由**碰撞箱**那条链下发 (`PublishHitboxes`), 冷却数字只是"搭便车";
//   碰撞箱整条链删掉后, 冷却数字的**矩形路径** (离线字模逐像素拼矩形) 就成了唯一使用者
//   ⇒ 把定位与字节自检**搬到这里** (与 FillRect 同一套做法, 一个字节都没改判定):
//     Graphics::SetColor(const Color&)  0x586CC0, 入口 `8B 10 89 51 30`
//       (= mov edx,[eax]; mov [ecx+0x30], edx —— 把 4 个 dword 的 Color 写进 g+0x30)
//     Graphics::DrawRect(const Rect&)   0x586EF0, 入口 `8B 48 0C` (与 FillRect 同形)
//   ★ 这是**行为等价**的搬迁: 以前每一帧 PublishHitboxes 会写这两个字段, 现在
//     PublishCooldown 每一帧写 (前端两处都是每帧调用)。判据与回读自检都在 DLL 侧独立再做一次。
static const uintptr_t kGfxSetColorEN = 0x00586CC0; // Graphics::SetColor(const Color&)
static const uintptr_t kGfxDrawRectEN = 0x00586EF0; // Graphics::DrawRect(const Rect&)

// 工具侧诊断: 定位结果 + 当时读到的原始字节 + 现算出来的被调者 (不符时一眼看出算成了谁)
static bool g_cd_located = false;
static uintptr_t g_cd_callee = 0;
static unsigned char g_cd_bytes[2][5] = {{0}, {0}};          // [0]=钩点 [1]=FillRect
static unsigned char g_gfx_bytes[2][5] = {{0}, {0}};         // [0]=SetColor [1]=DrawRect (v40 搬来的)

// ---------------------------------------------------------------------------
// "二分定位"探针开关: `set PZTK_CD_RED=1` 启动工具 ⇒ 每张冷却中的卡上先画一个
//   覆盖整张卡面的大红实心块, 再把数字画上去。界面**不加勾选框** (免得普通用户误开,
//   那会挡住卡面)。判读方式:
//     * 连红块都看不见  ⇒ 坐标/时机/FillRect 的调用有问题 (与颜色、数字无关);
//     * 红块看得见、没有白色数字 ⇒ 颜色或数字绘制有问题。
//   ★ 默认关闭: 正常路径已经完全够用 (v27 的根因是"FillRect 地址没下发", 已修)。
// ---------------------------------------------------------------------------
static bool CdRedProbeEnabled()
{
    static int cached = -1;
    if (cached < 0)
    {
        const char *v = std::getenv("PZTK_CD_RED");
        cached = (v != nullptr && v[0] != '\0' && v[0] != '0') ? 1 : 0;
    }
    return cached != 0;
}

// ---------------------------------------------------------------------------
// ★★ v37: 定位 `FONT_CONTINUUMBOLD14` 这个 `_Font*` 的**全局槽地址** ★★
//
//   为什么要"现读指令里的立即数"而不是写死 `0x6A7384`: 与钩点同一条教训 ——
//   写死的地址在别的构建上会**静默**指到别的东西, 而这里本来就**能**从游戏自己的
//   代码里读出来 (出厂 exe, 无 ASLR, 文件偏移 = VA - 0x400000):
//     0x477A5C: 68 E4 9D 66 00   push 0x669DE4            ; 数据段里的 "FONT_CONTINUUMBOLD14"
//     0x477A8D: A3 84 73 6A 00   mov ds:0x6A7384, eax     ; ★ 槽地址就是这个立即数
//   ⇒ 判据 = **指令形状 + 立即数指向的字符串逐字符等于字体名**, 两条都对上才算定位成功。
//     任何一条不符就不下发 (0), DLL 侧看到 0 直接走矩形路径 —— 功能照旧, 只是没有新方式。
//   ★ 全程只读: 我们**从不**写游戏内存 (这里只是 ReadBytes)。
// ---------------------------------------------------------------------------
static const uintptr_t kFontNamePushSiteEN = static_cast<uintptr_t>(PtHook::kFontNamePushSiteVA);
static const uintptr_t kFontGlobalStoreEN = static_cast<uintptr_t>(PtHook::kFontGlobalStoreVA);
static unsigned char g_font_bytes[2][5] = {{0}, {0}}; // [0]=push 名字 [1]=mov 槽
static uintptr_t g_font_slot = 0;                     // 定位到的全局槽地址 (0 = 没定位到)

static uint32_t CdU32Le(const unsigned char *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// 跳过原因位图 -> 人话。
//   ★ 中文文案必须留在**本文件** (而不是 hook/hook_ctl.h): gen_glyphs.ps1 只扫
//     frontend/ + src/ + inc/, 扫不到 hook/ 里的字符串, 写在那里的字会变成方框。
static const char *CdSkipText(uint32_t bit)
{
    switch (bit)
    {
    case PtHook::CDSKIP_BIT_OFF:            return "开关关着";
    case PtHook::CDSKIP_BIT_NO_ADDR:        return "工具没下发 SetColor/DrawRect 地址";
    case PtHook::CDSKIP_BIT_BOARD:          return "Board 指针不可读";
    case PtHook::CDSKIP_BIT_BANK:           return "mSeedBank 指针不合法";
    case PtHook::CDSKIP_BIT_UNREADABLE:     return "卡数组不可读";
    case PtHook::CDSKIP_BIT_COUNT:          return "卡数不在 1..10";
    case PtHook::CDSKIP_BIT_EMPTY:          return "空槽(正常)";
    case PtHook::CDSKIP_BIT_READY:          return "已就绪(正常)";
    case PtHook::CDSKIP_BIT_NOT_REFRESHING: return "卡不在冷却(正常: 可能被拿在光标上)";
    case PtHook::CDSKIP_BIT_TENTHS:         return "冷却读数为 0(刚好走完)";
    case PtHook::CDSKIP_BIT_GEOM:           return "卡宽高不合理(已按 50x70 兜底)";
    case PtHook::CDSKIP_BIT_PANEL:          return "字模格子放不进卡面可用区(卡太小)";
    // ★ v29: "绝不盖住阳光数" 的两条 —— 用户实机投诉的就是这两处相交
    case PtHook::CDSKIP_BIT_COST:           return "会压住卡上的阳光费用 ⇒ 跳过(绝不遮阳光)";
    case PtHook::CDSKIP_BIT_SUN:            return "会压住阳光计数器 ⇒ 跳过(绝不遮阳光)";
    // ★ v30 新增, v31 改语义: 连 2x2 聚合都放不下 (性能闸门; 正常版面 10 张卡也用不到)
    case PtHook::CDSKIP_BIT_BUDGET:         return "本帧矩形预算用完(连 2x2 都放不下) ⇒ 不画(保帧率)";
    default:                                return "";
    }
}

// 位图 -> "原因1 | 原因2" (只报 abnormal 的位; mask 里只有正常位时返回空串)
//   ★ 位序 = CdSkipBit 的定义顺序; 每一项都由 recttest 断言"有位就有文案"。
//   ★ 新增跳过位时**必须同时改这里和 CdSkipText()** —— recttest 有一条断言就是在数这两个表。
static std::string CdSkipMaskText(uint32_t mask, int32_t card_index)
{
    static const uint32_t kBits[] = {
        PtHook::CDSKIP_BIT_OFF,       PtHook::CDSKIP_BIT_NO_ADDR,        PtHook::CDSKIP_BIT_BOARD,
        PtHook::CDSKIP_BIT_BANK,      PtHook::CDSKIP_BIT_UNREADABLE,     PtHook::CDSKIP_BIT_COUNT,
        PtHook::CDSKIP_BIT_TENTHS,    PtHook::CDSKIP_BIT_GEOM,           PtHook::CDSKIP_BIT_PANEL,
        PtHook::CDSKIP_BIT_COST,      PtHook::CDSKIP_BIT_SUN,            PtHook::CDSKIP_BIT_BUDGET,
    };
    std::string out;
    int hit = 0;
    for (uint32_t bit : kBits)
    {
        if ((mask & bit) == 0)
            continue;
        const char *t = CdSkipText(bit);
        if (t[0] == '\0')
            continue;
        if (!out.empty())
            out += " | ";
        out += t;
        hit++;
    }
    if (hit > 0 && card_index >= 0)
    {
        char tail[32];
        std::snprintf(tail, sizeof(tail), " (卡#%d)", static_cast<int>(card_index));
        out += tail;
    }
    return out;
}

bool PvZ::PublishCooldown(bool on, uint32_t mode)
{
    if (this->hook_ctl == nullptr)
        return false;

    uintptr_t call_site = 0;
    uintptr_t fill_rect = 0;
    unsigned char pf[5] = {0};
    // 形状自检: 钩点必须是 call(0xE8); FillRect 入口前 3 字节 `8B 48 0C`
    //   (与 DrawRect 的 `8B 48 0C` 同一个形状 —— 反汇编核对过, 两者都是
    //    "从 EAX 取 Rect, 再 jmp/ret 4 的内部实现")
    const bool shape_ok = this->ReadBytes(g_cd_bytes[0], kCooldownCallSiteEN, 5) &&
                          g_cd_bytes[0][0] == 0xE8 && //
                          this->ReadBytes(pf, kGfxFillRectEN, 5) && pf[0] == 0x8B && pf[1] == 0x48 &&
                          pf[2] == 0x0C;
    memcpy(g_cd_bytes[1], pf, 5);

    // ---- ★★ v40: 矩形路径要用的另两个 Graphics 地址 (从删掉的碰撞箱链搬过来) ★★
    //   SetColor 的形状 = `8B 10 89 51 30` (mov edx,[eax]; mov [ecx+0x30], edx)
    //   DrawRect 的形状 = `8B 48 0C` (与 FillRect 同形)
    //   ★ 与 FillRect 一样是**逐字节自检**, 不符就不下发 (DLL 侧 CdFrameGate 会因此报
    //     "工具没下发地址" 并整帧不画 —— 那是可见的降级, 不是静默失效)。
    unsigned char ps[5] = {0};
    unsigned char pr[5] = {0};
    const bool gfx_ok = this->ReadBytes(ps, kGfxSetColorEN, 5) && ps[0] == 0x8B && ps[1] == 0x10 &&
                        ps[2] == 0x89 && ps[3] == 0x51 && ps[4] == 0x30 &&
                        this->ReadBytes(pr, kGfxDrawRectEN, 5) && pr[0] == 0x8B && pr[1] == 0x48 &&
                        pr[2] == 0x0C;
    memcpy(g_gfx_bytes[0], ps, 5);
    memcpy(g_gfx_bytes[1], pr, 5);

    // ---- ★ 用实时字节现算被调者, 并要求它**正好**是 Board::DrawUIBottom ----
    uintptr_t callee = 0;
    if (shape_ok)
    {
        const uint32_t rel = static_cast<uint32_t>(g_cd_bytes[0][1]) |
                             (static_cast<uint32_t>(g_cd_bytes[0][2]) << 8) |
                             (static_cast<uint32_t>(g_cd_bytes[0][3]) << 16) |
                             (static_cast<uint32_t>(g_cd_bytes[0][4]) << 24);
        callee = kCooldownCallSiteEN + 5 + static_cast<uintptr_t>(static_cast<intptr_t>(static_cast<int32_t>(rel)));
    }
    g_cd_callee = callee;
    if (shape_ok && callee == static_cast<uintptr_t>(PtHook::kCooldownCalleeEN))
    {
        call_site = kCooldownCallSiteEN;
        fill_rect = kGfxFillRectEN;
    }
    g_cd_located = (call_site != 0);

    PtHook::Control *ctl = this->hook_ctl;
    // ★ 地址只在**开关打开且形状自检通过**时下发 —— 关掉时把钩点地址清成 0,
    //   DLL 那边"地址为 0 ⇒ 不碰游戏代码"; 已经打上去的补丁由开关(cd_on)兜住:
    //   跳板照样跑, 只是立刻返回不画 (还原补丁要等卸载 —— 热路径上
    //   反复改代码比多跑一次空跳板危险得多)。
    ctl->cd_call_addr = static_cast<uint32_t>(call_site);
    // ★★ v27 修的"画了但看不见": FillRect 的地址**必须真的下发** ★★
    //   上一版这里只把地址存进了一个**局部量 `fill_rect` 再也没读过** (Control 里当时
    //   根本没有对应字段), 于是 DLL 的 `g_cd_fill` 恒为 0、绘制第一道闸门就 return
    //   ⇒ 界面显示"已生效"(那只说明钩点自检过了) 而卡面**一片空白**。
    //   ⇒ 现在把它写进控制块; DLL 侧还会**各自独立**再核对一次入口字节 (见 hook_main.cpp
    //     的 ResolveFillRect), 不通过就降级成 DrawRect 描边 —— 功能可见性不再依赖它。
    ctl->gfx_fill_rect_addr = static_cast<uint32_t>(fill_rect);
    // ★★ v40: SetColor / DrawRect 也由这里下发 (以前是碰撞箱那条链下发的) ★★
    //   自检不过 ⇒ 写 0 ⇒ DLL 的 CdFrameGate 报"工具没下发地址"、整帧不画 (可见的降级)。
    ctl->gfx_set_color_addr = gfx_ok ? static_cast<uint32_t>(kGfxSetColorEN) : 0u;
    ctl->gfx_draw_rect_addr = gfx_ok ? static_cast<uint32_t>(kGfxDrawRectEN) : 0u;
    ctl->cd_red_probe = CdRedProbeEnabled() ? 1u : 0u;
    // ★★ v28 根因就在这里 ★★ (v27 原文: `ctl->cd_on = (on && call_site != 0) ? 1u : 0u;`)
    //   把**勾选框的状态**与"这一次地址自检过没过"相与, 后果是:
    //     `call_site` 是**每帧从游戏进程现读 5 字节现算**出来的 —— 换关/读页/游戏重开
    //     的那几帧只要读失败 (或现算的被调者不是 Board::DrawUIBottom), 这一帧就把
    //     `cd_on` 写成 0。而 `cd_hook_ok` 是**安装成功就再也不复位**的闩锁 (只有卸载才清),
    //     于是界面同时还能显示"已生效" ⇒ 用户看到勾选框**勾着**, DLL 每帧读到 **0**,
    //     在第一道闸门就 return, 诊断报"开关关着", 而**没有任何地方**能看出两侧不一致。
    //   ⇒ 现在职责拆开、各管各的:
    //     `cd_on`       = **只**反映勾选框 (用户意图)。每帧原样下发, 绝不与地址自检相与。
    //     `cd_call_addr`= 这一帧地址到底能不能用 (0 ⇒ DLL 报"没下发地址", 那是**另一条**
    //                     原因, 不再冒充"开关关着"; 已打上的补丁由闩锁 + cd_on 兜住 ——
    //                     热路径上不反复改游戏代码)。
    ctl->cd_on = on ? 1u : 0u;
    // ★★ v37: 模式 (三选一) 与**字体全局槽**的定位 ★★
    //   ① 模式: 与 cd_on 同一套规矩 —— **原样下发**, 绝不与地址自检结果相与。
    //      模式未知 (不在 0..2) 时 DLL 会自己强制走矩形路径 (硬规矩), 这里不做修正。
    //   ② 字体全局槽: 只在下发值合法时非 0。定位失败的完整字节留在 g_font_bytes 里,
    //      诊断行会把它们打出来 (与钩点/FillRect 的写法一致)。
    ctl->cd_mode = mode;
    {
        unsigned char push_bytes[5] = {0};
        unsigned char store_bytes[5] = {0};
        uintptr_t slot = 0;
        const bool shape = this->ReadBytes(push_bytes, kFontNamePushSiteEN, 5) &&
                           push_bytes[0] == 0x68 && // push imm32 (字体名的 VA)
                           this->ReadBytes(store_bytes, kFontGlobalStoreEN, 5) &&
                           store_bytes[0] == 0xA3; // mov ds:imm32, eax
        memcpy(g_font_bytes[0], push_bytes, 5);
        memcpy(g_font_bytes[1], store_bytes, 5);
        if (shape)
        {
            const uint32_t name_va = CdU32Le(push_bytes + 1);
            const uint32_t slot_va = CdU32Le(store_bytes + 1);
            // 立即数指向的字符串必须**逐字符**等于字体名 (含结尾 '\0')
            char got[PtHook::kFontNameLen + 1] = {0};
            bool name_ok = this->ReadBytes(got, name_va, static_cast<size_t>(PtHook::kFontNameLen) + 1);
            if (name_ok)
            {
                for (int i = 0; i <= PtHook::kFontNameLen; i++)
                {
                    if (got[i] != PtHook::kFontNameNeedle[i])
                    {
                        name_ok = false;
                        break;
                    }
                }
            }
            // 槽地址必须落在这个模块的映像范围里 (0x400000..0x1000000 足够宽松)
            if (name_ok && slot_va >= 0x00400000u && slot_va < 0x01000000u)
                slot = static_cast<uintptr_t>(slot_va);
        }
        g_font_slot = slot;
        ctl->gfx_font_global_addr = static_cast<uint32_t>(slot);
    }
    // 记下"我们到底下发了什么": 诊断行的"工具="那一侧读的就是它 ——
    //   它与勾选框、与写进控制块的 cd_on **三者同源**, 所以"工具=开但 DLL=关"
    //   只可能是跨进程这一段断了, 不可能是显示错了。
    this->cd_published_on = on;
    this->cd_published_mode = mode;
    return ctl->cd_on != 0;
}

bool PvZ::CooldownHookOk()
{
    return this->hook_ctl != nullptr && this->hook_ctl->cd_hook_ok != 0;
}

// ---------------------------------------------------------------------------
// 卡片冷却时间的状态 + **"能看见的诊断"** (v27)
//
//   ★ 为什么要改: v26 的失败模式是"界面显示**已生效**, 卡面上什么都没有" ——
//     而"已生效"只说明**钩点自检通过** (补丁打上了), 完全不反映"真的画出来了"。
//     现在这一行直接给出: 本帧画了几张 / 冷却中几张 / 为什么没画 / 坐标与读数探针。
//     用户只要把这一行发回来就能定位, 不用来回试。
//   ★ 这一行**不再只在 PZTK_DEV=1 里**显示 (用户明确要求)。
// ---------------------------------------------------------------------------
// 跨进程连线异常码 -> 人话 (判断在共享头 CdWiringHealth(), 文案只在这一处)
//   ★ 中文必须留在本文件: gen_glyphs.ps1 只扫 frontend/ + src/ + inc/ (见 CdSkipText 说明)。
// ---------------------------------------------------------------------------
// ★★ v37: 模式 / 原生路径自检结论 -> 人话 ★★
//   ★ 中文必须留在本文件: gen_glyphs.ps1 只扫 frontend/ + src/ + inc/ (见 CdSkipText 说明)。
//   ★ 这两张表与 `PtHook::CdDrawMode` / `PtHook::CdNativeFail` **一一对应**,
//     recttest 有一条断言在数它们 (新增枚举值必须同时补文案)。
//   ★ 必须定义在 `CooldownDrawStatus()` **之前** (它就在这下面用它)。
// ---------------------------------------------------------------------------
static const char *CdModeText(uint32_t mode)
{
    switch (mode)
    {
    case PtHook::CDMODE_OFF:    return "关闭";
    case PtHook::CDMODE_RECT:   return "矩形(老方式)";
    case PtHook::CDMODE_NATIVE: return "游戏字体(新方式)";
    default:                    return "未知";
    }
}

static const char *CdNativeText(uint32_t st)
{
    switch (st)
    {
    case PtHook::CDNAT_OK:         return "可用(本帧就是它画的)";
    case PtHook::CDNAT_MODE_OFF:   return "本帧不是原生模式";
    case PtHook::CDNAT_NO_ADDR:    return "工具没下发字体全局槽 ⇒ 已降级成矩形";
    case PtHook::CDNAT_FONT_NULL:  return "字体全局槽还是 0(游戏还没加载字体) ⇒ 已降级成矩形";
    case PtHook::CDNAT_FONT_BAD:   return "字体对象/虚表不可读或不是 _Font ⇒ 已降级成矩形";
    case PtHook::CDNAT_VTABLE_BAD: return "虚表里的入口字节不符 ⇒ 已降级成矩形";
    case PtHook::CDNAT_ABI_BAD:    return "DrawString 结尾不是 ret 0x18(调用约定不符) ⇒ 已降级成矩形";
    case PtHook::CDNAT_LAYOUT_BAD: return "std::string 布局指纹不符(SSO) ⇒ 已降级成矩形";
    case PtHook::CDNAT_FIT_BAD:    return "文字框放不进可画区(会压阳光) ⇒ 已降级成矩形";
    case PtHook::CDNAT_DRAW_ERR:   return "绘制输入不合法 ⇒ 已降级成矩形";
    default:                       return "未知";
    }
}

static const char *CdWireText(uint32_t wire)
{
    switch (wire)
    {
    case PtHook::CDWIRE_OK:           return "连线正常";
    case PtHook::CDWIRE_NEVER_SENT:   return "工具勾着, 但跳板一次都没跑 (帧级断线)";
    case PtHook::CDWIRE_SEEN_OFF:     return "工具勾着, DLL 读到的却是关 —— 开关没送到 DLL 侧";
    case PtHook::CDWIRE_NOT_LATCHED:  return "补丁没打上 (钩点地址/字节不符)";
    case PtHook::CDWIRE_NO_DRAW:      return "有卡在冷却, 两侧都是开, 却一张都没画";
    default:                          return "未知";
    }
}

// ---------------------------------------------------------------------------
// ★★ v38: "中断原因" -> 人话 ★★
//   ★ 中文必须留在本文件 (与上面三张表同一个理由: gen_glyphs.ps1 只扫 frontend/ + src/ + inc/)。
//   ★ 判断在共享头 `PtHook::CdInterruptKindOf()` 里 (**唯一**一份), 这里只负责翻译。
//     recttest 断言: 每个枚举值都有文案、两两不同、且"钩点被撤"与"未注入"的文案能区分开。
// ---------------------------------------------------------------------------
static const char *CdInterruptText(uint32_t kind)
{
    switch (kind)
    {
    case PtHook::CDINT_OK:           return "正常 (钩点在, 本帧也画出来了)";
    case PtHook::CDINT_MODE_OFF:     return "冷却显示被关闭 (不是故障)";
    case PtHook::CDINT_NOT_INJECTED: return "★尚未注入 (DLL 没挂上钩子) —— 关卡外会持续重试, 关卡内只执行最后一次请求";
    case PtHook::CDINT_HOOK_GONE:    return "★钩点被撤/断链 (曾经生效过, 现在 hook_ok=0) —— DLL 已自行卸载, 工具会在关卡外持续重试";
    case PtHook::CDINT_NO_CALL_ADDR: return "钩点地址没下发 (工具侧地址自检没过)";
    case PtHook::CDINT_HOOK_FAIL:    return "补丁打不上 (钩点字节不符/写保护/回读不符)";
    case PtHook::CDINT_HOOK_PENDING: return "补丁还没打上 (正常过渡帧)";
    case PtHook::CDINT_NO_CALLS:     return "跳板一次都没跑 (这一帧没走到 DrawUIBottom)";
    case PtHook::CDINT_DRAW_ZERO:    return "钩点在, 本帧一张卡都没画 (看上面的跳过原因/预算)";
    default:                         return "未知";
    }
}

// ---------------------------------------------------------------------------
// ★★ v38: 一行"中断原因" ★★
//   判据在共享头 `PtHook::CdInterruptKindOf()` (**唯一**一份, 离线单测钉住),
//   这里只负责翻译 + 把关键数字一并打出来 (用户把这一行发回来就能定位)。
//   ★ 为什么要这一行: 用户报"注入不稳定, 偶尔会中断, 拖主界面也会中断" ——
//     "中断"至少有三类完全不同的原因 (钩点被撤 / 补丁没打上 / 钩点在但没画),
//     没有这一行就只能靠来回猜。现在一眼可辨 (见 CdInterruptText 的文案)。
// ---------------------------------------------------------------------------
std::string PvZ::HookInterruptText()
{
    if (this->hook_ctl == nullptr)
        return "中断原因: 没有共享通道 (还没建通道 / 没连上游戏)";
    const PtHook::Control *ctl = this->hook_ctl;
    const uint32_t kind = PtHook::CdInterruptKindOf(
        this->hook_seen_alive, ctl->hook_ok, ctl->hook_calls, ctl->cd_seen_mode, ctl->cd_call_addr,
        ctl->cd_hook_ok, ctl->cd_hook_fail, ctl->cd_calls, ctl->cd_cards_last);
    const char *tag = (kind == PtHook::CDINT_OK) ? "注入与绘制" : "★中断原因";
    char buf[360];
    std::snprintf(buf, sizeof(buf),
                  "%s: %s | 主钩子 ok=%u 调用=%u | 冷却钩点 地址=0x%X 补丁=%u(失败码%u) 跳板=%u "
                  "本帧画=%u | 心跳=%u",
                  tag, CdInterruptText(kind), static_cast<unsigned>(ctl->hook_ok),
                  static_cast<unsigned>(ctl->hook_calls), static_cast<unsigned>(ctl->cd_call_addr),
                  static_cast<unsigned>(ctl->cd_hook_ok), static_cast<unsigned>(ctl->cd_hook_fail),
                  static_cast<unsigned>(ctl->cd_calls), static_cast<unsigned>(ctl->cd_cards_last),
                  static_cast<unsigned>(ctl->tool_beat));
    return std::string(buf);
}

std::string PvZ::CooldownDrawStatus()
{
    if (this->hook_ctl == nullptr)
        return "卡片冷却时间: 没有共享通道 —— DLL 未注入";
    const PtHook::Control *ctl = this->hook_ctl;
    char buf[3072];
    // ★ v38: "中断原因"那一行 (判据在共享头, 这里算一次、两个分支都用)
    const std::string interrupt = this->HookInterruptText();

    // 跳过原因 (只报 abnormal 的位)
    const std::string skip = CdSkipMaskText(ctl->cd_skip_mask, ctl->cd_skip_card);
    const char *skip_txt = skip.empty() ? "无" : skip.c_str();

    // ---- ★★ v28: 两侧状态并列显示 + 由共享头纯函数给出的"连线结论" ★★ ----
    //   为什么要显示**工具侧**这一份: v27 的病就是"工具以为自己下发了开、DLL 读到的是关",
    //   而旧诊断只显示 DLL 那一侧 (于是只看到"开关关着", 完全不知道工具其实勾着)。
    //   现在两个数一起打出来, 不一致本身就是铁证。
    //   `tool_on` 用 cd_published_on (最后一次 PublishCooldown 的入参, 见 pvz.h 的说明)
    //   —— 与下方"工具=开/关"和勾选框永远一致。
    const bool tool_on = this->cd_published_on;
    const uint32_t wire =
        PtHook::CdWiringHealth(tool_on, ctl->cd_seen_on, ctl->cd_calls, ctl->cd_hook_ok,
                               ctl->cd_call_addr, ctl->cd_cards_last, ctl->cd_cards_cooling);
    // 连线异常文案的**唯一一份** (中文必须留在本文件, 见上面 CdSkipText 的说明)
    const char *wire_txt = CdWireText(wire);
    char wire_buf[192];
    if (wire == PtHook::CDWIRE_OK)
        std::snprintf(wire_buf, sizeof(wire_buf), "连线正常");
    else
        std::snprintf(wire_buf, sizeof(wire_buf), "★连线异常: %s", wire_txt);
    // ★ 开关两侧:**先显示工具侧那份** (g_cooldown_on), 再显示 DLL 回读的那份 (cd_seen_on)
    const char *tool_txt = tool_on ? "开" : "关";
    const char *seen_txt = (ctl->cd_seen_on != 0) ? "开" : "关";
    // ★★ v37: 模式也要**两侧并列** —— 模式决定画法, 两侧不一致时画的完全是两种东西 ★★
    const char *tool_mode_txt = CdModeText(this->cd_published_mode);
    const char *seen_mode_txt = CdModeText(ctl->cd_seen_mode);
    const uint32_t mode_wire = PtHook::CdModeHealth(this->cd_published_mode, ctl->cd_seen_mode,
                                                    ctl->cd_calls);
    const bool mode_unknown = !PtHook::CdModeValid(ctl->cd_seen_mode);
    char mode_buf[256];
    if (mode_wire == PtHook::CDMODE_WIRE_MISMATCH)
        std::snprintf(mode_buf, sizeof(mode_buf),
                      "★模式两侧不一致(工具=%s / DLL=%s) —— 跨进程这一段断了, 或游戏里是旧 DLL",
                      tool_mode_txt, seen_mode_txt);
    else if (mode_unknown)
        std::snprintf(mode_buf, sizeof(mode_buf), "★模式值未知(%u) ⇒ 已强制走矩形路径",
                      static_cast<unsigned>(ctl->cd_seen_mode));
    else
        std::snprintf(mode_buf, sizeof(mode_buf), "模式一致 (%s)", seen_mode_txt);
    // 本帧到底用哪种方式画的 (原生自检失败时会写回非 0 ⇒ 说明**已经降级**)
    const bool native_used = (ctl->cd_native_state == PtHook::CDNAT_OK);
    const char *way_txt = native_used ? "游戏字体(新方式)"
                                      : ((ctl->cd_seen_mode == PtHook::CDMODE_OFF) ? "未画(关闭)"
                                                                                   : "矩形(老方式)");

    if (ctl->cd_hook_ok != 0)
    {
        // ★ v29: 第二行加了"本帧真的用的数字几何" —— 用户投诉的是"好丑/看不清"(尺寸与位置),
        //   所以必须能直接看见"数字多大、底衬多宽", 否则只能靠猜。
        // ★ v30: 第三行加了**矩形预算用量** —— 字模是"逐像素画矩形", 所以"这一帧到底
        //   下发了几百个矩形"是"会不会卡"的唯一可见证据 (cb 显示 本帧/预算 与单卡用量)。
        // ★ v31: 第三行还显示**聚合档位** —— 1x1 逐像素与 2x2 聚合的观感明显不同,
        //   用户看到"有点糊"时必须能判断"这是预算降级, 不是字模坏了"。
        // ★ v37: 新增第四、五行 —— **绘制方式 / 原生路径自检结论 / 文字框大小与落点 /
        //   两个入口地址 / 两种方式的耗时(µs)** —— 用户要对比两种方式的"质量与开销"。
        const char *step_txt = (ctl->cd_rects_step == PtHook::CDSTEP_BLOCK1) ? "1x1逐像素"
                               : (ctl->cd_rects_step == PtHook::CDSTEP_BLOCK2) ? "2x2聚合"
                                                                              : "未画";
        std::snprintf(buf, sizeof(buf),
                      "工具=%s 模式=%s | DLL=%s 模式=%s | 画了 %u 张 | 冷却中 %u 张 | 跳过: %s | %s\n"
                      "卡面探针: Bank=0x%X 卡数=%d 首张冷却卡=(%d,%d %dx%d) 剩余=%d/%d tick "
                      "屏移=(%d,%d) | FillRect=%s(0x%X)\n"
                      "字模几何: 单个字符 %dx%d (逐像素灰度字模, 见 cd_font.h) | 位置外框 %dx%d "
                      "(可画区=卡面标题条以下、阳光费用之上, 数字在可画区里上下居中; "
                      "与阳光计数器/费用相交会跳过并报原因; v31 不填底衬、不画阴影)\n"
                      "矩形预算: 本帧 %d / %d (单卡 %d, 档位 %s; 超预算就先聚合成 2x2, 再超就整张不画)\n"
                      "绘制方式: %s | 原生自检=%s\n"
                      "原生细节: 字体=%s %dpt(标称) 全局槽=0x%X | 校准 %+d,%+d (居中后叠加) | "
                      "步进(tw,mHeight)=%dx%d | "
                      "墨迹盒=%dx%d @(%d,%d) | 可画带=卡顶+%d..卡顶+%d (高 %d) | "
                      "绘制缩放=%.3f,%.3f | 字体对象=0x%X | DrawString=0x%X StringWidth=0x%X\n"
                      "绘制耗时(QPC 只量我们这一段): 原生 %d us/帧 | 矩形 %d us/帧 | 本帧模式: %s\n"
                      "%s",
                      tool_txt, tool_mode_txt, seen_txt, seen_mode_txt, ctl->cd_cards_last,
                      ctl->cd_cards_cooling, skip_txt, wire_buf, //
                      static_cast<unsigned>(ctl->cd_probe_bank), static_cast<int>(ctl->cd_probe_count),
                      static_cast<int>(ctl->cd_probe_x), static_cast<int>(ctl->cd_probe_y),
                      static_cast<int>(ctl->cd_probe_w), static_cast<int>(ctl->cd_probe_h),
                      static_cast<int>(ctl->cd_probe_total - ctl->cd_probe_counter),
                      static_cast<int>(ctl->cd_probe_total), //
                      static_cast<int>(ctl->cd_probe_trans_x), static_cast<int>(ctl->cd_probe_trans_y),
                      ctl->cd_probe_fill != 0 ? "实心" : "缺(降级描边)",
                      static_cast<unsigned>(ctl->cd_probe_fill),
                      static_cast<int>(ctl->cd_probe_digit_w), static_cast<int>(ctl->cd_probe_digit_h),
                      static_cast<int>(ctl->cd_probe_panel_w), static_cast<int>(ctl->cd_probe_panel_h),
                      static_cast<int>(ctl->cd_rects_last), static_cast<int>(ctl->cd_rect_budget),
                      static_cast<int>(ctl->cd_rects_card), step_txt, //
                      way_txt, CdNativeText(ctl->cd_native_state), //
                      PtHook::kFontNameNeedle, static_cast<int>(PtHook::kFontPointSize),
                      static_cast<unsigned>(g_font_slot),
                      static_cast<int>(PtHook::kCdNativeNudgeX),
                      static_cast<int>(PtHook::kCdNativeNudgeY),
                      static_cast<int>(ctl->cd_native_w), static_cast<int>(ctl->cd_native_h),
                      static_cast<int>(ctl->cd_native_ink_w), static_cast<int>(ctl->cd_native_ink_h),
                      static_cast<int>(ctl->cd_native_x), static_cast<int>(ctl->cd_native_y),
                      static_cast<int>(PtHook::kCdTitleBandH), static_cast<int>(PtHook::kCdSunCostTopY),
                      static_cast<int>(PtHook::kCdSunCostTopY - PtHook::kCdTitleBandH),
                      static_cast<double>(ctl->cd_native_scale_x) / 1000.0,
                      static_cast<double>(ctl->cd_native_scale_y) / 1000.0,
                      static_cast<unsigned>(ctl->cd_native_font),
                      static_cast<unsigned>(ctl->cd_native_draw_fn),
                      static_cast<unsigned>(ctl->cd_native_sw_fn), //
                      static_cast<int>(ctl->cd_native_us), static_cast<int>(ctl->cd_rect_us),
                      mode_buf, interrupt.c_str());
        return std::string(buf);
    }

    // 未生效: 把"卡在哪一步"摊开 (工具侧定位结果 + DLL 侧的失败码)
    std::snprintf(buf, sizeof(buf),
                  "工具=%s 模式=%s | DLL=%s 模式=%s | 未生效 | 钩点=0x%X 绘制调用=%u 失败=%u "
                  "(0=无 1=没地址 2=字节不符 3=写保护 4=写失败 5=回读不符) | %s\n"
                  "工具侧: 定位=%s 现算被调者=0x%X (要求 0x%X) 钩点字节=%02X %02X %02X %02X %02X | "
                  "FillRect=%02X %02X %02X %02X %02X\n"
                  "字体槽(v37): 定位=%s 地址=0x%X 槽字节=%02X %02X %02X %02X %02X | "
                  "名字字节=%02X %02X %02X %02X %02X | %s\n"
                  "主钩子(UpdateShooter)调用=%u (它>0 说明 DLL 的钩子确实在游戏里跑)\n"
                  "%s",
                  tool_txt, tool_mode_txt, seen_txt, seen_mode_txt, ctl->cd_call_addr, ctl->cd_calls,
                  ctl->cd_hook_fail, wire_buf, g_cd_located ? "OK" : "失败",
                  static_cast<unsigned>(g_cd_callee),
                  static_cast<unsigned>(PtHook::kCooldownCalleeEN), g_cd_bytes[0][0], g_cd_bytes[0][1],
                  g_cd_bytes[0][2], g_cd_bytes[0][3], g_cd_bytes[0][4], g_cd_bytes[1][0],
                  g_cd_bytes[1][1], g_cd_bytes[1][2], g_cd_bytes[1][3], g_cd_bytes[1][4],
                  g_font_slot != 0 ? "OK" : "失败", static_cast<unsigned>(g_font_slot),
                  g_font_bytes[1][0], g_font_bytes[1][1], g_font_bytes[1][2], g_font_bytes[1][3],
                  g_font_bytes[1][4], g_font_bytes[0][0], g_font_bytes[0][1], g_font_bytes[0][2],
                  g_font_bytes[0][3], g_font_bytes[0][4], mode_buf, ctl->hook_calls,
                  interrupt.c_str());
    return std::string(buf);
}

// 一行版诊断 (常驻显示)。内容 = 上面那两行里的**第一行** —— 定位问题所需的全部数字都在里面。
//   ★ v28: 第一行现在就是 `工具=开 | DLL=开 | 画了 N 张 | 冷却中 M 张 | 跳过: xxx | 连线结论`,
//     所以"没生效"时也**照样**有这一行可用 (v27 那种"整行消失"的情况已经不可能出现)。
std::string PvZ::CooldownDrawBrief()
{
    if (this->hook_ctl == nullptr)
        return "未注入 DLL";
    const std::string full = CooldownDrawStatus();
    const size_t nl = full.find('\n');
    return (nl == std::string::npos) ? full : full.substr(0, nl);
}

// ★ v29: 常驻的"字模几何"一行 (短, 一眼能读完 —— 用户关心的就是"它到底多大")。
//   为什么短: 常驻行会被 ImGui 按块宽**裁掉尾巴**, 所以这里只放关键数
//   (单个字符宽高 + **矩形用量/预算/档位**), 完整解释留在 tooltip 的第三、四行。
//   ★ v30: 段粗字段已废弃 (字模是位图), 换成**矩形预算用量** —— 这是"会不会卡"的直接证据。
//   ★ v31: 再补上**聚合档位** (1x1 逐像素 / 2x2 聚合) —— 观感差异明显, 必须可见。
//   本帧一张都没画时明确写出来 (否则用户会以为"数字还是老的"):
//     `字模: 本帧没有画出来的卡 (见"跳过")`
std::string PvZ::CooldownDrawGeom()
{
    if (this->hook_ctl == nullptr)
        return "字模: 未注入 DLL";
    const PtHook::Control *ctl = this->hook_ctl;
    // ★★ v37: 第二种方式 (游戏字体) 的"几何"完全是另一套数 —— 文字宽高由**游戏自己
    //   量出来** (StringWidth x mHeight), 与字模画布无关 ⇒ 必须分开报, 否则用户会拿
    //   "单字 10x14" 去解释一个由游戏字体画出来的数字。
    //   两种方式的**耗时 (µs/帧)** 都在这一行里, 这正是用户要对比的"开销"。
    if (ctl->cd_native_state == PtHook::CDNAT_OK && ctl->cd_native_h > 0 && ctl->cd_cards_last > 0)
    {
        char nbuf[320];
        // ★★ v40: 把**实际用的字体与字号**写出来 (用户要求: 方便一眼核对"到底换成哪个了") ★★
        //   `%s %dpt` = 共享头里的字体名与标称字号 (常量); `高度 %d` = DLL 现读的
        //   `_Font::mHeight` (= `cd_native_h`) ⇒ **常量与运行期实测并排**, 换错字体立刻看得出来。
        // ★★ v42: 再把**落点校准量**写出来 (用户要求: "当前校准值"要能一眼看到) ★★
        //   格式 = `校准 +2,+2`; 它是共享头里的编译期常量 (`kCdNativeNudgeX/Y`), 所以这里
        //   打的就是**这一版真正在用**的值 —— 与 DLL 用的是同一份头文件, 不可能对不上。
        std::snprintf(nbuf, sizeof(nbuf),
                      "游戏字体: %s %dpt | 校准 %+d,%+d (居中后叠加) | 步进 %dx%d | "
                      "墨迹盒 %dx%d @(%d,%d) | 可画带 卡顶+%d..卡顶+%d | "
                      "缩放 %.3f,%.3f | 本帧 %u 张 | 耗时 原生 %d us / 矩形 %d us",
                      PtHook::kFontNameNeedle, static_cast<int>(PtHook::kFontPointSize),
                      static_cast<int>(PtHook::kCdNativeNudgeX),
                      static_cast<int>(PtHook::kCdNativeNudgeY),
                      static_cast<int>(ctl->cd_native_w), static_cast<int>(ctl->cd_native_h),
                      static_cast<int>(ctl->cd_native_ink_w), static_cast<int>(ctl->cd_native_ink_h),
                      static_cast<int>(ctl->cd_native_x), static_cast<int>(ctl->cd_native_y),
                      static_cast<int>(PtHook::kCdTitleBandH), static_cast<int>(PtHook::kCdSunCostTopY),
                      static_cast<double>(ctl->cd_native_scale_x) / 1000.0,
                      static_cast<double>(ctl->cd_native_scale_y) / 1000.0,
                      ctl->cd_cards_last,
                      static_cast<int>(ctl->cd_native_us), static_cast<int>(ctl->cd_rect_us));
        return std::string(nbuf);
    }
    if (ctl->cd_probe_digit_w <= 0 || ctl->cd_probe_digit_h <= 0)
    {
        char ebuf[240];
        const bool native_mode = (ctl->cd_seen_mode == PtHook::CDMODE_NATIVE);
        std::snprintf(ebuf, sizeof(ebuf), "字模: 本帧没有画出来的卡 (模式=%s, 原生自检: %s, 耗时 原生 %d us / 矩形 %d us)",
                      CdModeText(ctl->cd_seen_mode),
                      native_mode ? CdNativeText(ctl->cd_native_state) : "不需要",
                      static_cast<int>(ctl->cd_native_us), static_cast<int>(ctl->cd_rect_us));
        return std::string(ebuf);
    }
    const char *step_txt = (ctl->cd_rects_step == PtHook::CDSTEP_BLOCK1) ? "1x1逐像素"
                           : (ctl->cd_rects_step == PtHook::CDSTEP_BLOCK2) ? "2x2聚合"
                                                                          : "未画";
    char buf[240];
    std::snprintf(buf, sizeof(buf), "字模: 单字 %dx%d %s | 矩形 %d/%d (单卡 %d) | 耗时 矩形 %d us",
                  static_cast<int>(ctl->cd_probe_digit_w), static_cast<int>(ctl->cd_probe_digit_h),
                  step_txt, static_cast<int>(ctl->cd_rects_last),
                  static_cast<int>(ctl->cd_rect_budget), static_cast<int>(ctl->cd_rects_card),
                  static_cast<int>(ctl->cd_rect_us));
    return std::string(buf);
}

// 定义在文件后面 (要用 SeedType / ZombieType 常量)
static void PlantAttackX(int seed_type, float px, float &lo, float &hi);
static void ZombieBodyX(int type, float pos_x, float &lo, float &hi);
int PlantWindupRowDevOf(int seed_type);

// ★★ v40: 下面这两张数值表**一个字节都没动** (虽然它们当年是为"碰撞箱"抽出来的) ★★
//   现状: 移速分类 (`ZombieSpeedClassOf`)、出怪、魅惑、冻结、首发延迟的攻击窗口判定
//   (`ZombieBodyX` / `PlantAttackX`) 全都在用它们 ⇒ **必须保留**。
// ZombiePhase 的数值 (game-src/ConstEnums.h)
enum : int
{
    PHASE_ZOMBIE_DYING = 1,
    PHASE_ZOMBIE_BURNED = 2,
    PHASE_ZOMBIE_MOWERED = 3,
    PHASE_POLEVAULTER_PRE_VAULT = 11,
    PHASE_POLEVAULTER_IN_VAULT = 12,
    PHASE_BOBSLED_SLIDING = 17,
    PHASE_BOBSLED_BOARDING = 18,
    PHASE_BOBSLED_CRASHING = 19,
    PHASE_POGO_BOUNCING = 20,
    PHASE_POGO_HIGH_BOUNCE_1 = 21,
    PHASE_POGO_FORWARD_BOUNCE_7 = 28,
    PHASE_NEWSPAPER_MAD = 31,
    PHASE_DIGGER_TUNNELING = 32,
    PHASE_DIGGER_RISING = 33,    // ★ 反汇编取 (0x21)
    PHASE_DIGGER_STUNNED = 36,   // ★ 反汇编取 (0x24)
    PHASE_DIGGER_WALKING = 37,
    PHASE_DOLPHIN_WALKING = 51,
    PHASE_DOLPHIN_INTO_POOL = 52, // ★ 曾经漏掉: 落入默认分支 = 被"普僵"那行控制
    PHASE_DOLPHIN_RIDING = 53,    // ★ 曾经漏掉: 同上 (这就是"骑行"状态)
    PHASE_DOLPHIN_IN_JUMP = 54,
    PHASE_DOLPHIN_WALKING_IN_POOL = 55,
    PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN = 56,
    // 潜水僵尸: 57 岸上行走, 58 入水, 59 池中游泳, 60 上浮咬, 61 水中啃食, 62 下沉
    // (game-src/ConstEnums.h 的 ZombiePhase, 从 55 往下数)
    PHASE_SNORKEL_WALKING = 57,
    PHASE_SNORKEL_INTO_POOL = 58,
    PHASE_SNORKEL_WALKING_IN_POOL = 59,
    PHASE_SNORKEL_UP_TO_EAT = 60,
    PHASE_SNORKEL_EATING_IN_POOL = 61,
    PHASE_SNORKEL_DOWN_FROM_EAT = 62,
    PHASE_IMP_GETTING_THROWN = 71,
    PHASE_IMP_LANDING = 72,
    PHASE_BALLOON_FLYING = 73,
    PHASE_LADDER_CARRYING = 76,
    PHASE_YETI_RUNNING = 91,
};

// ZombieType 的数值 (game-src/ConstEnums.h) —— ★★ 2026-10-13 整表纠正: 以前**每项都少 1** ★★
//
//   ★ 症状 (2026-10-13 用户实机反馈, 当时是"碰撞箱"功能暴露出来的): 那张表把**雪橇小队**
//     当成"普僵"处理 (按 `ZombieBodyX` 的 default 分支拿 42 宽的窄框), "带物体/乘载"的
//     僵尸**整类**张冠李戴。★ v40 起碰撞箱功能已删除, 但**这张表仍然必须正确** ——
//     移速分类 / 出怪 / 魅惑 / 冻结 / 首发延迟的攻击窗口判定都在按它分流。
//   ★ 根因: 内存里读出的 `mZombieType` (+0x24) 是**真值**, 而旧表把 ZOMBIE_FLAG 写成 1
//     ⇒ 从它往下**每一项都小 1**: 旧 BOBSLED(13) 其实是冰车 ZAMBONI(13), 旧 DIGGER(17)
//     其实是海豚骑士 DOLPHIN_RIDER(17) …
//   ★ 正确值 = `ConstEnums.h:1349-1383` 的 `enum ZombieType` 从 `ZOMBIE_NORMAL = 0` 数出来:
//       NORMAL 0  FLAG 1  TRAFFIC_CONE 2  POLEVAULTER 3  PAIL 4  NEWSPAPER 5  DOOR 6
//       FOOTBALL 7  DANCER 8  BACKUP_DANCER 9  DUCKY_TUBE 10  SNORKEL 11  **ZAMBONI 12**
//       BOBSLED 13  DOLPHIN_RIDER 14  JACK_IN_THE_BOX 15  BALLOON 16  DIGGER 17  POGO 18
//       YETI 19  BUNGEE 20  LADDER 21  CATAPULT 22  GARGANTUAR 23  IMP 24  BOSS 25
//       PEA_HEAD 26  WALLNUT_HEAD 27  JALAPENO_HEAD 28  GATLING_HEAD 29  SQUASH_HEAD 30
//       TALLNUT_HEAD 31  REDEYE_GARGANTUAR 32  ZOMBATAR 33
//     （逐个核对过; 这里只列本文件用得到的那些）
enum : int
{
    ZOMBIE_FLAG = 2,
    ZOMBIE_POLEVAULTER = 4,
    ZOMBIE_NEWSPAPER = 6,
    ZOMBIE_FOOTBALL = 8,
    ZOMBIE_DANCER = 9, // ★ 顺带修掉两处写死成 8 的旧代码 (下面 "== 8" 已改成这个常量)
    ZOMBIE_BACKUP_DANCER = 10,
    ZOMBIE_SNORKEL = 12,
    ZOMBIE_ZAMBONI = 13,
    ZOMBIE_BOBSLED = 14, // ★★ 本轮的 bug: 旧值 13 = 冰车 ⇒ 雪橇小队落到 default 分支
    ZOMBIE_JACK_IN_THE_BOX = 16,
    ZOMBIE_BALLOON = 17,
    ZOMBIE_DIGGER = 18,
    ZOMBIE_POGO = 19,
    ZOMBIE_YETI = 20,
    ZOMBIE_BUNGEE = 21,
    ZOMBIE_LADDER = 22,
    ZOMBIE_CATAPULT = 23,
    ZOMBIE_GARGANTUAR = 24,
    ZOMBIE_IMP = 25,
    ZOMBIE_BOSS = 26,
    ZOMBIE_SQUASH_HEAD = 31,
    ZOMBIE_GIGA_GARGANTUAR = 33, // 红眼巨人
};

// ---------------------------------------------------------------------------
// 事件日志: DLL 只在状态跳变时产生条目, 工具每帧取走并写文本文件。
//   为什么放工具侧落盘: 游戏进程里尽量不做文件 IO (免得卡帧)。
//   条目内容: 相对时间 / 与上一条的间隔 / 事件 / 植物编号 / 行 / 种类 / 计数器 / 目标 / 排位图。
// ---------------------------------------------------------------------------
void PvZ::SetHookLog(bool on)
{
    if (this->hook_ctl == nullptr)
        return;
    this->hook_ctl->log_read = this->hook_ctl->log_write; // 丢弃积压 (开关切换时)
    this->hook_ctl->log_enable = on ? 1 : 0;

    if (on && this->hook_log_file == nullptr)
    {
        char exe_path[MAX_PATH] = {0};
        if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) != 0)
        {
            std::string dir(exe_path);
            const size_t pos = dir.find_last_of("\\/");
            dir = (pos == std::string::npos) ? std::string() : dir.substr(0, pos + 1);
            this->hook_log_path_a = dir + "pztk_hook_log.txt";
        }
        if (!this->hook_log_path_a.empty())
        {
            this->hook_log_file = std::fopen(this->hook_log_path_a.c_str(), "w"); // 每次开日志都重新写
            if (this->hook_log_file != nullptr)
            {
                SYSTEMTIME st;
                GetLocalTime(&st);
                char head[256];
                std::snprintf(head, sizeof(head),
                              "# PvZ Toolkit 帧级钩子事件日志  %04d-%02d-%02d %02d:%02d:%02d  DLL构建=%u\n"
                              "# 事件: 1=僵尸进排 2=放手 3=钉住 (只在状态跳变时记)\n"
                              "# 列: 时间s  间隔s  事件  植物编号  行  种类  计数器  目标  排位图  游戏帧号\n"
                              "# 提示: 帧号相同 = 同一游戏帧。同排植物若在同一帧出现「进入射程」= 对齐正常。\n",
                              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                              static_cast<unsigned>(PtHook::kDllBuild));
                std::fputs(head, this->hook_log_file);
                std::fflush(this->hook_log_file);
            }
        }
    }
    else if (!on && this->hook_log_file != nullptr)
    {
        std::fclose(this->hook_log_file);
        this->hook_log_file = nullptr;
    }
}

void PvZ::DrainHookLog()
{
    if (this->hook_ctl == nullptr || this->hook_log_file == nullptr)
        return;

    if (this->hook_log_qpc_freq == 0)
    {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        this->hook_log_qpc_freq = (f.QuadPart > 0) ? static_cast<uint64_t>(f.QuadPart) : 1;
    }

    static const char *kEventName[] = {"?", "进入射程", "放手", "钉住", "★钉住在僵尸排"};

    const uint32_t write = this->hook_ctl->log_write;
    uint32_t read = this->hook_ctl->log_read;
    if (write - read > static_cast<uint32_t>(PtHook::kLogRingSize))
        read = write - static_cast<uint32_t>(PtHook::kLogRingSize); // 溢出就丢掉最旧的

    for (; read != write; read++)
    {
        const PtHook::LogEntry &e = this->hook_ctl->log_ring[read % PtHook::kLogRingSize];
        if (this->hook_log_first_tick == 0)
        {
            this->hook_log_first_tick = e.tick;
            this->hook_log_prev_tick = e.tick;
        }
        const double t = static_cast<double>(e.tick - this->hook_log_first_tick) * 1000.0 /
                         static_cast<double>(this->hook_log_qpc_freq);
        const double dt = static_cast<double>(e.tick - this->hook_log_prev_tick) * 1000.0 /
                          static_cast<double>(this->hook_log_qpc_freq);
        this->hook_log_prev_tick = e.tick;

        char line[288];
        std::snprintf(line, sizeof(line),
                      "%8.3f  %7.3f  %-12s  植物#%-3d  行%d  种类%-3d  计数=%-4d  目标=%-4d  排=0x%02X  帧=%u  "
                      "%s  列=%d px=%d 窗口=[%d,%d] 本排x=[%d,%d]\n",
                      t, dt, (e.event < 5) ? kEventName[e.event] : "?", e.slot, e.row, e.seed, e.counter,
                      e.target, e.mask, e.frame, e.win_ok ? "窗" : "排", e.col, e.px, e.win_lo, e.win_hi,
                      e.x_lo, e.x_hi);
        std::fputs(line, this->hook_log_file);
    }
    this->hook_ctl->log_read = read;
    std::fflush(this->hook_log_file); // 让用户随时能打开看 (事件很少, 开销可忽略)
}

void PvZ::SetHookPin(bool on, int seed, int value)
{
    if (this->hook_ctl == nullptr)
        return;
    // seed = -1 = 全部射手植物; 0..47 = 具体种类
    if (seed < -1)
        seed = -1;
    if (seed > 47)
        seed = 47;
    if (value < 0)
        value = 0;

    this->hook_ctl->pin_seed = seed;
    this->hook_ctl->pin_value = value;
    this->hook_ctl->pin_enable = on ? 1 : 0;
    this->hook_ctl->pin_mode = on ? 1 : 0;
    if (on)
    {
        this->hook_ctl->jump_count = 0;
        this->hook_ctl->min_counter = -1; // -1 = 还没采到样本 (旧版用 0 当初值, 指标永远显示 0)
        this->hook_ctl->prev_counter = -1;
        this->hook_ctl->pinned_count = 0;
    }
}

std::string PvZ::HookStatus()
{
    if (!GameOn())
        return "帧级钩子: 未连接游戏";
    if (this->hook_ctl == nullptr || !this->hook_injected)
    {
        // 把"到底差在哪"直接说清楚: DLL 文件在不在、路径是什么
        const std::string path = this->LastHookDllPathA();
        const DWORD attr = path.empty() ? 0xFFFFFFFF : GetFileAttributesA(path.c_str());
        if (path.empty())
            return "帧级钩子: 未注入 (还没试过)";
        if (attr == 0xFFFFFFFF)
            return "帧级钩子: 未注入 —— 找不到 DLL: " + path;
        return "帧级钩子: 未注入 (DLL 存在但加载/挂钩失败): " + path;
    }

    static const char *kFailText[] = {"无", "打不开共享内存", "控制块不匹配", "MinHook 初始化失败",
                                      "没有钩子地址", "建钩子失败", "启用钩子失败"};
    const uint32_t fail = this->hook_ctl->hook_fail;
    const uint32_t dll_build = this->hook_ctl->dll_build;

    char buf[820];
    std::snprintf(buf, sizeof(buf),
                  "钩子: 已注入 DLL版本=%u(应 %u)%s ok=%u fail=%u(%s) 步骤=%u | 调用 %u 次 | 钉住 %u 次 | "
                  "坏指针 %u\n"
                  "  钉住时 counter=%d (目标 %d) | 最小 counter=%d | 复位 %d 次 | 有僵尸的排=0x%X | 游戏帧=%u | 最近 seed=%d\n"
                  "  窗口判定: %s 列=%d px=%d 窗口=[%d,%d] 本排x=[%d,%d] | 扫描=%s 活僵尸=%u 几何坏=%u 排内被钉=%u",
                  dll_build, PtHook::kDllBuild,
                  dll_build == PtHook::kDllBuild ? "" : " ★版本不符(注入的是旧DLL!)",
                  this->hook_ctl->hook_ok, fail, fail < 7 ? kFailText[fail] : "?",
                  this->hook_ctl->init_step, this->hook_ctl->hook_calls, this->hook_ctl->pinned_count,
                  this->hook_ctl->bad_self, this->hook_ctl->pinned_last, this->hook_ctl->pin_target,
                  this->hook_ctl->min_counter, this->hook_ctl->jump_count,
                  this->hook_ctl->zombie_rows, this->hook_ctl->frame_id, this->hook_ctl->last_seed,
                  this->hook_ctl->win_probe_ok ? "窗口" : "仅本排",
                  this->hook_ctl->win_probe_col, this->hook_ctl->win_probe_px,
                  this->hook_ctl->win_probe_lo, this->hook_ctl->win_probe_hi,
                  this->hook_ctl->win_probe_xlo, this->hook_ctl->win_probe_xhi,
                  (this->hook_ctl->z_scan == 0) ? "ok" : (this->hook_ctl->z_scan == 1 ? "几何非法" : "不可读"),
                  this->hook_ctl->z_live, this->hook_ctl->z_geom_bad, this->hook_ctl->pinned_enemy);
    return buf;
}

bool PvZ::PatchBytes(uintptr_t addr, const std::vector<unsigned char> &bytes)
{
    if (!GameOn() || addr == 0 || bytes.empty())
        return false;

    // 先在记录里找: 同一地址重复打补丁时只更新 patched, 不能把"已打补丁的内容"当原始字节备份
    for (CodePatchRecord &rec : this->code_patches)
    {
        if (rec.addr == addr && rec.original.size() == bytes.size())
        {
            rec.patched = bytes;
            return WriteBytes(bytes.data(), addr, bytes.size());
        }
    }

    std::vector<unsigned char> original(bytes.size());
    if (!ReadBytes(original.data(), addr, bytes.size()))
        return false;

    CodePatchRecord rec;
    rec.addr = addr;
    rec.original = original;
    rec.patched = bytes;
    this->code_patches.push_back(rec);

    return WriteBytes(bytes.data(), addr, bytes.size());
}

bool PvZ::RestorePatch(uintptr_t addr)
{
    for (size_t i = 0; i < this->code_patches.size(); i++)
    {
        if (this->code_patches[i].addr != addr)
            continue;
        const CodePatchRecord rec = this->code_patches[i];
        this->code_patches.erase(this->code_patches.begin() + i);
        return WriteBytes(rec.original.data(), rec.addr, rec.original.size());
    }
    return false;
}

void PvZ::RestoreAllPatches()
{
    for (const CodePatchRecord &rec : this->code_patches)
        WriteBytes(rec.original.data(), rec.addr, rec.original.size());
    this->code_patches.clear();
}

int PvZ::PatchCount()
{
    return static_cast<int>(this->code_patches.size());
}

bool PvZ::HasPatchAt(uintptr_t addr)
{
    for (const CodePatchRecord &rec : this->code_patches)
        if (rec.addr == addr)
            return true;
    return false;
}

// 诊断行: 女仆秘籍 / 智慧树指令都靠"LawnApp 字段 = game_mode + 固定差值"推偏移,
// 如果不生效就要先看这几个读数是否正常 (时钟应该在稳定递增, 标志位应该是 0/1)。
std::string PvZ::DebugLine(){
    if (!GameOn())
        return "未连接游戏";

    // ★ 路径形式: data().lawn 是 gLawnApp 指针变量的地址, 要先解引用才能拿到 LawnApp 基址
    const std::initializer_list<uintptr_t> app_path = {data().lawn,
                                                       data().game_mode + LAWNAPP_APPCOUNTER_DELTA};
    const int counter = ReadMemory<int>(app_path);
    const uintptr_t lawn_app = ReadMemory<uintptr_t>({data().lawn}); // 诊断用: 真实的 LawnApp 基址
    int frame = -1;
    if (counter >= 0)
        frame = (counter % (20 * 23)) / 20;

    char flags[8] = {0};
    for (int i = 0; i < 7; i++)
        flags[i] = WisdomCodeEnabled(i) ? '1' : '0';

    // 数一下场上的僵尸与舞王
    int zombies = 0, dancers = 0;
    uint32_t count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().zombie_count_max});
    const uintptr_t zbase = ReadMemory<uintptr_t>({data().lawn, data().board, data().zombie});
    if (zbase != 0 && count_max != 0)
    {
        if (count_max > 4096)
            count_max = 4096;
        unsigned int struct_size = isGOTY() ? 0x168 : 0x15c;
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
            struct_size = 0x160;
#endif
        for (uint32_t i = 0; i < count_max; i++)
        {
            const uintptr_t z = zbase + struct_size * i;
            if (ReadMemory<unsigned char>({z + data().zombie_dead}) != 0)
                continue;
            zombies++;
            // ★ 2026-10-13: 原来是写死的 8 (ZOMBIE_DANCER 真值 = 9) —— 诊断里的"舞王 N"恒为 0。
            if (ReadMemory<int>({z + ZOMBIE_TYPE_OFFSET}) == ZOMBIE_DANCER_FWD)
                dancers++;
        }
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "诊断: 时钟=%d 相位=%d 标志位=%s 僵尸=%d(舞王%d) 版本号=%d LawnApp=%08X",
                  counter, frame, flags, zombies, dancers, this->find_result,
                  static_cast<unsigned int>(lawn_app));
    return buf;
}

void PvZ::LockButter(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().lock_butter, on);
}

void PvZ::NoCrater(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().no_crater, on);
}

void PvZ::NoIceTrail(bool on)
{
    if (!GameOn())
        return;

    this->hack_no_ice_trail = on;

    if (on)
    {
        // ★ 真正的"无痕" = **让游戏自己根本不产生冰痕**, 而不是把冰藏起来:
        //   冰痕由 Board::mIceTimer[] / mIceMinX[] 驱动, 而 `mIceTimer[] = 3000` 只在
        //   Zombie::UpdateZamboni 里写一次:
        //       if (anIceX < 800) { mBoard->mIceTimer[mRow] = 3000; ... }
        //   冰痕还会**妨碍种植** (Board.cpp:2825: `mIceTimer == 0 || mIceMinX > 750` 才可种),
        //   所以只把计时清零是不够的 —— 必须让那句赋值不生效。
        //
        //   以前的做法是把整个 UpdateZamboni 入口字节改成 ret (废掉函数) ✗ —— 那会连带杀掉
        //   速度插值 (初始工具的 bug)。现在**只把那条 mov 的立即数 3000 改成 0** ✓:
        //   游戏自己每帧写 0 -> 不产生冰痕, 也不阻挡种植, 而速度插值完全不受影响 ✓
        //   地址不写死: 用版本表里 UpdateZamboni 的入口地址 + 指令特征定位 (版本无关) ✓
        const uintptr_t fn = data().no_ice_trail.empty() ? 0 : data().no_ice_trail[0].mem_addr;
        uintptr_t site = 0;
        size_t body_len = 0;  // 函数体长度 (到第一个 ret)
        int n3000 = 0;        // 函数体内 "3000" 立即数出现次数 (诊断用)
        if (fn != 0)
        {
            unsigned char code[0x200] = {0};
            if (ReadBytes(code, fn, sizeof(code)))
            {
                // ★ 只在本函数体内找: 遇到第一个 ret(0xC3) 就停。
                //   UpdateZamboni 很短, 若扫满 0x200 字节会越进后面的函数 (UpdateLadder 等),
                //   那里也可能有 "存 3000" 的指令 -> 误打补丁 (实测会引出别的异常)。
                size_t limit = sizeof(code);
                for (size_t i = 0; i < sizeof(code); i++)
                {
                    if (code[i] == 0xC3) // ret
                    {
                        limit = i;
                        break;
                    }
                }
                body_len = limit;
                for (size_t q = 0; q + 4 <= limit; q++)
                {
                    if (code[q] == 0xB8 && code[q + 1] == 0x0B && code[q + 2] == 0x00 &&
                        code[q + 3] == 0x00)
                        n3000++;
                }

                // ★ 定位方式 (两套, 取先命中的那个):
                //   那条存指令的形式是  mov dword ptr [board + ice_trail_cd + row*4], 3000
                //   策略 1 (最精确): 代码里出现 **ice_trail_cd 这个位移值**(4 字节小端) 紧跟 3000 立即数
                //                    —— 与寻址形式 (SIB/disp8/disp32) 无关 ✓
                //   策略 2 (兜底): 本函数体内**唯一**的 "C7 [内存], 3000" 存储 ——
                //                  只放宽寻址形式 (接受 SIB), **不放宽作用域** (仍限制在第一个 ret 前),
                //                  所以不会再像之前那样误打邻居函数 ✓
                //   (之前一直没命中的原因: 形状校验要求 p == p0+2+disp_len, 没有算 SIB 那个字节 ✗)
                const uintptr_t ice_off = data().ice_trail_cd; // 先落地到局部变量 (data() 返回副本)
                const unsigned char *off_bytes = reinterpret_cast<const unsigned char *>(&ice_off);
                for (size_t p = 0; p + 8 <= limit; p++)
                {
                    if (code[p] != off_bytes[0] || code[p + 1] != off_bytes[1] ||
                        code[p + 2] != off_bytes[2] || code[p + 3] != off_bytes[3])
                        continue;
                    if (code[p + 4] != 0xB8 || code[p + 5] != 0x0B || code[p + 6] != 0x00 ||
                        code[p + 7] != 0x00)
                        continue;
                    // 再确认前面确实是一条 C7 开头的内存存储 (排除巧合)
                    if (p < 2 || code[p - 2] != 0xC7 || (code[p - 1] >> 6) == 3)
                        continue;
                    site = fn + p + 4; // 3000 这个立即数的地址
                    break;
                }
                if (site == 0)
                {
                    for (size_t p = 4; p + 4 <= limit; p++)
                    {
                        if (code[p] != 0xB8 || code[p + 1] != 0x0B || code[p + 2] != 0x00 ||
                            code[p + 3] != 0x00)
                            continue;
                        bool store = false;
                        for (size_t back = 2; back <= 8 && back <= p; back++)
                        {
                            if (code[p - back] == 0xC7 && (code[p - back + 1] >> 6) != 3)
                            {
                                store = true;
                                break;
                            }
                        }
                        if (store)
                        {
                            site = fn + p;
                            break;
                        }
                    }
                }
            }
        }

        // ★ 清场写法很讲究: 这里写 **1** (只写这一次), 而不是每帧写。
        //   - 必须让它经过一次 "1 -> 0" 的转换: Board::UpdateIce 只有在计时减到 0 时才
        //     ParticleSystemDie() 杀掉冰闪光 (用户实测: 直接写 0 会跳过整段 -> 闪光永久残留);
        //   - 但不能长期保持 1: 种植预览/种植判定看的是 `mIceTimer == 0` (Board.cpp:2825),
        //     计时不为 0 时**没有种植预览**(用户反馈) -> 所以**稳态必须是 0**;
        //   - 稳态由补丁保证 (把游戏那句 3000 改成 0, 见下), 这里只是"开的那一下"清掉已有冰痕。
        const int ui = GameUI();
        if (ui == 2 || ui == 3)
        {
            for (int r = 0; r < 6; r++)
                WriteMemory<int>(1, {data().lawn, data().board, data().ice_trail_cd + r * 4});
        }

        if (site != 0 && this->PatchBytes(site, std::vector<unsigned char>{0, 0, 0, 0}))
        {
            this->ice_timer_store_addr = site;
            this->ice_timer_max_seen = 0;
            char tmp[192];
            std::snprintf(tmp, sizeof(tmp),
                          "真无痕: 已生效 (可正常种植)", static_cast<unsigned>(site));
            this->ice_trail_msg = tmp;
            return;
        }

        // 定位/打补丁失败 -> 退回旧补丁: 确实不产生冰痕, 但冰车速度会受影响 (界面会提示)
        enable_hack(data().no_ice_trail, true);
        this->ice_timer_store_addr = 0;
        {
            char tmp[240];
            std::snprintf(tmp, sizeof(tmp),
                          "补丁点未命中(仍在写 3000): 函数 %08X 体内 %u 字节, 见 3000 共 %d 处; "
                          "已退回旧补丁(冰车速度会受影响)",
                          static_cast<unsigned>(fn), static_cast<unsigned>(body_len), n3000);
            this->ice_trail_msg = tmp;
        }
    }
    else
    {
        if (this->ice_timer_store_addr != 0)
        {
            this->RestorePatch(this->ice_timer_store_addr);
            this->ice_timer_store_addr = 0;
        }
        enable_hack(data().no_ice_trail, false);
        this->ice_trail_msg = "已关闭";
    }
}

// ===========================================================================
// 多倍出怪 (出怪点数 ×N) —— 2026-10-09
//
// 【为什么要这么做】点数(mZombiePoints)是 ZombiePicker 的成员, 而 ZombiePicker 是
//   Board::PickZombieWaves 里的**栈上局部变量**, 游戏从不把它写回 Board ——
//   所以"写内存改点数"这条路根本不存在, 只能改游戏自己的代码。
//
// 【改哪里】Board::PickZombieWaves 的"补满循环"入口(EN 1.0.0.1051 实测反汇编):
//     0040a075  89 4C 24 24        mov  [esp+0x24],ecx   ; picker->mZombiePoints = 本波预算
//     0040a079  85 C9              test ecx,ecx
//     0040a07b  7E 76              jle  0040a0f3         ; 预算<=0 -> 不补
//     0040a07d  8B 5C 24 0C        mov  ebx,[esp+0xc]    ; ★ 补丁点(这 6 字节)
//     0040a081  EB 04              jmp  0040a087
//     0040a083  8B 74 24 20        mov  esi,[esp+0x20]   ; ← 回边(0040a0f1)的目标
//     0040a087  83 FE 32           cmp  esi,0x32         ; 剩余只数 < 50 (MAX_ZOMBIES_IN_WAVE)
//     0040a094  E8 ...             call PickZombieType(points=ecx, wave=ebx, picker=&[esp+0x20])
//     0040a0d3  29 4C 24 24        sub  [esp+0x24],ecx   ; 预算 -= gZombieDefs[type].mZombieValue
//     0040a0f1  7F 90              jg   0040a083         ; 回边
//   ★ 0040a07d 这一处**每波恰好执行一次**: 首次进循环由 0040a079 顺序落下来必经它,
//     而回边 0040a0f1 跳的是 0040a083, 绕过它。所以在这里把预算乘 N 就是"点数 ×N",
//     之后挑哪种僵尸、权重表、首现波限制、无尽权重曲线、50 只上限**全由游戏自己跑**。
//   ★ 该 6 字节里没有任何"被跳进来的中间位置"(我核对过: 附近只有 0040a079 / 0040a083 /
//     0040a087 是跳转目标, 后两个都在改写范围之外), 所以改写是安全的。
//
// 【怎么改】把那 6 字节换成 `jmp 代码洞` + nop; 代码洞放在游戏进程里新分配的
//   可执行页(VirtualAllocEx), 内容 18 字节:
//       mov  ebx,[esp+0xc]        ; 复原被改掉的指令
//       imul ecx,[esp+0x24],N     ; ecx = 预算 * N      ← N 是立即数, 改倍率 = 改这 1 字节
//       mov  [esp+0x24],ecx       ; 回写(回边每轮都从 [esp+0x24] 重读)
//       jmp  0040a087             ; 与原来那条 jmp 同目标
//   只动 EBX(本来就要写) 与 ECX(本来就要等于预算), **不碰 ESI / EDI / EDX**。
//
// 【安全性】① 锚点扫不到、或命中超过 1 处 -> 完全不打补丁, 界面报"未命中", 绝不乱写;
//          ② 代码洞**只分配不释放**(关闭/退出只还原那 6 字节): 万一工具被强杀,
//             游戏跳过去的仍是一块有效内存, 不会跳进已释放的页面;
//          ③ 倍率 1 (或关闭) 时只还原补丁; 换倍率只改代码洞里那 1 个字节;
//          ④ 游戏关掉又重开(pid 变)时旧地址失效 -> 自动重新定位。
// ===========================================================================

// 特征: PickZombieWaves 的"补满循环入口"。?? = 整字节通配, 4?/5?/7? = 高半字节通配
// (栈偏移与短跳相对量随版本变, 所以通配; 寄存器选择与 cmp 的 0x32 是稳定锚点)。
static const char *kWaveMulPattern =
    "89 4? 24 ?? "    // mov  [esp+disp8], ecx    ; 存本波预算
    "85 C9 "          // test ecx, ecx
    "7E ?? "          // jle  短跳                ; 预算<=0 不补
    "8B 5? 24 ?? "    // mov  ebx, [esp+disp8]    ; ★ 补丁点从这里开始
    "EB ?? "          // jmp  短跳                ; 首次进循环必经这里
    "8B 7? 24 ?? "    // mov  esi, [esp+disp8]    ; ← 回边目标
    "83 FE 32 "       // cmp  esi, 0x32           ; 50 = MAX_ZOMBIES_IN_WAVE
    "7D ?? "          // jge  短跳
    "8D 4? 24 ?? "    // lea  r32, [esp+disp8]    ; &picker
    "50 53 51 57 E8"; // push eax/ebx/ecx/edi; call PickZombieType

// 特征串里"补丁点"之前的字节数 (89 4? 24 ?? + 85 C9 + 7E ?? = 4+2+2)
static const size_t kWaveMulPatternToSite = 8;

// 代码洞里 N 那个立即数字节的偏移 (mov ebx 4 字节 + imul 的 4 字节)
static const size_t kWaveMulCaveNOffset = 8;

// 构造代码洞内容 (纯函数, 便于离线自测):
//   mov ebx,[esp+0xc]; imul ecx,[esp+0x24],n; mov [esp+0x24],ecx; jmp resume
static std::vector<unsigned char> BuildWaveMulCave(int n, uintptr_t cave, uintptr_t resume)
{
    std::vector<unsigned char> c;
    c.push_back(0x8B);
    c.push_back(0x5C);
    c.push_back(0x24);
    c.push_back(0x0C); // mov ebx,[esp+0xc]
    c.push_back(0x6B);
    c.push_back(0x4C);
    c.push_back(0x24);
    c.push_back(0x24); // imul ecx,[esp+0x24],imm8
    c.push_back(static_cast<unsigned char>(n));
    c.push_back(0x89);
    c.push_back(0x4C);
    c.push_back(0x24);
    c.push_back(0x24); // mov [esp+0x24],ecx
    c.push_back(0xE9); // jmp rel32
    const size_t rel_pos = c.size();
    for (int i = 0; i < 4; i++)
        c.push_back(0);
    const int32_t rel = static_cast<int32_t>(resume - (cave + c.size()));
    std::memcpy(&c[rel_pos], &rel, 4);
    return c;
}

// 构造打在"补满循环入口"上的 6 字节 (纯函数): jmp 代码洞 + nop
static std::vector<unsigned char> BuildWaveMulPatch(uintptr_t site, uintptr_t cave)
{
    std::vector<unsigned char> p;
    p.push_back(0xE9);
    const int32_t rel = static_cast<int32_t>(cave - (site + 5));
    for (int i = 0; i < 4; i++)
        p.push_back(static_cast<unsigned char>(static_cast<uint32_t>(rel) >> (8 * i)));
    p.push_back(0x90);
    return p;
}

// 扫出"补满循环入口"那 6 字节的地址 (0 = 没找到 / 有歧义 -> 界面如实报"未命中")
uintptr_t PvZ::FindWaveMulSite()
{
    if (!GameOn())
        return 0;

    const uintptr_t fn = data().call_pick_zombie_waves; // Board::PickZombieWaves 入口
    if (fn == 0)
        return 0;

    // 只在本函数体内找。实测 EN 1.0.0.1051 的函数体是 0x4092e0..0x40a10f (0xE2F 字节),
    // 读 0x1200 留足余量; 特征本身很长(27 字节), 不会误命中邻居函数。
    const size_t kScanSize = 0x1200;
    const std::vector<uintptr_t> hits = this->ScanPattern(kWaveMulPattern, fn, kScanSize, 4);

    // ★ 必须唯一命中: 0 处 = 本版不支持; 多处 = 有歧义, 都**不**动游戏代码
    if (hits.size() != 1)
        return 0;

    return hits[0] + kWaveMulPatternToSite;
}

bool PvZ::WaveMulSupported()
{
    if (!GameOn())
        return false;
    // 只有"已经打过补丁"时才走缓存 (靠 wave_mul_site + pid 判断) —— 那时地址已经确定。
    // 否则每次调用都要扫一遍函数体 (读 4KB 内存), 所以**别每帧调**。
    if (this->wave_mul_site != 0 && this->wave_mul_pid == this->GamePid())
        return true;
    return this->FindWaveMulSite() != 0;
}

void PvZ::SetWaveMul(bool on, int n)
{
    if (n < 1)
        n = 1;
    if (n > 24)
        n = 24;
    this->hack_wave_mul_n = n;

    if (!GameOn())
    {
        this->wave_mul_msg = "未连接游戏";
        return;
    }

    // 游戏关掉又重开 -> 上个进程里的地址全失效, 状态清空 (补丁本身也随之消失)
    if (this->wave_mul_pid != 0 && this->wave_mul_pid != this->GamePid())
    {
        this->wave_mul_site = 0;
        this->wave_mul_cave = 0;
        this->hack_wave_mul = false;
    }

    // ---- 关闭 (或倍率 1): 只还原那 6 字节, 代码洞留着不管 ----
    if (!on || n <= 1)
    {
        if (this->wave_mul_site != 0)
        {
            this->RestorePatch(this->wave_mul_site);
            this->wave_mul_site = 0;
        }
        this->hack_wave_mul = false;
        this->wave_mul_msg = "已关闭 (倍率 1x)";
        return;
    }

    // ---- 已经生效, 只是改倍率: 只改代码洞里那 1 个立即数字节 ----
    if (this->hack_wave_mul && this->wave_mul_site != 0 && this->wave_mul_cave != 0)
    {
        const unsigned char nb = static_cast<unsigned char>(n);
        if (WriteBytes(&nb, this->wave_mul_cave + kWaveMulCaveNOffset, 1))
        {
            char tmp[160];
            std::snprintf(tmp, sizeof(tmp), "已生效: 出怪点数 x%d", n,
                          static_cast<unsigned>(this->wave_mul_site),
                          static_cast<unsigned>(this->wave_mul_cave));
            this->wave_mul_msg = tmp;
        }
        else
        {
            this->wave_mul_msg = "改倍率失败: 代码洞不可写";
        }
        return;
    }

    // ---- 首次开启: 定位锚点 -> 分配代码洞 -> 打补丁 ----
    const uintptr_t site = this->FindWaveMulSite();
    if (site == 0)
    {
        this->wave_mul_msg = "未命中: 没找到出怪点数锚点 (本版暂不支持; 未改动游戏代码)";
        return;
    }

    // 代码洞: 只分配不释放 (见本节安全性②)。申请 64 字节, 只用前 18 字节。
    LPVOID cave = VirtualAllocEx(this->handle, nullptr, 64, MEM_COMMIT | MEM_RESERVE,
                                 PAGE_EXECUTE_READWRITE);
    if (cave == nullptr)
    {
        this->wave_mul_msg = "失败: 无法在游戏进程里分配可执行页 (代码洞)";
        return;
    }

    const uintptr_t cave_addr = reinterpret_cast<uintptr_t>(cave);
    const uintptr_t resume = site + 10; // 0040a087 = "jmp 短跳" 的目标 (跳过回边目标那条)
    const std::vector<unsigned char> cave_code =
        BuildWaveMulCave(n, cave_addr, resume);
    if (!WriteBytes(cave_code.data(), cave_addr, cave_code.size()))
    {
        this->wave_mul_msg = "失败: 代码洞写入失败";
        return;
    }

    const std::vector<unsigned char> patch = BuildWaveMulPatch(site, cave_addr);
    if (!this->PatchBytes(site, patch))
    {
        this->wave_mul_msg = "失败: 补丁写入失败 (未生效)";
        return;
    }

    // 自检: 回读补丁与代码洞首字节, 确认真的写进去了
    unsigned char check[8] = {0};
    bool ok = ReadBytes(check, site, 6) && check[0] == 0xE9 && check[5] == 0x90;
    if (ok)
    {
        unsigned char cn = 0;
        ok = ReadBytes(&cn, cave_addr + kWaveMulCaveNOffset, 1) && cn == static_cast<unsigned char>(n);
    }
    if (!ok)
    {
        this->RestorePatch(site);
        this->wave_mul_msg = "自检失败: 补丁回读不一致, 已还原";
        return;
    }

    this->wave_mul_site = site;
    this->wave_mul_cave = cave_addr;
    this->wave_mul_pid = this->GamePid();
    this->hack_wave_mul = true;

    char tmp[192];
    std::snprintf(tmp, sizeof(tmp),
                  "已生效: 出怪点数 x%d (每波补满循环的预算乘 %d)", n, n,
                  static_cast<unsigned>(site), static_cast<unsigned>(cave_addr));
    this->wave_mul_msg = tmp;
}

// ===========================================================================
// 高级暂停 —— 2026-10-09
//
// 【用户要的语义】开关勾上后**不是主动暂停游戏**, 而是把**游戏自带的暂停**(按空格)变成
//   "高级暂停": 不弹暂停对话框(所以鼠标不被禁)、音乐不停、**世界冻住但鼠标/选卡/种植照常**。
//
// 【游戏自带暂停的真实机制】(EN 1.0.0.1051 实测反汇编)
//   Board::KeyDown (0x41B820) 空格分支:
//       41b8c1  cmp  edi,0x20          ; KEYCODE_SPACE
//       41b8c6  call CanPauseNow       ; = GetDialogCount() <= 0
//       41b8e4  call eax               ; PlaySample(SOUND_PAUSE)
//       41b8ec  call 0x4502C0          ; ★ LawnApp::DoPauseDialog  <- 补丁点 A
//   LawnApp::DoPauseDialog (0x4502C0): `mBoard->Pause(true)` + 建 "GAME PAUSED" 对话框
//   Board::Pause(bool) (0x4127A0): 置 Board::mPaused(+0x164), 并**停声音/停音乐**
//   Board::Update (0x415D40): UpdateMousePosition()(内含 UpdateCursor()) 在暂停判断**之前**;
//       若 mPaused 就隐藏 mCursorPreview/mCursorObject 然后 return (世界冻住)
//   ★ 但 UpdateCursor 自己也有 mPaused 判断(会 SetCursor(POINTER) 直接 return), 而 PickUpTool /
//     点卡槽 / SeedPacket / CanInteractWithBoardButtons 也都判 mPaused
//     ⇒ **只要用 mPaused, 鼠标与操作就必然被禁** —— 而用户明确不要这个。
//
// 【所以本实现完全不碰 mPaused】改用一个**我们自己的标志字节**(放在代码洞页里):
//   ① 空格那条 `call DoPauseDialog` 改成 `call 代码洞1`: 代码洞1 只做 `标志 ^= 1` 然后 ret,
//      既不弹框也不碰音乐;
//   ② Board::Update 里**冻结段**的第一条 `call`(= mEffectSystem->Update, 0x415E8C) 改成
//      `jmp 代码洞2`: 标志为 1 就直接跳到函数**完整收尾**(0x416064), 冻结段被整段跳过;
//      标志为 0 就重放原来那条 call 再跳回。
//   冻结段 = mEffectSystem->Update / mAdvice->Update / UpdateTutorial / 各种计数器 /
//            UpdateLayers / UpdateGridItems / UpdateFwoosh / UpdateGame / UpdateFog /
//            mChallenge->Update / UpdateLevelEndSequence
//   ⇒ 粒子、天上掉阳光、池边闪光、死亡特效、"一大波僵尸"横幅**全都冻住**, 像真暂停;
//      而 `UpdateMousePosition()`(内含 UpdateCursor()) 与 `CanInteractWithBoardButtons()` /
//      `mMenuButton->Update()` / `mStoreButton->Update()` 都在它**之前**跑, 且 mPaused 恒为 false
//   ⇒ 鼠标、种植预览、选卡、种植、铲子、工具、游戏内菜单按钮**全都照常**; 按空格再切回来。
//   （边界选在"按钮更新之后、特效之前": 再往前会把菜单按钮的刷新也冻掉, 再往后就冻不住粒子）
//
// 【★ 为什么跳 0x416064 而不是暂停分支用的 0x415E18】(本实现最容易踩的坑)
//   0x415E18 的收尾只 `pop ebp` —— 因为它位于 `push ebx/esi/edi` **之前**。而从冻结段
//   (0x416010) 跳过去时 ebx/esi/edi 都已经 push 过 ⇒ **栈失衡 + 寄存器错乱 ⇒ 崩**。
//   0x416064 才是完整收尾: `mov ecx,[esp+0x114]`(SEH cookie) + pop edi/esi/ebx/ebp +
//   `mov fs:0,ecx` + `add esp,0x110` + ret。而且它本来就是游戏自己的一条"提前返回"目标
//   (源码 `if (mTimeStopCounter > 0) return;` 就跳这里), 所以拿它当提前返回是游戏自己的用法。
//   (实测寄存器保存位置: push ebx @0x415E2E / push esi @0x415E2F / push edi @0x415E85, 都在冻结段之前)
//
// 【安全性】① 版本不对/地址为 0/原始字节校验不过 -> **完全不动游戏代码**, 界面报未命中;
//          ② 代码洞页只分配不释放(关闭/退出只还原那 5+5 字节) -> 工具被强杀也不会跳到已释放页;
//          ③ 关闭时**先把标志清 0** 再还原补丁, 免得把游戏留在冻结状态;
//          ④ 游戏换进程(pid 变)自动重来; 装完回读自检。
// ===========================================================================

// 代码洞页布局 (64 字节): +0x00 标志字节 / +0x08 代码洞1 / +0x10 代码洞2
static const size_t kAdvPauseFlagOff = 0x00;
static const size_t kAdvPauseToggleOff = 0x08;
static const size_t kAdvPauseWorldOff = 0x10;

// 高级暂停的三处落点 (只对 EN 1.0.0.1051 从真实 exe 反汇编收录)
static const uintptr_t kAdvPauseSiteEN = 0x0041B8EC;     // 空格分支里的 call DoPauseDialog
static const uintptr_t kAdvPauseWorldEN = 0x00415E8C;    // Board::Update 冻结段第一条 call(EffectSystem::Update)
static const uintptr_t kAdvPauseEpilogueEN = 0x00416064; // Board::Update 完整收尾(提前返回目标)
// Board::UpdateGameObjects()=0x4130D0 尾部的两个"跟随鼠标"更新点(手感优化用):
//   mov edi,[ebx+disp32] ; call rel32      -> mCursorPreview->Update()  (种植预览)
//   mov esi,[ebx+disp32] ; call rel32      -> mCursorObject->Update()   (手上的卡片/加农炮落点)
static const uintptr_t kAdvPausePreviewSiteEN = 0x004131C9;
static const uintptr_t kAdvPauseObjectSiteEN = 0x004131D4;
// 加农炮"误点保护"计时器的递减点(Board::Update 里 mov eax,[ebp+0x5754]) 与
// mPrevMouseX/Y 的存储块(无分支, 整块 42 字节原样复制):
static const uintptr_t kAdvPauseCobDelaySiteEN = 0x00415EA2;
static const uintptr_t kAdvPausePrevMouseSiteEN = 0x0041603A;
static const size_t kAdvPausePrevMouseLen = 42;

// 代码洞1: `xor byte [标志], 1; ret` (8 字节) —— 纯函数, 便于离线自测
static std::vector<unsigned char> BuildAdvPauseToggleCave(uintptr_t flag_addr)
{
    std::vector<unsigned char> c;
    c.push_back(0x80);
    c.push_back(0x35); // xor byte ptr [disp32], imm8
    for (int i = 0; i < 4; i++)
        c.push_back(static_cast<unsigned char>(static_cast<uint32_t>(flag_addr) >> (8 * i)));
    c.push_back(0x01);
    c.push_back(0xC3); // ret
    return c;          // 8 字节
}

// 代码洞2 (2026-10-09 手感优化后, 50 字节):
//   标志=0 -> 重放那条 call(冻结段照常跑) 然后跳回;
//   标志=1 -> **先补调两个"跟随鼠标"的更新**, 再跳函数完整收尾(其余冻结段整段跳过)。
//
//   ★ 为什么冻结时还要补调这两个: 它们的调用点在 `Board::UpdateGameObjects()` 的**末尾**
//     (Board.cpp:5202-5203), 而那个函数正是被 `UpdateGame()` 调进去的 —— 于是整段冻结时
//     "手上拿的卡片/加农炮落点"和"种植预览"就都不跟鼠标了(用户实机反馈的手感问题)。
//     实测反汇编(Board::UpdateGameObjects = 0x4130D0 的尾部):
//       4131c9  8B BB 3C 01 00 00   mov  edi,[ebx+0x13C]   ; mCursorPreview
//       4131cf  E8 CC 5B 02 00      call 0x438DA0          ; CursorPreview::Update (this=EDI)
//       4131d4  8B B3 38 01 00 00   mov  esi,[ebx+0x138]   ; mCursorObject
//       4131da  E8 A1 55 00 00      call 0x438780          ; CursorObject::Update   (this=ESI)
//       4131df  ...                                        ; 之后才是种子包循环
//     ⇒ 这套编译器把 `this` 放在 **EDI/ESI**(不是 ECX), 所以补调必须照样用 EDI/ESI。
//     板子指针在 **EBP**(`Board::Update` 开头 `mov ebp,ecx`) —— 于是位移与函数地址都能
//     从上面那两条 `mov` 的原始字节里**现算**, 只写死这两个调用点的地址(EN)。
//   ★ **不**补调种子包 `SeedPacket::Update`: 它会走冷却计数(`mRefreshCounter++ -> Activate()`,
//     SeedPacket.cpp:150-160) —— 补调就等于"暂停时卡牌还在冷却回满", 那不是暂停该有的样子。
//     冻住它就够; 卡片的悬停高亮是绘制期按鼠标位置算的, 不受影响。
//   冻结路径用的 EDI/ESI 随后会被收尾的 `pop edi/pop esi` 覆盖, 所以可以随便改;
//   EBP 必须活着(两个被调函数都是标准函数, 会保存它)。
//
//   ★ 第二轮实机反馈又补了两段(见下 (1)(2))。**总原则: 模拟状态冻住, 输入/UI 状态继续活。**
//   (1) `mCobCannonCursorDelayCounter`(Board+0x5754) 的递减在 `0x415EA2`, 也在冻结点之后:
//         `8B 85 54 57 00 00 / 33 DB / 3B C3 / 7E 09 / 83 C0 FF / 89 85 54 57 00 00`
//       暂停时它**永远停在 30** ⇒ `MouseDownCobcannonFire` 的"误点保护"(Board.cpp:3751)永远命中
//       ⇒ **暂停时加农炮打不出去, 而且它提前 return 就跳过了 `ClearCursor()` ⇒ 瞄准模式卡死**
//       (只有"种别的植物"才会清掉) —— 正是用户反馈的现象。这里改成等价的两条无寄存器指令。
//   (2) `mPrevMouseX/Y`(Board+0x5558/0x555C) 的存储块在 `0x41603A..0x416064`: **无分支**,
//       而且用的就是 EBP=Board ⇒ 可以**整块原样搬进代码洞**(42 字节)。加农炮准心的
//       **绘制条件**用的正是它(`CursorObject.cpp:206` 用 `mPrevMouseX/Y` 做命中测试)。
//   这两段都不含相对跳转(只有 (1) 那个 `jle`, 这里是自己本地编码的相对跳转, 位置可控)。
struct AdvPauseWorldSpec
{
    uintptr_t flag_addr;
    uintptr_t cave;
    uintptr_t call_target; // 被我们改掉的那条 call 的目标(正常路径要重放它)
    uintptr_t back;        // 正常路径跳回点 = site_b + 5
    uintptr_t epilogue;    // 冻结路径跳的完整收尾
    uint32_t preview_off;  // mCursorPreview 在 Board 里的偏移
    uintptr_t preview_fn;  // CursorPreview::Update
    uint32_t object_off;   // mCursorObject 在 Board 里的偏移
    uintptr_t object_fn;   // CursorObject::Update
    uint32_t cob_delay_off; // mCobCannonCursorDelayCounter 在 Board 里的偏移
    const unsigned char *prev_mouse_bytes; // 游戏自己那段 mPrevMouse 存储(原样复制)
    size_t prev_mouse_len;
};

static std::vector<unsigned char> BuildAdvPauseWorldCave(const AdvPauseWorldSpec &s)
{
    const uintptr_t flag_addr = s.flag_addr;
    const uintptr_t cave = s.cave;
    const uintptr_t call_target = s.call_target;
    const uintptr_t back = s.back;
    const uintptr_t epilogue = s.epilogue;
    const uint32_t preview_off = s.preview_off;
    const uintptr_t preview_fn = s.preview_fn;
    const uint32_t object_off = s.object_off;
    const uintptr_t object_fn = s.object_fn;
    const uint32_t cob_delay_off = s.cob_delay_off;
    const unsigned char *prev_mouse_bytes = s.prev_mouse_bytes;
    const size_t prev_mouse_len = s.prev_mouse_len;
    std::vector<unsigned char> c;
    auto put32 = [&](uint32_t v) {
        for (int i = 0; i < 4; i++)
            c.push_back(static_cast<unsigned char>(v >> (8 * i)));
    };
    // cmp byte [flag], 0   (7 字节)
    c.push_back(0x80);
    c.push_back(0x3D);
    put32(static_cast<uint32_t>(flag_addr));
    c.push_back(0x00);
    // jne +10 -> 偏移 19 (冻结路径)      (2 字节)
    c.push_back(0x75);
    c.push_back(0x0A);
    // call <call_target>  (重放被改掉的那条 call)   (5 字节)
    c.push_back(0xE8);
    put32(static_cast<uint32_t>(call_target - (cave + c.size() + 4)));
    // jmp <back>          (5 字节)
    c.push_back(0xE9);
    put32(static_cast<uint32_t>(back - (cave + c.size() + 4)));
    // ---- 冻结路径 ----
    // 原则: **模拟状态该冻住, 输入/UI 状态该继续活**。下面这几段都是"输入/UI", 所以照走:
    //   (1) 加农炮"误点保护"计时器 -- 不递减的话, 暂停时加农炮永远打不出去, 瞄准模式也退不出来
    //       (MouseDownCobcannonFire 的早退会跳过 ClearCursor())。
    //   (2) mPrevMouseX/Y -- 加农炮准心的**绘制条件**用的就是它(CursorObject.cpp:206)。
    //   (3)(4) 光标物 / 种植预览的 Update -- 让卡片、准心、幽灵植物跟随鼠标。
    // (1) cmp dword [ebp+cob_delay_off],0 ; jle +7 ; sub dword [ebp+cob_delay_off],1
    c.push_back(0x83);
    c.push_back(0xBD);
    put32(cob_delay_off);
    c.push_back(0x00);
    c.push_back(0x7E);
    c.push_back(0x07);
    c.push_back(0x83);
    c.push_back(0xAD);
    put32(cob_delay_off);
    c.push_back(0x01);
    // (2) 原样复制游戏自己那段 mPrevMouse 存储 (无分支, 且用的就是 EBP=Board, 可整块搬)
    for (size_t i = 0; i < prev_mouse_len; i++)
        c.push_back(prev_mouse_bytes[i]);
    // (3) mov edi,[ebp+preview_off] ; mov eax,preview_fn ; call eax
    c.push_back(0x8B);
    c.push_back(0xBD);
    put32(preview_off);
    c.push_back(0xB8);
    put32(static_cast<uint32_t>(preview_fn));
    c.push_back(0xFF);
    c.push_back(0xD0);
    // (4) mov esi,[ebp+object_off] ; mov eax,object_fn ; call eax
    c.push_back(0x8B);
    c.push_back(0xB5);
    put32(object_off);
    c.push_back(0xB8);
    put32(static_cast<uint32_t>(object_fn));
    c.push_back(0xFF);
    c.push_back(0xD0);
    // jmp <epilogue>   (5 字节)
    c.push_back(0xE9);
    put32(static_cast<uint32_t>(epilogue - (cave + c.size() + 4)));
    return c;
}

// 打在某条指令上的 5 字节补丁 (纯函数): 与原来的指令等长, 不动指令边界。
//   ★★ 是"call"还是"jmp"必须由调用方说清楚 —— 这是本功能踩过的**真实崩溃点**:
//      A 处原本是一条 `call DoPauseDialog`; 若把它换成 `jmp 代码洞1`, 就没有压入返回地址,
//      而代码洞1 结尾的 `ret` 会去弹栈上的垃圾 -> 跳到栈数据上执行 -> Access Violation。
//      (实测崩在 EIP=栈区地址, 寄存器正好是 Board::KeyDown 空格分支的现场:
//       EDI=0x20(VK_SPACE)、ESI=Board、ECX=*(Board+0x8C)=LawnApp。)
//      所以: 被替换的是 call 且代码洞会 `ret` 回来 -> 必须 call(0xE8);
//            被替换的是普通指令、或代码洞自己负责跳回 -> 用 jmp(0xE9)。
static std::vector<unsigned char> BuildAdvPausePatch(uintptr_t site, uintptr_t target, bool as_call)
{
    std::vector<unsigned char> p;
    p.push_back(as_call ? 0xE8 : 0xE9);
    const int32_t rel = static_cast<int32_t>(target - (site + 5));
    for (int i = 0; i < 4; i++)
        p.push_back(static_cast<unsigned char>(static_cast<uint32_t>(rel) >> (8 * i)));
    return p;
}

bool PvZ::AdvPauseActive()
{
    if (this->adv_pause_page == 0)
        return false;
    unsigned char v = 0;
    if (!ReadBytes(&v, this->adv_pause_page + kAdvPauseFlagOff, 1))
        return false;
    return v != 0;
}

void PvZ::SetAdvPause(bool on)
{
    if (!GameOn())
    {
        this->adv_pause_msg = "未连接游戏";
        return;
    }

    // 游戏关掉又重开 -> 代码洞与补丁都随进程消失, 状态清空
    if (this->adv_pause_pid != 0 && this->adv_pause_pid != this->GamePid())
    {
        this->adv_pause_page = 0;
        this->hack_adv_pause = false;
        this->adv_pause_call_site = 0;
        this->adv_pause_world_site = 0;
    }

    // ---- 关闭: 先把标志清 0, 再还原补丁 (免得把游戏留在冻结状态) ----
    if (!on)
    {
        if (this->adv_pause_page != 0)
        {
            const unsigned char zero = 0;
            WriteBytes(&zero, this->adv_pause_page + kAdvPauseFlagOff, 1);
        }
        if (this->hack_adv_pause)
        {
            this->RestorePatch(this->adv_pause_world_site);
            this->RestorePatch(this->adv_pause_call_site);
            this->hack_adv_pause = false;
        }
        this->adv_pause_msg = "已关闭";
        return;
    }

    if (this->hack_adv_pause)
    {
        // ★ 自愈: 主循环在"游戏断开一帧"时会 RestoreAllPatches()(imgui_main.cpp),
        //   万一那只是**瞬间断连**(窗口重建/全屏切换等)而进程还活着, 补丁就被还原了,
        //   但我们这边的 hack_adv_pause 还是 true —— 那样"取消勾选再勾上"也救不回来
        //   (会早退报"已生效"却不重装)。所以这里回读确认一次: 不是我们的字节就当作没装。
        unsigned char cka = 0;
        unsigned char ckb = 0;
        if (ReadBytes(&cka, this->adv_pause_call_site, 1) && cka == 0xE9 &&
            ReadBytes(&ckb, this->adv_pause_world_site, 1) && ckb == 0xE9)
        {
            this->adv_pause_msg = "已生效: 按空格切换暂停/继续";
            return;
        }
        this->hack_adv_pause = false; // 已被还原 -> 往下走重新安装
    }

    if (this->find_result != PVZ_1_0_0_1051_EN)
    {
        this->adv_pause_msg = "未命中: 高级暂停目前只收录 EN 1.0.0.1051 (未改动游戏代码)";
        return;
    }

    const uintptr_t site_a = kAdvPauseSiteEN;
    const uintptr_t site_b = kAdvPauseWorldEN;
    const uintptr_t epi = kAdvPauseEpilogueEN;

    // ---- ★ 强校验原始字节: 换版本 / 被别的补丁改过就绝不动手 ----
    unsigned char ob_a[5] = {0};
    unsigned char ob_b[5] = {0};
    unsigned char ob_epi[11] = {0};
    if (!ReadBytes(ob_a, site_a, 5) || ob_a[0] != 0xE8)
    {
        this->adv_pause_msg = "高级暂停: 校验失败(未改动游戏代码)";
        return;
    }
    if (!ReadBytes(ob_b, site_b, 5) || ob_b[0] != 0xE8)
    {
        this->adv_pause_msg = "高级暂停: 冻结段校验失败(未改动游戏代码)";
        return;
    }
    // 收尾签名: 8B 8C 24 ?? ?? ?? ?? 5F 5E 5B 5D 64 89 0D
    if (!ReadBytes(ob_epi, epi, 11) || ob_epi[0] != 0x8B || ob_epi[1] != 0x8C || ob_epi[2] != 0x24 ||
        ob_epi[7] != 0x5F || ob_epi[8] != 0x5E || ob_epi[9] != 0x5B || ob_epi[10] != 0x5D)
    {
        this->adv_pause_msg = "校验失败: 收尾地址签名不符 (未改动游戏代码)";
        return;
    }

    // 冻结段那条 call 的目标 (= 我们要"重放"的函数), 直接从原始字节里现算 -> 不写死
    const uint32_t b_rel = static_cast<uint32_t>(ob_b[1]) | (static_cast<uint32_t>(ob_b[2]) << 8) |
                          (static_cast<uint32_t>(ob_b[3]) << 16) | (static_cast<uint32_t>(ob_b[4]) << 24);
    const uintptr_t call_target = site_b + 5 + static_cast<int32_t>(b_rel);

    // ---- 手感优化: 从 UpdateGameObjects 尾部的两条 `mov <reg>,[ebx+disp32]` 现算
    //      "跟随鼠标"那两个更新用的位移与函数地址 (校验不过就完全不动游戏代码) ----
    auto le32 = [](const unsigned char *p) {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    };
    unsigned char ob_pv[11] = {0};
    unsigned char ob_ob[11] = {0};
    if (!ReadBytes(ob_pv, kAdvPausePreviewSiteEN, 11) || ob_pv[0] != 0x8B || ob_pv[1] != 0xBB ||
        ob_pv[6] != 0xE8 || //
        !ReadBytes(ob_ob, kAdvPauseObjectSiteEN, 11) || ob_ob[0] != 0x8B || ob_ob[1] != 0xB3 ||
        ob_ob[6] != 0xE8)
    {
        this->adv_pause_msg = "校验失败: 光标更新调用点字节不符 (未改动游戏代码)";
        return;
    }
    const uint32_t preview_off = le32(&ob_pv[2]); // mCursorPreview 在 Board 里的偏移
    const uint32_t object_off = le32(&ob_ob[2]);  // mCursorObject  在 Board 里的偏移
    const uintptr_t preview_fn =
        kAdvPausePreviewSiteEN + 11 + static_cast<int32_t>(le32(&ob_pv[7]));
    const uintptr_t object_fn = kAdvPauseObjectSiteEN + 11 + static_cast<int32_t>(le32(&ob_ob[7]));

    // ---- 加农炮"误点保护"计时器的位移 + mPrevMouse 存储块 (都从游戏原始字节里取) ----
    //   0x415EA2: 8B 85 <disp32> = mov eax,[ebp+disp32]  -> disp32 就是该字段在 Board 里的偏移
    //   0x41603A: 42 字节 mPrevMouse 存储块, 无分支 -> 整块复制
    unsigned char ob_cd[6] = {0};
    unsigned char ob_pm[kAdvPausePrevMouseLen > 42 ? kAdvPausePrevMouseLen : 42] = {0};
    if (!ReadBytes(ob_cd, kAdvPauseCobDelaySiteEN, 6) || ob_cd[0] != 0x8B || ob_cd[1] != 0x85 ||
        !ReadBytes(ob_pm, kAdvPausePrevMouseSiteEN, kAdvPausePrevMouseLen) || //
        ob_pm[0] != 0x8B || ob_pm[1] != 0x85 || ob_pm[2] != 0x8C ||      // mov eax,[ebp+0x8c] (mApp)
        ob_pm[18] != 0x89 || ob_pm[19] != 0x8D)                          // mov [ebp+..],ecx
    {
        this->adv_pause_msg = "校验失败: 加农炮/鼠标状态代码字节不符 (未改动游戏代码)";
        return;
    }
    const uint32_t cob_delay_off = le32(&ob_cd[2]);

    // ---- 分配代码洞页 (只分配不释放) ----
    //   256 字节: 偏移 0 标志 / 8 洞1(8B) / 16 洞2(108B) -> 到 123, 余量充足
    LPVOID page = VirtualAllocEx(this->handle, nullptr, 256, MEM_COMMIT | MEM_RESERVE,
                                 PAGE_EXECUTE_READWRITE);
    if (page == nullptr)
    {
        this->adv_pause_msg = "失败: 无法在游戏进程里分配代码洞页";
        return;
    }
    const uintptr_t pg = reinterpret_cast<uintptr_t>(page);
    const unsigned char zero = 0;
    WriteBytes(&zero, pg + kAdvPauseFlagOff, 1);

    const std::vector<unsigned char> cave1 = BuildAdvPauseToggleCave(pg + kAdvPauseFlagOff);
    AdvPauseWorldSpec spec;
    spec.flag_addr = pg + kAdvPauseFlagOff;
    spec.cave = pg + kAdvPauseWorldOff;
    spec.call_target = call_target;
    spec.back = site_b + 5;
    spec.epilogue = epi;
    spec.preview_off = preview_off;
    spec.preview_fn = preview_fn;
    spec.object_off = object_off;
    spec.object_fn = object_fn;
    spec.cob_delay_off = cob_delay_off;
    spec.prev_mouse_bytes = ob_pm;
    spec.prev_mouse_len = kAdvPausePrevMouseLen;
    const std::vector<unsigned char> cave2 = BuildAdvPauseWorldCave(spec);
    if (!WriteBytes(cave1.data(), pg + kAdvPauseToggleOff, cave1.size()) ||
        !WriteBytes(cave2.data(), pg + kAdvPauseWorldOff, cave2.size()))
    {
        this->adv_pause_msg = "失败: 代码洞写入失败";
        return;
    }

    // ★ A 处原来是 call -> 补丁必须还是 call(0xE8), 否则代码洞1 的 ret 会弹垃圾地址(实测崩过);
    //   B 处由代码洞2 自己负责跳回 -> 用 jmp(0xE9)。
    const std::vector<unsigned char> pa = BuildAdvPausePatch(site_a, pg + kAdvPauseToggleOff, true);
    const std::vector<unsigned char> pb = BuildAdvPausePatch(site_b, pg + kAdvPauseWorldOff, false);
    if (!this->PatchBytes(site_a, pa) || !this->PatchBytes(site_b, pb))
    {
        this->RestorePatch(site_a);
        this->RestorePatch(site_b);
        this->adv_pause_msg = "失败: 补丁写入失败 (未生效)";
        return;
    }

    // ---- 自检: 回读两处补丁首字节 (A 必须是 call 0xE8, B 是 jmp 0xE9) ----
    unsigned char chk_a[5] = {0};
    unsigned char chk_b[5] = {0};
    const bool ok = ReadBytes(chk_a, site_a, 5) && chk_a[0] == 0xE8 && //
                    ReadBytes(chk_b, site_b, 5) && chk_b[0] == 0xE9;
    if (!ok)
    {
        this->RestorePatch(site_a);
        this->RestorePatch(site_b);
        this->adv_pause_msg = "自检失败: 补丁回读不一致, 已还原";
        return;
    }

    this->adv_pause_call_site = site_a;
    this->adv_pause_world_site = site_b;
    this->adv_pause_page = pg;
    this->adv_pause_pid = this->GamePid();
    this->hack_adv_pause = true;

    char tmp[200];
    std::snprintf(tmp, sizeof(tmp),
                  "已生效: 按空格切换高级暂停 (特效+世界冻结; 鼠标/选卡/种植照常, 音乐继续, 不弹框) "
                  "",
                  static_cast<unsigned>(site_a), static_cast<unsigned>(site_b),
                  static_cast<unsigned>(pg));
    this->adv_pause_msg = tmp;
}

void PvZ::ZombieNotExplode(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().zombie_not_explode, on);
}

int PvZ::GetSlotSeed(int index)
{
    int seed_type = 0;
    int seed_type_im = 0;

    if (!GameOn())
        return -1;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return -1;

    unsigned int slot_seed_struct_size = 0x50;

    auto slot_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().slot});
    seed_type = ReadMemory<int>({slot_offset + data().slot_seed_type + index * slot_seed_struct_size});
    seed_type_im = ReadMemory<int>({slot_offset + data().slot_seed_type_im + index * slot_seed_struct_size});

    if (seed_type == 48) // 模仿者
        return seed_type_im + 48;
    else
        return seed_type;
}

void PvZ::WriteSlotSeedNow(int index, int type, bool imitater)
{
    unsigned int slot_seed_struct_size = 0x50;

    auto slot_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().slot});
    if (imitater)
    {
        WriteMemory<int>(48, {slot_offset + data().slot_seed_type + index * slot_seed_struct_size});
        WriteMemory<int>(type, {slot_offset + data().slot_seed_type_im + index * slot_seed_struct_size});
    }
    else
    {
        WriteMemory<int>(type, {slot_offset + data().slot_seed_type + index * slot_seed_struct_size});
        WriteMemory<int>(-1, {slot_offset + data().slot_seed_type_im + index * slot_seed_struct_size});
    }
}

void PvZ::RememberPendingSlotSeed(int index, int type, bool imitater)
{
    for (PendingSlotSeed &p : this->pending_slot_seeds)
    {
        if (p.index == index)
        {
            p.type = type;
            p.imitater = imitater;
            return;
        }
    }
    PendingSlotSeed p;
    p.index = index;
    p.type = type;
    p.imitater = imitater;
    this->pending_slot_seeds.push_back(p);
}

void PvZ::SetSlotSeed(int index, int type, bool imitater = false)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    // ★★ 2026-10-14: **选卡界面** (mGameScene == 2 = SCENE_LEVEL_INTRO) 里直接写 mSeedBank
    //   是**白写** —— 玩家按 "Let's Rock" 那一刻, `SeedChooserScreen::CloseSeedChooser()`
    //   (EN 1.0.0.1051: VA 0x486D20) 会按**选卡结果**逐槽 `SetPacketType()` 无条件覆盖
    //   `Board::mSeedBank`。所以这里改成"记台账", 等战斗正式开始 (`mGameScene == 3`
    //   = SCENE_PLAYING) 那一刻由 `TickPendingSlotSeeds()` 写**一次**。
    //   ★ 只在用户**确实点过「换卡片」**的格上写 ⇒ 绝不覆盖玩家自己在选卡界面选的卡。
    if (ui == 2)
    {
        RememberPendingSlotSeed(index, type, imitater);
        char tmp[160];
        std::snprintf(tmp, sizeof(tmp), "已在选卡界面记下第 %d 格 (战斗开始时写入)", index + 1);
        this->slot_seed_msg = tmp;
        return;
    }

    // 战斗中: 老路径, 立刻写 (这里本来就有效)
    WriteSlotSeedNow(index, type, imitater);
    char tmp[160];
    std::snprintf(tmp, sizeof(tmp), "已写入第 %d 格", index + 1);
    this->slot_seed_msg = tmp;
}

// ===========================================================================
// ★★ 2026-10-14 新增: 「取消必选卡片」(冒险模式**二周目** Crazy Dave 预选) ★★
//
// 【A. 「换卡片」延迟写入的判据 (纯函数 + 每帧 tick)】
//   `SlotSeedShouldApply()`: 给定当前 `mGameScene` / 本关是否已写 / 台账格数 ⇒ 该不该写。
//   只在 `2 -> 3` 那条边沿上写一次 (也就是 CloseSeedChooser 之后), 台账随即清空 ⇒
//   每关只写一次、幂等, 且下一关不会拿上一关的设定去覆盖玩家自己选的卡。
//
// 【B. 必选卡片的两套机制】 详见 inc/pvz.h 里的说明与 HANDOVER §4.14。
//   机制 A 的**纯函数**自检分两步 (可离线断言, 见 misc/recttest.cpp):
//     ① `DavePickCheckShape()` —— 12 字节必须是
//        `83 78 ?? 00 7E 06 | 55 E8 <rel32>` (cmp [eax+??],0 / jle +6 / push ebp / call rel32)
//     ② `DavePickHasPickedWrite()` —— call 目标的函数体里必须有"把 mCrazyDavePicked 置 1"
//        的写入 (`88 4E 38` = mov [esi+0x38],cl 形态; 也接受 `C6 4? 38 01` 形态)
//   任何一步不过 ⇒ **拒绝打补丁** (宁可功能不可用, 绝不改坏游戏)。
// ===========================================================================

// 机制 A 的签名: `83 78 ?? 00 7E 06 55 E8` (?? = PlayerInfo::mFinishedAdventure 的偏移;
// 非 GOTY 0x2C / GOTY 用 ?? 通配兜住)。全 exe 唯一, 见 HANDOVER §4.14 的实测记录。
const char *const kDavePickPattern = "83 78 ?? 00 7E 06 55 E8";
const int kDavePickPatternPatchOff = 4; // 要改成 EB 的那个 `7E` 在签名里的下标
const size_t kDavePickBodyScan = 0x400; // call 目标函数体的扫描窗口 (实测那条写入在 +0x28E)
// PvZ 主模块**不是 ASLR**: 基址固定 0x400000, 整个镜像远小于 16 MB。
// 用它判 `call rel32` 有没有算飞 (纯函数里也能做, 不需要进程)。
const int64_t kPvzImageBase = 0x400000;
const int64_t kPvzImageSpan = 0x1000000;

// 机制 B 的几何 (全部来自 game-src/Lawn/Widget/SeedChooserScreen.h 的偏移注释):
//   `mChosenSeeds[53]` 在 +0xA4、`ChosenSeed` 步长 0x3C —— 两条互相印证:
//     (mApp@+0xD10 - mChosenSeeds@+0xA4) / 0x3C = **53** = NUM_SEED_TYPES (ConstEnums.h)
//     逐项字段: mSeedType +0x20 / mSeedState +0x24 / mSeedIndexInBank +0x28 / mCrazyDavePicked +0x38
//   ★ 但**构造函数只初始化前 49 项** (循环上界 = NUM_SEEDS_IN_CHOOSER = 49), 所以布局自检
//     就查这 49 项 —— 这也是"这个指针现在真的指向一个选卡界面的 mChosenSeeds"的运行时证据。
const uintptr_t kChosenSeedsBase = 0xa4;
const uintptr_t kChosenSeedStride = 0x3c;
const int kChosenSeedCount = 49; // NUM_SEEDS_IN_CHOOSER (数组本体是 53 项)
const uintptr_t kChosenSeedTypeOff = 0x20;
const uintptr_t kChosenSeedPickedOff = 0x38;
const uintptr_t kWidgetMouseVisibleOff = 0x55; // Widget::mMouseVisible (只有选卡界面显示着才是 1)
const int kSceneLevelIntro = 2;                // GameScenes::SCENE_LEVEL_INTRO (选卡)
const int kScenePlaying = 3;                   // GameScenes::SCENE_PLAYING  (战斗正式开始)

// 地址是不是落在游戏主模块 (基址 0x400000, 非 ASLR) 的某个**可执行节**里。
// 为什么单独判: call rel32 是**算**出来的, 万一签名在别的版本里出现在别的上下文,
// 算飞的目标也必须当场拒绝 (读到 .rdata 里碰巧同形的字节不算证据)。
static bool AddrInExecSection(Pt::Process &proc, uintptr_t addr)
{
    const uintptr_t base = 0x400000;
    unsigned char dos[0x40] = {0};
    if (!proc.ReadBytes(dos, base, sizeof(dos)) || dos[0] != 'M' || dos[1] != 'Z')
        return false;

    uint32_t pe_off = 0;
    std::memcpy(&pe_off, dos + 0x3c, sizeof(pe_off));
    if (pe_off == 0 || pe_off > 0x1000)
        return false;

    unsigned char pe[0x18] = {0};
    if (!proc.ReadBytes(pe, base + pe_off, sizeof(pe)) || pe[0] != 'P' || pe[1] != 'E')
        return false;

    uint16_t section_count = 0, opt_size = 0;
    std::memcpy(&section_count, pe + 0x06, sizeof(section_count));
    std::memcpy(&opt_size, pe + 0x14, sizeof(opt_size));
    if (section_count == 0 || section_count > 96 || opt_size > 0x400)
        return false;

    const uintptr_t sec_table = base + pe_off + 0x18 + opt_size;
    for (uint16_t i = 0; i < section_count; i++)
    {
        unsigned char sh[40] = {0};
        if (!proc.ReadBytes(sh, sec_table + 40 * i, sizeof(sh)))
            return false;
        uint32_t vsize = 0, vaddr = 0, chars = 0;
        std::memcpy(&vsize, sh + 0x08, sizeof(vsize));
        std::memcpy(&vaddr, sh + 0x0c, sizeof(vaddr));
        std::memcpy(&chars, sh + 0x24, sizeof(chars));
        if ((chars & 0x20000000) == 0) // IMAGE_SCN_MEM_EXECUTE
            continue;
        const uintptr_t lo = base + vaddr;
        if (addr >= lo && addr < lo + vsize)
            return true;
    }
    return false;
}

PvZ::DavePickVerdict PvZ::DavePickCheckShape(const unsigned char *code, size_t size, uintptr_t base,
                                            size_t hit_off, DavePickSite &out, const char **why)
{
    if (why != nullptr)
        *why = "";
    // 形状 = 12 字节: 83 78 ?? 00 7E 06 | 55 E8 <rel32>
    if (code == nullptr || hit_off > size || size - hit_off < 12)
    {
        if (why != nullptr)
            *why = "命中处读不到 12 字节";
        return DPV_SHORT;
    }

    const unsigned char *p = code + hit_off;
    if (p[0] != 0x83 || p[1] != 0x78 || p[3] != 0x00 || p[4] != 0x7e || p[5] != 0x06 ||
        p[6] != 0x55 || p[7] != 0xe8)
    {
        if (why != nullptr)
            *why = "形状不是 83 78 ?? 00 7E 06 + 55 E8";
        return DPV_SHAPE;
    }

    out.site = base + hit_off;
    out.patch_addr = out.site + kDavePickPatternPatchOff;
    out.cmp_offset = p[2];

    int32_t rel = 0;
    std::memcpy(&rel, p + 8, sizeof(rel));
    const int64_t target = static_cast<int64_t>(out.site) + 12 + rel; // call 的下一条 = site+12
    // `rel32` 必须落在**同一个镜像窗口**里 (近调用), 否则就是算飞了 ⇒ 当场拒绝。
    // 这里只做"算术上合理"的检查; "目标是否真的在可执行节里"由调用方 (AddrInExecSection) 判。
    if (rel == 0 || target < kPvzImageBase || target >= kPvzImageBase + kPvzImageSpan)
    {
        if (why != nullptr)
            *why = "call rel32 算出来的目标越界";
        return DPV_CALL;
    }
    out.call_target = static_cast<uintptr_t>(target);
    return DPV_OK;
}

size_t PvZ::DavePickPickedWriteLen(const unsigned char *p, size_t avail)
{
    if (p == nullptr || avail < 3)
        return 0;
    // `C6 4? 38 01` = mov byte [reg+0x38], 1   (reg 字段必须是 /0)
    if (p[0] == 0xc6 && (p[1] & 0xc7) == 0x46 && p[2] == 0x38)
        return (avail >= 4 && p[3] == 0x01) ? 4 : 0;
    // `88 4? 38` = mov [reg+0x38], cl          (源寄存器必须是 cl, 前面刚被置 1)
    if (p[0] == 0x88 && (p[1] & 0xc7) == 0x46 && (p[1] & 0x38) == 0x08 && p[2] == 0x38)
        return 3;
    return 0;
}

bool PvZ::DavePickHasPickedWrite(const unsigned char *body, size_t len, size_t *out_off)
{
    if (out_off != nullptr)
        *out_off = 0;
    if (body == nullptr)
        return false;
    for (size_t i = 0; i + 3 <= len; i++)
    {
        if (DavePickPickedWriteLen(body + i, len - i) != 0)
        {
            if (out_off != nullptr)
                *out_off = i;
            return true;
        }
    }
    return false;
}

PvZ::DavePickClearPlan PvZ::DavePickPlanClear(const unsigned char *chosen, size_t size)
{
    DavePickClearPlan plan;
    if (chosen == nullptr || size < static_cast<size_t>(kChosenSeedCount) * kChosenSeedStride)
        return plan; // layout_ok = false, zero_offsets 空 ⇒ 调用方一个字节都不会写

    // 布局自检: 49 项 `mSeedType == i` 必须**全部**成立 (选卡界面构造函数逐项这么写的)。
    // 这是"这个指针现在真的指向一个选卡界面的 mChosenSeeds"的运行时证据;
    // 任何一项不符 ⇒ 直接判不支持, **不写任何字节**。
    for (int i = 0; i < kChosenSeedCount; i++)
    {
        const size_t at = static_cast<size_t>(i) * kChosenSeedStride + kChosenSeedTypeOff;
        int32_t t = 0;
        std::memcpy(&t, chosen + at, sizeof(t));
        if (t != i)
        {
            plan.bad_index = i;
            return plan;
        }
    }
    plan.layout_ok = true;

    // 只把当前非 0 的 `mCrazyDavePicked` 写 0 ⇒ 幂等 (第二帧起就没有可写的了)。
    for (int i = 0; i < kChosenSeedCount; i++)
    {
        const size_t at = static_cast<size_t>(i) * kChosenSeedStride + kChosenSeedPickedOff;
        if (chosen[at] != 0)
        {
            plan.zero_offsets.push_back(static_cast<uint32_t>(at));
            plan.picked++;
        }
    }
    return plan;
}

bool PvZ::SlotSeedShouldApply(int ui_now, bool wrote_this_battle, int pending_count)
{
    // 台账空 = 用户没在选卡界面指定过任何格 ⇒ 什么都不写 (绝不覆盖玩家自己选的卡)
    if (pending_count <= 0)
        return false;
    // 只有"战斗正式开始"(SCENE_PLAYING = 3) 才写 —— 那一刻 CloseSeedChooser() 已经跑完
    if (ui_now != kScenePlaying)
        return false;
    // 本关已经写过 = 幂等 (每关只写一次)
    if (wrote_this_battle)
        return false;
    return true;
}

void PvZ::TickPendingSlotSeeds()
{
    if (!GameOn())
    {
        this->slot_seed_last_ui = -1;
        return;
    }

    const int ui = GameUI();
    const bool wrote_this_battle = (this->slot_seed_last_ui == kScenePlaying);
    if (SlotSeedShouldApply(ui, wrote_this_battle, static_cast<int>(this->pending_slot_seeds.size())))
    {
        for (const PendingSlotSeed &p : this->pending_slot_seeds)
            WriteSlotSeedNow(p.index, p.type, p.imitater);

        char tmp[192];
        std::snprintf(tmp, sizeof(tmp), "战斗开始: 已按选卡界面记下的内容写入 %d 格",
                      static_cast<int>(this->pending_slot_seeds.size()));
        this->slot_seed_msg = tmp;
        // 写完就清台账: 下一关不会拿这一关的设定去覆盖玩家自己选的卡 (见头文件说明)
        this->pending_slot_seeds.clear();
    }
    this->slot_seed_last_ui = ui;
}

// 生产路径与离线单测**共用**的判定 (纯函数: 只吃字节, 不碰进程) ——
//   把"该不该打这个补丁"整条判断收在一个地方, 于是"篡改一个字节 ⇒ 必须拒绝"这种
//   负对照可以离线断言 (见 misc/recttest.cpp)。
//   head/head_len : 命中处开始的字节 (至少 12 字节)
//   hit_va        : 命中处对应的虚拟地址 (算 call 目标与补丁地址用)
//   body_in_exec  : call 目标是否落在游戏模块的**可执行节**里 (由 AddrInExecSection 判)
//   body/body_len : call 目标函数体开头的一段字节 (读不到就传 nullptr/0)
static PvZ::DavePickVerdict DavePickDecide(const unsigned char *head, size_t head_len, uintptr_t hit_va,
                                          bool body_in_exec, const unsigned char *body, size_t body_len,
                                          PvZ::DavePickSite &out, size_t *out_body_off,
                                          const char **why)
{
    out = PvZ::DavePickSite();
    if (out_body_off != nullptr)
        *out_body_off = 0;

    // ① 形状自检 (12 字节 + 算出 call 目标)
    PvZ::DavePickSite site;
    const char *local_why = nullptr;
    const PvZ::DavePickVerdict shape =
        PvZ::DavePickCheckShape(head, head_len, hit_va, 0, site, why != nullptr ? why : &local_why);
    if (shape != PvZ::DPV_OK)
        return shape;

    // ② call 目标必须在可执行节里
    if (!body_in_exec)
        return PvZ::DPV_CALL;

    // ③ 目标函数体里必须真有"把 mCrazyDavePicked 置 1"的写入
    size_t off = 0;
    if (body == nullptr || !PvZ::DavePickHasPickedWrite(body, body_len, &off))
        return PvZ::DPV_NO_WRITE;

    out = site;
    if (out_body_off != nullptr)
        *out_body_off = off;
    return PvZ::DPV_OK;
}

PvZ::DavePickVerdict PvZ::FindDavePickSite(DavePickSite &out, size_t *out_body_off)
{
    out = DavePickSite();
    if (out_body_off != nullptr)
        *out_body_off = 0;
    if (!GameOn())
        return DPV_NONE;

    // ① 扫签名 (通配版: 兼容 GOTY 那个不同的 PlayerInfo 偏移)。**必须唯一命中** ——
    //    0 处 = 本版不支持; 多处 = 有歧义 ⇒ 两种情况都**不碰游戏代码**。
    const std::vector<uintptr_t> hits = this->ScanGamePattern(kDavePickPattern, 4);
    if (hits.empty())
        return DPV_NOT_FOUND;
    if (hits.size() != 1)
        return DPV_AMBIGUOUS;

    // ② 读命中处的 12 字节; 解出 call 目标后去读它的函数体、并判它是否在可执行节里。
    //    最后把三件事交给 DavePickDecide (与离线单测共用的那一个判定)。
    unsigned char head[12] = {0};
    if (!ReadBytes(head, hits[0], sizeof(head)))
        return DPV_SHORT;

    DavePickSite probe;
    const char *why = nullptr;
    const DavePickVerdict shape = DavePickCheckShape(head, sizeof(head), hits[0], 0, probe, &why);
    if (shape != DPV_OK)
        return shape;

    const bool body_in_exec = AddrInExecSection(*this, probe.call_target);
    std::vector<unsigned char> body(kDavePickBodyScan, 0);
    const bool body_ok =
        body_in_exec && ReadBytes(body.data(), probe.call_target, body.size());
    return DavePickDecide(head, sizeof(head), hits[0], body_in_exec,
                          body_ok ? body.data() : nullptr, body_ok ? body.size() : 0, out, out_body_off,
                          &why);
}

bool PvZ::SeedChooserOpen(uintptr_t *out_screen)
{
    if (out_screen != nullptr)
        *out_screen = 0;
    if (!GameOn())
        return false;
    // 选卡阶段就是 SCENE_LEVEL_INTRO (= 2, 工具版本表里那个 game_ui/mGameScene)
    if (GameUI() != kSceneLevelIntro)
        return false;
    // SeedChooserScreen* = [lawn + mGameSelector + 4] —— 用 game_selector+4 跨版本都对
    const uintptr_t screen = ReadMemory<uintptr_t>({data().lawn, data().game_selector + 4});
    if (screen == 0)
        return false;
    // Widget::mMouseVisible: 只有选卡界面**真的显示着**才是 1 (被销毁/隐藏时是 0)
    if (ReadMemory<unsigned char>({screen + kWidgetMouseVisibleOff}) == 0)
        return false;
    if (out_screen != nullptr)
        *out_screen = screen;
    return true;
}

void PvZ::SetNoDavePick(bool on)
{
    this->hack_no_dave_pick = on;
    if (!GameOn())
    {
        this->no_dave_pick_msg = "未连接游戏";
        return;
    }

    const uint32_t pid = this->GamePid();
    // 游戏关掉又重开: 上个进程里的补丁随进程消失, 地址也全失效 ⇒ 状态清零, 重新来过
    if (this->dave_pick_pid != 0 && this->dave_pick_pid != pid)
    {
        this->dave_pick_site = 0;
        this->dave_pick_target = 0;
        this->dave_pick_patched = false;
        this->dave_pick_pid = 0;
        this->dave_pick_verdict = DPV_NONE;
        this->dave_pick_body_off_valid = false;
    }

    if (!on)
    {
        if (this->dave_pick_patched && this->dave_pick_site != 0)
            this->RestorePatch(this->dave_pick_site); // 可逆: 退出/关闭都还原
        this->dave_pick_patched = false;
        this->dave_pick_site = 0;
        this->dave_pick_target = 0;
        this->dave_pick_pid = 0;
        this->dave_pick_verdict = DPV_NONE;
        this->dave_pick_body_off_valid = false;
        this->no_dave_pick_msg = "已关闭";
        return;
    }

    if (this->dave_pick_patched)
    {
        // ★ 自愈 (与高级暂停 SetAdvPause 同一套做法): 主循环在"游戏断开一帧"时会
        //   `RestoreAllPatches()` (imgui_main.cpp:3855)。万一那只是**瞬间断连**
        //   (窗口重建 / 全屏切换) 而游戏进程还活着, 补丁就已经被还原了, 但我们这边的
        //   `dave_pick_patched` 还是 true ⇒ "取消勾选再勾上"也救不回来 (会早退报"已生效"
        //   却一个字节都没改)。所以这里回读确认一次: 不是我们的字节就当作没装, 往下重走。
        unsigned char back = 0;
        if (this->dave_pick_site != 0 && ReadBytes(&back, this->dave_pick_site, 1) && back == 0xeb)
        {
            this->RefreshDavePickMsg();
            return;
        }
        this->dave_pick_patched = false; // 已被还原 -> 往下重新扫描 + 重新打
    }

    // 已经在本进程里查过且没通过 ⇒ **不重复扫** (扫 .text 是 2.4 MB 的读, 不能每帧做);
    // 游戏换进程 (pid 变) 时会走到上面那段清零, 于是自动重试。
    if (this->dave_pick_pid == pid && this->dave_pick_verdict != DPV_NONE &&
        this->dave_pick_verdict != DPV_OK)
    {
        this->RefreshDavePickMsg();
        return;
    }

    DavePickSite site;
    size_t body_off = 0;
    const DavePickVerdict verdict = this->FindDavePickSite(site, &body_off);
    this->dave_pick_verdict = verdict;
    this->dave_pick_pid = pid;
    this->dave_pick_body_off = body_off;
    this->dave_pick_body_off_valid = (verdict == DPV_OK);
    this->dave_pick_site = 0;
    this->dave_pick_target = 0;
    this->dave_pick_cmp_offset = 0;
    this->dave_pick_patched = false;

    if (verdict == DPV_OK)
    {
        this->dave_pick_target = site.call_target;
        this->dave_pick_cmp_offset = site.cmp_offset;
        // `7E 06` (jle +6) -> `EB 06` (jmp +6): 长度一样, 无条件跳过后面的 push+call
        const std::vector<unsigned char> patch{0xeb};
        unsigned char back = 0;
        if (!this->PatchBytes(site.patch_addr, patch) ||
            !ReadBytes(&back, site.patch_addr, 1) || back != 0xeb)
        {
            this->RestorePatch(site.patch_addr); // 回读不一致 -> 立刻还原
            this->dave_pick_verdict = DPV_PATCH;
            this->dave_pick_body_off_valid = false;
            this->dave_pick_target = 0;
            this->dave_pick_cmp_offset = 0;
        }
        else
        {
            this->dave_pick_site = site.patch_addr;
            this->dave_pick_patched = true;
        }
    }
    this->RefreshDavePickMsg();
}

void PvZ::TickNoDavePick()
{
    if (!this->hack_no_dave_pick)
        return;
    if (!GameOn())
    {
        this->no_dave_pick_msg = "未连接游戏";
        return;
    }

    // 换进程: 状态清零 + 重打一遍 (机制 A 的补丁只活在那个进程里)
    if (this->dave_pick_pid != 0 && this->dave_pick_pid != this->GamePid())
        this->SetNoDavePick(true);

    // ★ 自愈 (每帧 1 字节的回读, 与 SetAdvPause 同一套判据): 主循环在"游戏断开一帧"
    //   时会 `RestoreAllPatches()`; 瞬间断连时游戏进程还活着, 补丁被还原而这边
    //   `dave_pick_patched` 仍是 true ⇒ 界面会一直说"已生效"却什么都没改。
    //   回读发现不是我们的字节就重走一遍 SetNoDavePick (它里面有"重新扫描 + 重打"那条路)。
    if (this->dave_pick_patched && this->dave_pick_site != 0)
    {
        unsigned char back = 0;
        if (!ReadBytes(&back, this->dave_pick_site, 1) || back != 0xeb)
            this->SetNoDavePick(true);
    }

    this->dave_pick_b_cleared = 0;
    this->dave_pick_b_active = false;
    this->dave_pick_b_layout_ok = false;
    this->dave_pick_b_bad_index = -1;

    uintptr_t screen = 0;
    if (this->SeedChooserOpen(&screen))
    {
        this->dave_pick_b_active = true;
        std::vector<unsigned char> buf(static_cast<size_t>(kChosenSeedCount) * kChosenSeedStride, 0);
        if (ReadBytes(buf.data(), screen + kChosenSeedsBase, buf.size()))
        {
            const DavePickClearPlan plan = DavePickPlanClear(buf.data(), buf.size());
            this->dave_pick_b_layout_ok = plan.layout_ok;
            this->dave_pick_b_bad_index = plan.bad_index;
            for (uint32_t off : plan.zero_offsets)
            {
                const unsigned char zero = 0;
                if (WriteBytes(&zero, screen + kChosenSeedsBase + off, 1))
                    this->dave_pick_b_cleared++;
            }
        }
    }
    this->RefreshDavePickMsg();
}

// 状态行文案 (纯函数; 生产路径 RefreshDavePickMsg 与离线单测用的是**同一个实现**)。
//   三个分支就是用户看到的三句话: 已生效 / 未生效(不支持此版本) / 等待选卡界面。
static const char *DavePickStatusText(bool enabled, bool game_on, bool patched, bool b_active,
                                      bool b_layout_ok)
{
    if (!enabled)
        return "已关闭";
    if (!game_on)
        return "未连接游戏";
    // 机制 A 打上了 ⇒ 进关卡时根本不抽卡, 任何界面都是"已生效"
    if (patched)
        return "已生效";
    if (b_active)
        // 选卡界面开着: 布局自检过 = 机制 B 已经把那几张放开了
        return b_layout_ok ? "已生效" : "未生效(不支持此版本)";
    // 机制 A 没打上, 而选卡界面还没开 ⇒ 等它开了用机制 B (普通界面只留这一句)
    return "等待选卡界面";
}

// DEV 行文案 (纯函数, 同上: 只有 PZTK_DEV=1 才显示, 普通界面一个字都不多)
static std::string DavePickDevText(bool enabled, int verdict, uintptr_t patch_addr, uint32_t cmp_off,
                                  uintptr_t target, bool body_off_valid, size_t body_off,
                                  bool b_active, bool b_layout_ok, int b_bad, int b_cleared)
{
    if (!enabled)
        return "必选卡: 未开启";

    static const char *kVertext[] = {"未查过",         "自检通过",     "命中处字节不足",
                                     "签名形状不符",   "目标不在代码节", "目标无置 1 写入",
                                     "补丁写入/回读失败", "签名未命中", "签名多处命中"};
    const size_t kvn = sizeof(kVertext) / sizeof(kVertext[0]);
    const size_t vi = (verdict >= 0 && static_cast<size_t>(verdict) < kvn) ? static_cast<size_t>(verdict) : 0;

    char hit[80] = "无";
    if (patch_addr != 0)
        std::snprintf(hit, sizeof(hit), "%08X(EB)", static_cast<unsigned>(patch_addr));

    char tgt[128] = "无";
    if (target != 0)
    {
        if (body_off_valid)
            std::snprintf(tgt, sizeof(tgt), "%08X(+0x%X 处 88 4E 38)", static_cast<unsigned>(target),
                          static_cast<unsigned>(body_off));
        else
            std::snprintf(tgt, sizeof(tgt), "%08X", static_cast<unsigned>(target));
    }

    char bl[128] = "未在选卡界面";
    if (b_active)
    {
        if (b_layout_ok)
            std::snprintf(bl, sizeof(bl), "布局 49/49 通过, 本帧清 %d 张", b_cleared);
        else
            std::snprintf(bl, sizeof(bl), "布局不过: 第 %d 项 mSeedType != %d", b_bad, b_bad);
    }

    char buf[640];
    std::snprintf(buf, sizeof(buf),
                  "必选卡: 签名 %s 唯一命中 -> 补丁 %s | 偏移=%02X | call 目标 %s | %s | 机制B: %s",
                  kDavePickPattern, hit, static_cast<unsigned>(cmp_off), tgt, kVertext[vi], bl);
    return std::string(buf);
}

void PvZ::RefreshDavePickMsg()
{
    this->no_dave_pick_msg = DavePickStatusText(this->hack_no_dave_pick, GameOn(),
                                                this->dave_pick_patched, this->dave_pick_b_active,
                                                this->dave_pick_b_layout_ok);
}

std::string PvZ::DavePickDevLine()
{
    return DavePickDevText(this->hack_no_dave_pick, this->dave_pick_verdict, this->dave_pick_site,
                           this->dave_pick_cmp_offset, this->dave_pick_target,
                           this->dave_pick_body_off_valid, this->dave_pick_body_off,
                           this->dave_pick_b_active, this->dave_pick_b_layout_ok,
                           this->dave_pick_b_bad_index, this->dave_pick_b_cleared);
}

void PvZ::LilyPadOnPool(int from_col, int to_col)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    unsigned int plant_struct_size = 0x14c;

    auto plant_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});
    auto plant_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});
    bool has_plant[6][9] = {{false}};

    for (size_t i = 0; i < plant_count_max; i++)
    {
        auto plant_dead = ReadMemory<bool>({plant_offset + data().plant_dead + plant_struct_size * i});
        auto plant_squished = ReadMemory<bool>({plant_offset + data().plant_squished + plant_struct_size * i});
        auto plant_row = ReadMemory<uint32_t>({plant_offset + data().plant_row + plant_struct_size * i});
        auto plant_col = ReadMemory<uint32_t>({plant_offset + data().plant_col + plant_struct_size * i});
        if (!plant_dead && !plant_squished)
            has_plant[plant_row][plant_col] = true;
    }

    asm_init();
    int rows = GetRowCount();
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < 9; c++)
        {
            // 1.草地 2.裸地 3.泳池
            auto block_type = ReadMemory<int>({data().lawn, data().board,                 //
                                               data().block_type + 0x04 * r + 0x18 * c}); //
            if (block_type == 3 && !has_plant[r][c] && from_col - 1 <= c && c <= to_col - 1)
                asm_put_plant(r, c, 16, false, false); // 16 睡莲
        }
    }
    asm_ret();
    asm_code_inject();
}

void PvZ::FlowerPotOnRoof(int from_col, int to_col)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    int scene = GetScene();
    if (scene != 4 && scene != 5)
        return;

    unsigned int plant_struct_size = 0x14c;

    auto plant_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});
    auto plant_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});
    bool has_plant[5][9] = {{false}};

    for (size_t i = 0; i < plant_count_max; i++)
    {
        auto plant_dead = ReadMemory<bool>({plant_offset + data().plant_dead + plant_struct_size * i});
        auto plant_squished = ReadMemory<bool>({plant_offset + data().plant_squished + plant_struct_size * i});
        auto plant_row = ReadMemory<uint32_t>({plant_offset + data().plant_row + plant_struct_size * i});
        auto plant_col = ReadMemory<uint32_t>({plant_offset + data().plant_col + plant_struct_size * i});
        if (!plant_dead && !plant_squished)
            has_plant[plant_row][plant_col] = true;
    }

    asm_init();
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 9; c++)
            if (!has_plant[r][c] && from_col - 1 <= c && c <= to_col - 1)
                asm_put_plant(r, c, 33, false, false); // 33 花盆
    asm_ret();
    asm_code_inject();
}

void PvZ::Screenshot()
{
    if (!GameOn())
        return;

    RECT rcClient = {0, 0, 800, 600};
    // if (GetClientRect(this->hwnd, &rcClient) == 0)
    //     return;
    int cx = rcClient.right - rcClient.left;
    int cy = rcClient.bottom - rcClient.top;
    // if (cx == 0 || cy == 0)
    //     return;

    HDC hdcWindow = nullptr;
    HDC hdcMemDC = nullptr;
    HBITMAP hbmScreen = nullptr;

    hdcWindow = GetDC(this->hwnd);
    if (!hdcWindow)
        goto done;
    hdcMemDC = CreateCompatibleDC(hdcWindow);
    if (!hdcMemDC)
        goto done;
    hbmScreen = CreateCompatibleBitmap(hdcWindow, cx, cy);
    if (!hbmScreen)
        goto done;

    SelectObject(hdcMemDC, hbmScreen);
    if (BitBlt(hdcMemDC, 0, 0, cx, cy, hdcWindow, 0, 0, SRCCOPY) == 0)
        goto done;

    // 保存到剪贴板
    if (OpenClipboard(nullptr) != 0)
    {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hbmScreen);
        CloseClipboard();
    }

done:
    DeleteObject(hbmScreen);
    DeleteObject(hdcMemDC);
    ReleaseDC(this->hwnd, hdcWindow);
}

Lineup PvZ::GetLineup()
{
    Lineup lineup;

    if (!GameOn())
        return lineup;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return lineup;

    lineup.scene = GetScene();

    unsigned int plant_struct_size = 0x14c;
    unsigned int grid_item_struct_size = 0xec;
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (this->find_result == PVZ_BETA_0_1_1_1014_EN)
        grid_item_struct_size = 0x8c;
#endif

    auto plant_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});
    auto plant_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});
    for (size_t i = 0; i < plant_count_max; i++)
    {
        auto plant_dead = ReadMemory<bool>({plant_offset + data().plant_dead + plant_struct_size * i});
        auto plant_squished = ReadMemory<bool>({plant_offset + data().plant_squished + plant_struct_size * i});
        auto plant_type = ReadMemory<uint32_t>({plant_offset + data().plant_type + plant_struct_size * i});
        if (!plant_dead && !plant_squished && 0 <= plant_type && plant_type <= 47)
        {
            auto plant_row = ReadMemory<uint32_t>({plant_offset + data().plant_row + plant_struct_size * i});
            auto plant_col = ReadMemory<uint32_t>({plant_offset + data().plant_col + plant_struct_size * i});
            auto plant_asleep = ReadMemory<bool>({plant_offset + data().plant_asleep + plant_struct_size * i});
            auto plant_imitater = ReadMemory<int>({plant_offset + data().plant_imitater + plant_struct_size * i}) == 48;
            if (plant_type == 16 || plant_type == 33) // 睡莲 花盆
            {
                lineup.base[plant_row * 9 + plant_col] = (plant_type == 16) ? 1 : 2;
                lineup.base_im[plant_row * 9 + plant_col] = plant_imitater ? 1 : 0;
            }
            else if (plant_type == 30) // 南瓜
            {
                lineup.pumpkin[plant_row * 9 + plant_col] = 1;
                lineup.pumpkin_im[plant_row * 9 + plant_col] = plant_imitater ? 1 : 0;
            }
            else if (plant_type == 35) // 咖啡
            {
                lineup.coffee[plant_row * 9 + plant_col] = 1;
                lineup.coffee_im[plant_row * 9 + plant_col] = plant_imitater ? 1 : 0;
            }
            else // 主要植物
            {
                lineup.plant[plant_row * 9 + plant_col] = plant_type + 1;
                lineup.plant_im[plant_row * 9 + plant_col] = plant_imitater ? 1 : 0;
                lineup.plant_awake[plant_row * 9 + plant_col] = plant_asleep ? 0 : 1;
            }
        }
    }

    auto grid_item_count_max = ReadMemory<uint32_t>({data().lawn, data().board, data().grid_item_count_max});
    auto grid_item_offset = ReadMemory<uintptr_t>({data().lawn, data().board, data().grid_item});
    for (size_t i = 0; i < grid_item_count_max; i++)
    {
        auto grid_item_dead = ReadMemory<bool>({grid_item_offset + data().grid_item_dead + grid_item_struct_size * i});
        auto grid_item_type = ReadMemory<int>({grid_item_offset + data().grid_item_type + grid_item_struct_size * i});
        if (!grid_item_dead && (grid_item_type == 1 || grid_item_type == 3 || grid_item_type == 11))
        {
            auto grid_item_row = ReadMemory<uint32_t>({grid_item_offset + data().grid_item_row + grid_item_struct_size * i});
            auto grid_item_col = ReadMemory<uint32_t>({grid_item_offset + data().grid_item_col + grid_item_struct_size * i});
            if (grid_item_type == 1) // 墓碑
            {
                lineup.base[grid_item_row * 9 + grid_item_col] = 3;
                lineup.base_im[grid_item_row * 9 + grid_item_col] = 0;
            }
            else if (grid_item_type == 3) // 梯子
            {
                lineup.ladder[grid_item_row * 9 + grid_item_col] = 1;
            }
            else // 钉耙
            {
                lineup.rake_row = grid_item_row + 1;
            }
        }
    }

    return lineup;
}

void PvZ::SetLineup(Lineup lineup)
{
    if (!lineup.OK())
        return;

    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    int mode = GameMode();
    bool is_el = mode >= 11 && mode <= 15;
    bool is_iz = mode >= 61 && mode <= 70;
    if (!is_el && !is_iz)
        return;

    ClearGridItems({1, 2, 3, 11});
    ClearAllPlants();

    if (GetScene() != lineup.scene)
        SetScene(lineup.scene, true);

    if (lineup.rake_row != 0)
    {
        int r = lineup.rake_row - 1;
        int c = 8 - 1;
        PutRake(r, c);
    }

    asm_init();
    // 睡莲 花盆
    for (size_t r = 0; r < 6; r++)
    {
        for (size_t c = 0; c < 9; c++)
        {
            if (lineup.base[r * 9 + c] == 1)
                asm_put_plant(r, c, 16, lineup.base_im[r * 9 + c] == 1, is_iz);
            else if (lineup.base[r * 9 + c] == 2)
                asm_put_plant(r, c, 33, lineup.base_im[r * 9 + c] == 1, is_iz);
        }
    }
    // 主要植物
    for (size_t r = 0; r < 6; r++)
    {
        for (size_t c = 0; c < 9; c++)
        {
            if (lineup.plant[r * 9 + c] == 0)
                continue;

            int plant_type = lineup.plant[r * 9 + c] - 1;
            bool plant_imitater = lineup.plant_im[r * 9 + c] == 1;
            bool plant_asleep = lineup.plant_awake[r * 9 + c] == 0;

            if (plant_type < 0 || plant_type > 47       //
                || plant_type == 16 || plant_type == 33 //
                || plant_type == 30 || plant_type == 35)
                continue;

            asm_put_plant(r, c, plant_type, plant_imitater, is_iz);

            // 蘑菇类植物唤醒
            if ((lineup.scene == 0 || lineup.scene == 2 || lineup.scene == 4) //
                && lineup.may_sleep[plant_type] && !plant_asleep)
            {
                asm_push_exx(Reg::EAX);
                if (isGOTY())
                    asm_mov_exx_exx(Reg::EDI, Reg::EAX);
#ifdef _PVZ_BETA_LEAK_SUPPORT
                else if (isBETA())
                    asm_mov_exx_exx(Reg::ECX, Reg::EAX);
#endif
                asm_push_byte(0);
                asm_call(data().call_set_plant_sleeping);
                asm_pop_exx(Reg::EAX);
            }

            // 土豆雷和阳光菇长大
            // mov [eax+54],00000001
            if (plant_type == 4 || plant_type == 9)
            {
#ifdef _PVZ_BETA_LEAK_SUPPORT
                if (isBETA())
                    asm_add_list(0xc7, 0x40, 0x5c);
                else
#endif
                    asm_add_list(0xc7, 0x40, 0x54);
                asm_add_dword(1);
            }
        }
    }
    // 南瓜壳
    for (size_t r = 0; r < 6; r++)
    {
        for (size_t c = 0; c < 9; c++)
        {
            if (lineup.pumpkin[r * 9 + c] == 1)
                asm_put_plant(r, c, 30, lineup.pumpkin_im[r * 9 + c] == 1, is_iz);
        }
    }
    // 咖啡豆
    for (size_t r = 0; r < 6; r++)
    {
        for (size_t c = 0; c < 9; c++)
        {
            if (lineup.coffee[r * 9 + c] == 1)
                asm_put_plant(r, c, 35, lineup.coffee_im[r * 9 + c] == 1, is_iz);
        }
    }
    // 墓碑
    for (size_t r = 0; r < 6; r++)
    {
        for (size_t c = 0; c < 9; c++)
        {
            if (lineup.base[r * 9 + c] == 3)
                asm_put_grave(r, c);
        }
    }
    // 梯子
    for (size_t r = 0; r < 6; r++)
    {
        for (size_t c = 0; c < 9; c++)
        {
            if (lineup.ladder[r * 9 + c] == 1)
                asm_put_ladder(r, c);
        }
    }
    asm_ret();
    asm_code_inject();

    Sleep(GetFrameDuration());
}

// 根据出怪种类生成出怪列表
void PvZ::generate_spawn_list()
{
    asm_init();
    if (this->find_result == PVZ_GOTY_1_1_0_1056_ZH || //
        this->find_result == PVZ_GOTY_1_1_0_1056_JA)
    {
        asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
    }
    else
    {
#ifdef _PVZ_BETA_LEAK_SUPPORT
        if (isBETA())
        {
            asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().board);
        }
        else
#endif
        {
            asm_mov_exx_dword_ptr(Reg::EDI, data().lawn);
            asm_mov_exx_dword_ptr_exx_add(Reg::EDI, data().board);
        }
    }
    asm_call(data().call_pick_zombie_waves);
    asm_ret();
    asm_code_inject();
}

// 更新选卡界面的出怪预览
void PvZ::update_spawn_preview()
{
    enable_hack(data().hack_street_zombies, true);
    asm_init();
    asm_mov_exx_dword_ptr(Reg::EBX, data().lawn);
    asm_mov_exx_dword_ptr_exx_add(Reg::EBX, data().board);
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
        asm_mov_exx_exx(Reg::ECX, Reg::EBX);
#endif
    asm_call(data().call_remove_cutscene_zombies);
    asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
    asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().board);
    asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().cut_scene);
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
        asm_mov_exx_exx(Reg::ECX, Reg::EAX);
    else
#endif
        asm_push_exx(Reg::EAX);
    asm_call(data().call_place_street_zombies);
    asm_ret();
    asm_code_inject();
    enable_hack(data().hack_street_zombies, false);
}

std::array<int, 1000> PvZ::GetSpawnList()
{
    std::array<int, 1000> zl;
    zl.fill(-1);

    if (!GameOn())
        return zl;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return zl;

    zl = ReadMemory<int, 1000>({data().lawn, data().board, data().spawn_list});

#ifdef _DEBUG
    std::cout << std::dec; // ffffffff -> -1
    std::cout << std::endl;
    for (size_t i = 0; i < 20; i++)
    {
        for (size_t j = 0; j < 50; j++)
            std::cout << zl[i * 50 + j] << " ";
        std::cout << std::endl;
    }
    std::cout << std::endl;
#endif

    for (size_t i = 0; i < 20; i++)
    {
        // 每一波遇到 -1 之后忽略后面的出怪
        // 所以就算读到了数据也改成 -1
        bool ignore_rest = false;
        for (size_t j = 0; j < 50; j++)
        {
            if (zl[i * 50 + j] == -1)
            {
                ignore_rest = true;
                continue;
            }
            if (ignore_rest)
            {
                zl[i * 50 + j] = -1;
            }
        }
    }

    return zl;
}

void PvZ::SetSpawnList(std::array<int, 1000> zl)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    WriteMemory(zl, {data().lawn, data().board, data().spawn_list});

    if (ui == 2)
        update_spawn_preview();
}

void PvZ::InternalSpawn(std::array<bool, 33> zombies)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    WriteMemory(zombies, {data().lawn, data().board, data().spawn_type});

    std::array<int, 2000> zombies_list;
    zombies_list.fill(-1);
    WriteMemory(zombies_list, {data().lawn, data().board, data().spawn_list});

    generate_spawn_list();
    if (ui == 2)
        update_spawn_preview();
}

void PvZ::CustomizeSpawn(std::array<bool, 33> zombies, bool limit_giga, bool simulate, int giga_weight)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    std::array<bool, 20> giga_waves;
    giga_waves.fill(true);
    if (limit_giga)
        for (size_t w = 11; w <= 19; w++)
            giga_waves[w - 1] = false;

    std::vector<double> weights = {
        4000, // 普僵
        0,    // 旗帜
        4000, // 路障
        2000, // 撑杆
        3000, // 铁桶
        1000, // 读报
        3500, // 铁门
        2000, // 橄榄
        1000, // 舞王
        0,    // 伴舞
        0,    // 鸭子
        2000, // 潜水
        2000, // 冰车
        2000, // 雪橇
        1500, // 海豚
        1000, // 小丑
        2000, // 气球
        1000, // 矿工
        1000, // 跳跳
        1,    // 雪人
        1000, // 蹦极
        1000, // 扶梯
        1500, // 投篮
        1500, // 白眼
        0,    // 小鬼
        0,    // 僵博
        4000, // 豌豆
        3000, // 坚果
        1000, // 辣椒
        2000, // 机枪
        2000, // 窝瓜
        2000, // 高墙
        6000  // 红眼
    };

#ifdef _DEBUG
    if (this->find_result == PVZ_1_0_0_1051_EN)
        for (size_t i = 0; i < 33; i++)
            assert(ReadMemory<int>({0x0069da94 + i * 0x1c}) == static_cast<int>(weights[i]));
#endif

    weights[0] = 400;
    weights[2] = 1000;
    std::vector<double> weights_flag = weights;
    weights[32] = giga_weight;
    std::vector<double> weights_normal = weights;
    std::discrete_distribution<unsigned int> dist_flag(weights_flag.begin(), weights_flag.end());
    std::discrete_distribution<unsigned int> dist_normal(weights_normal.begin(), weights_normal.end());
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937 gen(static_cast<unsigned int>(seed));

    // 僵尸列表
    std::array<int, 1000> zombies_list;
    zombies_list.fill(-1);

    bool has_flag = zombies[1];
    bool has_yeti = zombies[19];
    bool has_bungee = zombies[20];
    bool has_giga = zombies[32];

    bool limit_flag = true;
    bool limit_yeti = true;
    bool limit_bungee = true;

    int count = 0;
    for (size_t i = 0; i < 33; i++)
        if (zombies[i])
            count++;

    if (count > 0)
    {
        int type = 0;
        for (size_t i = 0; i < 1000; i++)
        {
            do
            {
                if (simulate)
                {
                    if (((i / 50) % 10) == 9) // 旗帜波
                        type = dist_flag(gen);
                    else
                        type = dist_normal(gen);
                }
                else
                {
                    type += 1;
                    type %= 33;
                }
            } while ((!zombies[type])                              //
                     || (has_flag && limit_flag && type == 1)      //
                     || (has_yeti && limit_yeti && type == 19)     //
                     || (has_bungee && limit_bungee && type == 20) //
                     || (has_giga && limit_giga && type == 32 && !giga_waves[(i / 50) % 20])); //

            zombies_list[i] = type;
        }

        std::vector<size_t> index_flag = {450,                                       //
                                          950};                                      //
        std::vector<size_t> index_zombie = {451, 452, 453, 454, 455, 456, 457, 458,  //
                                            951, 952, 953, 954, 955, 956, 957, 958}; //
        std::vector<size_t> index_bungee = {459, 460, 461, 462,                      //
                                            959, 960, 961, 962};                     //

        if ((has_flag && limit_flag) || simulate)
            for (auto i : index_flag)
                zombies_list[i] = 1;

        if (simulate)
            for (auto i : index_zombie)
                zombies_list[i] = 0;

        if (has_bungee && limit_bungee)
            for (auto i : index_bungee)
                zombies_list[i] = 20;

        if (has_yeti && limit_yeti)
            zombies_list[rand() % 1000] = 19;
    }

    WriteMemory(zombies_list, {data().lawn, data().board, data().spawn_list});
    if (ui == 2)
        update_spawn_preview();
}

void PvZ::SetMusic(int id)
{
    if (!GameOn())
        return;

    asm_init();
#ifdef _PVZ_BETA_LEAK_SUPPORT
    if (isBETA())
    {
        asm_push_dword(id);
        asm_mov_exx_dword_ptr(Reg::ECX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::ECX, data().music);
    }
    else
#endif
    {
        asm_mov_exx(Reg::EDI, id);
        asm_mov_exx_dword_ptr(Reg::EAX, data().lawn);
        asm_mov_exx_dword_ptr_exx_add(Reg::EAX, data().music);
    }
    asm_call(data().call_play_music);
    asm_ret();
    asm_code_inject();
}

void PvZ::NoFog(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().no_fog, on);
}

void PvZ::SeeVase(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().see_vase, on);
}

void PvZ::BackgroundRunning(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().background_running, on);
}

void PvZ::UserdataReadonly(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().disable_delete_userdata, on);
    enable_hack(data().disable_save_userdata, on);

    if (this->find_result == PVZ_GOTY_1_2_0_1096_EN)
    {
        // Steam云同步自带不能删档的问题, 额外加上禁止存档
        enable_hack(HACK<uint8_t, 3>{0x00498440, {0xc2, 0x0c, 0x00}, {0x6a, 0xff, 0x68}}, on);
    }
}

void PvZ::DebugMode(int mode)
{
    if (!GameOn())
        return;
    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return;

    WriteMemory<int>(mode, {data().lawn, data().board, data().debug_mode});
}

int PvZ::GetFrameDuration()
{
    int time_ms = 10;

    if (!GameOn())
        return time_ms;

    time_ms = ReadMemory<int>({data().lawn, data().frame_duration});

    return time_ms;
}

void PvZ::SetFrameDuration(int time_ms)
{
    if (!GameOn())
        return;

    WriteMemory<int>(time_ms, {data().lawn, data().frame_duration});
}

void PvZ::UnlockLimboPage(bool on)
{
    if (!GameOn())
        return;

    enable_hack(data().unlock_limbo_page, on);
}

// ---------------------------------------------------------------------------
// 固定随机数值 (僵尸移速 / 植物攻速)
//
// 依据:
//   game-src/Lawn/Zombie.cpp  Zombie::PickRandomSpeed()
//   game-src/Lawn/Plant.cpp   Plant::UpdateShooter()
// 字段偏移见 inc/data.h 的 ZOMBIE_SPEED_OFFSET / PLANT_* 常量。
// ---------------------------------------------------------------------------

// 分类名称与游戏内合法区间, 顺序与 ZombieSpeedClass 一致
//
// 注意: 这些区间来自 game-src 的反编译源码, 但那份源码的函数体出自 0.9.9 反编译,
// 常量与官方原版可能有出入 —— 实测普通僵尸能到 0.36 以上 (接近 0.37),
// 所以这里按实测把普通僵尸的上限改成 0.37。运行时还会用 ObservedSpeedRange()
// 学到实际区间并自动放宽, 不依赖这里的常量是否完全准确。
const ZombieSpeedClassInfo zombie_speed_classes[ZSC_COUNT] =
    {
        // ---- 可调整 (游戏里是随机区间), 顺序即界面显示顺序 ----
        {"普僵/路障/铁桶/铁门等", "Normal group", 0.23f, 0.37f, 0.30f},      //
        {"撑杆(持杆)", "Pole vaulting", 0.66f, 0.68f, 0.67f},                //
        {"撑杆(脱杆)", "Pole vaulting (no pole)", 0.23f, 0.37f, 0.30f},      //
        {"读报(看报)", "Newspaper (reading)", 0.23f, 0.37f, 0.30f},          //
        {"读报(暴怒)", "Newspaper (mad)", 0.89f, 0.91f, 0.90f},              //
        {"橄榄球", "Football", 0.66f, 0.68f, 0.67f},                         //
        {"潜水（岸上）", "Snorkel (on land)", 0.66f, 0.68f, 0.67f},           //
        {"冰车僵尸", "Zamboni", 0.10f, 0.25f, 0.20f},                        // 实测 0.25→0.10 (见下)
        {"雪橇小队(下车)", "Bobsled (on foot)", 0.23f, 0.37f, 0.30f},        //
        {"海豚骑士(骑行)", "Dolphin (riding)", 0.89f, 0.91f, 0.90f},         //
        {"海豚骑士(游泳)", "Dolphin (swimming)", 0.23f, 0.37f, 0.30f},       //
        {"小丑", "Jack-in-the-box", 0.66f, 0.68f, 0.67f},                    //
        {"气球(飞行/行走)", "Balloon", 0.23f, 0.37f, 0.30f},                 //
        {"矿工(未出土)", "Digger (tunneling)", 0.66f, 0.68f, 0.67f},         //
        {"扶梯(持梯)", "Ladder (carrying)", 0.79f, 0.81f, 0.80f},            //
        {"扶梯(脱梯)", "Ladder (no ladder)", 0.23f, 0.37f, 0.30f},           //
        {"投篮车", "Catapult", 0.23f, 0.37f, 0.30f},                         //
        {"巨人僵尸(白眼/红眼)", "Gargantuar (normal / Giga)", 0.23f, 0.37f, 0.30f},
        {"小鬼(一般)", "Imp (normal)", 0.23f, 0.37f, 0.30f},                 //
        {"倭瓜头", "Squash head", 0.79f, 0.81f, 0.80f},                      //

        // ---- 游戏固定值, 界面上只做说明 ----
        {"舞王/伴舞/旗帜/跳跳", "Dancer / Backup / Flag / Pogo", 0.45f, 0.45f, 0.45f},
        {"雪人(普通/暴走)", "Yeti (walking / running)", 0.40f, 0.40f, 0.40f, "0.40 / 0.80"},
        {"小鬼(IZ)", "Imp (I, Zombie)", 0.90f, 0.90f, 0.90f},  //
        {"雪橇小队(乘车)", "Bobsled (riding)", 0.60f, 0.60f, 0.60f},
        {"矿工(出土后, 一般/IZ)", "Digger (walking)", 0.12f, 0.12f, 0.12f, "0.12 / 0.23"},
        // 潜水僵尸入水那一刻被写死 0.20 (Zombie::UpdateZombieSnorkel, Zombie.cpp:1952-1958),
        // 之后在池子里这一段不会再 PickRandomSpeed, 所以游泳全程都是 0.20;
        // 上岸时 StartWalkAnim() 里会重新 PickRandomSpeed (Zombie.cpp:6799) 回到 0.66~0.68。
        {"潜水（游泳）", "Snorkel (swimming)", 0.20f, 0.20f, 0.20f},
};

// 从不保证对齐的字节缓冲里按字段类型取值
template <typename T>
static inline T object_field(const unsigned char *base, size_t offset)
{
    T value;
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}


// ZombieType 的**唯一一份**取值表在本文件中段 (ZOMBIE_FLAG … ZOMBIE_GIGA_GARGANTUAR),
//   ★ 2026-10-13: 这里原来又写了一份 `ZOMBIE_DIGGER = 17` (与上面那份同值、同样少 1) —— 已删,
//     避免"同一个种类两处各写一遍、改一处漏一处"。

// 不适合接管的状态: 位置固定, 或者速度由跳跃距离这类临时逻辑决定。
// 这些僵尸既不显示在移速列表里, 也不会被改写速度。
static bool zombie_speed_skip(int type, int phase)
{
    if (type == ZOMBIE_BUNGEE || type == ZOMBIE_BOSS) // 蹦极 / 僵王: 位置固定
        return true;

    if (phase == PHASE_ZOMBIE_DYING || phase == PHASE_ZOMBIE_BURNED || phase == PHASE_ZOMBIE_MOWERED)
        return true; // 死亡 / 烧焦 / 被小推车压过

    if (type == ZOMBIE_POGO && phase >= PHASE_POGO_HIGH_BOUNCE_1 && phase <= PHASE_POGO_FORWARD_BOUNCE_7)
        return true; // 高跳 / 前跳: 速度 = 跳跃距离 / 跳跃时间

    if (phase == PHASE_POLEVAULTER_IN_VAULT || phase == PHASE_DOLPHIN_IN_JUMP)
        return true; // 撑杆跳 / 海豚跳, 在空中

    if (type == ZOMBIE_IMP && (phase == PHASE_IMP_GETTING_THROWN || phase == PHASE_IMP_LANDING))
        return true; // 小鬼被抛出 / 落地

    return false;
}

// ---------------------------------------------------------------------------
// 植物攻速分类
//
// mLaunchRate 是定义表里写死的常量, 属于固定值, 不改动; 随机的是每发间隔的
// 抖动 mLaunchCounter = mLaunchRate - Sexy::Rand(15)。因为 Rand(15) 取 0..14,
// 所以各类的合法间隔区间就是 [mLaunchRate-14, mLaunchRate]:
//   150  -> 136~150  (豌豆/尖刺射手/孢子射手/大喷菇/杨桃)
//   200  -> 186~200  (忧郁菇)
//   300  -> 286~300  (投手类)
//   2500 -> 2350~2500 (产速: 向日葵/双子向日葵/阳光菇/灯笼草/金盏花)
// 地刺与大嘴花不走 mLaunchRate, 只做说明。
// ---------------------------------------------------------------------------
const PlantSpeedClassInfo plant_speed_classes[PSC_COUNT] =
    {
        // ---- 可调整 ----
        // 名称 / 英文 / 间隔下限 / 间隔上限 / 默认间隔 / 首发延迟下限 / 上限 (帧)
        // 首发延迟下限取 1 帧而不是 0: 设成 0 有概率把首发弹吞掉。
        // 首发延迟上限 = mLaunchRate(150) - 该分类里最大的"补发检查点":
        //   豌豆类含双发/裂荚(检查点 26) -> 150-26 = 124 (1.24 秒)
        //   尖刺射手类含猫尾草(检查点 50) -> 150-50 = 100 (1.00 秒)
        // 计数器不能超过 mLaunchRate, 而检查点又要占掉若干帧, 所以这是真实可达上限。
        // 下限 10 帧 (0.10 秒): 钉住需要一点余量, 否则我们两次写入之间计数器会
        // 被游戏减到 0 并重置, 钉不住 (详见 FixRandomValues 里的说明)。
        {"豌豆类（除双发、裂荚）", "Pea family (except Repeater/Split Pea)", 136, 150, 150, 5, 150},
        {"双发、裂荚", "Repeater / Split Pea", 136, 150, 150, 5, 124},
        {"猫尾草", "Cattail", 136, 150, 150, 5, 100},
        {"大喷菇、仙人掌", "Fume / Cactus", 136, 150, 150, 1, 150},
        {"杨桃", "Starfruit", 136, 150, 150, 0, 0}, // 首发延迟不参与, 只固定攻击间隔
        {"孢子射手类", "Spore shooters", 136, 150, 150, 5, 150},
        {"忧郁菇", "Gloom-shroom", 186, 200, 200, 5, 200},
        {"投手类", "Lobbers", 286, 300, 300, 5, 300},
        {"生产类", "Producers", 2350, 2500, 2500, 0, 0}, // 首发延迟对它没有意义, 不参与
        // ---- 不可调整, 只做说明 ----
        {"地刺类", "Spikeweed / Spikerock", 0, 0, 0, 0, 0},
        {"大嘴花", "Chomper", 0, 0, 0, 0, 0},
};

// SeedType 的数值 (game-src/ConstEnums.h)
enum : int
{
    SEED_PEASHOOTER = 0,
    SEED_SUNFLOWER = 1,
    SEED_SNOWPEA = 5,
    SEED_CHOMPER = 6,
    SEED_REPEATER = 7,
    SEED_PUFFSHROOM = 8,
    SEED_SUNSHROOM = 9,
    SEED_FUMESHROOM = 10,
    SEED_SCAREDYSHROOM = 13,
    SEED_THREEPEATER = 18,
    SEED_SPIKEWEED = 21,
    SEED_SEASHROOM = 24,
    SEED_PLANTERN = 25,
    SEED_CACTUS = 26,
    SEED_SPLITPEA = 28,
    SEED_STARFRUIT = 29,
    SEED_CABBAGEPULT = 32,
    SEED_KERNELPULT = 34,
    SEED_MARIGOLD = 38,
    SEED_MELONPULT = 39,
    SEED_GATLINGPEA = 40,
    SEED_TWINSUNFLOWER = 41,
    SEED_GLOOMSHROOM = 42,
    SEED_CATTAIL = 43,
    SEED_WINTERMELON = 44,
    SEED_SPIKEROCK = 46,
};

int PlantSpeedClassOf(int seed_type)
{
    switch (seed_type)
    {
    case SEED_PEASHOOTER:  // 豌豆射手
    case SEED_SNOWPEA:     // 寒冰射手
    case SEED_THREEPEATER: // 三线射手
    case SEED_GATLINGPEA:  // 机枪射手
        return PSC_PEA;

    case SEED_REPEATER: // 双发射手  (有补发检查点 25, 单独一类)
    case SEED_SPLITPEA: // 裂荚射手  (同上)
        return PSC_REPEATER;

    case SEED_FUMESHROOM: // 大喷菇
    case SEED_CACTUS:     // 仙人掌
        return PSC_MISC;

    case SEED_STARFRUIT: // 杨桃 (只参与攻击间隔)
        return PSC_STARFRUIT;

    case SEED_CATTAIL: // 猫尾草 (补发检查点 50, 单独一类)
        return PSC_CATTAIL;

    case SEED_PUFFSHROOM:    // 小喷菇
    case SEED_SCAREDYSHROOM: // 胆小菇
    case SEED_SEASHROOM:     // 海蘑菇
        return PSC_SPORE;

    case SEED_CABBAGEPULT: // 卷心菜投手
    case SEED_KERNELPULT:  // 玉米投手
    case SEED_MELONPULT:   // 西瓜投手
    case SEED_WINTERMELON: // 冰瓜投手
        return PSC_PULT;

    case SEED_GLOOMSHROOM: // 忧郁菇
        return PSC_GLOOM;

    case SEED_SUNFLOWER:      // 向日葵
    case SEED_SUNSHROOM:      // 阳光菇
    case SEED_PLANTERN:       // 灯笼草
    case SEED_MARIGOLD:       // 金盏花
    case SEED_TWINSUNFLOWER:  // 双子向日葵
        return PSC_SUN;

    case SEED_SPIKEWEED: // 地刺
    case SEED_SPIKEROCK: // 钢地刺
        return PSC_SPIKEWEED;

    case SEED_CHOMPER: // 大嘴花
        return PSC_CHOMPER;

    default:
        return -1; // 没有攻击节奏的植物 (坚果 / 花盆 / 加农炮 等)
    }
}

// 某些植物靠"计数器等于某个值"来补发, 所以设置首发延迟时要把这个检查点
// 加进初值里去, 让计数器一定会路过它:
//   双发 / 裂荚: mLaunchCounter == 25 时补发前一发
//   猫尾草:      mLaunchCounter == 50 时补发一发
//
// 偏移取的是该植物【出弹动画的长度 + 1】(FindTargetAndFire 里设的 mShootingCounter):
//   双发 / 裂荚 / 左向双发  mShootingCounter = 26
//   猫尾草                  mShootingCounter = 50
// 原因: 两个检查点写的是同一个 mShootingCounter, 如果第二个检查点在第一个动画
// 还没走完时就到来, 就会把还没出弹的计数覆盖掉 —— 表现为一轮少一颗。
// 取 26 (=25+1) 让检查点间隔比动画多 1 帧, 保证第一颗先出膛再排第二颗。
static int PlantWindupCheckpoint(int seed_type)
{
    switch (seed_type)
    {
    case SEED_REPEATER: // 双发射手
    case SEED_SPLITPEA: // 裂荚射手
        return 26;
    case SEED_CATTAIL: // 猫尾草
        return 50;
    default:
        return 0;
    }
}

// 给前面的 SetWindupHook 用 (它定义在这个静态函数之前)
int PlantWindupCheckpointOf(int seed_type) { return PlantWindupCheckpoint(seed_type); }

// 首发延迟用的"索敌行范围"(行差): 与游戏的 FindTargetZombie 一致 ——
//   0 = 只看本排 (大多数射手)
//   1 = 本排 ±1 (忧郁菇 3x3; 三线射手一次打三排)
//   7 = 任意排 (香蒲全场制导; 杨桃本就不参与首发延迟, 一并给全排以免误判)
int PlantWindupRowDevOf(int seed_type)
{
    switch (seed_type)
    {
    case SEED_GLOOMSHROOM:
    case SEED_THREEPEATER:
        return 1;
    case SEED_CATTAIL:
    case SEED_STARFRUIT:
        return 7;
    default:
        return 0;
    }
}

// 会打身后的植物 (目前只有裂荚射手): 身后窗口右边界相对 mX 的偏移。// 其它植物返回 0 —— 它们的攻击窗口左边界是 px+60, 身后的僵尸本来就不算进射程。
int PlantWindupBackHiOf(int seed_type)
{
    return (seed_type == SEED_SPLITPEA) ? 16 : 0;
}

// 僵尸碰撞箱表 (相对 mPosX 的偏移 + 宽度) —— 照抄核心 ZombieBodyX / 游戏 Zombie::GetBodyRect。
//   DLL 用它把僵尸身位换算成 [lo, hi] 与植物攻击矩形求交。
//   ★ 默认是 [x+36, x+78] —— 僵尸身体在 mPosX **右侧**; 早先 DLL 用 ±40 近似会整体偏左 ~40px,
//   对忧郁菇 (240 宽窄窗口) 这种窄射程植物误差致命 (用户实测: 只有忧郁菇判错)。
void FillZombieBodyTable(PtHook::Control *ctl)
{
    if (ctl == nullptr)
        return;
    for (int t = 0; t < 64; t++)
    {
        int rx = 36, rw = 42; // 默认 (普通僵尸)
        switch (t)
        {
        case ZOMBIE_BUNGEE: rx = -20; rw = 110; break;
        case ZOMBIE_FOOTBALL: rx = 50; rw = 57; break;
        case ZOMBIE_DIGGER: rx = 50; rw = 28; break;
        case ZOMBIE_GARGANTUAR:
        case ZOMBIE_GIGA_GARGANTUAR: rx = -17; rw = 125; break;
        case ZOMBIE_ZAMBONI:
        case ZOMBIE_CATAPULT: rx = 0; rw = 153; break;
        case ZOMBIE_SNORKEL: rx = 12; rw = 62; break;
        case ZOMBIE_BOBSLED: rx = -50; rw = 275; break;
        case ZOMBIE_BOSS: rx = 700; rw = 90; break;
        default: break;
        }
        ctl->z_body_rx[t] = rx;
        ctl->z_body_rw[t] = rw;
    }
}

// 攻击窗口 (相对植物 mX 的像素偏移) —— 直接复用游戏 Rect 的换算 PlantAttackX。
// 传 px = 0 得到的就是"相对偏移"本身。香蒲/杨桃是无限窗, 给一个大范围即可。
static void PlantAttackX(int seed_type, float px, float &lo, float &hi); // 定义在文件后面

void PlantWindupWindowOf(int seed_type, int &lo, int &hi)
{
    float f_lo = 0.0f, f_hi = 0.0f;
    PlantAttackX(seed_type, 0.0f, f_lo, f_hi);
    if (f_lo < -100000.0f)
        f_lo = -100000.0f;
    if (f_hi > 100000.0f)
        f_hi = 100000.0f;
    lo = static_cast<int>(f_lo);
    hi = static_cast<int>(f_hi);
}

// 写入"安全帧"所需的余量。
//
// 带检查点的植物 (双发/裂荚/猫尾草): 它们的检查点靠"计数器路过"触发, 而两个检查点
// 写的是同一个 mShootingCounter。如果我们在"刚开过火、计数器还很低"的时候写入一个
// >= 检查点的值, 计数器会立刻再路过一次检查点 —— 多打一颗, 表现为 2~4 帧的极短
// 间隔(子弹错步)。实测危险窗口是 L = 26~50, 安全区是 L > 52。
// 所以要求离检查点至少还有"一整个出弹动画"的距离。
//
// 其它植物没有检查点, 抬升计数器只是让首发更早, 不存在二次路过, 余量取最小即可。
static int PlantWindupMargin(int seed_type)
{
    switch (seed_type)
    {
    case SEED_REPEATER: // 双发射手 (mShootingCounter = 26)
    case SEED_SPLITPEA: // 裂荚射手 (26)
    case SEED_CATTAIL:  // 猫尾草 (50, 检查点本身也是 50)
        return 26;
    default:
        return 3;
    }
}

// 该植物"一轮里最后一次出弹"的出膛点 (mShootingCounter 的值)。
// 回 IDLE (重新钉住计数器) 之前必须等它过去, 否则一次检查排下的后续子弹
// 会被我们的钉住覆盖掉 —— 也就是"丢弹"。
//   忧郁菇: UpdateShooting 在 ==126/98/70/42 开火 (一轮 4 发)
//   机枪:   在 ==68/51/35/18 开火 (一轮 4 发)
//   猫尾草: 在 ==19 索敌并开火
//   其它:   在 ==1 开火; 没有射击动画的植物是立即 Fire
static int PlantLastFirePoint(int seed_type)
{
    switch (seed_type)
    {
    case SEED_GLOOMSHROOM:
        return 42;
    case SEED_GATLINGPEA:
        return 18;
    case SEED_CATTAIL:
        return 19;
    default:
        return 1;
    }
}

// 僵尸碰撞箱的 x 区间 (Zombie::mZombieRect, 见 game-src/Lawn/Zombie.cpp 的 ZombieInitialize)
//   mZombieRect 是**按僵尸类型写死的常量**, GetZombieRect() 再整体平移 (mPosX, mPosY + bodyY);
//   我们只关心 x 方向: [mPosX + rect.x, mPosX + rect.x + rect.w]。
//   默认值 Rect(36, 0, 42, 115) (Zombie.cpp:145), 各类型的覆盖值:
//     ZOMBIE_BUNGEE       Rect(-20, 22, 110, 94)     (Zombie.cpp:275)
//     ZOMBIE_FOOTBALL     Rect(50, 0, 57, 115)       (282)
//     ZOMBIE_DIGGER       Rect(50, 0, 28, 115)       (296)
//     ZOMBIE_GARGANTUAR   Rect(-17, -38, 125, 154)   (362)  红眼巨人同
//     ZOMBIE_ZAMBONI      Rect(0, -13, 153, 140)     (405)
//     ZOMBIE_CATAPULT     Rect(0, -13, 153, 140)     (422)
//     ZOMBIE_SNORKEL      Rect(12, 0, 62, 115)       (428)
//     ZOMBIE_BOBSLED      Rect(-50, 0, 275, 115)     (496)
//     ZOMBIE_BOSS         Rect(700, 80, 90, 430)     (680)
//   僵王特别说明: 它的 mPosX 恒为 0 (Zombie.cpp:678 是占位值), 而 mZombieRect 的 x=700
//   本来就是绝对坐标, 所以这里算出来正好是 [700, 790] —— 恰好是它的真实身位。
//   (以前工具把"0"当成僵王的横坐标, 于是它出生那一行的 min_x 被写成 0, 那一行所有植物
//    都被判成"前方没有僵尸"而永久钉住、不开火。)
static void ZombieBodyX(int type, float pos_x, float &lo, float &hi)
{
    float rx = 36.0f;
    float rw = 42.0f;
    switch (type)
    {
    case ZOMBIE_BUNGEE:      rx = -20.0f; rw = 110.0f; break;
    case ZOMBIE_FOOTBALL:    rx = 50.0f;  rw = 57.0f;  break;
    case ZOMBIE_DIGGER:      rx = 50.0f;  rw = 28.0f;  break;
    case ZOMBIE_GARGANTUAR:
    case ZOMBIE_GIGA_GARGANTUAR: rx = -17.0f; rw = 125.0f; break;
    case ZOMBIE_ZAMBONI:
    case ZOMBIE_CATAPULT:    rx = 0.0f;   rw = 153.0f; break;
    case ZOMBIE_SNORKEL:     rx = 12.0f;  rw = 62.0f;  break;
    case ZOMBIE_BOBSLED:     rx = -50.0f; rw = 275.0f; break;
    case ZOMBIE_BOSS:        rx = 700.0f; rw = 90.0f;  break;
    default: break;
    }
    lo = pos_x + rx;
    hi = lo + rw;
}

// 植物攻击矩形的 x 区间 (Plant::GetPlantAttackRect, game-src/Lawn/Plant.cpp:5248-5272)
//   px = 植物的 mX。PlantInitialize 里 mX = mBoard->GridToPixelX(col, row) (Plant.cpp:94),
//   也就是列中心 col*80 + 40 —— 与工具用列算出来的 px 完全一致, 所以可以直接套用。
//   默认分支 Rect(mX + 60, mY, BOARD_WIDTH, mHeight) -> 身前一大片;
//   忧郁菇 Rect(mX - 80, mY - 80, 240, 240) -> 身前身后各一小块 (这是它 240 宽的"网");
//   大喷菇 Rect(mX + 60, mY, 340, mHeight); 小喷菇 / 海蘑菇 Rect(mX + 60, mY, 230, mHeight);
//   猫尾草 Rect(-BOARD_WIDTH, -BOARD_HEIGHT, ...) -> 全屏;
//   杨桃走独立的 FindStarFruitTarget (角度公式太复杂, 这里按"全屏 + 全部行"处理, 宁宽勿窄)。
static void PlantAttackX(int seed_type, float px, float &lo, float &hi)
{
    float rx = 60.0f;
    float rw = 800.0f; // BOARD_WIDTH
    switch (seed_type)
    {
    case SEED_GLOOMSHROOM: rx = -80.0f; rw = 240.0f; break;
    case SEED_FUMESHROOM:  rx = 60.0f;  rw = 340.0f; break;
    case SEED_PUFFSHROOM:
    case SEED_SEASHROOM:   rx = 60.0f;  rw = 230.0f; break;
    case SEED_CATTAIL:
    case SEED_STARFRUIT:
        lo = -1.0e9f;
        hi = 1.0e9f;
        return;
    default: break;
    }
    lo = px + rx;
    hi = lo + rw;
}

// 判定"这棵植物的射程内有没有僵尸" —— 就是游戏 FindTargetZombie 的两个条件:
//   行: 其它植物只认本行; 忧郁菇行差 ±1 (Plant.cpp:4876-4882); 猫尾草没有行判定
//       (4874 的 if 把它排除); 杨桃走独立的 FindStarFruitTarget, 这里按全部行处理;
//       僵王由僵尸段算进了每一行 (4852-4855 强制 aRowDeviation = 0)。
//   相交: 僵尸碰撞箱 [row_lo, row_hi] 与植物攻击矩形 [win_lo, win_hi] 求重叠
//       (4950-4954 的 GetRectOverlap)。
//   额外: 裂荚的第二发用身后窗口 Rect(0, mY, mX + 16, mHeight) (Plant.cpp:5252-5255)。
// row_* 是每一行僵尸碰撞箱左边缘的最小值 / 右边缘的最大值, row_any 是"这一行有僵尸"。
// 用"最小左边缘 < 右边界 && 最大右边缘 > 左边界"来判"存在一个僵尸与之相交":
// 各僵尸碰撞箱宽度不同, 极端情况下可能把不同的僵尸凑成一对 (判宽了), 那也只是提前
// 脱手 —— 游戏自己的 FindTargetZombie 会再筛一次, 属于本工具一贯的"宁宽勿窄"。
static bool PlantHasTarget(int seed_type, int prow, int pcol, const float *row_lo,
                           const float *row_hi, const bool *row_any)
{
    const float px = pcol * 80.0f + 40.0f; // = 植物的 mX (Plant.cpp:94)

    float win_lo = 0.0f;
    float win_hi = 0.0f;
    PlantAttackX(seed_type, px, win_lo, win_hi);

    int rlo = prow;
    int rhi = prow;
    if (seed_type == SEED_GLOOMSHROOM || seed_type == SEED_THREEPEATER)
    {
        // 忧郁菇: 3x3 范围 -> 行差 ±1 (Plant.cpp:4876-4882)
        // 三线射手: 一次打三排 -> 上下各一排也要算 (老版漏了这条, 会让它在只有邻排有僵尸时
        //           被判定为"没目标", 于是首发延迟钉住时机不对)
        rlo = prow - 1;
        rhi = prow + 1;
    }
    else if (seed_type == SEED_CATTAIL || seed_type == SEED_STARFRUIT) // 任意行
    {
        rlo = 0;
        rhi = 7;
    }

    for (int r = rlo; r <= rhi; r++)
    {
        if (r < 0 || r >= 8 || !row_any[r])
            continue;
        if (row_lo[r] < win_hi && row_hi[r] > win_lo)
            return true;
    }

    // 裂荚的后头: 身后窗口 [0, px + 16] (只有本行)
    if (seed_type == SEED_SPLITPEA && prow >= 0 && prow < 8 && row_any[prow])
    {
        if (row_lo[prow] < px + 16.0f && row_hi[prow] > 0.0f)
            return true;
    }

    return false;
}

int ZombieSpeedClassOf(int zombie_type, int zombie_phase, bool iz_level)
{
    if (zombie_speed_skip(zombie_type, zombie_phase))
        return ZSC_SKIP;

    // 有两种以上状态的僵尸, 先按状态分开
    if (zombie_type == ZOMBIE_NEWSPAPER) // 看报 / 暴怒
        return (zombie_phase == PHASE_NEWSPAPER_MAD) ? ZSC_NEWSPAPER_MAD : ZSC_NEWSPAPER_READ;

    if (zombie_type == ZOMBIE_BOBSLED) // 乘车 / 下车
    {
        if (zombie_phase == PHASE_BOBSLED_SLIDING ||  //
            zombie_phase == PHASE_BOBSLED_BOARDING || //
            zombie_phase == PHASE_BOBSLED_CRASHING)
            return ZSC_BOBSLED_ON;

        return ZSC_BOBSLED_OFF;
    }

    if (zombie_type == ZOMBIE_BALLOON) // 飞行与行走合并成一项
        return ZSC_BALLOON;

    if (zombie_type == ZOMBIE_POGO) // 跳跳: 游戏里**任何状态**都是 0.45
    {
        // Zombie::PickRandomSpeed: ZOMBIE_POGO 与舞王/伴舞/旗帜同属 0.45 那一支, 与相位无关
        // (杆子被打断、落地行走之后也仍然是 0.45, 用户实测确认) -> 归到固定值那组, 不再单列可调行。
        return ZSC_DANCER;
    }

    // 下面的顺序必须与 Zombie::PickRandomSpeed() 的分支链一致: 先匹配先生效
    if (zombie_phase == PHASE_DOLPHIN_WALKING_IN_POOL)
        return ZSC_DOLPHIN_SWIM;

    if (zombie_phase == PHASE_DIGGER_WALKING) // 出土后: 一般 0.12 / IZ 0.23
        return ZSC_DIGGER_WALK;

    if (zombie_type == ZOMBIE_IMP && iz_level)
        return ZSC_IMP_IZ;

    if (zombie_phase == PHASE_YETI_RUNNING) // 暴走 0.80, 普通 0.40
        return ZSC_YETI;

    if (zombie_type == ZOMBIE_YETI)
        return ZSC_YETI;

    if (zombie_type == ZOMBIE_DANCER || zombie_type == ZOMBIE_BACKUP_DANCER || //
        zombie_type == ZOMBIE_FLAG)
        return ZSC_DANCER;

    // 游戏里这一组同区间, 这里按需求拆成独立的行
    if (zombie_phase == PHASE_DIGGER_TUNNELING)
        return ZSC_DIGGER_TUNNEL;

    if (zombie_phase == PHASE_POLEVAULTER_PRE_VAULT)
        return ZSC_POLE_VAULTING;

    if (zombie_type == ZOMBIE_FOOTBALL)
        return ZSC_FOOTBALL;

    if (zombie_type == ZOMBIE_SNORKEL)
    {
        // 池子里那一段: 入水瞬间被写死 0.20, 全程不再 PickRandomSpeed (Zombie.cpp:1952-1958);
        // 上岸时 StartWalkAnim() 会重新掷速度 (Zombie.cpp:6799) -> 回到 0.66~0.68 的可调整区间。
        if (zombie_phase >= PHASE_SNORKEL_INTO_POOL && zombie_phase <= PHASE_SNORKEL_DOWN_FROM_EAT)
            return ZSC_SNORKEL_SWIM;

        return ZSC_SNORKEL;
    }

    if (zombie_type == ZOMBIE_JACK_IN_THE_BOX)
        return ZSC_JACK;

    if (zombie_phase == PHASE_LADDER_CARRYING)
        return ZSC_LADDER_CARRY;

    if (zombie_type == ZOMBIE_SQUASH_HEAD)
        return ZSC_SQUASH_HEAD;

    // 海豚骑士"骑行"相关的相位: 游戏只在 WALKING_IN_POOL(55) 时改速度为 0.3
    // (Zombie::PickRandomSpeed), 入水/骑行/跃起(52/53/54) 都沿用陆地那次掷的 0.89~0.91。
    // ★ 52/53/54 曾经没有分支 -> 落到 ZSC_NORMAL, 于是"海豚骑行"被"普僵"那行控制 (用户实测)。
    if (zombie_phase == PHASE_DOLPHIN_WALKING || zombie_phase == PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN ||
        zombie_phase == PHASE_DOLPHIN_INTO_POOL || zombie_phase == PHASE_DOLPHIN_RIDING ||
        zombie_phase == PHASE_DOLPHIN_IN_JUMP)
        return ZSC_DOLPHIN_RIDE;

    if (zombie_type == ZOMBIE_POLEVAULTER) // 撑杆(脱杆)
        return ZSC_POLE_AFTER;

    if (zombie_type == ZOMBIE_LADDER) // 扶梯(脱梯)
        return ZSC_LADDER_AFTER;

    if (zombie_type == ZOMBIE_CATAPULT) // 投篮车: 投篮不影响速度值, 不区分状态
        return ZSC_CATAPULT;

    if (zombie_type == ZOMBIE_GARGANTUAR || zombie_type == ZOMBIE_GIGA_GARGANTUAR)
        return ZSC_GARGANTUAR;

    if (zombie_type == ZOMBIE_IMP) // 小鬼(一般关卡)
        return ZSC_IMP_NORMAL;

    if (zombie_type == ZOMBIE_ZAMBONI) // 冰车
        return ZSC_ZAMBONI;

    // 其余只有一种状态的僵尸 (普僵 / 路障 / 铁桶 / 铁门 / 鸭子 / 各种植物僵尸)
    // 区间相同, 合并成一组
    return ZSC_NORMAL;
}

bool PvZ::RandomFixSupported()
{
#ifdef _PVZ_BETA_LEAK_SUPPORT
    // 两个泄露测试版的 Zombie / Plant 布局与正式版不同, 不支持该功能
    if (isBETA())
        return false;
#endif
    return true;
}

// 是否处于解谜模式"我是僵尸"
static bool is_izombie_level(int game_mode)
{
    return game_mode >= GAME_MODE_IZOMBIE_FIRST && game_mode <= GAME_MODE_IZOMBIE_LAST;
}

bool PvZ::FixRandomValues(const std::array<float, ZSC_COUNT> &zombie_value,
                          const std::array<bool, ZSC_COUNT> &zombie_enabled, //
                          const std::array<float, PSC_COUNT> &plant_interval,
                          const std::array<bool, PSC_COUNT> &plant_enabled,
                          const std::array<float, PSC_COUNT> &windup_sec,
                          const std::array<bool, PSC_COUNT> &windup_enabled,
                          std::vector<ZombieSpeedInfo> &out_zombie, //
                          std::vector<PlantSpeedInfo> &out_plant)
{
    out_zombie.clear();
    out_plant.clear();

    if (!RandomFixSupported() || !GameOn())
        return false;

    int ui = GameUI();
    if (ui != 2 && ui != 3)
        return false;

    const bool iz_level = is_izombie_level(GameMode());

    // ---------------- 僵尸移速 ----------------

    // 每一行僵尸的"身位区间" [lo, hi] (左边缘的最小值 / 右边缘的最大值), 用来做
    // "这一行里是否存在一个僵尸, 它的碰撞箱与植物攻击矩形相交" 的判定 —— 与游戏
    // FindTargetZombie 的做法一致 (先按矩形筛, 再取最靠左的那个当目标)。
    // 以前只存"最靠左僵尸的 mPosX", 有两个毛病:
    //   1) 只要有一个僵尸已经走到植物左边, 同一行所有植物都会被判成"前方没有僵尸",
    //      于是被永久钉住、不打前面还在推进的僵尸;
    //   2) 僵王的 mPosX 是 0 (占位值), 会把那一行的 min_x 写成 0, 整行同样遭殃。
    // lo/hi 的初始值互为倒置, 用 zombie_row_any[] 单独记"这一行有没有僵尸"。
    float zombie_row_lo[8];
    float zombie_row_hi[8];
    bool zombie_row_any[8];
    for (int r = 0; r < 8; r++)
    {
        zombie_row_lo[r] = 1.0e9f;
        zombie_row_hi[r] = -1.0e9f;
        zombie_row_any[r] = false;
    }

    const size_t zombie_struct_size = isGOTY() ? 0x168 : 0x15c;
    const uintptr_t zombie_base = ReadMemory<uintptr_t>({data().lawn, data().board, data().zombie});
    uint32_t zombie_count = ReadMemory<uint32_t>({data().lawn, data().board, data().zombie_count_max});

    if (zombie_base != 0 && zombie_count != 0)
    {
        if (zombie_count > 4096)
            zombie_count = 4096;

        // 缓冲只增不减, 避免每帧反复分配
        const size_t zombie_bytes = zombie_struct_size * zombie_count;
        if (object_buffer.size() < zombie_bytes)
            object_buffer.resize(zombie_bytes);

        if (ReadBytes(object_buffer.data(), zombie_base, zombie_bytes))
        {
            const size_t dead_off = static_cast<size_t>(data().zombie_dead);
            const size_t phase_off = static_cast<size_t>(data().zombie_status);

            if (zombie_chill_pending.size() < zombie_count)
                zombie_chill_pending.resize(zombie_count, 0);

            // (每一行最靠左的存活僵尸在上面已经统计好)

            for (uint32_t i = 0; i < zombie_count; i++)
            {
                const unsigned char *z = object_buffer.data() + zombie_struct_size * i;
                const uintptr_t addr = zombie_base + zombie_struct_size * i;

                if (z[dead_off] != 0) // 已经消失的槽位跳过
                {
                    zombie_chill_pending[i] = 0;
                    continue;
                }

                int type = object_field<int>(z, ZOMBIE_TYPE_OFFSET);
                int phase = object_field<int>(z, phase_off);
                float speed = object_field<float>(z, ZOMBIE_SPEED_OFFSET);

                // 统计每一行的僵尸身位 (死亡 / 烧焦 / 被压过的不算)
                if (phase != PHASE_ZOMBIE_DYING && phase != PHASE_ZOMBIE_BURNED && phase != PHASE_ZOMBIE_MOWERED)
                {
                    const int zrow = object_field<int>(z, ZOMBIE_ROW_OFFSET);
                    const float zx = object_field<float>(z, ZOMBIE_POS_X_OFFSET);
                    if (zx > -1000.0f && zx < 800.0f) // 还没走进草坪的僵尸先不算
                    {
                        float zlo = 0.0f;
                        float zhi = 0.0f;
                        ZombieBodyX(type, zx, zlo, zhi);

                        if (type == ZOMBIE_BOSS)
                        {
                            // 僵王: 游戏里 FindTargetZombie 对它强制 aRowDeviation = 0
                            // (Plant.cpp:4852-4855), 也就是**任何一行都能索敌它**。
                            for (int r = 0; r < 8; r++)
                            {
                                zombie_row_any[r] = true;
                                if (zlo < zombie_row_lo[r])
                                    zombie_row_lo[r] = zlo;
                                if (zhi > zombie_row_hi[r])
                                    zombie_row_hi[r] = zhi;
                            }
                        }
                        else if (zrow >= 0 && zrow < 8)
                        {
                            zombie_row_any[zrow] = true;
                            if (zlo < zombie_row_lo[zrow])
                                zombie_row_lo[zrow] = zlo;
                            if (zhi > zombie_row_hi[zrow])
                                zombie_row_hi[zrow] = zhi;
                        }
                    }
                }

                int klass = ZombieSpeedClassOf(type, phase, iz_level);
                int chilled = object_field<int>(z, ZOMBIE_CHILLED_COUNTER_OFFSET);

                // 上一次为了让游戏重算动画速率而置的冰冻标记, 在这里收尾
                if (zombie_chill_pending[i] != 0)
                {
                    if (chilled <= 0)
                    {
                        zombie_chill_pending[i] = 0; // 游戏跑过 UpdatePlaying, 已经重算过了
                    }
                    else if (--zombie_chill_pending[i] == 0)
                    {
                        // 超时: 游戏没有处理这个僵尸, 撤销标记, 别让它一直保持冰冻
                        int zero = 0;
                        WriteBytes(&zero, addr + ZOMBIE_CHILLED_COUNTER_OFFSET, sizeof(zero));
                        chilled = 0;
                    }
                }

                if (klass == ZSC_SKIP) // 不统计也不调整 (蹦极 / 僵王 / 跳跃中 / 已死亡 ...)
                    continue;

                // 固定值分类不参与调整, 也不参与区间学习
                const bool adjustable = (klass < ZSC_ADJUSTABLE_COUNT);

                // 该分类没被固定时, 读到的就是游戏自己随机出来的值 —— 用它学习真实区间。
                // 只取普通行走状态 (相位 0), 避免把特殊状态的临时速度混进分类区间。
                if (adjustable && !zombie_enabled[klass] && phase == 0 && //
                    speed >= 0.05f && speed <= 10.0f)
                {
                    if (speed < zombie_observed_min[klass])
                        zombie_observed_min[klass] = speed;
                    if (speed > zombie_observed_max[klass])
                        zombie_observed_max[klass] = speed;
                }

                if (adjustable && zombie_enabled[klass])
                {
                    float want = zombie_value[klass];
                    if (speed != want)
                    {
                        WriteBytes(&want, addr + ZOMBIE_SPEED_OFFSET, sizeof(want));
                        speed = want;

                        // 关键一步:
                        // 僵尸的实际位移来自动画轨道速度 (_ground 轨道), 而它正比于 mVelX;
                        // 但游戏只在 Zombie::UpdateAnimSpeed() 里由 mVelX 反推动画速率,
                        // 那个函数平时不会调用 (只在初始化 / 换阶段 / 冰冻结束等时机)。
                        // 所以只改 mVelX 的话, 界面显示的数值是统一的, 实际速度却还是
                        // 最初随机出来的动画速率 —— 这正是"显示一样、表现不同"的原因。
                        //
                        // Zombie::UpdatePlaying 里 mChilledCounter 从 1 减到 0 时会调用
                        // UpdateAnimSpeed(), 且没有其它副作用, 所以这里把计数器置 1,
                        // 让游戏下一帧自己按我们固定的 mVelX 重算动画速率。
                        // 相位 1/2/3 (死亡 / 烧焦 / 被小推车压过) 不会跑 UpdatePlaying, 跳过。
                        bool playing_phase = (phase != 1 && phase != 2 && phase != 3);
                        if (playing_phase && chilled == 0)
                        {
                            int one = 1;
                            if (WriteBytes(&one, addr + ZOMBIE_CHILLED_COUNTER_OFFSET, sizeof(one)))
                            {
                                zombie_chill_pending[i] = 10; // 最多等 10 个刷新周期
                                chilled = 1;
                            }
                        }
                    }
                }

                out_zombie.push_back({static_cast<int>(i), type, klass, speed, adjustable && zombie_enabled[klass]});
            }
        }
    }

    // ---------------- 植物攻速 ----------------
    //
    // 只固定随机抖动, 不改写 mLaunchRate (它是定义表里的固定值)。
    // 游戏开火后执行 mLaunchCounter = mLaunchRate - Sexy::Rand(15), 计数器平时
    // 单调递减, 所以"比上次观测值大"就说明刚被随机重置, 这时把它改写成用户
    // 设定的确定间隔即可。界面上给的是"每秒攻击次数", 这里换算回帧数。

    // 首发延迟: 设置 (开关或数值) 一变, 就对场上所有同类植物重写一次计数器 ——
    // 这样已经种下去的老植物也能改 (生产类已从首发延迟里移除, 所以不会误伤产出节奏)。
    for (int k = 0; k < PSC_ADJUSTABLE_COUNT; k++)
    {
        const PlantSpeedClassInfo &info = plant_speed_classes[k];

        int want = -1;
        if (windup_enabled[k] && info.first_max > 0)
        {
            want = static_cast<int>(std::lround(windup_sec[k] * 100.0));
            if (want < info.first_min)
                want = info.first_min;
            if (want > info.first_max)
                want = info.first_max;
        }

        windup_reapply[k] = (want != windup_applied[k]);
        windup_applied[k] = want;
    }

    const size_t plant_struct_size = 0x14c;
    const uintptr_t plant_base = ReadMemory<uintptr_t>({data().lawn, data().board, data().plant});
    uint32_t plant_count = ReadMemory<uint32_t>({data().lawn, data().board, data().plant_count_max});

    if (plant_base != 0 && plant_count != 0)
    {
        if (plant_count > 4096)
            plant_count = 4096;

        if (plant_last_launch_counter.size() < plant_count)
            plant_last_launch_counter.resize(plant_count, 0);
        if (plant_last_interval.size() < plant_count)
            plant_last_interval.resize(plant_count, 0);
        if (plant_was_dead.size() < plant_count)
            plant_was_dead.resize(plant_count, 1);
        if (plant_last_seed.size() < plant_count)
            plant_last_seed.resize(plant_count, -1);
        if (plant_had_target.size() < plant_count)
            plant_had_target.resize(plant_count, 0);
        if (windup_no_target.size() < plant_count)
            windup_no_target.resize(plant_count, 0);
        // 默认 IDLE(1): 新槽位/新植物一开始就钉住, 等僵尸出现才脱手
        if (plant_windup_idle.size() < plant_count)
            plant_windup_idle.resize(plant_count, 1);
        if (plant_last_shooting.size() < plant_count)
            plant_last_shooting.resize(plant_count, 0);
        if (windup_written.size() < plant_count)
            windup_written.resize(plant_count, -1);
        if (plant_fired_cycle.size() < plant_count)
            plant_fired_cycle.resize(plant_count, 0);
        if (windup_arm_tick.size() < plant_count)
            windup_arm_tick.resize(plant_count, -1000);
        if (windup_dump_state.size() < plant_count)
            windup_dump_state.resize(plant_count);

        this->windup_tick++;

        // 诊断日志: 首次进入时打开 (PTK_WINDUP_DEBUG = 0 时不产生这个文件)
#if PTK_WINDUP_DEBUG
        if (this->windup_dump == nullptr)
        {
            this->windup_dump = std::fopen("pvztoolkit_windup_log.txt", "w");
            if (this->windup_dump != nullptr)
                std::fprintf(this->windup_dump,
                             "帧号  事件   编号 种类      数据\n"
                             "--------------------------------------------------\n");
        }
#endif

        // 缓冲只增不减, 避免每帧反复分配
        const size_t plant_bytes = plant_struct_size * plant_count;
        if (object_buffer.size() < plant_bytes)
            object_buffer.resize(plant_bytes);

        if (ReadBytes(object_buffer.data(), plant_base, plant_bytes))
        {
            // 整类写入: 用上一拍的"有无不安全植物"结论, 本拍重新统计
            windup_blocked_prev = windup_blocked;
            windup_pending_prev = windup_pending; // 边沿当帧不写, 等一拍让安全判定生效
            windup_blocked.fill(false);
            bool windup_did_write[PSC_COUNT] = {};

            const size_t dead_off = static_cast<size_t>(data().plant_dead);
            const size_t squished_off = static_cast<size_t>(data().plant_squished);
            const size_t type_off = static_cast<size_t>(data().plant_type);
            const size_t row_off = static_cast<size_t>(data().plant_row);
            const size_t col_off = static_cast<size_t>(data().plant_col);

            for (uint32_t i = 0; i < plant_count; i++)
            {
                const unsigned char *p = object_buffer.data() + plant_struct_size * i;

                const bool was_dead = (plant_was_dead[i] != 0);

                if (p[dead_off] != 0 || p[squished_off] != 0)
                {
                    plant_was_dead[i] = 1;
                    plant_last_seed[i] = -1;
                    plant_last_launch_counter[i] = 0;
                    plant_last_interval[i] = 0;
                    continue;
                }

                plant_was_dead[i] = 0;

                const int seed = object_field<int>(p, type_off);
                const bool is_new_plant = was_dead || (plant_last_seed[i] != seed);
                plant_last_seed[i] = seed;

                const int klass = PlantSpeedClassOf(seed);

                if (klass < 0) // 没有攻击节奏的植物不列入
                {
                    plant_last_launch_counter[i] = 0;
                    plant_last_interval[i] = 0;
                    continue;
                }

                int counter = object_field<int>(p, PLANT_LAUNCH_COUNTER_OFFSET);
                // 必须记住"读到的值", 而不是我们写进去的值 —— 否则下一帧游戏的
                // 真实值 (~139) 会大于我们写的值 (例如 1), 于是每帧都被误判成
                // "刚重置", 重试分支就会每帧重写一次, 植物永远走不完一个周期。
                const int counter_read = counter;

                // 回读校验: 上一次写入的下一帧, 内存里到底是什么值 —— 直接证明写入
                // 有没有被游戏覆盖 (若仍是写入前的值, 说明被覆盖了)
                // 回读校验: 上一次写入的下一帧内存里到底是什么值。
                // 注意不要每行 fflush —— 每帧一条 fprintf+fflush 会把工具从 100Hz
                // 拖到 ~10Hz, 进而让钉住失效。交给文件缓冲即可。
                if (this->windup_dump != nullptr && windup_arm_tick[i] == this->windup_tick - 1)
                {
                    std::fprintf(this->windup_dump, "%6d CHK   #%-3d seed=%-3d 写后1帧 L=%-4d S=%d\n",
                                 this->windup_tick, i, seed, counter,
                                 object_field<int>(p, PLANT_SHOOTING_COUNTER_OFFSET));
                }
                const bool adjustable = (klass < PSC_ADJUSTABLE_COUNT);
                const bool pinned = adjustable && plant_enabled[klass];
                const PlantSpeedClassInfo &info = plant_speed_classes[klass];

                // 射程内是否有僵尸: 同一行、且在植物前方 (留 40 像素余量)。
                // 用"上一帧没有、这一帧有"的跳变来触发首发延迟 —— 这就是持久化:
                // 只要僵尸进入射程, 植物就会按固定延迟反应, 而不是只有种下那一次。
                const int prow = object_field<int>(p, row_off);
                const int pcol = object_field<int>(p, col_off);

                // ---- "射程内是否有僵尸": 照游戏 FindTargetZombie 的行判定 + 矩形相交 ----
                // 以前是"植物右边 40 像素以外全算", 于是忧郁菇这种攻击矩形只有 240 宽
                // (身前 160 / 身后 80, 源码 Plant.cpp:5268) 的植物会在僵尸还隔着好几格时
                // 就判"进射程", 首发延迟被提前消耗掉 —— 表现就是忧郁菇的首发延迟不准。
                // 现在直接用僵尸碰撞箱 (ZombieBodyX) 与植物攻击矩形 (PlantAttackX) 相交,
                // 细节与"宁宽勿窄"的理由都写在 PlantHasTarget 上面。
                const bool has_target = PlantHasTarget(seed, prow, pcol, zombie_row_lo, zombie_row_hi,
                                                       zombie_row_any);

                // 正在播放攻击动画 (mShootingCounter != 0) 时不要重置:
                // 大喷菇 / 忧郁菇的动画很长, 中途重置会把动画打断 (看起来像卡住);
                // 对其它植物也能避免"刚开火又被重新计时"造成的吞弹。
                const int shooting_now = object_field<int>(p, PLANT_SHOOTING_COUNTER_OFFSET);
                // 进射程的写入只在"安全帧"做: L 要高于该植物的检查点一定余量。
                // 否则写入可能正好落在游戏要执行检查点 (== 25 / == 0) 的那几帧里,
                // 那一 tick 游戏先按旧节奏动过了, 我们晚一步 —— 表现就是
                // "L 过低时首轮少弹甚至吞弹"。不安全时不消费边沿, 下一帧再试。
                const bool had_target_before = (plant_had_target[i] != 0);
                // 安全帧 = 三点同时成立:
                //   1) L 高于"检查点 + 余量" —— 不会二次路过检查点 (PlantWindupMargin)
                //   2) mShootingCounter == 0     —— 没有"待出膛"的子弹
                // 第 2 条至关重要: FindTargetAndFire 只设 mShootingCounter, 子弹要等
                // UpdateShooting 减到 0 才真正 Fire()。若在动画进行中改写 L, 那颗待出膛的
                // 子弹照旧出来, 接着新周期的检查点又排一颗 → 两发挤在一起 (实测 2~4 帧),
                // 表现为"子弹错步"(一轮 1 颗或 3 颗)。日志证据: 六棵植物在完全相同的帧
                // 出错(3133 / 4309), 说明是共同的外部写入造成的, 而不是各自动作。
                const bool shooting_busy = (object_field<int>(p, PLANT_SHOOTING_COUNTER_OFFSET) != 0);
                const bool can_arm_now = (plant_windup_idle[i] != 0) &&
                                         (counter > PlantWindupCheckpoint(seed) + PlantWindupMargin(seed)) &&
                                         !shooting_busy;

                // ---- 首发延迟: 每棵植物独立的 IDLE / ACTIVE 状态机 ----
                // IDLE  : 射程内没有僵尸 (或植物在睡觉) -> 每帧把计数器钉住
                // ACTIVE: 射程内有僵尸 -> 完全不写, 让计数器自然流动
                //
                // 好处: "僵尸出现"到"第一次检查"之间正好是设定的延迟 (不需要
                // 猜测植物当前走到哪儿), 而且我们绝不在植物攻击过程中插手,
                // 因此不会覆盖"待出膛"的子弹、不会打断忧郁菇/机枪的一轮多发改击。
                // 不用类级绑定: 某一行没僵尸不应该把别的行的同类也钉住。
                const bool asleep_now =
                    (object_field<unsigned char>(p, static_cast<size_t>(data().plant_asleep)) != 0);

                if (has_target)
                {
                    windup_no_target[i] = 0;
                    plant_windup_idle[i] = 0; // 脱手, 立刻开始计时
                }
                else
                {
                    if (windup_no_target[i] < 1000)
                        windup_no_target[i]++;
                    if (windup_no_target[i] >= 30) // 连续 30 帧没目标才算真的离开
                    {
                        // ★ 不丢弹闸: 还要确认"本轮最后一发已经出膛"才能回 IDLE。
                        // 否则僵尸刚死、植物还有待出膛的子弹时, 我们一钉住就把那颗
                        // 覆盖掉了 (忧郁菇/机枪一轮 4 发, 中间被覆盖就少好几颗)。
                        const int shooting_left = object_field<int>(p, PLANT_SHOOTING_COUNTER_OFFSET);
                        if (shooting_left == 0 || shooting_left <= PlantLastFirePoint(seed))
                            plant_windup_idle[i] = 1; // 重新钉住
                    }
                }

                if (asleep_now) // 睡着时无论有无僵尸都钉住 (醒来时间不确定)
                    plant_windup_idle[i] = 1;

                // 本帧是否需要钉住: IDLE 状态就每帧写 (这就是新方案的全部写入条件)
                const bool target_entered = adjustable && info.first_max > 0 && windup_enabled[klass] &&
                                            plant_windup_idle[i] != 0;

                // ---------------- 首发延迟 (前摇) ----------------
                // 游戏在 PlantInitialize() 里给第一次攻击掷出一个等待时间
                // (发射类 RandRangeInt(0, mLaunchRate), 生产类 RandRangeInt(300, mLaunchRate/2)),
                // 之后由 UpdateShooter()/产出逻辑递减, 减到 0 那一下就是"下一次检查",
                // 这时才真正开火。所以把计数器改小 = 第一发更早, 改成 0 = 立刻。
                //
                // 两种情况要写:
                //   1. 设置刚变化 -> 对场上所有同类植物重写一次 (用户打开开关想立刻看到效果);
                //   2. 槽位刚从"空"变成"有植物" -> 新种下的, 按设定值初始化。
                int diag_wrote = -1;
                int diag_reason = 0;

                if (adjustable && info.first_max > 0)
                {
                    // 游戏自己的信号: FindTargetAndFire / LaunchStarFruit 只有在真的
                    // 索敌成功时才会把 mShootingCounter 置为非 0。所以"计数器刚被重置
                    // 但 mShootingCounter 仍是 0" = 这次检查白费了 (杨桃的向后索敌、
                    // 裂荚的前后索敌特别容易这样)。这时重新按固定延迟排一次, 植物就会
                    // 持续重试直到真的打出去 —— 不需要复刻各植物不同的索敌范围。
                    const int shooting_now = object_field<int>(p, PLANT_SHOOTING_COUNTER_OFFSET);
                    const bool counter_reset = (plant_last_launch_counter[i] != 0 && counter > plant_last_launch_counter[i]);

                    // mShootingCounter 每帧递减, 所以"变大"= 刚开始一次新动画 = 这次检查
                    // 真的打出去了。不能只看 != 0: 裂荚两个头共用同一个计数器, 前头的动画
                    // 会让后头索敌失败时也被误判成"打出去了", 于是漏掉重试、白等一轮。
                    const bool fired_now = (shooting_now > plant_last_shooting[i]);
                    plant_last_shooting[i] = shooting_now;

                    // 本周期是否开过火: 裂荚前头在 == 25 检查点开火, 归零那一帧只有
                    // 后头在索敌。若只看归零那一下, 前方有僵尸而后方没有时会被误判成
                    // "整轮打空了", 补写成 26 后前头立刻又打一发 —— 变成快速连吐。
                    const bool fired_this_cycle = (plant_fired_cycle[i] != 0);
                    if (fired_now)
                        plant_fired_cycle[i] = 1;

                    // 诊断日志: 记录每一次"真的打出去了", 并结算与上一次进射程的间隔。
                    // 同一时刻附近出现的多条 FIRE 就是"首轮弹数"; S 值能区分是哪个头
                    // (裂荚后头 = 26, 前头约 33)。
                    if (fired_now && this->windup_dump != nullptr)
                    {
                        WindupDumpState &d = this->windup_dump_state[i];
                        if (d.arm_frame >= 0 && d.first_fire < 0)
                        {
                            const int delta = this->windup_tick - d.arm_frame;
                            d.first_fire = this->windup_tick;
                            d.volley = 1;
                            std::fprintf(this->windup_dump,
                                         "%6d FIRE  #%-3d seed=%-3d S=%-3d L=%-4d <- 距ARM %d 帧 (%.2f 秒) %s\n",
                                         this->windup_tick, i, seed, shooting_now, counter_read, delta,
                                         delta / 100.0, (delta >= 136) ? "<<< 首轮吞弹 (>=1.36秒)" : "OK");
                        }
                        else
                        {
                            std::fprintf(this->windup_dump, "%6d FIRE  #%-3d seed=%-3d S=%d\n",
                                         this->windup_tick, i, seed, shooting_now);
                        }
                        std::fflush(this->windup_dump);
                    }

                    int target_f = (windup_applied[klass] >= 0) ? windup_applied[klass] : info.first_min;

                    // 把该植物的"补发检查点"加进初值, 保证一定能路过它
                    target_f += PlantWindupCheckpoint(seed);

                    // 不能超过植物自己的 mLaunchRate: 游戏会把越界的 mLaunchCounter
                    // 按 mLaunchRate - Rand(15) 重置, 于是我们每写一次就被清一次、
                    // 补写又触发, 形成死循环 (实测日志里每帧重复 "重试 -> 写后1帧=148~150",
                    // 一发都打不出去)。
                    // 上限刚好等于最大可设的首发延迟: 写 mLaunchRate(150) 时
                    // 25 检查点在 125 帧后、再 26 帧出膛 = 151 帧 ≈ 1.50 秒。
                    const int launch_rate = object_field<int>(p, PLANT_LAUNCH_RATE_OFFSET);
                    if (launch_rate > 0 && target_f > launch_rate)
                        target_f = launch_rate;

                    // 钉住的值不能贴地: 我们的 100Hz 与游戏 100Hz 不同相, 两次写入之间
                    // 游戏会减 1~3 帧。实测写 1/3 都撑不住 (计数器会走到 0, 游戏就执行
                    // 检查并重置成 ~140, 钉不住), 5 帧 (0.05 秒) 才稳定生效。
                    if (target_f < 5)
                        target_f = 5;

                    bool do_write = false;
                    int reason = 0;

                    // 安全帧保护 (can_arm_now = 当前 L 高于检查点 + 3) 用于"补写"与
                    // "进射程": 如果植物已经路过检查点, 再写一个 >= 检查点的值就会让它
                    // 【二次路过】那个检查点 —— 对双发/裂荚就是多打一颗 (3 颗), 或者把
                    // 子弹挤到下一轮 (1 颗)。
                    //
                    // 但"新种下"不受此限: 刚种下的植物还没走完任何周期, 初始计数器是
                    // RandRangeInt(0, mLaunchRate) 的随机值, 只会往下走, 二次路过在
                    // 物理上不可能。若也加保护, 初始值偏低 (约 1/5 概率) 的植物就完全
                    // 不被改写, 保留游戏那个很短的初始等待 —— 表现为"僵尸在场时种下,
                    // 部分植物立刻攻击"。所以这一条必须无条件生效。
                    //
                    // "改设置"同样不保护: 用户主动操作, 要保持立即生效。
                    if (windup_reapply[klass] && windup_applied[klass] >= 0)
                    {
                        do_write = true; // 设置刚变化: 场上老植物也改
                        reason = 4;
                    }
                    else if (windup_enabled[klass] && is_new_plant)
                    {
                        do_write = true; // 新种下的: 无条件按设定值初始化
                        reason = 1;
                    }
                    else if (windup_enabled[klass] && target_entered)
                    {
                        do_write = true;
                        reason = 2;
                    }
                    else if (windup_enabled[klass] && windup_applied[klass] >= 0 && can_arm_now &&
                             windup_written[i] >= 0 && counter_read < windup_written[i] - 6)
                    {
                        // 补写: 只有"计数器被拉到比我们写的值还低"才叫被游戏覆盖。
                        //
                        // 历史教训: 早先用过 (a) counter_read > written + 3 只能抓"被改大"
                        // (写 176 被压成 148 时抓不到); (b) 换成"不在预期衰减窗口就补",
                        // 结果把"植物正常开火后游戏把计数器重置回 ~140"也当成被覆盖 ——
                        // 写 1 时 140 既不在窗口内、下一帧(138)又不算 counter_reset,
                        // 于是每轮补写一次, 表现为同类植物稳定连发 (日志里"重试"刷屏)。
                        // 现在只判方向: 比我们写的值低 = 被压下去了 = 被覆盖。
                        do_write = true;
                        reason = 3;
                    }

                    if (do_write)
                    {
                        const int before_val = counter;
                        counter = target_f;
                        WriteBytes(&counter, plant_base + plant_struct_size * i + PLANT_LAUNCH_COUNTER_OFFSET,
                                   sizeof(counter));
                        diag_wrote = target_f;
                        diag_reason = reason;
                        windup_written[i] = target_f;

                        WindupWriteLog entry{static_cast<int>(i), seed, reason, before_val, target_f};
                        this->windup_log.insert(this->windup_log.begin(), entry);
                        if (this->windup_log.size() > 8)
                            this->windup_log.resize(8);

                        // ---- 诊断日志 ----
                        if (this->windup_dump != nullptr)
                        {
                            const char *rname = (reason == 1) ? "新种" : (reason == 2) ? "进射程"
                                                : (reason == 3) ? "重试" : "改设置";
                            // 每帧写入也记录 (性能问题已由"不强制刷盘"解决)
                            std::fprintf(this->windup_dump,
                                         "%6d ARM   #%-3d seed=%-3d %s L=%d -> %d S=%d\n",
                                         this->windup_tick, i, seed, rname, before_val, target_f,
                                         object_field<int>(p, PLANT_SHOOTING_COUNTER_OFFSET));
                            windup_arm_tick[i] = this->windup_tick;
                            if (reason == 2) // 只有"进射程"才算一次首轮测量的起点
                            {
                                WindupDumpState &d = this->windup_dump_state[i];
                                d.arm_frame = this->windup_tick;
                                d.first_fire = -1;
                                d.volley = 0;
                                d.written = target_f;
                                d.reported = false;
                            }
                            std::fflush(this->windup_dump);
                        }
                    }

                    // 读到的时间已经小于我们写的值 = 正在正常递减, 说明写入生效了
                    if (!do_write && windup_written[i] >= 0 && counter_read <= windup_written[i])
                        windup_written[i] = -1;

                    // 计数器重置 = 新周期开始, 清掉"本周期开过火"的标志
                    if (counter_reset)
                        plant_fired_cycle[i] = 0;
                }

                const int last = plant_last_launch_counter[i];

                // 还没观察到出手时, 先按定义表里的名义间隔显示
                int interval = plant_last_interval[i];
                if (interval <= 0)
                    interval = object_field<int>(p, PLANT_LAUNCH_RATE_OFFSET);

                // 本工具的刷新周期比游戏慢 (50Hz vs 100Hz), 用正常递减量估计
                // "每个刷新周期等于几帧", 下面判断重置时要把落后掉的帧补偿回去
                if (last != 0 && counter < last)
                {
                    const int dec = last - counter;
                    if (dec > 0 && dec < 64)
                        plant_sample_ticks = (plant_sample_ticks * 7 + dec) / 8;
                }

                // 计数器变大 = 游戏刚执行过 mLaunchCounter = mLaunchRate - Sexy::Rand(15)
                //
                // ★ 但钉住(IDLE)时不能这么判: "首发延迟"每帧把计数器按在同一个值上,
                //   游戏减 2 帧、我们再写回去 —— 从我们采样看就是"计数器变大" ✗
                //   攻击间隔会误以为游戏刚重置过, 于是把间隔值(如 150)灌进去,
                //   把钉住的效果冲掉 (实测: 回读值在 8~10 与 149/150 之间横跳)。
                //   两个功能写的是同一个 mLaunchCounter, 所以必须分状态:
                //     IDLE   -> 只有首发延迟在写
                //     ACTIVE -> 只有攻击间隔在写
                //   不参与首发延迟的分类 (first_max == 0, 如杨桃) 没有钉住这一说,
                //   攻击间隔全程写入。
                if (last != 0 && counter > last &&
                    (plant_windup_idle[i] == 0 || info.first_max <= 0))
                {
                    // 我们采样时游戏已经又递减过几帧, 补回来才是真实间隔
                    interval = counter + plant_sample_ticks - 1;

                    if (pinned)
                    {
                        // 攻击间隔 (秒) -> 间隔帧数 (游戏内部 100 帧每秒, 0.01 秒 = 1 帧)
                        double sec = plant_interval[klass];
                        if (!(sec > 0.0))
                            sec = info.counter_default / 100.0;

                        int target = static_cast<int>(std::lround(sec * 100.0));
                        if (target < info.counter_min)
                            target = info.counter_min;
                        if (target > info.counter_max)
                            target = info.counter_max;
                        if (target < 1)
                            target = 1;

                        counter = target;
                        interval = target;
                        WriteBytes(&counter, plant_base + plant_struct_size * i + PLANT_LAUNCH_COUNTER_OFFSET,
                                   sizeof(counter));
                    }
                    else if (info.counter_max > 0)
                    {
                        // 夹回游戏真实区间, 避免显示成区间外的数字
                        if (interval > info.counter_max)
                            interval = info.counter_max;
                        if (interval < info.counter_min)
                            interval = info.counter_min;
                    }
                }

                plant_last_launch_counter[i] = counter_read;
                plant_last_interval[i] = interval;
                out_plant.push_back({static_cast<int>(i), seed, klass, interval, pinned, counter,
                                     object_field<int>(p, PLANT_SHOOTING_COUNTER_OFFSET), diag_wrote, diag_reason});
            }

            // 整类写完了, 清掉待写入标记
            for (int k = 0; k < PSC_COUNT; k++)
                if (windup_did_write[k])
                    windup_pending[k] = false;
        }
    }

    return true;
}

void PvZ::RestorePlantLaunchRate()
{
    // 现在只固定 mLaunchCounter, 不再改写 mLaunchRate, 所以只需清掉观测状态
    plant_last_launch_counter.clear();
}

bool PvZ::ObservedSpeedRange(int klass, float &out_min, float &out_max) const
{
    if (klass < 0 || klass >= ZSC_COUNT)
        return false;

    if (zombie_observed_max[klass] < zombie_observed_min[klass])
        return false; // 还没观测到

    out_min = zombie_observed_min[klass];
    out_max = zombie_observed_max[klass];
    return true;
}

void PvZ::ResetObservedSpeedRange()
{
    for (int i = 0; i < ZSC_COUNT; i++)
    {
        zombie_observed_min[i] = std::numeric_limits<float>::max();
        zombie_observed_max[i] = std::numeric_limits<float>::lowest();
    }
}

} // namespace Pt
