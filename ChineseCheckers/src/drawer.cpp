#include "drawer.h"

#include "analysis.h"
#include "board.h"
#include "keyboard.h"
#include "mapdata.h"
#include "time.h"

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
    int fillcolor = button.selected ? RGB(255, 255, 255) :
        (button.enabled ? RGB(235, 240, 247) : RGB(226, 229, 234));
    // linecolor 保存按钮边框色。
    int linecolor = button.selected ? RGB(55, 82, 118) :
        (button.enabled ? RGB(120, 139, 165) : RGB(170, 176, 184));
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
        bool istarget = currentplayer >= 0 && currentplayer < static_cast<int>(state.players.size()) &&
            !state.players[currentplayer].lost && drawer_contains(state.players[currentplayer].targetids, point.id);
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
        if (pointid < 0 || pointid >= static_cast<int>(board.points.size()))
        {
            continue;
        }

        const pointdata& point = board.points[pointid];
        // ispreview 表示这是机器人已经决定并正在展示的最终落点。
        bool ispreview = pointid == state.robotpreviewtarget;
        if (ispreview)
        {
            setfillcolor(RGB(220, 235, 251));
            solidcircle(static_cast<int>(point.x), static_cast<int>(point.y), map_target_radius - 2);
        }
        setlinecolor(ispreview ? RGB(39, 105, 181) : RGB(24, 146, 88));
        setlinestyle(PS_SOLID, ispreview ? 5 : 3);
        circle(static_cast<int>(point.x), static_cast<int>(point.y), map_target_radius);
        setlinestyle(PS_SOLID, 1);

        // number 保存键盘操作时该落点的编号。
        int number = keyboard_get_target_number(state, pointid);
        if (number > 0)
        {
            drawer_number_text(static_cast<int>(point.x), static_cast<int>(point.y),
                std::to_wstring(number), ispreview ? RGB(39, 105, 181) : RGB(24, 146, 88));
        }
    }
}

// 这个函数绘制所有棋子。
static void drawer_pieces(const boarddata& board, const gamestate& state)
{
    for (int index = 0; index < static_cast<int>(state.pieces.size()); ++index)
    {
        const piecedata& piece = state.pieces[index];
        if (piece.pointid < 0 || piece.pointid >= static_cast<int>(board.points.size()) ||
            piece.owner < 0 || piece.owner >= static_cast<int>(state.players.size()) ||
            state.players[piece.owner].lost)
        {
            continue;
        }

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
    int finished = player.lost ? player.lossprogress : board_count_finished(state, playerindex);
    // rate 保存完成比例。
    double rate = state.piececount > 0 ? static_cast<double>(finished) / static_cast<double>(state.piececount) : 0.0;
    // left 保存进度条左边界。
    int left = map_left_width + 40;
    // textleft 保存进度文字和进度条统一左边界。
    int textleft = left + 34;
    // right 保存进度条右边界。
    int right = map_window_width - 46;
    // width 保存进度条总宽度。
    int width = right - textleft;

    setfillcolor(player.color);
    solidrectangle(left, top + 3, left + 18, top + 21);
    setlinecolor(RGB(255, 255, 255));
    rectangle(left, top + 3, left + 18, top + 21);

    std::wstring title = L"玩家" + std::to_wstring(player.id) + L"  " + player.colorname +
        (player.isrobot ? L"  机器人" : L"  人工");
    // titlecolor 保存玩家进度标题颜色。
    int titlecolor = RGB(34, 43, 55);
    if (player.lost)
    {
        titlecolor = RGB(221, 67, 67);
    }
    drawer_text_rect(textleft, top, right, top + 19, title, titlecolor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // detail 保存玩家结果、排名、进度和总剩余时间。
    std::wstring detail;
    if (player.lost)
    {
        detail = player.lossreason + L"  第" + std::to_wstring(player.rank) + L"名";
    }
    else if (player.finished)
    {
        detail = L"已完成  第" + std::to_wstring(player.rank) + L"名";
    }
    if (!detail.empty())
    {
        detail += L"  ";
    }
    detail += L"完成" + std::to_wstring(finished) + L"/" + std::to_wstring(state.piececount) +
        L"  总剩余" + time_format_seconds(time_get_player_remaining(playerindex));
    drawer_text_rect(textleft, top + 17, right, top + 35, detail,
        player.lost ? RGB(221, 67, 67) : RGB(90, 99, 113), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    setfillcolor(RGB(231, 235, 241));
    solidrectangle(textleft, top + 36, textleft + width, top + 46);
    setfillcolor(player.color);
    solidrectangle(textleft, top + 36, textleft + static_cast<int>(width * rate), top + 46);
    setlinecolor(RGB(168, 176, 188));
    rectangle(textleft, top + 36, textleft + width, top + 46);
}

// 这个函数绘制底部局势分析。
static void drawer_analysis_info(const gamestate& state, int top)
{
    const analysisdata& data = analysis_get_data();
    if (data.players.empty())
    {
        return;
    }

    // left 保存分析区域左边界。
    int left = map_left_width + 40;
    // right 保存分析区域右边界。
    int rightlimit = map_window_width - 46;
    // width 保存胜率条宽度。
    int width = rightlimit - left;
    drawer_text_rect(left, top, rightlimit, top + 22, L"局势分析", RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // lastpositive 保存最后一个仍有胜率的玩家下标。
    int lastpositive = -1;
    for (int index = 0; index < static_cast<int>(data.players.size()); ++index)
    {
        const analysisplayerdata& item = data.players[index];
        if (item.probability > 0 && item.playerindex >= 0 && item.playerindex < static_cast<int>(state.players.size()))
        {
            lastpositive = index;
        }
    }

    // currentx 保存当前胜率条绘制横坐标。
    int currentx = left;
    for (int index = 0; index < static_cast<int>(data.players.size()); ++index)
    {
        const analysisplayerdata& item = data.players[index];
        if (item.playerindex < 0 || item.playerindex >= static_cast<int>(state.players.size()) ||
            item.probability <= 0)
        {
            continue;
        }

        // right 保存该玩家胜率条右边界。
        int right = index == lastpositive ?
            left + width : currentx + width * item.probability / 100;
        setfillcolor(state.players[item.playerindex].color);
        solidrectangle(currentx, top + 28, right, top + 46);
        currentx = right;
    }
    setlinecolor(RGB(168, 176, 188));
    rectangle(left, top + 28, left + width, top + 46);

    // text 保存各玩家胜率文字。
    std::wstring text;
    for (const analysisplayerdata& item : data.players)
    {
        if (!text.empty())
        {
            text += L"  ";
        }
        text += L"P" + std::to_wstring(item.playerindex + 1) + L" " +
            std::to_wstring(item.probability) + L"%";
    }
    drawer_text_rect(left, top + 50, rightlimit, top + 92, text, RGB(90, 99, 113), DT_LEFT | DT_WORDBREAK);
}

// 这个函数绘制键盘操作编号提示。
static void drawer_keyboard_info(const gamestate& state, int top)
{
    // left 保存键盘提示左边界。
    int left = map_left_width + 40;
    // right 保存键盘提示右边界。
    int right = map_window_width - 46;

    if (state.phase == phase_select_piece)
    {
        // text 保存当前玩家棋子编号列表。
        std::wstring text = keyboard_make_piece_text(state);
        if (!text.empty())
        {
            drawer_text_rect(left, top, right, top + 28,
                L"当前棋子编号：" + text, RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    else if (state.phase == phase_select_target)
    {
        // text 保存当前可走位置编号列表。
        std::wstring text = keyboard_make_target_text(state);
        if (!text.empty())
        {
            drawer_text_rect(left, top, right, top + 28,
                L"可走位置编号：" + text + L"  0：返回", RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

// 这个函数绘制设置页五个项目标题。
static void drawer_setup_labels(const gamestate& state)
{
    // labels 保存设置页从上到下的项目标题。
    std::wstring labels[] = { L"每步时长", L"单方时长", L"游戏人数", L"棋子数量", L"机器人数量" };
    // left 保存项目标题左边界。
    int left = map_left_width + 40;
    // right 保存项目标题右边界。
    int right = map_window_width - 46;

    for (int index = 0; index < 5; ++index)
    {
        // top 保存项目标题顶部坐标。
        int top = map_setup_first_button_y + index * map_setup_row_gap - map_setup_label_offset;
        // color 保存当前标题颜色。
        int color = state.setupindex == index ? RGB(20, 28, 38) : RGB(96, 106, 120);
        drawer_text_rect(left, top, right, top + 22, labels[index], color, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

// 这个函数在游戏结束页绘制获胜玩家提示。
static void drawer_winner_info(const gamestate& state, int top)
{
    // rankedplayers 保存所有已经确定名次的玩家，并按名次排序。
    std::vector<int> rankedplayers;
    for (int index = 0; index < static_cast<int>(state.players.size()); ++index)
    {
        if (state.players[index].rank > 0)
        {
            rankedplayers.push_back(index);
        }
    }
    std::sort(rankedplayers.begin(), rankedplayers.end(),
        [&](int left, int right)
        {
            return state.players[left].rank < state.players[right].rank;
        });

    if (rankedplayers.empty())
    {
        return;
    }

    // left 保存获胜提示左边界。
    int left = map_left_width + 40;
    // right 保存获胜提示右边界。
    int right = map_window_width - 46;
    if (state.phase == phase_game_over)
    {
        // 游戏结束时每一个名次独占一行，避免横向文字拥挤和换行重叠。
        for (int index = 0; index < static_cast<int>(rankedplayers.size()); ++index)
        {
            int playerindex = rankedplayers[index];
            std::wstring text = L"第" + std::to_wstring(state.players[playerindex].rank) +
                L"名    玩家" + std::to_wstring(state.players[playerindex].id);
            drawer_text_rect(left, top + index * 28, right, top + index * 28 + 24,
                text, state.players[playerindex].color, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        return;
    }

    if (state.lastfinished >= 0 && state.lastfinished < static_cast<int>(state.players.size()))
    {
        const playerdata& player = state.players[state.lastfinished];
        std::wstring text = L"玩家" + std::to_wstring(player.id) +
            L"号已完成，当前排名第" + std::to_wstring(player.rank) + L"名";
        drawer_text_rect(left, top, right, top + 24, text, player.color, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

// 这个函数绘制右侧信息栏。
static void drawer_info(const gamestate& state)
{
    setfillcolor(RGB(250, 251, 253));
    solidrectangle(map_left_width, 0, map_window_width, map_window_height);
    setlinecolor(RGB(210, 216, 224));
    line(map_left_width, 0, map_left_width, map_window_height);

    // infoleft 保存右侧信息文字统一左边界。
    int infoleft = map_left_width + 40;
    // inforight 保存右侧信息文字统一右边界。
    int inforight = map_window_width - 46;

    settextstyle(24, 0, L"微软雅黑");
    drawer_text_rect(infoleft, 28, inforight, 64, L"跳棋游戏", RGB(20, 28, 38), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    settextstyle(18, 0, L"微软雅黑");
    if (state.phase == phase_game_over)
    {
        drawer_text_rect(infoleft, 78, inforight, 108, L"游戏结束", RGB(34, 43, 55),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        drawer_winner_info(state, 116);
    }
    else
    {
        drawer_text_rect(infoleft, 78, inforight, 108, L"阶段：" + gamedata_phase_text(state.phase),
            RGB(65, 76, 92), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    if (state.phase != phase_game_over && !state.turnorder.empty() && state.currentorderindex >= 0 &&
        state.currentorderindex < static_cast<int>(state.turnorder.size()))
    {
        // playerindex 保存当前玩家下标。
        int playerindex = state.turnorder[state.currentorderindex];
        if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
        {
            playerindex = -1;
        }
        if (playerindex >= 0)
        {
            const playerdata& player = state.players[playerindex];
            std::wstring current = L"当前是" + std::to_wstring(player.id) + L"号玩家（" + player.colorname + L"）";
            drawer_text_rect(infoleft, 116, inforight, 146, current, player.color, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            drawer_text_rect(infoleft, 146, inforight, 172,
                L"本步剩余：" + time_format_seconds(time_get_step_remaining()), RGB(90, 99, 113), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // statustop 保存状态提示顶部坐标。
    int statustop = state.turnorder.empty() ? 148 : 178;
    // statusbottom 保存状态提示底部坐标。
    int statusbottom = state.turnorder.empty() ? 236 : 224;
    if (state.phase != phase_game_over)
    {
        drawer_text_rect(infoleft, statustop, inforight, statusbottom, state.status,
            RGB(90, 99, 113), DT_LEFT | DT_WORDBREAK);
    }

    if (state.phase == phase_player_count || state.phase == phase_piece_count)
    {
        drawer_setup_labels(state);
    }

    int progress_top = state.phase == phase_game_over ? 330 : (state.lastfinished >= 0 ? 316 : 264); // progress_top 保存玩家进度区顶部。
    int progress_gap = 48; // progress_gap 保存每个玩家进度条的纵向间距。
    if (!state.players.empty())
    {
        if (state.phase != phase_game_over)
        {
            drawer_winner_info(state, progress_top - 64);
            drawer_text_rect(infoleft, progress_top - 34, inforight, progress_top - 6,
                L"每一位玩家完成进度", RGB(34, 43, 55), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    for (int index = 0; index < static_cast<int>(state.players.size()); ++index)
    {
        drawer_progress(state, index, progress_top + index * progress_gap);
    }

    // analysis_top 保存局势分析区顶部。
    int analysis_top = progress_top + static_cast<int>(state.players.size()) * progress_gap + 14;
    if (!state.players.empty() && analysis_top < 590)
    {
        analysis_top = 590;
    }
    drawer_analysis_info(state, analysis_top);

    // keyboard_top 保存键盘编号提示区顶部。
    int keyboard_top = analysis_top + 104;
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
