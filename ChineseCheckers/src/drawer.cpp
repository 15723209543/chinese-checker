#include "drawer.h"

#include "board.h"
#include "keyboard.h"
#include "mapdata.h"

#include <algorithm>
#include <string>

// 这个函数判断列表里是否包含指定孔位。
static bool drawer_contains(const std::vector<int>& values, int value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

// 这个函数用指定矩形绘制一行文字。
static void drawer_text_rect(int left, int top, int right, int bottom, const std::wstring& text, int color, unsigned int format)
{
    RECT rect{ left, top, right, bottom }; // rect 保存文字绘制区域。
    settextcolor(color);
    drawtext(text.c_str(), &rect, format);
}

// 这个函数在指定圆形区域中心绘制编号。
static void drawer_number_text(int centerx, int centery, const std::wstring& text, int color)
{
    RECT rect{ centerx - 16, centery - 12, centerx + 16, centery + 12 }; // rect 保存编号绘制区域。
    settextstyle(16, 0, L"微软雅黑");
    settextcolor(color);
    drawtext(text.c_str(), &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// 这个函数绘制右侧按钮。
static void drawer_button(const buttondata& button)
{
    // fillcolor 保存按钮背景色。
    int fillcolor = button.enabled ? RGB(245, 248, 252) : RGB(226, 229, 234);
    // linecolor 保存按钮边框色。
    int linecolor = button.enabled ? RGB(82, 103, 132) : RGB(170, 176, 184);
    setfillcolor(fillcolor);
    setlinecolor(linecolor);
    solidrectangle(button.left, button.top, button.right, button.bottom);
    rectangle(button.left, button.top, button.right, button.bottom);

    drawer_text_rect(button.left, button.top, button.right, button.bottom, button.text,
        button.enabled ? RGB(32, 42, 56) : RGB(126, 132, 141), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// 这个函数绘制棋盘孔位底色。
static void drawer_holes(const boarddata& board, const gamestate& state)
{
    // currentplayer 保存当前行动玩家下标。
    int currentplayer = -1;
    if (!state.turnorder.empty() && state.currentorderindex >= 0 && state.currentorderindex < static_cast<int>(state.turnorder.size()))
    {
        currentplayer = state.turnorder[state.currentorderindex];
    }

    for (const pointdata& point : board.points)
    {
        // istarget 表示该孔位是否是当前玩家停车区。
        bool istarget = currentplayer >= 0 && drawer_contains(state.players[currentplayer].targetids, point.id);
        setlinecolor(istarget ? RGB(50, 150, 92) : RGB(178, 186, 194));
        setfillcolor(istarget ? RGB(226, 247, 234) : RGB(246, 248, 250));
        solidcircle(static_cast<int>(point.x), static_cast<int>(point.y), map_hole_radius);
        circle(static_cast<int>(point.x), static_cast<int>(point.y), map_hole_radius);
    }
}

// 这个函数绘制当前可落点标记。
static void drawer_targets(const boarddata& board, const gamestate& state)
{
    for (int pointid : state.movetargets)
    {
        const pointdata& point = board.points[pointid];
        setlinecolor(RGB(24, 146, 88));
        setlinestyle(PS_SOLID, 3);
        circle(static_cast<int>(point.x), static_cast<int>(point.y), map_target_radius);
        setlinestyle(PS_SOLID, 1);

        // number 保存键盘操作时该落点的编号。
        int number = keyboard_get_target_number(state, pointid);
        if (number > 0)
        {
            drawer_number_text(static_cast<int>(point.x), static_cast<int>(point.y),
                std::to_wstring(number), RGB(24, 146, 88));
        }
    }
}

// 这个函数绘制所有棋子。
static void drawer_pieces(const boarddata& board, const gamestate& state)
{
    for (int index = 0; index < static_cast<int>(state.pieces.size()); ++index)
    {
        const piecedata& piece = state.pieces[index];
        const pointdata& point = board.points[piece.pointid];
        const playerdata& player = state.players[piece.owner];

        setfillcolor(player.color);
        setlinecolor(index == state.selectedpiece ? RGB(35, 35, 35) : RGB(255, 255, 255));
        solidcircle(static_cast<int>(point.x), static_cast<int>(point.y), map_piece_radius);
        setlinestyle(PS_SOLID, index == state.selectedpiece ? 4 : 2);
        circle(static_cast<int>(point.x), static_cast<int>(point.y), map_piece_radius);
        setlinestyle(PS_SOLID, 1);

        // number 保存键盘操作时该棋子的编号。
        int number = keyboard_get_piece_number(state, index);
        if (number >= 0)
        {
            drawer_number_text(static_cast<int>(point.x), static_cast<int>(point.y),
                std::to_wstring(number), RGB(255, 255, 255));
        }
    }
}

// 这个函数绘制玩家进度条。
static void drawer_progress(const gamestate& state, int playerindex, int top)
{
    const playerdata& player = state.players[playerindex];
    // finished 保存已进入停车区数量。
    int finished = board_count_finished(state, playerindex);
    // rate 保存完成比例。
    double rate = state.piececount > 0 ? static_cast<double>(finished) / static_cast<double>(state.piececount) : 0.0;
    // left 保存进度条左边界。
    int left = map_left_width + 34;
    // width 保存进度条总宽度。
    int width = 252;

    setfillcolor(player.color);
    solidrectangle(left, top + 3, left + 18, top + 21);
    setlinecolor(RGB(255, 255, 255));
    rectangle(left, top + 3, left + 18, top + 21);

    std::wstring title = L"玩家" + std::to_wstring(player.id) + L" " + player.colorname + L"  " +
        std::to_wstring(finished) + L"/" + std::to_wstring(state.piececount);
    drawer_text_rect(left + 28, top, map_window_width - 24, top + 28, title, RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    setfillcolor(RGB(231, 235, 241));
    solidrectangle(left, top + 32, left + width, top + 46);
    setfillcolor(player.color);
    solidrectangle(left, top + 32, left + static_cast<int>(width * rate), top + 46);
    setlinecolor(RGB(168, 176, 188));
    rectangle(left, top + 32, left + width, top + 46);
}

// 这个函数绘制键盘操作编号提示。
static void drawer_keyboard_info(const gamestate& state, int top)
{
    if (state.phase == phase_select_piece)
    {
        // text 保存当前玩家棋子编号列表。
        std::wstring text = keyboard_make_piece_text(state);
        if (!text.empty())
        {
            drawer_text_rect(map_left_width + 34, top, map_window_width - 24, top + 26,
                L"当前棋子编号：" + text, RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    else if (state.phase == phase_select_target)
    {
        // text 保存当前可走位置编号列表。
        std::wstring text = keyboard_make_target_text(state);
        if (!text.empty())
        {
            drawer_text_rect(map_left_width + 34, top, map_window_width - 24, top + 26,
                L"可走位置编号：" + text, RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            drawer_text_rect(map_left_width + 34, top + 26, map_window_width - 24, top + 52,
                L"0：返回上一步", RGB(90, 99, 113), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

// 这个函数绘制右侧信息栏。
static void drawer_info(const gamestate& state)
{
    setfillcolor(RGB(250, 251, 253));
    solidrectangle(map_left_width, 0, map_window_width, map_window_height);
    setlinecolor(RGB(210, 216, 224));
    line(map_left_width, 0, map_left_width, map_window_height);

    settextstyle(24, 0, L"微软雅黑");
    drawer_text_rect(map_left_width + 34, 26, map_window_width - 24, 62, L"跳棋游戏", RGB(20, 28, 38), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    settextstyle(18, 0, L"微软雅黑");
    drawer_text_rect(map_left_width + 34, 74, map_window_width - 24, 104, L"阶段：" + gamedata_phase_text(state.phase), RGB(65, 76, 92), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (!state.turnorder.empty())
    {
        // playerindex 保存当前玩家下标。
        int playerindex = state.turnorder[state.currentorderindex];
        const playerdata& player = state.players[playerindex];
        std::wstring current = L"当前是" + std::to_wstring(player.id) + L"号玩家（" + player.colorname + L"）";
        drawer_text_rect(map_left_width + 34, 106, map_window_width - 24, 140, current, player.color, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    drawer_text_rect(map_left_width + 34, 148, map_window_width - 24, 190, state.status, RGB(90, 99, 113), DT_LEFT | DT_WORDBREAK);

    int progress_top = 210; // progress_top 保存玩家进度区顶部。
    if (!state.players.empty())
    {
        drawer_text_rect(map_left_width + 34, progress_top - 34, map_window_width - 24, progress_top - 6,
            L"每一位玩家完成进度", RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    for (int index = 0; index < static_cast<int>(state.players.size()); ++index)
    {
        drawer_progress(state, index, progress_top + index * 58);
    }

    // keyboard_top 保存键盘编号提示区顶部。
    int keyboard_top = progress_top + static_cast<int>(state.players.size()) * 58 + 16;
    drawer_keyboard_info(state, keyboard_top);

    for (const buttondata& button : state.buttons)
    {
        drawer_button(button);
    }
}

// 这个函数绘制完整游戏界面。
void drawer_draw(const boarddata& board, const gamestate& state)
{
    setbkcolor(RGB(238, 242, 246));
    cleardevice();

    setfillcolor(RGB(255, 255, 255));
    solidrectangle(0, 0, map_left_width, map_window_height);

    drawer_holes(board, state);
    drawer_targets(board, state);
    drawer_pieces(board, state);
    drawer_info(state);
}

// 这个函数检查鼠标是否点击了按钮。
bool drawer_hit_button(const gamestate& state, int x, int y, int& code, int& value)
{
    for (const buttondata& button : state.buttons)
    {
        if (x >= button.left && x <= button.right && y >= button.top && y <= button.bottom)
        {
            code = button.enabled ? button.code : button_none;
            value = button.value;
            return true;
        }
    }
    code = button_none;
    value = 0;
    return false;
}
