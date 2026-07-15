#include "board.h"
#include "robot.h"
#include "../src/time.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

struct simulationresult
{
    bool ended; // 对局是否在测试上限内结束。
    int winner; // 最后剩余或最先完成的玩家下标。
    int moves; // 全局总移动次数。
    int maxrepeat; // 同一整盘局面的最大重复次数。
    int maxplayerturns; // 单名玩家消耗的最大回合数。
    int naturalfinished; // 正常把全部棋子送入最终区的玩家数量。
    int zoneviolations; // 六成触发时仍有机器人尚未完成撤离的警报次数。
    int evacuationmisses; // 达到三成后存在合法撤离走法却没有选择撤离的次数。
};

// simulationtotalseconds 保存模拟时每名玩家的总时长。
static int simulationtotalseconds = 300;

// simulationremaining 保存模拟时每名玩家的剩余时间。
static int simulationremaining[6] = { 300, 300, 300, 300, 300, 300 };

// 这个函数为机器人测试返回每步时长。
int time_get_step_seconds()
{
    return 15;
}

// 这个函数为机器人测试返回单方总时长。
int time_get_total_seconds()
{
    return simulationtotalseconds;
}

// 这个函数为机器人测试返回指定玩家剩余时间。
int time_get_player_remaining(int playerindex)
{
    if (playerindex < 0 || playerindex >= 6)
    {
        return simulationtotalseconds;
    }
    return simulationremaining[playerindex];
}

// 这个函数生成模拟局面的稳定哈希值。
static std::uint64_t simulation_hash(const gamestate& state)
{
    // hash 保存按棋子顺序生成的局面哈希。
    std::uint64_t hash = 1469598103934665603ULL;
    for (const piecedata& piece : state.pieces)
    {
        // value 同时编码玩家和孔位。
        std::uint64_t value = static_cast<std::uint64_t>((piece.owner + 2) * 257 + piece.pointid + 2);
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    // 当前行动位置也是完整局面的一部分，避免把不同玩家的连续跳过误判为走棋循环。
    hash ^= static_cast<std::uint64_t>(state.currentorderindex + 1);
    hash *= 1099511628211ULL;
    return hash;
}

// 这个函数把指定玩家从模拟棋盘和行动顺序中移除。
static void simulation_remove_player(gamestate& state, int playerindex, bool lost)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return;
    }

    if (lost)
    {
        state.players[playerindex].lost = true;
        for (int pieceindex : state.players[playerindex].pieceids)
        {
            state.pieces[pieceindex].pointid = -1;
        }
    }
    else
    {
        state.players[playerindex].finished = true;
    }
    state.turnorder.erase(std::remove(state.turnorder.begin(), state.turnorder.end(), playerindex), state.turnorder.end());
}

// 这个函数创建一盘六机器人十子模拟对局。
static void simulation_initialize(gamestate& state, const boarddata& board)
{
    state = {};
    state.phase = phase_select_piece;
    state.playercount = 6;
    state.piececount = 10;
    state.robotcount = 6;
    state.currentorderindex = 0;
    state.selectedpiece = -1;
    state.robotpreviewtarget = -1;
    state.winnerindex = -1;

    std::vector<int> arms = board_get_arms(state.playercount);
    for (int index = 0; index < state.playercount; ++index)
    {
        playerdata player{};
        player.id = index + 1;
        player.arm = arms[index];
        player.targetarm = (player.arm + 3) % 6;
        player.targetids = board_get_zone(board, player.targetarm, state.piececount);
        player.isrobot = true;
        std::vector<int> starts = board_get_zone(board, player.arm, state.piececount);
        for (int pointid : starts)
        {
            piecedata piece{};
            piece.owner = index;
            piece.pointid = pointid;
            player.pieceids.push_back(static_cast<int>(state.pieces.size()));
            state.pieces.push_back(piece);
        }
        state.players.push_back(player);
        state.turnorder.push_back(index);
        simulationremaining[index] = simulationtotalseconds;
    }
}

// 这个函数验证外来最终区不会成为有效落点，并验证六成阈值检查能够识别滞留棋子。
static bool simulation_validate_goal_rules(const boarddata& board)
{
    gamestate state{};
    simulation_initialize(state, board);

    state.piececount = 3;
    if (board_get_zone_protection_count(state) != 2)
    {
        return false;
    }
    state.piececount = 6;
    if (board_get_zone_protection_count(state) != 4)
    {
        return false;
    }
    state.piececount = 10;
    if (board_get_zone_protection_count(state) != 6)
    {
        return false;
    }

    // 六个真实角尖在清空周围棋子后都必须存在向中心撤离的合法落点。
    for (int playerindex = 0; playerindex < static_cast<int>(state.players.size()); ++playerindex)
    {
        gamestate tipstate = state;
        for (piecedata& piece : tipstate.pieces)
        {
            piece.pointid = -1;
        }
        int tippiece = tipstate.players[playerindex].pieceids.front(); // tippiece 保存该玩家角尖棋子。
        tipstate.pieces[tippiece].pointid = board_get_zone(board,
            tipstate.players[playerindex].arm, tipstate.piececount).front();
        if (board_get_targets(board, tipstate, tippiece).empty())
        {
            return false;
        }
    }

    for (int playerindex = 0; playerindex < static_cast<int>(state.players.size()); ++playerindex)
    {
        for (int pieceindex : state.players[playerindex].pieceids)
        {
            std::vector<int> targets = board_get_targets(board, state, pieceindex);
            int startid = state.pieces[pieceindex].pointid;
            for (int targetid : targets)
            {
                for (int zoneowner = 0; zoneowner < static_cast<int>(state.players.size()); ++zoneowner)
                {
                    if (zoneowner != playerindex && std::find(state.players[zoneowner].targetids.begin(),
                        state.players[zoneowner].targetids.end(), targetid) != state.players[zoneowner].targetids.end())
                    {
                        // 已经位于该区域的初始棋子可以向出口方向撤离，但外部棋子不能进入。
                        if (std::find(state.players[zoneowner].targetids.begin(),
                            state.players[zoneowner].targetids.end(), startid) ==
                            state.players[zoneowner].targetids.end())
                        {
                            return false;
                        }
                    }
                }
            }
        }
    }

    if (!board_find_zone_violators(state, 0).empty())
    {
        return false;
    }

    int protectioncount = board_get_zone_protection_count(state);
    for (int index = 0; index < protectioncount; ++index)
    {
        int pieceindex = state.players[0].pieceids[index];
        state.pieces[pieceindex].pointid = state.players[0].targetids[index];
    }
    return !board_find_zone_violators(state, 0).empty();
}

// 这个函数判断孔位是否属于已经达到三成警戒线的其他玩家最终区。
static bool simulation_point_in_warning_zone(const gamestate& state, int playerindex, int pointid)
{
    int warningcount = (state.piececount * 3 + 9) / 10;
    for (int zoneowner = 0; zoneowner < static_cast<int>(state.players.size()); ++zoneowner)
    {
        if (zoneowner == playerindex || board_count_finished(state, zoneowner) < warningcount)
        {
            continue;
        }

        if (std::find(state.players[zoneowner].targetids.begin(), state.players[zoneowner].targetids.end(),
            pointid) != state.players[zoneowner].targetids.end())
        {
            return true;
        }
    }
    return false;
}

// 这个函数运行一盘不等待动画的机器人完整模拟。
static simulationresult simulation_run_game(const boarddata& board)
{
    gamestate state{};
    simulation_initialize(state, board);
    robot_new_game();

    simulationresult result{};
    result.winner = -1;
    std::unordered_map<std::uint64_t, int> positions;
    int playerturns[6] = { 0, 0, 0, 0, 0, 0 };
    const int movelimit = 2000;

    while (state.turnorder.size() > 1 && result.moves < movelimit)
    {
        if (state.currentorderindex >= static_cast<int>(state.turnorder.size()))
        {
            state.currentorderindex = 0;
        }
        // currentplayer 保存本回合机器人玩家。
        int currentplayer = state.turnorder[state.currentorderindex];
        // currenthash 保存行动前局面哈希。
        std::uint64_t currenthash = simulation_hash(state);
        int repeat = ++positions[currenthash];
        if (repeat > result.maxrepeat)
        {
            result.maxrepeat = repeat;
        }

        // evacuationrequired 表示本回合至少有一枚三成警戒区棋子能够合法撤离。
        bool evacuationrequired = false;
        for (int pieceindex : state.players[currentplayer].pieceids)
        {
            if (simulation_point_in_warning_zone(state, currentplayer, state.pieces[pieceindex].pointid) &&
                !board_get_targets(board, state, pieceindex).empty())
            {
                evacuationrequired = true;
                break;
            }
        }

        robotdecision decision = robot_make_decision(board, state);
        if (evacuationrequired && (!decision.valid ||
            !simulation_point_in_warning_zone(state, currentplayer,
                state.pieces[decision.pieceindex].pointid)))
        {
            ++result.evacuationmisses;
        }
        ++playerturns[currentplayer];
        simulationremaining[currentplayer] -= 5;
        bool currentremoved = false;
        if (decision.valid)
        {
            std::vector<int> targets = board_get_targets(board, state, decision.pieceindex);
            if (std::find(targets.begin(), targets.end(), decision.targetid) == targets.end())
            {
                break;
            }

            int finishedbefore = board_count_finished(state, currentplayer);
            movedata move{};
            move.playerindex = currentplayer;
            move.pieceindex = decision.pieceindex;
            move.fromid = state.pieces[decision.pieceindex].pointid;
            move.toid = decision.targetid;
            move.orderindex = state.currentorderindex;
            state.history.push_back(move);
            state.pieces[decision.pieceindex].pointid = decision.targetid;
            ++result.moves;

            int finishedafter = board_count_finished(state, currentplayer);
            int protectioncount = board_get_zone_protection_count(state);
            std::vector<int> violators;
            if (finishedbefore < protectioncount && finishedafter >= protectioncount)
            {
                violators = board_find_zone_violators(state, currentplayer);
            }
            for (int violator : violators)
            {
                if (!state.players[violator].lost && !state.players[violator].finished)
                {
                    ++result.zoneviolations;
                    currentremoved = currentremoved || violator == currentplayer;
                    simulation_remove_player(state, violator, true);
                }
            }
            if (!currentremoved && board_player_finished(state, currentplayer))
            {
                ++result.naturalfinished;
                simulation_remove_player(state, currentplayer, false);
                currentremoved = true;
                if (result.winner < 0)
                {
                    result.winner = currentplayer;
                }
            }
        }

        if (!currentremoved && simulationremaining[currentplayer] <= 0)
        {
            simulation_remove_player(state, currentplayer, true);
            currentremoved = true;
        }

        if (state.turnorder.empty())
        {
            break;
        }
        if (currentremoved)
        {
            if (state.currentorderindex >= static_cast<int>(state.turnorder.size()))
            {
                state.currentorderindex = 0;
            }
        }
        else
        {
            auto iterator = std::find(state.turnorder.begin(), state.turnorder.end(), currentplayer);
            int position = static_cast<int>(iterator - state.turnorder.begin());
            state.currentorderindex = (position + 1) % static_cast<int>(state.turnorder.size());
        }
    }

    if (result.winner < 0 && state.turnorder.size() == 1)
    {
        result.winner = state.turnorder[0];
    }
    result.ended = state.turnorder.size() <= 1;
    for (int turns : playerturns)
    {
        if (turns > result.maxplayerturns)
        {
            result.maxplayerturns = turns;
        }
    }
    return result;
}

// 这个函数运行多盘模拟并输出循环、时限和胜者分布。
int main()
{
    boarddata board{};
    board_build(board);
    bool goalrulespassed = simulation_validate_goal_rules(board);
    std::cout << "goalrules=" << goalrulespassed << '\n';
    const int gamecount = 60;
    int winners[6] = { 0, 0, 0, 0, 0, 0 };
    bool passed = goalrulespassed;

    for (int game = 0; game < gamecount; ++game)
    {
        simulationresult result = simulation_run_game(board);
        if (result.winner >= 0 && result.winner < 6)
        {
            ++winners[result.winner];
        }
        std::cout << "game=" << game + 1 << " ended=" << result.ended <<
            " winner=" << result.winner + 1 << " moves=" << result.moves <<
            " maxrepeat=" << result.maxrepeat << " maxturns=" << result.maxplayerturns <<
            " natural=" << result.naturalfinished << " zoneviolations=" << result.zoneviolations <<
            " evacuationmisses=" << result.evacuationmisses << '\n';
        passed = passed && result.ended && result.maxrepeat <= 2 && result.maxplayerturns <= 60 &&
            result.zoneviolations == 0 && result.evacuationmisses == 0;
    }

    std::cout << "winners";
    for (int index = 0; index < 6; ++index)
    {
        std::cout << " p" << index + 1 << '=' << winners[index];
    }
    std::cout << '\n';
    return passed ? 0 : 1;
}
