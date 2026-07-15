#pragma once

#include "gamedata.h"
#include "logger.h"

#include <string>
#include <vector>

// 这个函数处理一次键盘按键，并在非法按键时保护游戏状态。
void keyboard_handle_key(const boarddata& board, gamestate& state, loggerdata& logger, int key, int scancode, bool extended);

// 这个函数处理一次键盘字符消息，主要用于兼容小键盘数字。
void keyboard_handle_char(const boarddata& board, gamestate& state, loggerdata& logger, wchar_t keychar);

// 这个函数返回当前玩家全部棋子的全局棋子下标。
std::vector<int> keyboard_get_current_pieceids(const gamestate& state);

// 这个函数返回棋子在当前玩家手中的键盘编号，非当前玩家棋子返回 -1。
int keyboard_get_piece_number(const gamestate& state, int pieceindex);

// 这个函数返回落点的键盘编号，未编号落点返回 -1。
int keyboard_get_target_number(const gamestate& state, int pointid);

// 这个函数生成右侧信息栏显示的当前棋子编号文本。
std::wstring keyboard_make_piece_text(const gamestate& state);

// 这个函数生成右侧信息栏显示的可走位置编号文本。
std::wstring keyboard_make_target_text(const gamestate& state);
