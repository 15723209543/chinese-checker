#include "robot.h"

#include "board.h"
#include "time.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>
#include <windows.h>

struct robotcandidate
{
    robotdecision decision; // 候选走法对应的棋子和落点。
    int score; // 候选走法综合评分。
    int repeatcount; // 走后局面过去出现的次数。
    int localrepeatcount; // 走后本方棋子布局过去出现的次数。
    bool reverse; // 是否立即撤回该棋子上一次移动。
    double progressgain; // 本次移动沿起点到终点方向的推进量。
    int emergencyvalue; // 对方区域达到三成后，本步对紧急撤离局面的改善值。
    int evacuationrisk; // 棋子当前所在对方最终区的风险等级。
    int evacuationlayer; // 滞留棋子在对方最终区中的层数，数值越小越靠近角尖。
    bool evacuationmove; // 本步是否直接移动仍在对方最终区中的棋子。
    bool leaveszone; // 本步是否能把残留棋子直接移出对方最终区。
};

// robot_player 保存当前机器人计时对应的玩家下标。
static int robot_player = -1;

// robot_order 保存当前机器人计时对应的回合顺序下标。
static int robot_order = -1;

// robot_starttick 保存机器人开始思考的毫秒时间。
static DWORD robot_starttick = 0;

// robot_stage 保存机器人当前展示阶段，0思考、1选棋子、2选落点、3完成。
static int robot_stage = 0;

// robot_currentdecision 保存机器人本回合预先计算出的最优走法。
static robotdecision robot_currentdecision{ false, -1, -1 };

// robot_currentfromid 保存机器人当前走法的起点孔位。
static int robot_currentfromid = -1;

// robot_currentgain 保存机器人当前走法的推进量。
static double robot_currentgain = 0.0;

// robot_piece_delay 保存机器人展示选中棋子前的短暂思考时间。
static constexpr DWORD robot_piece_delay = 2000;

// robot_target_delay 保存机器人开始展示决定落点的时间。
static constexpr DWORD robot_target_delay = 3500;

// robot_move_delay 保存机器人执行整步移动的严格总时长。
static constexpr DWORD robot_move_delay = 5000;

// robot_positioncounts 保存本局各棋盘局面已经出现的次数。
static std::unordered_map<std::uint64_t, int> robot_positioncounts;

// robot_playerpositioncounts 保存每名机器人自己的棋子布局已经出现的次数。
static std::unordered_map<std::uint64_t, int> robot_playerpositioncounts;

// robot_edgecounts 保存本局每枚棋子重复走同一条边的次数。
static std::unordered_map<std::uint64_t, int> robot_edgecounts;

// robot_stagnation 保存每名机器人连续没有有效推进的回合数。
static int robot_stagnation[6] = { 0, 0, 0, 0, 0, 0 };

// robot_progressstyle 保存每名机器人本局的推进风格系数。
static int robot_progressstyle[6] = { 100, 100, 100, 100, 100, 100 };

// robot_random 保存跨回合随机引擎，使相同设置的每局走法不完全一致。
static std::mt19937 robot_random(static_cast<unsigned int>(
    std::chrono::high_resolution_clock::now().time_since_epoch().count() ^ GetTickCount()));

// 这个函数计算两个孔位之间的屏幕距离。
static double robot_distance(const pointdata& left, const pointdata& right)
{
    // dx 保存横向距离。
    double dx = left.x - right.x;
    // dy 保存纵向距离。
    double dy = left.y - right.y;
    return std::sqrt(dx * dx + dy * dy);
}

// 这个函数判断孔位列表是否包含指定孔位。
static bool robot_contains(const std::vector<int>& values, int value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

// 这个函数判断己方最终区在三成警戒后是否仍有外来棋子没有撤离。
static bool robot_goal_evacuation_warning(const gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return false;
    }

    int warningcount = (state.piececount * 3 + 9) / 10; // warningcount 保存三成警戒棋子数。
    if (board_count_finished(state, playerindex) < warningcount)
    {
        return false;
    }

    for (const piecedata& piece : state.pieces)
    {
        if (piece.owner == playerindex || piece.pointid < 0 || piece.owner < 0 ||
            piece.owner >= static_cast<int>(state.players.size()) || state.players[piece.owner].lost ||
            state.players[piece.owner].finished)
        {
            continue;
        }
        if (robot_contains(state.players[playerindex].targetids, piece.pointid))
        {
            return true;
        }
    }
    return false;
}

// 这个函数返回当前行动玩家下标。
static int robot_current_player(const gamestate& state)
{
    if (state.turnorder.empty() || state.currentorderindex < 0 ||
        state.currentorderindex >= static_cast<int>(state.turnorder.size()))
    {
        return -1;
    }
    return state.turnorder[state.currentorderindex];
}

// 这个函数计算一组孔位的中心坐标。
static pointdata robot_zone_center(const boarddata& board, const std::vector<int>& zone)
{
    pointdata center{};
    // validcount 保存参与中心计算的有效孔位数量。
    int validcount = 0;
    for (int pointid : zone)
    {
        if (pointid < 0 || pointid >= static_cast<int>(board.points.size()))
        {
            continue;
        }
        center.x += board.points[pointid].x;
        center.y += board.points[pointid].y;
        ++validcount;
    }
    if (validcount > 0)
    {
        center.x /= validcount;
        center.y /= validcount;
    }
    return center;
}

// 这个函数计算棋子沿己方起点中心到最终区中心方向的推进比例。
static double robot_piece_progress(const boarddata& board, const gamestate& state,
    const playerdata& player, int pointid)
{
    if (pointid < 0 || pointid >= static_cast<int>(board.points.size()))
    {
        return 0.0;
    }

    // starts 保存玩家起始区域孔位。
    std::vector<int> starts = board_get_zone(board, player.arm, state.piececount);
    // startcenter 保存起始区域中心。
    pointdata startcenter = robot_zone_center(board, starts);
    // targetcenter 保存最终区域中心。
    pointdata targetcenter = robot_zone_center(board, player.targetids);
    // axisx 和 axisy 保存推进轴方向。
    double axisx = targetcenter.x - startcenter.x;
    double axisy = targetcenter.y - startcenter.y;
    // axislength 保存推进轴长度平方。
    double axislength = axisx * axisx + axisy * axisy;
    if (axislength <= 0.001)
    {
        return 0.0;
    }

    const pointdata& point = board.points[pointid];
    return ((point.x - startcenter.x) * axisx + (point.y - startcenter.y) * axisy) / axislength;
}

// 这个函数计算指定模拟移动后的棋盘哈希值。
static std::uint64_t robot_position_hash(const gamestate& state, int movedpiece, int targetid)
{
    // hash 保存按棋子顺序生成的稳定棋盘哈希。
    std::uint64_t hash = 1469598103934665603ULL;
    for (int index = 0; index < static_cast<int>(state.pieces.size()); ++index)
    {
        const piecedata& piece = state.pieces[index];
        // pointid 保存模拟移动后该棋子所在孔位。
        int pointid = index == movedpiece ? targetid : piece.pointid;
        // value 同时编码棋子所属玩家和孔位。
        std::uint64_t value = static_cast<std::uint64_t>((piece.owner + 2) * 257 + pointid + 2);
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// 这个函数计算指定玩家模拟移动后的本方棋子布局哈希值。
static std::uint64_t robot_player_position_hash(const gamestate& state, int playerindex,
    int movedpiece, int targetid)
{
    // positions 保存本方棋子的孔位；排序后不受同色棋子编号互换影响。
    std::vector<int> positions;
    for (int pieceindex : state.players[playerindex].pieceids)
    {
        if (pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()))
        {
            continue;
        }
        positions.push_back(pieceindex == movedpiece ? targetid : state.pieces[pieceindex].pointid);
    }
    std::sort(positions.begin(), positions.end());

    // hash 同时编码玩家和本方全部棋子孔位，防止不同玩家之间相互干扰。
    std::uint64_t hash = 1469598103934665603ULL;
    hash ^= static_cast<std::uint64_t>(playerindex + 1);
    hash *= 1099511628211ULL;
    for (int pointid : positions)
    {
        hash ^= static_cast<std::uint64_t>(pointid + 2);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// 这个函数生成一枚棋子走一条有向边的统计键值。
static std::uint64_t robot_edge_key(int pieceindex, int fromid, int targetid)
{
    return (static_cast<std::uint64_t>(pieceindex + 1) << 32) |
        (static_cast<std::uint64_t>(fromid + 1) << 16) |
        static_cast<std::uint64_t>(targetid + 1);
}

// 这个函数判断候选走法是否立即撤销该棋子的上一次移动。
static bool robot_is_reverse_move(const gamestate& state, int pieceindex, int targetid)
{
    for (auto iterator = state.history.rbegin(); iterator != state.history.rend(); ++iterator)
    {
        if (iterator->pieceindex == pieceindex)
        {
            return iterator->fromid == targetid;
        }
    }
    return false;
}

// 这个函数计算孔位位于其他玩家最终区时的停靠风险。
static int robot_opponent_zone_risk(const gamestate& state, int playerindex, int pointid)
{
    // risk 保存所有对手最终区中最高的风险等级，0安全、1一般、2接近三成、3已到三成。
    int risk = 0;
    for (int opponent = 0; opponent < static_cast<int>(state.players.size()); ++opponent)
    {
        if (opponent == playerindex || !robot_contains(state.players[opponent].targetids, pointid))
        {
            continue;
        }

        // warningcount 保存机器人开始强制撤离时所需的三成棋子数。
        int warningcount = (state.piececount * 3 + 9) / 10;
        // finishedcount 保存该区域主人已经进入最终区的棋子数。
        int finishedcount = board_count_finished(state, opponent);
        int currentrisk = finishedcount >= warningcount ? 3 :
            (finishedcount + 1 >= warningcount ? 2 : 1);
        if (currentrisk > risk)
        {
            risk = currentrisk;
        }
    }
    return risk;
}

// 这个函数判断机器人是否仍有棋子滞留在对方最终区，并可限定最低风险等级。
static bool robot_has_evacuation_piece(const gamestate& state, int playerindex, int minimumrisk)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return false;
    }

    for (int pieceindex : state.players[playerindex].pieceids)
    {
        if (pieceindex >= 0 && pieceindex < static_cast<int>(state.pieces.size()) &&
            state.pieces[pieceindex].pointid >= 0 &&
            robot_opponent_zone_risk(state, playerindex, state.pieces[pieceindex].pointid) >= minimumrisk)
        {
            return true;
        }
    }
    return false;
}

// 这个函数返回孔位在三角区域中从角尖向出口计算的层数，不在区域中返回 -1。
static int robot_zone_layer(const std::vector<int>& zone, int pointid)
{
    // iterator 保存目标孔位在区域数据中的位置。
    auto iterator = std::find(zone.begin(), zone.end(), pointid);
    if (iterator == zone.end())
    {
        return -1;
    }

    int position = static_cast<int>(iterator - zone.begin()); // position 保存孔位在线性区域数据中的下标。
    int layer = 0; // layer 保存当前检查的三角层数。
    int layerend = 1; // layerend 保存当前层结束后的首个下标。
    while (position >= layerend)
    {
        ++layer;
        layerend += layer + 1;
    }
    return layer;
}

// 这个函数返回孔位在任一对方最终区中的最深层号，不在对方最终区返回 -1。
static int robot_opponent_zone_layer(const gamestate& state, int playerindex, int pointid)
{
    int result = -1; // result 保存找到的最小层号，也就是最靠近角尖的位置。
    for (int zoneowner = 0; zoneowner < static_cast<int>(state.players.size()); ++zoneowner)
    {
        if (zoneowner == playerindex)
        {
            continue;
        }

        int layer = robot_zone_layer(state.players[zoneowner].targetids, pointid);
        if (layer >= 0 && (result < 0 || layer < result))
        {
            result = layer;
        }
    }
    return result;
}

// 这个函数估算机器人把全部外来终点区棋子撤出至少还需要推进多少层。
static int robot_evacuation_burden(const gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return 0;
    }

    // burden 保存所有滞留棋子距离对应区域出口的剩余层数总和。
    int burden = 0;
    for (int pieceindex : state.players[playerindex].pieceids)
    {
        if (pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()) ||
            state.pieces[pieceindex].pointid < 0)
        {
            continue;
        }

        for (int zoneowner = 0; zoneowner < static_cast<int>(state.players.size()); ++zoneowner)
        {
            if (zoneowner == playerindex)
            {
                continue;
            }

            int layer = robot_zone_layer(state.players[zoneowner].targetids,
                state.pieces[pieceindex].pointid);
            if (layer < 0)
            {
                continue;
            }

            // side 保存该三角区域的边长。
            int side = 1;
            while (side * (side + 1) / 2 < static_cast<int>(state.players[zoneowner].targetids.size()))
            {
                ++side;
            }
            burden += side - layer;
            break;
        }
    }
    return burden;
}

// 这个函数评价终点区撤离局面，优先避免困死，其次减少滞留棋子并增加可用出口。
static int robot_emergency_escape_value(const boarddata& board, const gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return 0;
    }

    // escapeoptions 保存所有受困棋子下一回合能够直接撤离的落点总数。
    int escapeoptions = 0;
    // blockedcount 保存当前没有任何合法出口的受困棋子数量。
    int blockedcount = 0;
    for (int pieceindex : state.players[playerindex].pieceids)
    {
        if (pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()) ||
            state.pieces[pieceindex].pointid < 0 ||
            robot_opponent_zone_risk(state, playerindex, state.pieces[pieceindex].pointid) <= 0)
        {
            continue;
        }

        int pieceoptions = static_cast<int>(board_get_targets(board, state, pieceindex).size());
        escapeoptions += pieceoptions;
        if (pieceoptions == 0)
        {
            ++blockedcount;
        }
    }

    // burden 让角尖棋子向外推进与边缘棋子直接离场按实际节省的撤离步数公平比较。
    int burden = robot_evacuation_burden(state, playerindex);
    // 困死惩罚最高；在不制造困死的前提下，最小化全部撤离所需层数并保留更多路线。
    return -blockedcount * 400000 - burden * 100000 + escapeoptions * 1000;
}

// 这个函数计算机器人某一步的局面评分。
static int robot_score_move(const boarddata& board, const gamestate& state, const playerdata& player,
    int pieceindex, int fromid, int targetid, int repeatcount, bool reverse, double& progressgain)
{
    // beforeprogress 保存移动前推进比例。
    double beforeprogress = robot_piece_progress(board, state, player, fromid);
    // afterprogress 保存移动后推进比例。
    double afterprogress = robot_piece_progress(board, state, player, targetid);
    progressgain = afterprogress - beforeprogress;

    // playerindex 保存当前机器人玩家下标。
    int playerindex = state.pieces[pieceindex].owner;
    // remainingrate 保存该玩家剩余时间比例，用来在后半程提高推进紧迫度。
    double remainingrate = time_get_total_seconds() > 0 ?
        static_cast<double>(time_get_player_remaining(playerindex)) / time_get_total_seconds() : 1.0;
    if (remainingrate < 0.0)
    {
        remainingrate = 0.0;
    }
    if (remainingrate > 1.0)
    {
        remainingrate = 1.0;
    }

    // urgency 保存时间越少时越高的推进权重。
    double urgency = 1.0 + (1.0 - remainingrate) * 1.8 +
        static_cast<double>(robot_stagnation[playerindex]) * 0.12;
    // style 保存本局随机分配给该玩家的轻微风格差异。
    double style = static_cast<double>(robot_progressstyle[playerindex]) / 100.0;
    // score 保存该走法评分。
    int score = static_cast<int>(progressgain * 12000.0 * urgency * style);

    // fromingoal 和 targetingoal 表示移动前后是否位于己方最终区。
    bool fromingoal = robot_contains(player.targetids, fromid);
    bool targetingoal = robot_contains(player.targetids, targetid);
    if (!fromingoal && targetingoal)
    {
        score += 9000;
    }
    else if (fromingoal && targetingoal)
    {
        // 已入营棋子继续向营地深处整理，而不是在营地边缘来回横跳。
        score += static_cast<int>(progressgain * 6000.0);
    }

    // starts 保存起始区孔位，用来鼓励尽快疏散后排棋子。
    std::vector<int> starts = board_get_zone(board, player.arm, state.piececount);
    bool frominstart = robot_contains(starts, fromid);
    bool targetinstart = robot_contains(starts, targetid);
    if (frominstart && !targetinstart)
    {
        score += 1400;
    }
    else if (!frominstart && targetinstart)
    {
        score -= 4200;
    }


    // fromrisk 和 targetrisk 表示移动前后停在对手最终区的风险等级。
    int fromrisk = robot_opponent_zone_risk(state, playerindex, fromid);
    int targetrisk = robot_opponent_zone_risk(state, playerindex, targetid);
    if (targetrisk == 3)
    {
        score -= 50000;
    }
    else if (targetrisk == 2)
    {
        score -= 26000;
    }
    else if (targetrisk == 1)
    {
        score -= 4200;
    }
    if (fromrisk > 0 && targetrisk == 0)
    {
        score += fromrisk == 3 ? 22000 : (fromrisk == 2 ? 11000 : 4500);
    }

    // movedistance 保存本次移动的直线距离，适度鼓励有效长跳。
    double movedistance = robot_distance(board.points[fromid], board.points[targetid]);
    if (movedistance > 41.0 && progressgain > 0.0)
    {
        score += static_cast<int>(movedistance * 2.0);
    }
    if (progressgain < -0.01)
    {
        score -= 1800 + static_cast<int>(-progressgain * 9000.0);
    }
    else if (progressgain > 0.015)
    {
        score += 600;
    }
    else
    {
        score -= 350 + robot_stagnation[playerindex] * 180;
    }

    if (reverse)
    {
        score -= 60000;
    }
    score -= repeatcount * 45000;

    // edgecount 保存该棋子过去重复走同一方向边的次数。
    int edgecount = robot_edgecounts[robot_edge_key(pieceindex, fromid, targetid)];
    score -= edgecount * 9000;
    return score;
}

// 这个函数重置机器人内部计时状态。
void robot_reset()
{
    robot_player = -1;
    robot_order = -1;
    robot_starttick = 0;
    robot_stage = 0;
    robot_currentdecision = { false, -1, -1 };
    robot_currentfromid = -1;
    robot_currentgain = 0.0;
}

// 这个函数开始一局新的机器人对局并清空重复局面记忆。
void robot_new_game()
{
    robot_reset();
    robot_positioncounts.clear();
    robot_playerpositioncounts.clear();
    robot_edgecounts.clear();
    for (int index = 0; index < 6; ++index)
    {
        robot_stagnation[index] = 0;
    }

    // styles 保存六种实力接近但不完全相同的推进风格，并在每局随机分配。
    int styles[6] = { 96, 98, 100, 102, 104, 106 };
    std::shuffle(styles, styles + 6, robot_random);
    for (int index = 0; index < 6; ++index)
    {
        robot_progressstyle[index] = styles[index];
    }
}

// 这个函数判断当前是否轮到机器人行动。
bool robot_is_turn(const gamestate& state)
{
    if (state.phase != phase_select_piece && state.phase != phase_select_target)
    {
        return false;
    }

    // playerindex 保存当前行动玩家下标。
    int playerindex = robot_current_player(state);
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return false;
    }
    return state.players[playerindex].isrobot && !state.players[playerindex].lost && !state.players[playerindex].finished;
}

// 这个函数根据当前棋盘局势选择机器人最优走法。
robotdecision robot_make_decision(const boarddata& board, const gamestate& state)
{
    robotdecision decision{ false, -1, -1 };
    // playerindex 保存当前机器人玩家下标。
    int playerindex = robot_current_player(state);
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return decision;
    }

    // currenthash 保存本回合行动前局面哈希，并记录它已经出现一次。
    std::uint64_t currenthash = robot_position_hash(state, -1, -1);
    int currentrepeat = ++robot_positioncounts[currenthash];
    // currentlocalhash 保存本方棋子布局，其他玩家走动不能掩盖本方循环。
    std::uint64_t currentlocalhash = robot_player_position_hash(state, playerindex, -1, -1);
    int currentlocalrepeat = ++robot_playerpositioncounts[currentlocalhash];
    const playerdata& player = state.players[playerindex];
    // evacuationplanningactive 表示仍需规划初始终点区的撤离顺序，避免提前拆掉跳板。
    bool evacuationplanningactive = robot_has_evacuation_piece(state, playerindex, 1);
    // emergencyactive 表示对方最终区已经达到三成且仍有本方棋子滞留。
    bool emergencyactive = robot_has_evacuation_piece(state, playerindex, 3);
    // candidates 保存全部合法候选走法。
    std::vector<robotcandidate> candidates;
    for (int pieceindex : player.pieceids)
    {
        if (pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()) ||
            state.pieces[pieceindex].pointid < 0)
        {
            continue;
        }

        // fromid 保存该棋子当前位置。
        int fromid = state.pieces[pieceindex].pointid;
        std::vector<int> targets = board_get_targets(board, state, pieceindex);
        for (int targetid : targets)
        {
            // nexthash 保存执行候选走法后的棋盘哈希。
            std::uint64_t nexthash = robot_position_hash(state, pieceindex, targetid);
            // repeatcount 保存候选局面过去出现的次数。
            int repeatcount = robot_positioncounts[nexthash];
            // localrepeatcount 保存候选本方布局过去出现的次数。
            std::uint64_t nextlocalhash = robot_player_position_hash(state, playerindex,
                pieceindex, targetid);
            int localrepeatcount = robot_playerpositioncounts[nextlocalhash];
            // reverse 表示该走法是否立即原路撤回。
            bool reverse = robot_is_reverse_move(state, pieceindex, targetid);
            // progressgain 接收该候选走法的推进量。
            double progressgain = 0.0;
            // score 保存该候选走法评分。
            int score = robot_score_move(board, state, player, pieceindex, fromid, targetid,
                repeatcount, reverse, progressgain);
            score -= localrepeatcount * 100000;
            // emergencyvalue 保存模拟执行本步后，紧急撤离局面的改善程度。
            int emergencyvalue = 0;
            if (evacuationplanningactive)
            {
                gamestate simulated = state;
                simulated.pieces[pieceindex].pointid = targetid;
                emergencyvalue = robot_emergency_escape_value(board, simulated, playerindex);
            }
            int fromrisk = robot_opponent_zone_risk(state, playerindex, fromid);
            int targetrisk = robot_opponent_zone_risk(state, playerindex, targetid);
            int evacuationlayer = robot_opponent_zone_layer(state, playerindex, fromid);
            bool evacuationmove = fromrisk > 0;
            bool leaveszone = fromrisk > 0 && targetrisk == 0;
            candidates.push_back({ { true, pieceindex, targetid }, score, repeatcount,
                localrepeatcount, reverse,
                progressgain, emergencyvalue, fromrisk, evacuationlayer, evacuationmove, leaveszone });
        }
    }

    if (candidates.empty())
    {
        return decision;
    }

    if (emergencyactive)
    {
        // 三成警戒触发后，能一步离场时停止其他一切选择，只保留直接离场走法。
        bool hasemergencyleaving = std::any_of(candidates.begin(), candidates.end(),
            [](const robotcandidate& candidate)
            {
                return candidate.evacuationrisk >= 3 && candidate.leaveszone;
            });
        if (hasemergencyleaving)
        {
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                [](const robotcandidate& candidate)
                {
                    return candidate.evacuationrisk < 3 || !candidate.leaveszone;
                }), candidates.end());
        }
        else
        {
            int innermostlayer = 1000; // innermostlayer 保存警戒区内当前能动棋子的最深层号。
            bool hasemergencymove = false; // hasemergencymove 表示至少有警戒区棋子能够向外推进。
            for (const robotcandidate& candidate : candidates)
            {
                if (candidate.evacuationrisk >= 3 && candidate.evacuationmove &&
                    candidate.evacuationlayer >= 0)
                {
                    hasemergencymove = true;
                    if (candidate.evacuationlayer < innermostlayer)
                    {
                        innermostlayer = candidate.evacuationlayer;
                    }
                }
            }
            if (hasemergencymove)
            {
                candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                    [&](const robotcandidate& candidate)
                    {
                        return candidate.evacuationrisk < 3 || !candidate.evacuationmove ||
                            candidate.evacuationlayer != innermostlayer;
                    }), candidates.end());
            }
        }
    }
    else if (evacuationplanningactive)
    {
        // innermostlayer 保存当前存在合法走法的滞留棋子中最靠近角尖的层号。
        int innermostlayer = 1000;
        bool hasdirectevacuation = false;
        for (const robotcandidate& candidate : candidates)
        {
            if (candidate.evacuationmove && candidate.evacuationlayer >= 0)
            {
                hasdirectevacuation = true;
                if (candidate.evacuationlayer < innermostlayer)
                {
                    innermostlayer = candidate.evacuationlayer;
                }
            }
        }
        if (hasdirectevacuation)
        {
            // 永远先推进最深处且当前能动的棋子，避免角尖棋子被留到最后后彻底堵死。
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                [&](const robotcandidate& candidate)
                {
                    return !candidate.evacuationmove || candidate.evacuationlayer != innermostlayer;
                }), candidates.end());
        }
    }

    if (emergencyactive)
    {
        // bestemergencyvalue 保存所有走法中最有利于及时离场的局面评价。
        int bestemergencyvalue = candidates.front().emergencyvalue;
        for (const robotcandidate& candidate : candidates)
        {
            if (candidate.emergencyvalue > bestemergencyvalue)
            {
                bestemergencyvalue = candidate.emergencyvalue;
            }
        }
        // 三成警戒触发后，普通推进与配合评分不能覆盖撤离任务。
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                return candidate.emergencyvalue < bestemergencyvalue;
            }), candidates.end());
    }

    if (evacuationplanningactive && !emergencyactive && !candidates.empty())
    {
        // 警戒线之前也按最安全的撤离顺序行动，防止深处棋子因跳板过早撤走而被困住。
        int bestplanningvalue = candidates.front().emergencyvalue;
        for (const robotcandidate& candidate : candidates)
        {
            if (candidate.emergencyvalue > bestplanningvalue)
            {
                bestplanningvalue = candidate.emergencyvalue;
            }
        }
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                return candidate.emergencyvalue < bestplanningvalue;
        }), candidates.end());
    }

    // 三成警戒后仍有人撤离时，区域主人继续走棋但不再选择会进一步封住出口的落点。
    if (robot_goal_evacuation_warning(state, playerindex) && !candidates.empty())
    {
        bool hasoutsidegoal = std::any_of(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                return !robot_contains(player.targetids, candidate.decision.targetid);
            });
        if (hasoutsidegoal)
        {
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                [&](const robotcandidate& candidate)
                {
                    return robot_contains(player.targetids, candidate.decision.targetid);
                }), candidates.end());
        }
    }

    // 残局只推进尚未进入己方最终区的棋子，避免九成完成后整理已完成棋子而反复循环。
    int finishedcount = board_count_finished(state, playerindex);
    if (!evacuationplanningactive && finishedcount >= state.piececount - 2 && !candidates.empty())
    {
        bool hasentry = std::any_of(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                int fromid = state.pieces[candidate.decision.pieceindex].pointid;
                return !robot_contains(player.targetids, fromid) &&
                    robot_contains(player.targetids, candidate.decision.targetid);
            });
        bool hasforwardstraggler = std::any_of(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                int fromid = state.pieces[candidate.decision.pieceindex].pointid;
                return !robot_contains(player.targetids, fromid) && candidate.progressgain > 0.015;
            });
        bool hasstragglermove = std::any_of(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                int fromid = state.pieces[candidate.decision.pieceindex].pointid;
                return !robot_contains(player.targetids, fromid);
            });

        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                int fromid = state.pieces[candidate.decision.pieceindex].pointid;
                bool straggler = !robot_contains(player.targetids, fromid);
                if (hasentry)
                {
                    return !straggler || !robot_contains(player.targetids, candidate.decision.targetid);
                }
                if (hasforwardstraggler)
                {
                    return !straggler || candidate.progressgain <= 0.015;
                }
                return hasstragglermove && !straggler;
            }), candidates.end());
    }

    // hasforward 保存是否存在真正向终点推进的候选走法。
    bool hasforward = std::any_of(candidates.begin(), candidates.end(),
        [](const robotcandidate& candidate)
        {
            return candidate.progressgain > 0.015;
        });
    // hasnonreverse 保存是否存在不立即撤回的候选走法。
    bool hasnonreverse = std::any_of(candidates.begin(), candidates.end(),
        [](const robotcandidate& candidate)
        {
            return !candidate.reverse;
        });
    // hasunseen 保存是否存在不会回到历史整盘局面的候选走法。
    bool hasunseen = std::any_of(candidates.begin(), candidates.end(),
        [](const robotcandidate& candidate)
        {
            return candidate.repeatcount == 0;
        });
    // haslocalunseen 表示本步不会回到该玩家自己的历史棋子布局。
    bool haslocalunseen = std::any_of(candidates.begin(), candidates.end(),
        [](const robotcandidate& candidate)
        {
            return candidate.localrepeatcount == 0;
        });

    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
        [&](const robotcandidate& candidate)
        {
            if (hasnonreverse && candidate.reverse)
            {
                return true;
            }
            if (hasunseen && candidate.repeatcount > 0)
            {
                return true;
            }
            if (haslocalunseen && candidate.localrepeatcount > 0)
            {
                return true;
            }
            // 连续六回合没有推进后，只要存在前进棋，就不再选择横移或后退。
            return robot_stagnation[playerindex] >= 6 && hasforward && candidate.progressgain <= 0.015;
        }), candidates.end());

    if (candidates.empty())
    {
        return decision;
    }

    // 本方布局第二次重复后强制选择推进量最大的走法，阻止多枚棋子组成长循环。
    if (currentlocalrepeat >= 2)
    {
        double bestprogress = candidates.front().progressgain; // bestprogress 保存当前最大推进量。
        for (const robotcandidate& candidate : candidates)
        {
            if (candidate.progressgain > bestprogress)
            {
                bestprogress = candidate.progressgain;
            }
        }
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [&](const robotcandidate& candidate)
            {
                return candidate.progressgain + 0.000001 < bestprogress;
            }), candidates.end());
    }

    // bestscore 保存过滤后最高候选评分。
    int bestscore = candidates.front().score;
    for (const robotcandidate& candidate : candidates)
    {
        if (candidate.score > bestscore)
        {
            bestscore = candidate.score;
        }
    }

    // margin 保存高质量候选允许的评分差，给对称局面保留多种可靠走法，重复时只选最优推进。
    int margin = currentrepeat >= 3 || currentlocalrepeat >= 2 ? 0 : 650;
    // shortlist 保存分数接近最优值的高质量候选。
    std::vector<int> shortlist;
    // weights 保存高质量候选的随机权重，分数越高越容易被选中。
    std::vector<double> weights;
    for (int index = 0; index < static_cast<int>(candidates.size()); ++index)
    {
        if (candidates[index].score >= bestscore - margin)
        {
            shortlist.push_back(index);
            weights.push_back(static_cast<double>(candidates[index].score - (bestscore - margin) + 1));
        }
    }

    // distribution 只在高质量候选范围内制造每局差异。
    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    int selected = shortlist[distribution(robot_random)];
    decision = candidates[selected].decision;
    robot_currentgain = candidates[selected].progressgain;
    robot_currentfromid = state.pieces[decision.pieceindex].pointid;
    return decision;
}

// 这个函数推进机器人五秒行动流程并返回本帧要展示或执行的动作。
robotaction robot_update(const boarddata& board, const gamestate& state)
{
    robotaction action{};
    action.kind = robot_action_wait;
    action.decision = robot_currentdecision;
    action.remainingseconds = 5;

    if (!robot_is_turn(state))
    {
        robot_reset();
        return action;
    }

    // playerindex 保存当前机器人玩家下标。
    int playerindex = robot_current_player(state);
    // now 保存当前系统毫秒计数。
    DWORD now = GetTickCount();
    if (robot_player != playerindex || robot_order != state.currentorderindex || robot_starttick == 0)
    {
        robot_player = playerindex;
        robot_order = state.currentorderindex;
        robot_starttick = now;
        robot_stage = 0;
        robot_currentdecision = robot_make_decision(board, state);
    }

    // elapsed 保存机器人本回合已经展示的毫秒数。
    DWORD elapsed = now - robot_starttick;
    // remaining 保存距离五秒行动完成还剩的毫秒数。
    DWORD remaining = elapsed >= robot_move_delay ? 0 : robot_move_delay - elapsed;
    action.decision = robot_currentdecision;
    action.remainingseconds = static_cast<int>((remaining + 999) / 1000);

    if (!robot_currentdecision.valid)
    {
        if (elapsed >= robot_move_delay)
        {
            robot_stage = 3;
            action.kind = robot_action_skip;
        }
        return action;
    }

    if (robot_stage == 0 && elapsed >= robot_piece_delay)
    {
        robot_stage = 1;
        action.kind = robot_action_select_piece;
        return action;
    }
    if (robot_stage == 1 && elapsed >= robot_target_delay)
    {
        robot_stage = 2;
        action.kind = robot_action_select_target;
        return action;
    }
    if (robot_stage == 2 && elapsed >= robot_move_delay)
    {
        robot_stage = 3;
        action.kind = robot_action_move;
        if (robot_currentfromid >= 0)
        {
            ++robot_edgecounts[robot_edge_key(robot_currentdecision.pieceindex,
                robot_currentfromid, robot_currentdecision.targetid)];
        }
        if (robot_currentgain > 0.015)
        {
            robot_stagnation[playerindex] = 0;
        }
        else
        {
            ++robot_stagnation[playerindex];
        }
        return action;
    }
    return action;
}
