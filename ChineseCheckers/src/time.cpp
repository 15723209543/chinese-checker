#include "time.h"

#include "analysis.h"
#include "game.h"

#include <algorithm>
#include <windows.h>

#include <string>
#include <vector>

struct timedata
{
    int stepseconds; // 每步时长设置。
    int totalseconds; // 每名玩家总时长设置。
    int stepremaining; // 当前步剩余秒数。
    int activeplayer; // 当前计时玩家下标。
    DWORD lasttick; // 上次更新时间。
    bool active; // 是否正在计时。
    bool timeout; // 是否已经发生超时。
    std::vector<int> remaining; // 每名玩家剩余总时间。
};

// time_current 保存当前计时数据。
static timedata time_current{ 30, 300, 30, -1, 0, false, false, {} };

// 这个函数把毫秒折算成至少 1 秒的消耗。
static int time_elapsed_seconds(DWORD now)
{
    if (time_current.lasttick == 0 || now <= time_current.lasttick)
    {
        return 0;
    }

    // elapsed 保存距离上次更新时间的毫秒。
    DWORD elapsed = now - time_current.lasttick;
    if (elapsed < 1000)
    {
        return 0;
    }
    time_current.lasttick += (elapsed / 1000) * 1000;
    return static_cast<int>(elapsed / 1000);
}

// 这个函数扣除当前玩家经过的时间。
static void time_apply_elapsed()
{
    if (!time_current.active || time_current.activeplayer < 0 ||
        time_current.activeplayer >= static_cast<int>(time_current.remaining.size()))
    {
        return;
    }

    // seconds 保存经过的整秒数。
    int seconds = time_elapsed_seconds(GetTickCount());
    if (seconds <= 0)
    {
        return;
    }

    time_current.stepremaining -= seconds;
    time_current.remaining[time_current.activeplayer] -= seconds;
    if (time_current.stepremaining < 0)
    {
        time_current.stepremaining = 0;
    }
    if (time_current.remaining[time_current.activeplayer] < 0)
    {
        time_current.remaining[time_current.activeplayer] = 0;
    }
}

// 这个函数重置计时设置和运行状态。
void time_reset()
{
    time_current.stepseconds = 30;
    time_current.totalseconds = 300;
    time_current.stepremaining = time_current.stepseconds;
    time_current.activeplayer = -1;
    time_current.lasttick = 0;
    time_current.active = false;
    time_current.timeout = false;
    time_current.remaining.clear();
}

// 这个函数设置每步时长。
void time_set_step_seconds(int seconds)
{
    time_current.stepseconds = seconds;
    if (!time_current.active)
    {
        time_current.stepremaining = seconds;
    }
}

// 这个函数设置每名玩家总时长。
void time_set_total_seconds(int seconds)
{
    time_current.totalseconds = seconds;
}

// 这个函数返回每步时长。
int time_get_step_seconds()
{
    return time_current.stepseconds;
}

// 这个函数返回每名玩家总时长。
int time_get_total_seconds()
{
    return time_current.totalseconds;
}

// 这个函数返回指定玩家的总剩余时间。
int time_get_player_remaining(int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(time_current.remaining.size()))
    {
        return time_current.totalseconds;
    }
    return time_current.remaining[playerindex];
}

// 这个函数返回当前这一步剩余时间。
int time_get_step_remaining()
{
    return time_current.stepremaining;
}

// 这个函数开始整局计时。
void time_start_game(const gamestate& state)
{
    time_current.remaining.assign(state.players.size(), time_current.totalseconds);
    time_current.timeout = false;
    time_start_turn(state);
}

// 这个函数开始当前玩家回合计时。
void time_start_turn(const gamestate& state)
{
    if (state.turnorder.empty() || state.currentorderindex < 0 ||
        state.currentorderindex >= static_cast<int>(state.turnorder.size()))
    {
        time_current.active = false;
        time_current.activeplayer = -1;
        return;
    }

    time_current.activeplayer = state.turnorder[state.currentorderindex];
    time_current.stepremaining = time_current.stepseconds;
    time_current.lasttick = GetTickCount();
    time_current.active = true;
    time_current.timeout = false;
}

// 这个函数结算当前回合已经消耗的时间。
void time_finish_turn()
{
    time_apply_elapsed();
    time_current.active = false;
    time_current.activeplayer = -1;
}

// 这个函数停止计时。
void time_stop()
{
    time_apply_elapsed();
    time_current.active = false;
    time_current.activeplayer = -1;
}

// 这个函数处理一名玩家超时判负后的游戏状态。
static void time_handle_timeout_loss(const boarddata& board, gamestate& state, loggerdata& logger, int loser)
{
    if (loser >= 0 && loser < static_cast<int>(time_current.remaining.size()))
    {
        time_current.remaining[loser] = 0;
    }

    time_current.active = false;
    time_current.activeplayer = -1;
    time_current.stepremaining = 0;
    game_eliminate_players(board, state, logger, { loser }, L"超时判负", false);
}

// 这个函数每帧更新计时，并在超时时改写游戏状态。
void time_update(const boarddata& board, gamestate& state, loggerdata& logger)
{
    if (!time_current.active ||
        (state.phase != phase_select_piece && state.phase != phase_select_target))
    {
        return;
    }

    time_apply_elapsed();
    if (time_current.activeplayer < 0 ||
        time_current.activeplayer >= static_cast<int>(state.players.size()))
    {
        return;
    }

    if (time_current.stepremaining <= 0 || time_current.remaining[time_current.activeplayer] <= 0)
    {
        // loser 保存超时玩家下标。
        int loser = time_current.activeplayer;
        time_handle_timeout_loss(board, state, logger, loser);
    }
}

// 这个函数把秒数格式化成 mm:ss。
std::wstring time_format_seconds(int seconds)
{
    if (seconds < 0)
    {
        seconds = 0;
    }

    // minutes 保存分钟数。
    int minutes = seconds / 60;
    // remain 保存剩余秒数。
    int remain = seconds % 60;
    std::wstring text = std::to_wstring(minutes) + L":";
    if (remain < 10)
    {
        text += L"0";
    }
    text += std::to_wstring(remain);
    return text;
}
