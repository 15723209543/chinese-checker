#pragma once

#include "gamedata.h"

struct robotdecision
{
    bool valid; // 是否找到了可执行走法。
    int pieceindex; // 机器人决定移动的棋子下标。
    int targetid; // 机器人决定落到的孔位编号。
};

enum robotactionkind
{
    robot_action_wait,
    robot_action_select_piece,
    robot_action_select_target,
    robot_action_move,
    robot_action_skip
};

struct robotaction
{
    robotactionkind kind; // 当前一帧需要执行的机器人动作。
    robotdecision decision; // 机器人为本回合选择的棋子和落点。
    int remainingseconds; // 距离机器人完成移动的倒计时秒数。
};

// 这个函数重置机器人内部计时状态。
void robot_reset();

// 这个函数开始一局新的机器人对局并清空重复局面记忆。
void robot_new_game();

// 这个函数判断当前是否轮到机器人行动。
bool robot_is_turn(const gamestate& state);

// 这个函数根据当前棋盘局势选择机器人最优走法。
robotdecision robot_make_decision(const boarddata& board, const gamestate& state);

// 这个函数推进机器人五秒行动流程并返回本帧要展示或执行的动作。
robotaction robot_update(const boarddata& board, const gamestate& state);
