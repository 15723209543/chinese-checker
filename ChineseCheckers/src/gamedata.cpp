#include "gamedata.h"

// 这个函数返回游戏阶段的中文名称。
std::wstring gamedata_phase_text(phasekind phase)
{
    switch (phase)
    {
    case phase_player_count:
        return L"游戏设置";
    case phase_piece_count:
        return L"选择棋子数量";
    case phase_select_piece:
        return L"选择棋子";
    case phase_select_target:
        return L"选择落点";
    case phase_game_over:
        return L"游戏结束";
    default:
        return L"未知阶段";
    }
}

// 这个函数返回三角区的中文名称。
std::wstring gamedata_arm_text(int arm)
{
    switch (arm)
    {
    case 0:
        return L"上方营地";
    case 1:
        return L"右上营地";
    case 2:
        return L"右下营地";
    case 3:
        return L"下方营地";
    case 4:
        return L"左下营地";
    case 5:
        return L"左上营地";
    default:
        return L"未知营地";
    }
}
