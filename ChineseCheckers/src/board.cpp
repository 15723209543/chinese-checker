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
        if (state.pieces[index].pointid == pointid)
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

// 这个函数计算指定棋子的全部可落点。
std::vector<int> board_get_targets(const boarddata& board, const gamestate& state, int pieceindex)
{
    // targets 使用集合避免重复落点。
    std::set<int> targets;
    if (pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()))
    {
        return {};
    }

    // startid 保存选中棋子的起点。
    int startid = state.pieces[pieceindex].pointid;
    for (int nearid : board.points[startid].nearids)
    {
        if (!board_point_has_piece(state, nearid))
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
                visited.find(jump.toid) == visited.end())
            {
                visited.insert(jump.toid);
                targets.insert(jump.toid);
                queue.push(jump.toid);
            }
        }
    }

    return std::vector<int>(targets.begin(), targets.end());
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
    for (int pieceindex : player.pieceids)
    {
        // pointid 保存棋子当前孔位。
        int pointid = state.pieces[pieceindex].pointid;
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
    return board_count_finished(state, playerindex) == state.piececount;
}
