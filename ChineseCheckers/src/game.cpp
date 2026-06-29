#include "game.h"

#include "board.h"
#include "drawer.h"
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

// 这个函数重置为初始选择人数阶段。
static void game_reset(gamestate& state)
{
    state.phase = phase_player_count;
    state.playercount = 0;
    state.piececount = 0;
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
    state.status = L"请选择 3 到 6 名玩家参加。";
}

// 这个函数刷新当前阶段需要显示的按钮。
static void game_refresh_buttons(gamestate& state)
{
    state.buttons.clear();

    if (state.phase == phase_player_count)
    {
        state.buttons.push_back(game_make_button(520, button_player_count, 3, L"3 人"));
        state.buttons.push_back(game_make_button(568, button_player_count, 4, L"4 人"));
        state.buttons.push_back(game_make_button(616, button_player_count, 5, L"5 人"));
        state.buttons.push_back(game_make_button(664, button_player_count, 6, L"6 人"));
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
        std::to_wstring(state.players[playerindex].id) + L"点击自己的棋子。";
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
        state.status = L"请玩家" + std::to_wstring(state.players[playerindex].id) + L"点击自己的棋子。";
    }
}

// 这个函数撤销上一次移动或取消当前选择。
static void game_back(gamestate& state, loggerdata& logger)
{
    if (state.phase == phase_piece_count)
    {
        state.phase = phase_player_count;
        state.playercount = 0;
        state.status = L"已返回人数选择。";
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
                state.phase = phase_piece_count;
                state.players.clear();
                state.pieces.clear();
                state.turnorder.clear();
                state.movetargets.clear();
                state.selectedpiece = -1;
                state.status = L"已返回棋子数量选择。";
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
        state.phase = phase_piece_count;
        state.status = L"已选择 " + std::to_wstring(value) + L" 名玩家，请选择每人的棋子数量。";
        logger_write(logger, L"选择游戏人数：" + std::to_wstring(value));
    }
    else if (code == button_piece_count)
    {
        state.piececount = value;
        logger_write(logger, L"选择每人棋子数量：" + std::to_wstring(value));
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
    state.status = L"已显示所有可落地位置，点击绿色圆圈落地。";
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

// 这个函数处理键盘快捷键。
static void game_process_key(gamestate& state, loggerdata& logger, int key)
{
    if (key == VK_ESCAPE)
    {
        game_back(state, logger);
    }
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
                game_process_key(state, logger, message.vkcode);
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
