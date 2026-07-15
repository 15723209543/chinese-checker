#pragma once

#include "gamedata.h"
#include "logger.h"

// 这个函数是游戏公共入口，负责启动图形界面和主循环。
int game_run();

// 这个函数返回当前行动玩家下标。
int game_get_current_player(const gamestate& state);

// 这个函数按公共规则执行一次移动，并处理计时、排名和换手。
void game_apply_move(const boarddata& board, gamestate& state, loggerdata& logger, int pieceindex, int targetid, const std::wstring& source);

// 这个函数把一组违规或超时玩家判负，并按当前回合状态继续或结束游戏。
void game_eliminate_players(const boarddata& board, gamestate& state, loggerdata& logger,
    const std::vector<int>& playerindices, const std::wstring& reason, bool turnfinished);
