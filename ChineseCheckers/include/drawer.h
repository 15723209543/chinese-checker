#pragma once

#include "gamedata.h"

// 这个函数绘制完整游戏界面。
void drawer_draw(const boarddata& board, const gamestate& state);

// 这个函数检查鼠标是否点击了按钮。
bool drawer_hit_button(const gamestate& state, int x, int y, int& code, int& value);
