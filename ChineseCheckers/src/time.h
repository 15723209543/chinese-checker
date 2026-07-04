#pragma once

#include "gamedata.h"
#include "logger.h"

#include <string>

// 这个函数重置计时设置和运行状态。
void time_reset();

// 这个函数设置每步时长。
void time_set_step_seconds(int seconds);

// 这个函数设置每名玩家总时长。
void time_set_total_seconds(int seconds);

// 这个函数返回每步时长。
int time_get_step_seconds();

// 这个函数返回每名玩家总时长。
int time_get_total_seconds();

// 这个函数返回指定玩家的总剩余时间。
int time_get_player_remaining(int playerindex);

// 这个函数返回当前这一步剩余时间。
int time_get_step_remaining();

// 这个函数开始整局计时。
void time_start_game(const gamestate& state);

// 这个函数开始当前玩家回合计时。
void time_start_turn(const gamestate& state);

// 这个函数结算当前回合已经消耗的时间。
void time_finish_turn();

// 这个函数停止计时。
void time_stop();

// 这个函数每帧更新计时，并在超时时改写游戏状态。
void time_update(const boarddata& board, gamestate& state, loggerdata& logger);

// 这个函数把秒数格式化成 mm:ss。
std::wstring time_format_seconds(int seconds);
