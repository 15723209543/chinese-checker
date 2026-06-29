#pragma once

#include "gamedata.h"

#include <vector>

// 这个函数生成棋盘孔位、相邻边和跳跃边。
void board_build(boarddata& board);

// 这个函数根据人数返回对称分配的起点角。
std::vector<int> board_get_arms(int playercount);

// 这个函数返回指定角和棋子数量对应的三角区孔位。
std::vector<int> board_get_zone(const boarddata& board, int arm, int piececount);

// 这个函数根据鼠标坐标查找被点击的孔位。
int board_find_point_at(const boarddata& board, int x, int y);

// 这个函数查找指定孔位上的棋子。
int board_find_piece_at(const gamestate& state, int pointid);

// 这个函数判断指定孔位是否有棋子。
bool board_point_has_piece(const gamestate& state, int pointid);

// 这个函数计算指定棋子的全部可落点。
std::vector<int> board_get_targets(const boarddata& board, const gamestate& state, int pieceindex);

// 这个函数统计玩家已经进入停车区的棋子数量。
int board_count_finished(const gamestate& state, int playerindex);

// 这个函数判断玩家是否已经全部进入停车区。
bool board_player_finished(const gamestate& state, int playerindex);
