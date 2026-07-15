#include "board.h"

#include "mapdata.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <set>

// 这个函数计算两个屏幕点之间的距离。
static double board_distance(double ax, double ay, double bx, double by)
{
    // dx 保存横向距离。
    double dx = ax - bx;
    // dy 保存纵向距离。
    double dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

// 这个函数根据棋子数量返回三角区边长。
static int board_side_length(int piececount)
{
    if (piececount == 3)
    {
        return 2;
    }
    if (piececount == 6)
    {
        return 3;
    }
    return 4;
}

// 这个函数按行列把一个孔位加入三角区列表。
static void board_push_row(const boarddata& board, int row, int startcol, int count, std::vector<int>& zone)
{
    for (int index = 0; index < count; ++index)
    {
        // col 保存当前要加入的列。
        int col = startcol + index;
        if (row >= 0 && row < map_row_count && col >= 0 && col < map_row_lengths[row])
        {
            zone.push_back(board.idmap[row][col]);
        }
    }
}

// 这个函数把三角区孔位按真实角尖到棋盘中心的层级重新排序。
static void board_reorder_zone(const boarddata& board, int arm, std::vector<int>& zone)
{
    if (zone.empty())
    {
        return;
    }

    int tipid = zone.front(); // tipid 保存该三角区真实角尖孔位。
    for (int pointid : zone)
    {
        const pointdata& point = board.points[pointid];
        const pointdata& tip = board.points[tipid];
        bool better = false; // better 表示当前孔位比已选孔位更靠近该方向角尖。
        if (arm == 0)
        {
            better = point.y < tip.y;
        }
        else if (arm == 3)
        {
            better = point.y > tip.y;
        }
        else if (arm == 1 || arm == 2)
        {
            better = point.x > tip.x;
        }
        else
        {
            better = point.x < tip.x;
        }
        if (better)
        {
            tipid = pointid;
        }
    }

    // distances 保存区域内每个孔位距离真实角尖的相邻步数。
    std::vector<int> distances(board.points.size(), -1);
    std::queue<int> queue;
    distances[tipid] = 0;
    queue.push(tipid);
    while (!queue.empty())
    {
        int currentid = queue.front(); // currentid 保存本次扩展孔位。
        queue.pop();
        for (int nearid : board.points[currentid].nearids)
        {
            if (distances[nearid] >= 0 ||
                std::find(zone.begin(), zone.end(), nearid) == zone.end())
            {
                continue;
            }
            distances[nearid] = distances[currentid] + 1;
            queue.push(nearid);
        }
    }

    std::sort(zone.begin(), zone.end(), [&](int left, int right)
        {
            if (distances[left] != distances[right])
            {
                return distances[left] < distances[right];
            }
            return left < right;
        });
}

// 这个函数根据坐标查找对应孔位，主要用于预计算跳跃中点。
static int board_find_point_by_position(const boarddata& board, double x, double y)
{
    for (const pointdata& point : board.points)
    {
        if (board_distance(point.x, point.y, x, y) < 1.0)
        {
            return point.id;
        }
    }
    return -1;
}

// 这个函数判断连续跳跃的模拟局面中某个孔位是否有棋子。
static bool board_has_piece_virtual(const gamestate& state, int pointid, int startid, int currentid)
{
    if (pointid == currentid)
    {
        return true;
    }
    if (pointid == startid)
    {
        return false;
    }
    return board_point_has_piece(state, pointid);
}

// 这个函数判断连续跳跃的模拟局面中某个孔位是否为空。
static bool board_empty_virtual(const gamestate& state, int pointid, int startid, int currentid)
{
    if (pointid == startid || pointid == currentid)
    {
        return false;
    }
    return !board_point_has_piece(state, pointid);
}

// 这个函数生成棋盘孔位、相邻边和跳跃边。
void board_build(boarddata& board)
{
    board.points.clear();
    board.jumps.clear();

    for (int row = 0; row < map_row_count; ++row)
    {
        for (int col = 0; col < map_max_col_count; ++col)
        {
            board.idmap[row][col] = -1;
        }
    }

    for (int row = 0; row < map_row_count; ++row)
    {
        // length 保存当前行孔位数量。
        int length = map_row_lengths[row];
        for (int col = 0; col < length; ++col)
        {
            pointdata point{};
            point.id = static_cast<int>(board.points.size());
            point.row = row;
            point.col = col;
            point.x = map_board_center_x + (static_cast<double>(col) - (static_cast<double>(length) - 1.0) / 2.0) * map_point_space;
            point.y = map_board_top_y + static_cast<double>(row) * map_row_space;
            board.idmap[row][col] = point.id;
            board.points.push_back(point);
        }
    }

    for (pointdata& point : board.points)
    {
        for (const pointdata& other : board.points)
        {
            if (point.id == other.id)
            {
                continue;
            }

            // distance 保存两个孔位的屏幕距离。
            double distance = board_distance(point.x, point.y, other.x, other.y);
            if (std::fabs(distance - map_point_space) < 1.1)
            {
                point.nearids.push_back(other.id);
            }
        }
    }

    board.jumps.assign(board.points.size(), std::vector<jumpdata>());
    for (const pointdata& point : board.points)
    {
        for (const pointdata& other : board.points)
        {
            if (point.id == other.id)
            {
                continue;
            }

            // distance 保存两个孔位的屏幕距离。
            double distance = board_distance(point.x, point.y, other.x, other.y);
            if (std::fabs(distance - map_point_space * 2.0) < 1.2)
            {
                // midx 和 midy 保存跳跃中点坐标。
                double midx = (point.x + other.x) / 2.0;
                double midy = (point.y + other.y) / 2.0;
                // overid 保存被跳过的孔位。
                int overid = board_find_point_by_position(board, midx, midy);
                if (overid >= 0)
                {
                    board.jumps[point.id].push_back({ overid, other.id });
                }
            }
        }
    }
}

// 这个函数根据人数返回对称分配的起点角。
std::vector<int> board_get_arms(int playercount)
{
    if (playercount == 2)
    {
        return { 0, 3 };
    }
    if (playercount == 3)
    {
        return { 0, 2, 4 };
    }
    if (playercount == 4)
    {
        return { 0, 2, 3, 5 };
    }
    if (playercount == 5)
    {
        return { 0, 1, 2, 3, 4 };
    }
    return { 0, 1, 2, 3, 4, 5 };
}

// 这个函数返回指定角和棋子数量对应的三角区孔位。
std::vector<int> board_get_zone(const boarddata& board, int arm, int piececount)
{
    // side 保存三角区边长。
    int side = board_side_length(piececount);
    // zone 保存最终孔位列表。
    std::vector<int> zone;

    if (arm == 0)
    {
        for (int layer = 0; layer < side; ++layer)
        {
            board_push_row(board, layer, 0, layer + 1, zone);
        }
    }
    else if (arm == 1)
    {
        for (int layer = 0; layer < side; ++layer)
        {
            // row 从右上角尖端向棋盘中心推进。
            int row = 7 - layer;
            int count = layer + 1;
            board_push_row(board, row, map_row_lengths[row] - count, count, zone);
        }
    }
    else if (arm == 2)
    {
        for (int layer = 0; layer < side; ++layer)
        {
            int row = 9 + layer;
            int count = layer + 1;
            board_push_row(board, row, map_row_lengths[row] - count, count, zone);
        }
    }
    else if (arm == 3)
    {
        for (int layer = 0; layer < side; ++layer)
        {
            int row = 16 - layer;
            board_push_row(board, row, 0, layer + 1, zone);
        }
    }
    else if (arm == 4)
    {
        for (int layer = 0; layer < side; ++layer)
        {
            int row = 9 + layer;
            board_push_row(board, row, 0, layer + 1, zone);
        }
    }
    else if (arm == 5)
    {
        for (int layer = 0; layer < side; ++layer)
        {
            int row = 7 - layer;
            board_push_row(board, row, 0, layer + 1, zone);
        }
    }

    // 斜向三角区按行收集时并不等于真实层级，统一通过图距离校正。
    board_reorder_zone(board, arm, zone);
    return zone;
}

// 这个函数根据鼠标坐标查找被点击的孔位。
int board_find_point_at(const boarddata& board, int x, int y)
{
    for (const pointdata& point : board.points)
    {
        if (board_distance(point.x, point.y, static_cast<double>(x), static_cast<double>(y)) <= map_piece_radius + 5)
        {
            return point.id;
        }
    }
    return -1;
}

// 这个函数查找指定孔位上的棋子。
int board_find_piece_at(const gamestate& state, int pointid)
{
    for (int index = 0; index < static_cast<int>(state.pieces.size()); ++index)
    {
        // piece 保存当前检查的棋子。
        const piecedata& piece = state.pieces[index];
        if (piece.pointid < 0 || piece.owner < 0 || piece.owner >= static_cast<int>(state.players.size()) ||
            state.players[piece.owner].lost)
        {
            continue;
        }

        if (piece.pointid == pointid)
        {
            return index;
        }
    }
    return -1;
}

// 这个函数判断指定孔位是否有棋子。
bool board_point_has_piece(const gamestate& state, int pointid)
{
    return board_find_piece_at(state, pointid) >= 0;
}

// 这个函数返回孔位在三角区域中从角尖向出口计算的层数，不在区域中返回 -1。
static int board_zone_layer(const std::vector<int>& zone, int pointid)
{
    // iterator 保存目标孔位在区域数据中的位置。
    auto iterator = std::find(zone.begin(), zone.end(), pointid);
    if (iterator == zone.end())
    {
        return -1;
    }

    // position 保存孔位在按层排列的区域数据中的下标。
    int position = static_cast<int>(iterator - zone.begin());
    int layer = 0; // layer 保存当前检查的三角层数。
    int layerend = 1; // layerend 保存当前层结束后的首个下标。
    while (position >= layerend)
    {
        ++layer;
        layerend += layer + 1;
    }
    return layer;
}

// 这个函数判断指定玩家能否把棋子停在目标孔位。
static bool board_point_allowed_for_player(const gamestate& state, int playerindex, int startid, int pointid)
{
    for (int zoneowner = 0; zoneowner < static_cast<int>(state.players.size()); ++zoneowner)
    {
        if (zoneowner == playerindex)
        {
            continue;
        }

        const playerdata& owner = state.players[zoneowner];
        if (std::find(owner.targetids.begin(), owner.targetids.end(), pointid) != owner.targetids.end())
        {
            // 外部棋子不能进入；原本就在区域内的棋子只允许向更靠近出口的下一层撤离。
            int startlayer = board_zone_layer(owner.targetids, startid);
            int targetlayer = board_zone_layer(owner.targetids, pointid);
            return startlayer >= 0 && targetlayer > startlayer;
        }
    }
    return true;
}

// 这个函数判断棋子从当前孔位到下一落点是否遵守己方最终区只进不出的规则。
static bool board_goal_path_allowed(const gamestate& state, int playerindex, int currentid, int targetid)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return false;
    }

    const playerdata& player = state.players[playerindex];
    // currentingoal 表示当前一步起点是否已经位于己方最终区域。
    bool currentingoal = std::find(player.targetids.begin(), player.targetids.end(), currentid) != player.targetids.end();
    // targetingoal 表示当前一步落点是否仍位于己方最终区域。
    bool targetingoal = std::find(player.targetids.begin(), player.targetids.end(), targetid) != player.targetids.end();
    return !currentingoal || targetingoal;
}

// 这个函数计算指定棋子的全部可落点。
std::vector<int> board_get_targets(const boarddata& board, const gamestate& state, int pieceindex)
{
    // targets 使用集合避免重复落点。
    std::set<int> targets;
    if (pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()))
    {
        return {};
    }
    if (state.pieces[pieceindex].owner < 0 ||
        state.pieces[pieceindex].owner >= static_cast<int>(state.players.size()) ||
        state.players[state.pieces[pieceindex].owner].lost)
    {
        return {};
    }

    // startid 保存选中棋子的起点。
    int startid = state.pieces[pieceindex].pointid;
    if (startid < 0 || startid >= static_cast<int>(board.points.size()))
    {
        return {};
    }
    for (int nearid : board.points[startid].nearids)
    {
        if (!board_point_has_piece(state, nearid) &&
            board_goal_path_allowed(state, state.pieces[pieceindex].owner, startid, nearid) &&
            board_point_allowed_for_player(state, state.pieces[pieceindex].owner, startid, nearid))
        {
            targets.insert(nearid);
        }
    }

    // queue 保存连续跳跃搜索队列。
    std::queue<int> queue;
    // visited 保存跳跃已经到达过的孔位，防止循环。
    std::set<int> visited;
    queue.push(startid);
    visited.insert(startid);

    while (!queue.empty())
    {
        // currentid 保存本次扩展的模拟位置。
        int currentid = queue.front();
        queue.pop();

        for (const jumpdata& jump : board.jumps[currentid])
        {
            if (board_has_piece_virtual(state, jump.overid, startid, currentid) &&
                board_empty_virtual(state, jump.toid, startid, currentid) &&
                board_goal_path_allowed(state, state.pieces[pieceindex].owner, currentid, jump.toid) &&
                visited.find(jump.toid) == visited.end())
            {
                visited.insert(jump.toid);
                queue.push(jump.toid);
                // 其他玩家的最终区域只允许连续跳跃穿过，或让原有棋子向出口方向撤离。
                if (board_point_allowed_for_player(state, state.pieces[pieceindex].owner, startid, jump.toid))
                {
                    targets.insert(jump.toid);
                }
            }
        }
    }

    return std::vector<int>(targets.begin(), targets.end());
}

// 这个函数返回最终区域开始保护所需的己方棋子数量。
int board_get_zone_protection_count(const gamestate& state)
{
    // 六成按整数向上取整，等价于 (6n + 9) / 10。
    return (state.piececount * 6 + 9) / 10;
}

// 这个函数判断指定玩家的最终区域是否已经进入保护状态。
bool board_player_zone_protected(const gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()) ||
        state.players[playerindex].lost)
    {
        return false;
    }

    return board_count_finished(state, playerindex) >= board_get_zone_protection_count(state);
}

// 这个函数查找达到保护阈值瞬间仍占据指定最终区域的全部违规玩家。
std::vector<int> board_find_zone_violators(const gamestate& state, int zoneowner)
{
    if (zoneowner < 0 || zoneowner >= static_cast<int>(state.players.size()) ||
        !board_player_zone_protected(state, zoneowner))
    {
        return {};
    }

    // violators 使用集合去除同一玩家多枚棋子滞留时产生的重复记录。
    std::set<int> violators;
    const playerdata& owner = state.players[zoneowner];
    for (const piecedata& piece : state.pieces)
    {
        if (piece.owner == zoneowner || piece.pointid < 0 ||
            piece.owner < 0 || piece.owner >= static_cast<int>(state.players.size()) ||
            state.players[piece.owner].lost || state.players[piece.owner].finished)
        {
            continue;
        }

        if (std::find(owner.targetids.begin(), owner.targetids.end(), piece.pointid) != owner.targetids.end())
        {
            violators.insert(piece.owner);
        }
    }
    return std::vector<int>(violators.begin(), violators.end());
}

// 这个函数统计玩家已经进入停车区的棋子数量。
int board_count_finished(const gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return 0;
    }

    // count 保存已经进入停车区的棋子数量。
    int count = 0;
    const playerdata& player = state.players[playerindex];
    if (player.lost)
    {
        return 0;
    }

    for (int pieceindex : player.pieceids)
    {
        // pointid 保存棋子当前孔位。
        int pointid = state.pieces[pieceindex].pointid;
        if (pointid < 0)
        {
            continue;
        }
        if (std::find(player.targetids.begin(), player.targetids.end(), pointid) != player.targetids.end())
        {
            ++count;
        }
    }
    return count;
}

// 这个函数判断玩家是否已经全部进入停车区。
bool board_player_finished(const gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return false;
    }
    if (state.players[playerindex].lost)
    {
        return false;
    }
    return board_count_finished(state, playerindex) == state.piececount;
}
