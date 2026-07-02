#include "game.h"

#include "board.h"
#include "drawer.h"
#include "keyboard.h"
#include "keyboard_data.h"
#include "logger.h"
#include "mapdata.h"

#include <algorithm>
#include <graphics.h>
#include <windows.h>

// 这个函数创建一个右侧信息栏按钮。
static buttondata game_make_button(int top, int code, int value, const std::wstring& text, bool enabled = true)
{
    buttondata button{};
    button.left = map_button_x;
    button.top = top;
    button.right = map_button_x + map_button_width;
    button.bottom = top + map_button_height;
    button.code = code;
    button.value = value;
    button.enabled = enabled;
    button.text = text;
    return button;
}

// 这个函数创建一个指定矩形的右侧信息栏按钮。
static buttondata game_make_rect_button(int left, int top, int right, int bottom, int code, int value, const std::wstring& text, bool enabled = true)
{
    buttondata button{};
    button.left = left;
    button.top = top;
    button.right = right;
    button.bottom = bottom;
    button.code = code;
    button.value = value;
    button.enabled = enabled;
    button.text = text;
    return button;
}

// 这个函数返回玩家颜色。
static COLORREF game_player_color(int index)
{
    static COLORREF colors[] = {
        RGB(221, 67, 67),
        RGB(51, 116, 220),
        RGB(48, 154, 88),
        RGB(230, 142, 45),
        RGB(142, 82, 204),
        RGB(39, 160, 179)
    };
    return colors[index % 6];
}

// 这个函数返回玩家颜色名称。
static std::wstring game_player_color_name(int index)
{
    static std::wstring names[] = {
        L"红色",
        L"蓝色",
        L"绿色",
        L"橙色",
        L"紫色",
        L"青色"
    };
    return names[index % 6];
}

// 这个函数生成初始设置阶段的提示文字。
static std::wstring game_setup_status(const gamestate& state)
{
    return L"设置：" + std::to_wstring(state.playercount) + L"人，每人" +
        std::to_wstring(state.piececount) + L"子。←→人数，↑减棋子，↓加棋子，回车开始。";
}

// 这个函数重置为初始选择人数阶段。
static void game_reset(gamestate& state)
{
    state.phase = phase_player_count;
    state.playercount = 3;
    state.piececount = 3;
    state.currentorderindex = 0;
    state.selectedpiece = -1;
    state.winnerindex = -1;
    state.running = true;
    state.players.clear();
    state.pieces.clear();
    state.turnorder.clear();
    state.movetargets.clear();
    state.buttons.clear();
    state.history.clear();
    state.status = game_setup_status(state);
}

// 这个函数刷新当前阶段需要显示的按钮。
static void game_refresh_buttons(gamestate& state)
{
    state.buttons.clear();

    if (state.phase == phase_player_count)
    {
        state.buttons.push_back(game_make_rect_button(map_button_x, 472, map_button_x + 54, 510, button_player_minus, 0, L"-"));
        state.buttons.push_back(game_make_rect_button(map_button_x + 62, 472, map_button_x + 168, 510, button_none, 0,
            std::to_wstring(state.playercount) + L" 人", false));
        state.buttons.push_back(game_make_rect_button(map_button_x + 176, 472, map_button_x + map_button_width, 510, button_player_plus, 0, L"+"));
        state.buttons.push_back(game_make_button(520, button_piece_count, 3,
            state.piececount == 3 ? L"每人 3 个棋子（当前）" : L"每人 3 个棋子"));
        state.buttons.push_back(game_make_button(568, button_piece_count, 6,
            state.piececount == 6 ? L"每人 6 个棋子（当前）" : L"每人 6 个棋子"));
        state.buttons.push_back(game_make_button(616, button_piece_count, 10,
            state.piececount == 10 ? L"每人 10 个棋子（当前）" : L"每人 10 个棋子"));
        state.buttons.push_back(game_make_button(664, button_start_setup, 0, L"开始游戏"));
        state.buttons.push_back(game_make_button(710, button_exit, 0, L"退出游戏"));
    }
    else if (state.phase == phase_piece_count)
    {
        state.buttons.push_back(game_make_button(520, button_piece_count, 3, L"每人 3 个棋子"));
        state.buttons.push_back(game_make_button(568, button_piece_count, 6, L"每人 6 个棋子"));
        state.buttons.push_back(game_make_button(616, button_piece_count, 10, L"每人 10 个棋子"));
        state.buttons.push_back(game_make_button(664, button_back, 0, L"返回上一步"));
        state.buttons.push_back(game_make_button(710, button_exit, 0, L"退出游戏"));
    }
    else if (state.phase == phase_select_piece || state.phase == phase_select_target)
    {
        state.buttons.push_back(game_make_button(664, button_back, 0, L"返回上一步"));
        state.buttons.push_back(game_make_button(710, button_exit, 0, L"退出游戏"));
    }
    else if (state.phase == phase_game_over)
    {
        state.buttons.push_back(game_make_button(616, button_restart, 0, L"重新开始"));
        state.buttons.push_back(game_make_button(664, button_back, 0, L"返回上一步"));
        state.buttons.push_back(game_make_button(710, button_exit, 0, L"退出游戏"));
    }
}

// 这个函数写入玩家起点和终点分配日志。
static void game_log_player_places(const gamestate& state, loggerdata& logger)
{
    for (const playerdata& player : state.players)
    {
        std::wstring text = L"玩家" + std::to_wstring(player.id) + L"（" + player.colorname + L"）起点：" +
            gamedata_arm_text(player.arm) + L"，停车区：" + gamedata_arm_text(player.targetarm);
        logger_write(logger, text);
    }
}

// 这个函数根据人数和棋子数创建玩家与棋子。
static void game_create_players(gamestate& state, const boarddata& board, loggerdata& logger)
{
    state.players.clear();
    state.pieces.clear();
    state.turnorder.clear();
    state.history.clear();
    state.movetargets.clear();
    state.selectedpiece = -1;
    state.currentorderindex = 0;
    state.winnerindex = -1;

    // arms 保存本局玩家起点角。
    std::vector<int> arms = board_get_arms(state.playercount);
    for (int index = 0; index < state.playercount; ++index)
    {
        playerdata player{};
        player.id = index + 1;
        player.arm = arms[index];
        player.targetarm = (player.arm + 3) % 6;
        player.color = game_player_color(index);
        player.colorname = game_player_color_name(index);
        player.targetids = board_get_zone(board, player.targetarm, state.piececount);

        // starts 保存该玩家起始区孔位。
        std::vector<int> starts = board_get_zone(board, player.arm, state.piececount);
        for (int pointid : starts)
        {
            piecedata piece{};
            piece.owner = index;
            piece.pointid = pointid;
            player.pieceids.push_back(static_cast<int>(state.pieces.size()));
            state.pieces.push_back(piece);
        }

        state.players.push_back(player);
    }

    game_log_player_places(state, logger);
}

// 这个函数按玩家编号生成行动顺序。
static void game_make_default_order(gamestate& state, loggerdata& logger)
{
    state.turnorder.clear();
    for (int index = 0; index < static_cast<int>(state.players.size()); ++index)
    {
        state.turnorder.push_back(index);
    }

    logger_write(logger, L"行动顺序固定为玩家编号从小到大");
    for (int order = 0; order < static_cast<int>(state.turnorder.size()); ++order)
    {
        // playerindex 保存当前顺序中的玩家下标。
        int playerindex = state.turnorder[order];
        logger_write(logger, L"顺序" + std::to_wstring(order + 1) + L"：玩家" +
            std::to_wstring(state.players[playerindex].id));
    }
}

// 这个函数完成棋子数量选择后的初始化并直接开始游戏。
static void game_prepare_start(gamestate& state, const boarddata& board, loggerdata& logger)
{
    game_create_players(state, board, logger);
    game_make_default_order(state, logger);
    state.phase = phase_select_piece;
    state.currentorderindex = 0;
    state.selectedpiece = -1;
    state.movetargets.clear();

    // playerindex 保存首位玩家。
    int playerindex = state.turnorder.empty() ? -1 : state.turnorder[0];
    state.status = L"游戏开始，按玩家编号顺序行动。请玩家" +
        std::to_wstring(state.players[playerindex].id) + L"选择棋子，键盘编号已显示在棋子上。";
    logger_write(logger, L"开始游戏，首先行动：玩家" + std::to_wstring(state.players[playerindex].id));
}

// 这个函数返回当前行动玩家下标。
static int game_current_player(const gamestate& state)
{
    if (state.turnorder.empty() || state.currentorderindex < 0 || state.currentorderindex >= static_cast<int>(state.turnorder.size()))
    {
        return -1;
    }
    return state.turnorder[state.currentorderindex];
}

// 这个函数进入下一名玩家回合。
static void game_next_turn(gamestate& state)
{
    if (state.turnorder.empty())
    {
        return;
    }
    state.currentorderindex = (state.currentorderindex + 1) % static_cast<int>(state.turnorder.size());
    state.selectedpiece = -1;
    state.movetargets.clear();
    state.phase = phase_select_piece;

    // playerindex 保存新的当前玩家。
    int playerindex = game_current_player(state);
    if (playerindex >= 0)
    {
        state.status = L"请玩家" + std::to_wstring(state.players[playerindex].id) +
            L"选择棋子，键盘编号已显示在棋子上。";
    }
}

// 这个函数撤销上一次移动或取消当前选择。
static void game_back(gamestate& state, loggerdata& logger)
{
    if (state.phase == phase_piece_count)
    {
        state.phase = phase_player_count;
        state.status = game_setup_status(state);
        logger_write(logger, L"返回上一步：从棋子数量选择返回人数选择");
    }
    else if (state.phase == phase_select_target)
    {
        state.selectedpiece = -1;
        state.movetargets.clear();
        state.phase = phase_select_piece;
        state.status = L"已取消棋子选择，请重新选择棋子。";
        logger_write(logger, L"返回上一步：取消当前棋子选择");
    }
    else if (state.phase == phase_select_piece || state.phase == phase_game_over)
    {
        if (state.history.empty())
        {
            if (state.phase == phase_select_piece)
            {
                state.phase = phase_player_count;
                state.players.clear();
                state.pieces.clear();
                state.turnorder.clear();
                state.movetargets.clear();
                state.selectedpiece = -1;
                state.status = game_setup_status(state);
                logger_write(logger, L"返回上一步：从游戏开始返回棋子数量选择");
                return;
            }

            state.status = L"当前没有可以撤销的移动。";
            logger_write(logger, L"返回上一步无效：没有可以撤销的移动");
            return;
        }

        // move 保存最近一次移动记录。
        movedata move = state.history.back();
        state.history.pop_back();
        state.pieces[move.pieceindex].pointid = move.fromid;
        state.currentorderindex = move.orderindex;
        state.selectedpiece = -1;
        state.movetargets.clear();
        state.winnerindex = -1;
        state.phase = phase_select_piece;
        state.status = L"已撤销玩家" + std::to_wstring(state.players[move.playerindex].id) + L"的一步移动。";
        logger_write(logger, L"返回上一步：撤销玩家" + std::to_wstring(state.players[move.playerindex].id) +
            L" 棋子从孔位" + std::to_wstring(move.toid) + L"回到孔位" + std::to_wstring(move.fromid));
    }
    else
    {
        state.status = L"当前已经是第一步，不能继续返回。";
        logger_write(logger, L"返回上一步无效：已经是第一步");
    }
}

// 这个函数处理右侧按钮点击。
static void game_handle_button(gamestate& state, const boarddata& board, loggerdata& logger, int code, int value)
{
    if (code == button_none)
    {
        state.status = L"这个按钮当前不可用。";
        logger_write(logger, L"无效点击：不可用按钮");
    }
    else if (code == button_exit)
    {
        state.running = false;
        logger_write(logger, L"点击退出游戏");
    }
    else if (code == button_player_count)
    {
        state.playercount = value;
        state.status = game_setup_status(state);
        logger_write(logger, L"选择游戏人数：" + std::to_wstring(value));
    }
    else if (code == button_player_minus)
    {
        // oldcount 保存调整前的人数。
        int oldcount = state.playercount;
        if (state.playercount > 2)
        {
            --state.playercount;
        }
        state.status = game_setup_status(state);
        logger_write(logger, L"点击减少人数：" + std::to_wstring(oldcount) + L" -> " +
            std::to_wstring(state.playercount));
    }
    else if (code == button_player_plus)
    {
        // oldcount 保存调整前的人数。
        int oldcount = state.playercount;
        if (state.playercount < 6)
        {
            ++state.playercount;
        }
        state.status = game_setup_status(state);
        logger_write(logger, L"点击增加人数：" + std::to_wstring(oldcount) + L" -> " +
            std::to_wstring(state.playercount));
    }
    else if (code == button_piece_count)
    {
        state.piececount = value;
        state.status = game_setup_status(state);
        logger_write(logger, L"选择每人棋子数量：" + std::to_wstring(value));
    }
    else if (code == button_start_setup)
    {
        logger_write(logger, L"点击开始游戏，人数：" + std::to_wstring(state.playercount) +
            L"，每人棋子数量：" + std::to_wstring(state.piececount));
        game_prepare_start(state, board, logger);
    }
    else if (code == button_back)
    {
        game_back(state, logger);
    }
    else if (code == button_restart)
    {
        game_reset(state);
        logger_write(logger, L"重新开始游戏");
    }
}

// 这个函数处理棋盘上的棋子选择。
static void game_select_piece(gamestate& state, const boarddata& board, loggerdata& logger, int pointid)
{
    // playerindex 保存当前玩家下标。
    int playerindex = game_current_player(state);
    if (playerindex < 0)
    {
        state.status = L"当前没有行动玩家。";
        logger_write(logger, L"无效点击：没有行动玩家");
        return;
    }

    // pieceindex 保存被点击孔位上的棋子下标。
    int pieceindex = board_find_piece_at(state, pointid);
    if (pieceindex < 0)
    {
        state.status = L"这里没有棋子，本次点击无效。";
        logger_write(logger, L"无效点击：空孔位");
        return;
    }
    if (state.pieces[pieceindex].owner != playerindex)
    {
        state.status = L"只能选择当前玩家自己的棋子。";
        logger_write(logger, L"无效点击：玩家" + std::to_wstring(state.players[playerindex].id) + L"点击了其他玩家棋子");
        return;
    }

    state.selectedpiece = pieceindex;
    state.movetargets = board_get_targets(board, state, pieceindex);
    if (state.movetargets.empty())
    {
        state.selectedpiece = -1;
        state.phase = phase_select_piece;
        state.status = L"这个棋子当前没有可走位置，请选择其他棋子。";
        logger_write(logger, L"玩家" + std::to_wstring(state.players[playerindex].id) + L"选择的棋子无可走位置");
        return;
    }

    state.phase = phase_select_target;
    state.status = L"已显示所有可落地位置，可点击绿色圆圈或按落点编号落地。";
    logger_write(logger, L"玩家" + std::to_wstring(state.players[playerindex].id) + L"选择棋子，孔位" +
        std::to_wstring(pointid) + L"，可落点数量 " + std::to_wstring(state.movetargets.size()));
}

// 这个函数处理棋盘上的落点选择。
static void game_select_target(gamestate& state, loggerdata& logger, int pointid)
{
    if (state.selectedpiece < 0)
    {
        state.status = L"还没有选中棋子。";
        logger_write(logger, L"无效点击：未选棋子时选择落点");
        return;
    }

    if (std::find(state.movetargets.begin(), state.movetargets.end(), pointid) == state.movetargets.end())
    {
        state.status = L"这里不是合法落点，本次点击无效。";
        logger_write(logger, L"无效点击：选择了非法落点");
        return;
    }

    // playerindex 保存当前玩家。
    int playerindex = game_current_player(state);
    // fromid 保存移动前孔位。
    int fromid = state.pieces[state.selectedpiece].pointid;
    // toid 保存移动后孔位。
    int toid = pointid;
    movedata move{};
    move.playerindex = playerindex;
    move.pieceindex = state.selectedpiece;
    move.fromid = fromid;
    move.toid = toid;
    move.orderindex = state.currentorderindex;
    state.history.push_back(move);

    state.pieces[state.selectedpiece].pointid = toid;
    logger_write(logger, L"玩家" + std::to_wstring(state.players[playerindex].id) + L"移动棋子：孔位" +
        std::to_wstring(fromid) + L" -> " + std::to_wstring(toid));

    state.selectedpiece = -1;
    state.movetargets.clear();

    if (board_player_finished(state, playerindex))
    {
        state.winnerindex = playerindex;
        state.phase = phase_game_over;
        state.status = L"玩家" + std::to_wstring(state.players[playerindex].id) + L"全部进入停车区，获得胜利！";
        logger_write(logger, L"胜利：玩家" + std::to_wstring(state.players[playerindex].id) + L"全部棋子进入停车区");
        return;
    }

    game_next_turn(state);
}

// 这个函数处理棋盘区域点击，并保护不该点击的阶段。
static void game_handle_board_click(gamestate& state, const boarddata& board, loggerdata& logger, int x, int y)
{
    // pointid 保存鼠标点中的孔位编号。
    int pointid = board_find_point_at(board, x, y);
    if (pointid < 0)
    {
        state.status = L"没有点中棋盘孔位，本次点击无效。";
        logger_write(logger, L"无效点击：没有点中棋盘孔位");
        return;
    }

    if (state.phase == phase_select_piece)
    {
        game_select_piece(state, board, logger, pointid);
    }
    else if (state.phase == phase_select_target)
    {
        game_select_target(state, logger, pointid);
    }
    else
    {
        state.status = L"当前阶段不能操作棋盘，本次点击无效。";
        logger_write(logger, L"无效点击：当前阶段不能操作棋盘");
    }
}

// 这个函数处理一次鼠标左键点击。
static void game_process_click(gamestate& state, const boarddata& board, loggerdata& logger, int x, int y)
{
    // code 保存按钮操作类型。
    int code = button_none;
    // value 保存按钮数值。
    int value = 0;
    if (drawer_hit_button(state, x, y, code, value))
    {
        game_handle_button(state, board, logger, code, value);
        return;
    }

    if (x < map_left_width)
    {
        game_handle_board_click(state, board, logger, x, y);
    }
    else
    {
        state.status = L"请点击有效按钮或棋盘孔位。";
        logger_write(logger, L"无效点击：信息栏空白区域");
    }
}

// 这个函数判断当前是否处在初始设置阶段。
static bool game_is_setup_phase(const gamestate& state)
{
    return state.phase == phase_player_count || state.phase == phase_piece_count;
}

// 这个函数调整初始设置阶段的玩家人数。
static void game_adjust_player_count(gamestate& state, loggerdata& logger, int step)
{
    // oldcount 保存调整前的人数。
    int oldcount = state.playercount;
    state.playercount += step;
    if (state.playercount < 2)
    {
        state.playercount = 2;
    }
    if (state.playercount > 6)
    {
        state.playercount = 6;
    }

    state.status = game_setup_status(state);
    if (oldcount == state.playercount)
    {
        logger_write(logger, L"键盘调整人数无效：已经到边界 " + std::to_wstring(state.playercount));
    }
    else
    {
        logger_write(logger, L"键盘调整人数：" + std::to_wstring(oldcount) +
            L" -> " + std::to_wstring(state.playercount));
    }
}

// 这个函数返回棋子数量在可选列表中的下标。
static int game_piece_count_index(int piececount)
{
    // options 保存每人棋子数量的全部可选值。
    int options[] = { 3, 6, 10 };
    for (int index = 0; index < 3; ++index)
    {
        if (options[index] == piececount)
        {
            return index;
        }
    }
    return 0;
}

// 这个函数调整初始设置阶段的每人棋子数量。
static void game_adjust_piece_count(gamestate& state, loggerdata& logger, int step)
{
    // options 保存每人棋子数量的全部可选值。
    int options[] = { 3, 6, 10 };
    // oldcount 保存调整前的棋子数量。
    int oldcount = state.piececount;
    // index 保存当前棋子数量在可选列表中的下标。
    int index = game_piece_count_index(state.piececount) + step;
    if (index < 0)
    {
        index = 0;
    }
    if (index > 2)
    {
        index = 2;
    }

    state.piececount = options[index];
    state.status = game_setup_status(state);
    if (oldcount == state.piececount)
    {
        logger_write(logger, L"键盘调整棋子数量无效：已经到边界 " + std::to_wstring(state.piececount));
    }
    else
    {
        logger_write(logger, L"键盘调整每人棋子数量：" + std::to_wstring(oldcount) +
            L" -> " + std::to_wstring(state.piececount));
    }
}

// 这个函数处理初始设置阶段的键盘操作。
static bool game_process_setup_key(gamestate& state, const boarddata& board, loggerdata& logger, int key, bool extended)
{
    if (!game_is_setup_phase(state))
    {
        return false;
    }

    // 方向键只接受真正的方向键，避免 NumLock 关闭的小键盘数字串到设置操作。
    if (key == keyboard_key_left && extended)
    {
        game_adjust_player_count(state, logger, -1);
        return true;
    }
    if (key == keyboard_key_right && extended)
    {
        game_adjust_player_count(state, logger, 1);
        return true;
    }
    if (key == keyboard_key_up && extended)
    {
        game_adjust_piece_count(state, logger, -1);
        return true;
    }
    if (key == keyboard_key_down && extended)
    {
        game_adjust_piece_count(state, logger, 1);
        return true;
    }
    if (key == keyboard_key_enter)
    {
        logger_write(logger, L"键盘按回车按当前设置开始游戏");
        game_prepare_start(state, board, logger);
        return true;
    }

    return false;
}

// 这个函数处理键盘快捷键和数字键操作。
static void game_process_key(gamestate& state, const boarddata& board, loggerdata& logger, int key, int scancode, bool extended)
{
    if (key == keyboard_key_escape || key == keyboard_key_delete)
    {
        game_back(state, logger);
        return;
    }
    if (game_process_setup_key(state, board, logger, key, extended))
    {
        return;
    }
    keyboard_handle_key(board, state, logger, key, scancode, extended);
}

// 这个函数处理键盘字符消息。
static void game_process_char(gamestate& state, const boarddata& board, loggerdata& logger, wchar_t keychar)
{
    keyboard_handle_char(board, state, logger, keychar);
}

// 这个函数是游戏公共入口，负责启动图形界面和主循环。
int game_run()
{
    boarddata board{};
    gamestate state{};
    loggerdata logger{};

    board_build(board);
    game_reset(state);
    logger_open(logger);
    logger_write(logger, L"程序启动");

    initgraph(map_window_width, map_window_height);
    setbkmode(TRANSPARENT);
    BeginBatchDraw();

    while (state.running)
    {
        game_refresh_buttons(state);
        drawer_draw(board, state);
        FlushBatchDraw();

        ExMessage message{};
        while (peekmessage(&message, EX_MOUSE | EX_KEY))
        {
            if (message.message == WM_LBUTTONDOWN)
            {
                game_process_click(state, board, logger, message.x, message.y);
            }
            else if (message.message == WM_KEYDOWN)
            {
                game_process_key(state, board, logger, message.vkcode, message.scancode, message.extended);
            }
            else if (message.message == WM_CHAR)
            {
                game_process_char(state, board, logger, message.ch);
            }
        }

        Sleep(16);
    }

    EndBatchDraw();
    closegraph();
    logger_write(logger, L"程序结束");
    logger_close(logger);
    return 0;
}
