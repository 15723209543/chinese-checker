#include "analysis.h"

#include "board.h"

#include <algorithm>
#include <cmath>

// analysis_current 保存当前局势分析结果。
static analysisdata analysis_current;

// 这个函数判断列表中是否包含指定孔位。
static bool analysis_contains(const std::vector<int>& values, int value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

// 这个函数计算两个孔位之间的屏幕距离。
static double analysis_distance(const pointdata& left, const pointdata& right)
{
    // dx 保存横向距离。
    double dx = left.x - right.x;
    // dy 保存纵向距离。
    double dy = left.y - right.y;
    return std::sqrt(dx * dx + dy * dy);
}

// 这个函数判断棋子下标和棋子所在孔位是否仍然有效。
static bool analysis_piece_valid(const boarddata& board, const gamestate& state, int pieceindex)
{
    if (pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()))
    {
        return false;
    }

    // piece 保存当前检查的棋子。
    const piecedata& piece = state.pieces[pieceindex];
    if (piece.owner < 0 || piece.owner >= static_cast<int>(state.players.size()) ||
        state.players[piece.owner].lost)
    {
        return false;
    }
    return piece.pointid >= 0 && piece.pointid < static_cast<int>(board.points.size());
}

// 这个函数计算棋子从起点区到终点区的推进比例。
static double analysis_piece_progress(const boarddata& board, const gamestate& state, const playerdata& player, int pointid)
{
    // starts 保存玩家起点区。
    std::vector<int> starts = board_get_zone(board, player.arm, state.piececount);
    if (starts.empty() || player.targetids.empty())
    {
        return 0.0;
    }

    // beststart 保存距离终点最远的起点。
    int beststart = starts.front();
    // besttarget 保存距离起点最近的终点。
    int besttarget = player.targetids.front();
    // maxdistance 保存起点到终点的最大参考距离。
    double maxdistance = 1.0;
    for (int startid : starts)
    {
        for (int targetid : player.targetids)
        {
            // distance 保存该起点到终点的距离。
            double distance = analysis_distance(board.points[startid], board.points[targetid]);
            if (distance > maxdistance)
            {
                maxdistance = distance;
                beststart = startid;
                besttarget = targetid;
            }
        }
    }

    // currentdistance 保存当前位置到终点参考点的距离。
    double currentdistance = analysis_distance(board.points[pointid], board.points[besttarget]);
    // startdistance 保存起点参考距离。
    double startdistance = analysis_distance(board.points[beststart], board.points[besttarget]);
    if (startdistance <= 1.0)
    {
        return 0.0;
    }

    // progress 保存 0 到 1 的推进比例。
    double progress = 1.0 - currentdistance / startdistance;
    if (progress < 0.0)
    {
        progress = 0.0;
    }
    if (progress > 1.0)
    {
        progress = 1.0;
    }
    return progress;
}

// 这个函数判断某个棋子是否能借助自己的棋子跳跃。
static bool analysis_has_cooperate_jump(const boarddata& board, const gamestate& state, int pieceindex)
{
    if (!analysis_piece_valid(board, state, pieceindex))
    {
        return false;
    }

    // owner 保存该棋子的玩家下标。
    int owner = state.pieces[pieceindex].owner;
    // pointid 保存该棋子当前孔位。
    int pointid = state.pieces[pieceindex].pointid;
    for (const jumpdata& jump : board.jumps[pointid])
    {
        // overpiece 保存被跳过位置的棋子下标。
        int overpiece = board_find_piece_at(state, jump.overid);
        if (overpiece >= 0 && state.pieces[overpiece].owner == owner && !board_point_has_piece(state, jump.toid))
        {
            return true;
        }
    }
    return false;
}

// 这个函数判断某个棋子是否给其他玩家提供了跳跃桥梁。
static int analysis_bridge_count(const boarddata& board, const gamestate& state, int pieceindex)
{
    if (!analysis_piece_valid(board, state, pieceindex))
    {
        return 0;
    }

    // count 保存桥梁次数。
    int count = 0;
    // owner 保存该棋子的玩家下标。
    int owner = state.pieces[pieceindex].owner;
    // bridgeid 保存该棋子当前孔位。
    int bridgeid = state.pieces[pieceindex].pointid;

    for (int otherindex = 0; otherindex < static_cast<int>(state.pieces.size()); ++otherindex)
    {
        if (!analysis_piece_valid(board, state, otherindex))
        {
            continue;
        }
        if (state.pieces[otherindex].owner == owner)
        {
            continue;
        }

        // fromid 保存其他棋子当前位置。
        int fromid = state.pieces[otherindex].pointid;
        for (const jumpdata& jump : board.jumps[fromid])
        {
            if (jump.overid == bridgeid && !board_point_has_piece(state, jump.toid))
            {
                ++count;
            }
        }
    }

    return count;
}

// 这个函数计算指定玩家当前评分。
static int analysis_player_score(const boarddata& board, const gamestate& state, int playerindex)
{
    const playerdata& player = state.players[playerindex];
    if (player.lost || player.pieceids.empty())
    {
        return 0;
    }

    // score 保存加分前后的总分。
    int score = analysis_base_score;
    // penalty 保存扣分总量。
    int penalty = 0;

    for (int pieceindex : player.pieceids)
    {
        // pointid 保存棋子当前位置。
        int pointid = state.pieces[pieceindex].pointid;
        if (pointid < 0 || pointid >= static_cast<int>(board.points.size()))
        {
            continue;
        }

        // progress 保存该棋子推进比例。
        double progress = analysis_piece_progress(board, state, player, pointid);
        if (progress > 0.5)
        {
            score += static_cast<int>((progress - 0.5) * 2.0 * analysis_cross_bonus);
        }

        if (analysis_has_cooperate_jump(board, state, pieceindex))
        {
            score += analysis_cooperate_bonus;
        }

        if (analysis_contains(player.targetids, pointid))
        {
            score += analysis_target_bonus;
        }

        penalty += analysis_bridge_count(board, state, pieceindex) * analysis_bridge_penalty;
    }

    score -= penalty;
    if (score < 0)
    {
        score = 0;
    }
    return score;
}

// 这个函数把评分折算成总和为 100 的胜率。
static void analysis_make_probability()
{
    // total 保存所有玩家评分总和。
    int total = 0;
    for (const analysisplayerdata& player : analysis_current.players)
    {
        total += player.score;
    }
    if (total <= 0)
    {
        return;
    }

    // lastpositive 保存最后一个仍有评分的玩家下标，用来接收四舍五入余数。
    int lastpositive = -1;
    for (int index = 0; index < static_cast<int>(analysis_current.players.size()); ++index)
    {
        if (analysis_current.players[index].score > 0)
        {
            lastpositive = index;
        }
    }

    // used 保存已经分配的百分比。
    int used = 0;
    for (int index = 0; index < static_cast<int>(analysis_current.players.size()); ++index)
    {
        analysisplayerdata& player = analysis_current.players[index];
        if (player.score <= 0)
        {
            player.probability = 0;
        }
        else if (index == lastpositive)
        {
            player.probability = 100 - used;
        }
        else
        {
            player.probability = static_cast<int>(player.score * 100.0 / total + 0.5);
            used += player.probability;
        }
    }
}

// 这个函数清空局势分析数据。
void analysis_reset()
{
    analysis_current.players.clear();
}

// 这个函数根据当前棋盘局面重新计算评分和胜率。
void analysis_update(const boarddata& board, const gamestate& state)
{
    analysis_current.players.clear();
    if (state.players.empty())
    {
        return;
    }

    for (int playerindex = 0; playerindex < static_cast<int>(state.players.size()); ++playerindex)
    {
        analysisplayerdata player{};
        player.playerindex = playerindex;
        player.score = analysis_player_score(board, state, playerindex);
        player.probability = 0;
        analysis_current.players.push_back(player);
    }
    analysis_make_probability();
}

// 这个函数返回当前局势分析数据。
const analysisdata& analysis_get_data()
{
    return analysis_current;
}
