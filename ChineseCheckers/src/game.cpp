#include "game.h"

#include "analysis.h"
#include "board.h"
#include "drawer.h"
#include "keyboard.h"
#include "keyboard_data.h"
#include "logger.h"
#include "mapdata.h"
#include "robot.h"
#include "time.h"

#include <algorithm>
#include <graphics.h>
#include <windows.h>

// 这个函数创建一个右侧信息栏按钮。
static buttondata game_make_button(int top, int code, int value, const std::wstring& text, bool enabled = true, bool selected = false)
{
    buttondata button{};
    button.left = map_button_x;
    button.top = top;
    button.right = map_button_x + map_button_width;
    button.bottom = top + map_button_height;
    button.code = code;
    button.value = value;
    button.enabled = enabled;
    button.selected = selected;
    button.text = text;
    return button;
}

// 这个函数创建一个指定矩形的右侧信息栏按钮。
static buttondata game_make_rect_button(int left, int top, int right, int bottom, int code, int value, const std::wstring& text, bool enabled = true, bool selected = false)
{
    buttondata button{};
    button.left = left;
    button.top = top;
    button.right = right;
    button.bottom = bottom;
    button.code = code;
    button.value = value;
    button.enabled = enabled;
    button.selected = selected;
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
        std::to_wstring(state.piececount) + L"子。步时" + std::to_wstring(time_get_step_seconds()) +
        L"s，总时" + std::to_wstring(time_get_total_seconds() / 60) +
        L"min，机器人" + std::to_wstring(state.robotcount) +
        L"个。上下选项，左右调整，回车开始。";
}

// 这个函数重置为初始选择人数阶段。
static void game_reset(gamestate& state)
{
    state.phase = phase_player_count;
    state.playercount = 3;
    state.piececount = 3;
    state.robotcount = 0;
    state.setupindex = 0;
    state.currentorderindex = 0;
    state.selectedpiece = -1;
    state.robotpreviewtarget = -1;
    state.winnerindex = -1;
    state.nextlossorder = 0;
    state.running = true;
    state.players.clear();
    state.pieces.clear();
    state.turnorder.clear();
    state.movetargets.clear();
    state.buttons.clear();
    state.history.clear();
    state.ranking.clear();
    state.lastfinished = -1;
    time_reset();
    robot_new_game();
    analysis_reset();
    state.status = game_setup_status(state);
}

// 这个函数返回设置页指定项目的按钮纵坐标。
static int game_setup_button_top(int index)
{
    return map_setup_first_button_y + index * map_setup_row_gap;
}

// 这个函数返回每步时长在设置页的显示文字。
static std::wstring game_step_time_text()
{
    return std::to_wstring(time_get_step_seconds()) + L"s";
}

// 这个函数返回单方总时长在设置页的显示文字。
static std::wstring game_total_time_text()
{
    return std::to_wstring(time_get_total_seconds() / 60) + L"min";
}

// 这个函数向按钮列表添加一行设置项。
static void game_add_setup_row(gamestate& state, int index, int minuscode, int pluscode, const std::wstring& text)
{
    // top 保存该设置行的按钮顶部坐标。
    int top = game_setup_button_top(index);
    // selected 表示该设置行是否为当前键盘焦点。
    bool selected = state.setupindex == index;
    // minuswidth 保存减号按钮宽度。
    int minuswidth = 78;
    // pluswidth 保存加号按钮宽度。
    int pluswidth = 78;
    // gap 保存同一行按钮之间的空隙。
    int gap = 10;
    // centerleft 保存中间当前值按钮左边界。
    int centerleft = map_button_x + minuswidth + gap;
    // centerright 保存中间当前值按钮右边界。
    int centerright = map_button_x + map_button_width - pluswidth - gap;

    state.buttons.push_back(game_make_rect_button(map_button_x, top, map_button_x + minuswidth, top + map_button_height,
        minuscode, 0, L"-", true, selected));
    state.buttons.push_back(game_make_rect_button(centerleft, top, centerright, top + map_button_height,
        button_setup_focus, index, text, true, selected));
    state.buttons.push_back(game_make_rect_button(map_button_x + map_button_width - pluswidth, top,
        map_button_x + map_button_width, top + map_button_height, pluscode, 0, L"+", true, selected));
}

// 这个函数刷新当前阶段需要显示的按钮。
static void game_refresh_buttons(gamestate& state)
{
    state.buttons.clear();

    if (state.phase == phase_player_count || state.phase == phase_piece_count)
    {
        game_add_setup_row(state, 0, button_step_minus, button_step_plus, game_step_time_text());
        game_add_setup_row(state, 1, button_total_minus, button_total_plus, game_total_time_text());
        game_add_setup_row(state, 2, button_player_minus, button_player_plus, std::to_wstring(state.playercount) + L" 人");
        game_add_setup_row(state, 3, button_piece_minus, button_piece_plus, L"每人 " + std::to_wstring(state.piececount) + L" 个");
        game_add_setup_row(state, 4, button_robot_minus, button_robot_plus, std::to_wstring(state.robotcount) + L" 个机器人");
        state.buttons.push_back(game_make_button(map_setup_start_button_y, button_start_setup, 0, L"开始游戏"));
        state.buttons.push_back(game_make_button(map_setup_exit_button_y, button_exit, 0, L"退出游戏"));
    }
    else if (state.phase == phase_select_piece || state.phase == phase_select_target)
    {
        state.buttons.push_back(game_make_button(map_bottom_button_first_y, button_back, 0, L"返回上一步"));
        state.buttons.push_back(game_make_button(map_bottom_button_second_y, button_exit, 0, L"退出游戏"));
    }
    else if (state.phase == phase_game_over)
    {
        state.buttons.push_back(game_make_button(map_game_over_restart_y, button_restart, 0, L"重新开始"));
        state.buttons.push_back(game_make_button(map_bottom_button_first_y, button_back, 0, L"返回上一步"));
        state.buttons.push_back(game_make_button(map_bottom_button_second_y, button_exit, 0, L"退出游戏"));
    }
}

// 这个函数写入玩家起点和终点分配日志。
static void game_log_player_places(const gamestate& state, loggerdata& logger)
{
    for (const playerdata& player : state.players)
    {
        std::wstring text = L"玩家" + std::to_wstring(player.id) + L"（" + player.colorname +
            (player.isrobot ? L"，机器人" : L"，人工") + L"）起点：" +
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
    state.robotpreviewtarget = -1;
    state.currentorderindex = 0;
    state.winnerindex = -1;
    state.nextlossorder = 0;
    state.ranking.clear();
    state.lastfinished = -1;

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
        player.lost = false;
        player.isrobot = index >= state.playercount - state.robotcount;
        player.finished = false;
        player.rank = 0;
        player.lossprogress = 0;
        player.lossorder = 0;
        player.lossreason.clear();

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
    robot_new_game();
    game_create_players(state, board, logger);
    game_make_default_order(state, logger);
    state.phase = phase_select_piece;
    state.currentorderindex = 0;
    state.selectedpiece = -1;
    state.robotpreviewtarget = -1;
    state.movetargets.clear();
    time_start_game(state);
    analysis_update(board, state);

    // playerindex 保存首位玩家。
    int playerindex = state.turnorder.empty() ? -1 : state.turnorder[0];
    if (playerindex >= 0 && state.players[playerindex].isrobot)
    {
        state.status = L"游戏开始，按玩家编号顺序行动。玩家" +
            std::to_wstring(state.players[playerindex].id) + L"是机器人，将在5秒内完成移动。";
    }
    else
    {
        state.status = L"游戏开始，按玩家编号顺序行动。请玩家" +
            std::to_wstring(state.players[playerindex].id) + L"选择棋子，键盘编号已显示在棋子上。";
    }
    logger_write(logger, L"开始游戏，首先行动：玩家" + std::to_wstring(state.players[playerindex].id));
}

// 这个函数切换设置页当前选中的项目。
static void game_select_setup_index(gamestate& state, loggerdata& logger, int index);

// 这个函数调整初始设置阶段的每步时长。
static void game_adjust_step_time(gamestate& state, loggerdata& logger, int step);

// 这个函数调整初始设置阶段的单方总时长。
static void game_adjust_total_time(gamestate& state, loggerdata& logger, int step);

// 这个函数调整初始设置阶段的玩家人数。
static void game_adjust_player_count(gamestate& state, loggerdata& logger, int step);

// 这个函数调整初始设置阶段的每人棋子数量。
static void game_adjust_piece_count(gamestate& state, loggerdata& logger, int step);

// 这个函数调整初始设置阶段的机器人人数。
static void game_adjust_robot_count(gamestate& state, loggerdata& logger, int step);

// 这个函数调整设置页当前选中项目的数值。
static void game_adjust_selected_setup_value(gamestate& state, loggerdata& logger, int step);

// 这个函数判断当前行动者是否是机器人。
static bool game_current_is_robot(const gamestate& state);

// 这个函数返回当前行动玩家下标。
static int game_current_player(const gamestate& state)
{
    if (state.turnorder.empty() || state.currentorderindex < 0 || state.currentorderindex >= static_cast<int>(state.turnorder.size()))
    {
        return -1;
    }
    return state.turnorder[state.currentorderindex];
}

// 这个函数返回当前行动玩家下标。
int game_get_current_player(const gamestate& state)
{
    return game_current_player(state);
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
    state.robotpreviewtarget = -1;
    state.movetargets.clear();
    state.phase = phase_select_piece;
    robot_reset();

    // playerindex 保存新的当前玩家。
    int playerindex = game_current_player(state);
    if (playerindex >= 0)
    {
        if (state.players[playerindex].isrobot)
        {
            state.status = L"玩家" + std::to_wstring(state.players[playerindex].id) +
                L"是机器人，将在5秒内完成移动。";
        }
        else
        {
            state.status = L"请玩家" + std::to_wstring(state.players[playerindex].id) +
                L"选择棋子，键盘编号已显示在棋子上。";
        }
    }
    time_start_turn(state);
}

// 这个函数把玩家从当前行动顺序中移除。
static void game_remove_from_turnorder(gamestate& state, int playerindex)
{
    state.turnorder.erase(std::remove(state.turnorder.begin(), state.turnorder.end(), playerindex), state.turnorder.end());
    if (state.turnorder.empty())
    {
        state.currentorderindex = 0;
    }
    else if (state.currentorderindex >= static_cast<int>(state.turnorder.size()))
    {
        state.currentorderindex = 0;
    }
}

// 这个函数记录玩家完成排名。
static void game_record_rank(gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()) ||
        state.players[playerindex].rank > 0)
    {
        return;
    }

    state.players[playerindex].finished = true;
    state.players[playerindex].rank = static_cast<int>(state.ranking.size()) + 1;
    state.ranking.push_back(playerindex);
    state.lastfinished = playerindex;
    if (state.winnerindex < 0)
    {
        state.winnerindex = playerindex;
    }
}

// 这个函数根据判负进度和判负先后实时重算所有判负玩家的末尾排名。
static void game_update_loss_ranks(gamestate& state)
{
    // losers 保存已经判负的玩家下标。
    std::vector<int> losers;
    for (int index = 0; index < static_cast<int>(state.players.size()); ++index)
    {
        if (state.players[index].lost)
        {
            losers.push_back(index);
        }
    }

    std::sort(losers.begin(), losers.end(),
        [&](int left, int right)
        {
            const playerdata& leftplayer = state.players[left];
            const playerdata& rightplayer = state.players[right];
            if (leftplayer.lossprogress != rightplayer.lossprogress)
            {
                return leftplayer.lossprogress > rightplayer.lossprogress;
            }
            // 完成数量相同时，后判负者靠前，先判负者更靠后。
            return leftplayer.lossorder > rightplayer.lossorder;
        });

    // firstrank 保存判负玩家可以占用的第一个末尾名次。
    int firstrank = state.playercount - static_cast<int>(losers.size()) + 1;
    for (int index = 0; index < static_cast<int>(losers.size()); ++index)
    {
        state.players[losers[index]].rank = firstrank + index;
    }
}

// 这个函数把指定玩家的棋子从棋盘清除，但保留棋子列表和判负时完成进度。
static void game_clear_lost_player_pieces(gamestate& state, int playerindex)
{
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return;
    }

    for (int pieceindex : state.players[playerindex].pieceids)
    {
        if (pieceindex >= 0 && pieceindex < static_cast<int>(state.pieces.size()))
        {
            state.pieces[pieceindex].pointid = -1;
        }
    }
}

// 这个函数把一组违规或超时玩家判负，并按当前回合状态继续或结束游戏。
void game_eliminate_players(const boarddata& board, gamestate& state, loggerdata& logger,
    const std::vector<int>& playerindices, const std::wstring& reason, bool turnfinished)
{
    if (playerindices.empty())
    {
        return;
    }

    // oldplayer 保存判负处理前的当前玩家。
    int oldplayer = game_current_player(state);
    // oldorder 保存判负处理前的当前顺序位置。
    int oldorder = state.currentorderindex;
    // validlosers 保存本次真正新判负的玩家。
    std::vector<int> validlosers;
    time_stop();

    for (int playerindex : playerindices)
    {
        if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()) ||
            state.players[playerindex].lost || state.players[playerindex].finished)
        {
            continue;
        }

        playerdata& player = state.players[playerindex];
        player.lossprogress = board_count_finished(state, playerindex);
        player.lossorder = ++state.nextlossorder;
        player.lossreason = reason;
        player.lost = true;
        player.finished = false;
        game_clear_lost_player_pieces(state, playerindex);
        validlosers.push_back(playerindex);
        logger_write(logger, reason + L"：玩家" + std::to_wstring(player.id) +
            L"，判负时完成" + std::to_wstring(player.lossprogress) + L"/" +
            std::to_wstring(state.piececount));
    }

    if (validlosers.empty())
    {
        return;
    }

    for (int playerindex : validlosers)
    {
        state.turnorder.erase(std::remove(state.turnorder.begin(), state.turnorder.end(), playerindex), state.turnorder.end());
    }
    state.history.clear();
    state.selectedpiece = -1;
    state.robotpreviewtarget = -1;
    state.movetargets.clear();
    robot_reset();
    game_update_loss_ranks(state);
    analysis_update(board, state);

    // losstext 保存本次判负玩家及实时名次提示。
    std::wstring losstext;
    for (int playerindex : validlosers)
    {
        if (!losstext.empty())
        {
            losstext += L"、";
        }
        losstext += L"玩家" + std::to_wstring(state.players[playerindex].id) +
            L"第" + std::to_wstring(state.players[playerindex].rank) + L"名";
    }

    if (state.turnorder.size() == 1)
    {
        // lastplayer 保存最后一名仍在游戏中的玩家。
        int lastplayer = state.turnorder[0];
        game_record_rank(state, lastplayer);
        state.turnorder.clear();
        state.currentorderindex = 0;
        state.phase = phase_game_over;
        analysis_update(board, state);
        state.status = reason + L"：" + losstext + L"。玩家" +
            std::to_wstring(state.players[lastplayer].id) + L"成为最后剩余玩家，获得第" +
            std::to_wstring(state.players[lastplayer].rank) + L"名，游戏结束。";
        logger_write(logger, L"游戏结束：玩家" + std::to_wstring(state.players[lastplayer].id) +
            L"成为最后剩余玩家");
        return;
    }

    if (state.turnorder.empty())
    {
        state.currentorderindex = 0;
        state.phase = phase_game_over;
        state.status = reason + L"：" + losstext + L"。没有剩余玩家，游戏结束。";
        logger_write(logger, L"游戏结束：没有剩余玩家");
        return;
    }

    // oldstillactive 表示本步行动者是否仍在行动顺序中。
    auto oldposition = std::find(state.turnorder.begin(), state.turnorder.end(), oldplayer);
    bool oldstillactive = oldposition != state.turnorder.end();
    if (turnfinished && oldstillactive)
    {
        int position = static_cast<int>(oldposition - state.turnorder.begin());
        state.currentorderindex = (position + 1) % static_cast<int>(state.turnorder.size());
    }
    else
    {
        state.currentorderindex = oldorder;
        if (state.currentorderindex >= static_cast<int>(state.turnorder.size()))
        {
            state.currentorderindex = 0;
        }
    }

    state.phase = phase_select_piece;
    // nextplayer 保存判负处理后的当前玩家。
    int nextplayer = game_current_player(state);
    state.status = reason + L"：" + losstext + L"。现在轮到玩家" +
        std::to_wstring(state.players[nextplayer].id) + L"继续行动。";
    time_start_turn(state);
}

// 这个函数处理玩家完成后的排名、结束或继续。
static void game_handle_player_finished(const boarddata& board, gamestate& state, loggerdata& logger, int playerindex)
{
    game_record_rank(state, playerindex);
    game_remove_from_turnorder(state, playerindex);
    analysis_update(board, state);

    // rank 保存当前玩家获得的名次。
    int rank = state.players[playerindex].rank;
    logger_write(logger, L"完成：玩家" + std::to_wstring(state.players[playerindex].id) +
        L"全部棋子进入停车区，当前排名第" + std::to_wstring(rank));

    if (state.turnorder.size() == 1)
    {
        // lastplayer 保存最后一名未完成玩家。
        int lastplayer = state.turnorder[0];
        game_record_rank(state, lastplayer);
        state.turnorder.clear();
        state.currentorderindex = 0;
        state.selectedpiece = -1;
        state.robotpreviewtarget = -1;
        state.movetargets.clear();
        state.phase = phase_game_over;
        time_stop();
        analysis_update(board, state);
        state.status = L"玩家" + std::to_wstring(state.players[playerindex].id) +
            L"已完成，当前排名第" + std::to_wstring(rank) +
            L"。只剩玩家" + std::to_wstring(state.players[lastplayer].id) +
            L"，排名第" + std::to_wstring(state.players[lastplayer].rank) + L"，游戏结束。";
        logger_write(logger, L"游戏结束：只剩玩家" + std::to_wstring(state.players[lastplayer].id) +
            L"，自动获得第" + std::to_wstring(state.players[lastplayer].rank) + L"名");
        return;
    }

    if (state.turnorder.empty())
    {
        state.selectedpiece = -1;
        state.robotpreviewtarget = -1;
        state.movetargets.clear();
        state.phase = phase_game_over;
        time_stop();
        state.status = L"所有玩家均已完成，游戏结束。";
        logger_write(logger, L"游戏结束：所有玩家均已完成");
        return;
    }

    state.selectedpiece = -1;
    state.robotpreviewtarget = -1;
    state.movetargets.clear();
    state.phase = phase_select_piece;
    robot_reset();
    time_start_turn(state);
    // nextplayer 保存继续行动的玩家。
    int nextplayer = game_current_player(state);
    if (nextplayer >= 0)
    {
        state.status = L"玩家" + std::to_wstring(state.players[playerindex].id) +
            L"已完成，当前排名第" + std::to_wstring(rank) +
            L"。现在轮到玩家" + std::to_wstring(state.players[nextplayer].id) + L"继续。";
    }
}

// 这个函数按公共规则执行一次移动，并处理计时、排名和换手。
void game_apply_move(const boarddata& board, gamestate& state, loggerdata& logger, int pieceindex, int targetid, const std::wstring& source)
{
    // playerindex 保存当前玩家。
    int playerindex = game_current_player(state);
    if (playerindex < 0 || pieceindex < 0 || pieceindex >= static_cast<int>(state.pieces.size()) ||
        targetid < 0 || targetid >= static_cast<int>(board.points.size()) ||
        state.pieces[pieceindex].owner != playerindex)
    {
        state.status = source + L"移动无效：当前没有合法移动。";
        logger_write(logger, source + L"移动无效：参数越界");
        return;
    }


    // legaltargets 保存执行瞬间重新计算的合法落点，防止旧选择或误触绕过规则。
    std::vector<int> legaltargets = board_get_targets(board, state, pieceindex);
    if (std::find(legaltargets.begin(), legaltargets.end(), targetid) == legaltargets.end())
    {
        state.status = source + L"移动无效：该位置不是当前合法落点。";
        logger_write(logger, source + L"移动无效：落点已经失效或位于受保护最终区域");
        return;
    }

    // finishedbefore 保存行动玩家移动前已进入自己最终区的棋子数。
    int finishedbefore = board_count_finished(state, playerindex);
    // fromid 保存移动前孔位。
    int fromid = state.pieces[pieceindex].pointid;
    movedata move{};
    move.playerindex = playerindex;
    move.pieceindex = pieceindex;
    move.fromid = fromid;
    move.toid = targetid;
    move.orderindex = state.currentorderindex;
    state.history.push_back(move);

    state.pieces[pieceindex].pointid = targetid;
    time_finish_turn();
    analysis_update(board, state);
    logger_write(logger, source + L"移动棋子：玩家" + std::to_wstring(state.players[playerindex].id) +
        L" 孔位" + std::to_wstring(fromid) + L" -> " + std::to_wstring(targetid));

    state.selectedpiece = -1;
    state.robotpreviewtarget = -1;
    state.movetargets.clear();

    // 只有区域主人在本步首次达到六成阈值时，才检查仍未离开的外来棋子。
    int protectioncount = board_get_zone_protection_count(state);
    int finishedafter = board_count_finished(state, playerindex);
    if (finishedbefore < protectioncount && finishedafter >= protectioncount)
    {
        // violators 保存达到阈值瞬间仍停在该最终区域中的违规玩家。
        std::vector<int> violators = board_find_zone_violators(state, playerindex);
        if (!violators.empty())
        {
            game_eliminate_players(board, state, logger, violators,
                L"终点区未及时离开判负", true);
            return;
        }
    }

    if (board_player_finished(state, playerindex))
    {
        game_handle_player_finished(board, state, logger, playerindex);
        return;
    }

    game_next_turn(state);
}

// 这个函数撤销上一次移动或取消当前选择。
static void game_back(gamestate& state, const boarddata& board, loggerdata& logger)
{
    if (state.phase == phase_piece_count)
    {
        state.phase = phase_player_count;
        state.status = game_setup_status(state);
        logger_write(logger, L"返回上一步：从棋子数量选择返回人数选择");
    }
    else if (state.phase == phase_select_target)
    {
        if (state.robotpreviewtarget >= 0)
        {
            state.robotpreviewtarget = -1;
            state.status = L"已取消落点确认，棋子仍保持选中，请重新选择落点。";
            logger_write(logger, L"返回上一步：取消待确认落点，保留当前棋子选择");
            return;
        }
        state.selectedpiece = -1;
        state.movetargets.clear();
        state.phase = phase_select_piece;
        robot_reset();
        state.status = L"已取消棋子选择，请重新选择棋子。";
        logger_write(logger, L"返回上一步：取消当前棋子选择");
    }
    else if (state.phase == phase_select_piece || state.phase == phase_game_over)
    {
        // haslost 保存是否已经有人超时判负。
        bool haslost = std::any_of(state.players.begin(), state.players.end(),
            [](const playerdata& player)
            {
                return player.lost;
            });
        if (!state.ranking.empty() || haslost)
        {
            state.status = L"已经产生排名或超时判负，不能再撤销之前的走法。";
            logger_write(logger, L"返回上一步无效：已经产生排名或超时判负");
            return;
        }

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
                state.robotpreviewtarget = -1;
                time_stop();
                analysis_reset();
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
        state.robotpreviewtarget = -1;
        state.movetargets.clear();
        state.winnerindex = -1;
        state.phase = phase_select_piece;
        analysis_update(board, state);
        robot_reset();
        time_start_turn(state);
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
        state.setupindex = 2;
        state.playercount = value;
        state.status = game_setup_status(state);
        logger_write(logger, L"选择游戏人数：" + std::to_wstring(value));
    }
    else if (code == button_player_minus)
    {
        state.setupindex = 2;
        game_adjust_player_count(state, logger, -1);
    }
    else if (code == button_player_plus)
    {
        state.setupindex = 2;
        game_adjust_player_count(state, logger, 1);
    }
    else if (code == button_piece_count)
    {
        state.setupindex = 3;
        state.piececount = value;
        state.status = game_setup_status(state);
        logger_write(logger, L"选择每人棋子数量：" + std::to_wstring(value));
    }
    else if (code == button_setup_focus)
    {
        game_select_setup_index(state, logger, value);
    }
    else if (code == button_step_minus)
    {
        state.setupindex = 0;
        game_adjust_step_time(state, logger, -1);
    }
    else if (code == button_step_plus)
    {
        state.setupindex = 0;
        game_adjust_step_time(state, logger, 1);
    }
    else if (code == button_total_minus)
    {
        state.setupindex = 1;
        game_adjust_total_time(state, logger, -1);
    }
    else if (code == button_total_plus)
    {
        state.setupindex = 1;
        game_adjust_total_time(state, logger, 1);
    }
    else if (code == button_piece_minus)
    {
        state.setupindex = 3;
        game_adjust_piece_count(state, logger, -1);
    }
    else if (code == button_piece_plus)
    {
        state.setupindex = 3;
        game_adjust_piece_count(state, logger, 1);
    }
    else if (code == button_robot_minus)
    {
        state.setupindex = 4;
        game_adjust_robot_count(state, logger, -1);
    }
    else if (code == button_robot_plus)
    {
        state.setupindex = 4;
        game_adjust_robot_count(state, logger, 1);
    }
    else if (code == button_step_time)
    {
        state.setupindex = 0;
        time_set_step_seconds(value);
        state.status = game_setup_status(state);
        logger_write(logger, L"选择每步时长：" + std::to_wstring(value) + L"秒");
    }
    else if (code == button_total_time)
    {
        state.setupindex = 1;
        time_set_total_seconds(value);
        state.status = game_setup_status(state);
        logger_write(logger, L"选择单方总时长：" + std::to_wstring(value / 60) + L"分钟");
    }
    else if (code == button_start_setup)
    {
        logger_write(logger, L"点击开始游戏，人数：" + std::to_wstring(state.playercount) +
            L"，每人棋子数量：" + std::to_wstring(state.piececount));
        game_prepare_start(state, board, logger);
    }
    else if (code == button_back)
    {
        game_back(state, board, logger);
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
    state.robotpreviewtarget = -1;
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
    state.status = L"已显示所有可落地位置。第一次选择落点进行预览，再次选择同一落点确认移动。";
    logger_write(logger, L"玩家" + std::to_wstring(state.players[playerindex].id) + L"选择棋子，孔位" +
        std::to_wstring(pointid) + L"，可落点数量 " + std::to_wstring(state.movetargets.size()));
}

// 这个函数处理棋盘上的落点选择。
static void game_select_target(gamestate& state, const boarddata& board, loggerdata& logger, int pointid)
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

    if (state.robotpreviewtarget != pointid)
    {
        state.robotpreviewtarget = pointid;
        state.status = L"已选择落点，请再次点击蓝色落点确认移动；点击其他绿色落点可以更换。";
        logger_write(logger, L"鼠标选择待确认落点：孔位" + std::to_wstring(pointid));
        return;
    }

    game_apply_move(board, state, logger, state.selectedpiece, pointid, L"鼠标");
}

// 这个函数处理棋盘区域点击，并保护不该点击的阶段。
static void game_handle_board_click(gamestate& state, const boarddata& board, loggerdata& logger, int x, int y)
{
    if (game_current_is_robot(state))
    {
        state.status = L"当前是机器人回合，人工点击棋盘无效，请等待机器人自动行动。";
        logger_write(logger, L"无效点击：机器人回合点击棋盘");
        return;
    }

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
        game_select_target(state, board, logger, pointid);
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

// 这个函数判断当前行动者是否是机器人。
static bool game_current_is_robot(const gamestate& state)
{
    // playerindex 保存当前玩家下标。
    int playerindex = game_current_player(state);
    return playerindex >= 0 && playerindex < static_cast<int>(state.players.size()) &&
        state.players[playerindex].isrobot;
}

// 这个函数返回设置页项目名称。
static std::wstring game_setup_index_name(int index)
{
    if (index == 0)
    {
        return L"每步时长";
    }
    if (index == 1)
    {
        return L"单方时长";
    }
    if (index == 2)
    {
        return L"游戏人数";
    }
    if (index == 3)
    {
        return L"棋子数量";
    }
    return L"机器人数量";
}

// 这个函数切换设置页当前选中的项目。
static void game_select_setup_index(gamestate& state, loggerdata& logger, int index)
{
    // oldindex 保存切换前选中的项目。
    int oldindex = state.setupindex;
    if (index < 0)
    {
        index = 0;
    }
    if (index > 4)
    {
        index = 4;
    }

    state.setupindex = index;
    state.status = game_setup_status(state);
    if (oldindex == state.setupindex)
    {
        logger_write(logger, L"设置项切换无效：仍在" + game_setup_index_name(state.setupindex));
    }
    else
    {
        logger_write(logger, L"切换设置项：" + game_setup_index_name(oldindex) + L" -> " +
            game_setup_index_name(state.setupindex));
    }
}

// 这个函数返回每步时长在可选列表中的下标。
static int game_step_time_index(int seconds)
{
    // options 保存每步时长的全部可选值。
    int options[] = { 15, 30, 60, 90 };
    for (int index = 0; index < 4; ++index)
    {
        if (options[index] == seconds)
        {
            return index;
        }
    }
    return 0;
}

// 这个函数调整初始设置阶段的每步时长。
static void game_adjust_step_time(gamestate& state, loggerdata& logger, int step)
{
    // options 保存每步时长的全部可选值。
    int options[] = { 15, 30, 60, 90 };
    // oldseconds 保存调整前的每步秒数。
    int oldseconds = time_get_step_seconds();
    // index 保存当前每步秒数在可选列表中的下标。
    int index = game_step_time_index(oldseconds) + step;
    if (index < 0)
    {
        index = 0;
    }
    if (index > 3)
    {
        index = 3;
    }

    time_set_step_seconds(options[index]);
    state.status = game_setup_status(state);
    if (oldseconds == time_get_step_seconds())
    {
        logger_write(logger, L"调整每步时长无效：已经到边界 " + std::to_wstring(oldseconds) + L"秒");
    }
    else
    {
        logger_write(logger, L"调整每步时长：" + std::to_wstring(oldseconds) + L"秒 -> " +
            std::to_wstring(time_get_step_seconds()) + L"秒");
    }
}

// 这个函数返回单方总时长在可选列表中的下标。
static int game_total_time_index(int seconds)
{
    // options 保存单方总时长的全部可选值。
    int options[] = { 5 * 60, 10 * 60, 15 * 60, 30 * 60 };
    for (int index = 0; index < 4; ++index)
    {
        if (options[index] == seconds)
        {
            return index;
        }
    }
    return 0;
}

// 这个函数调整初始设置阶段的单方总时长。
static void game_adjust_total_time(gamestate& state, loggerdata& logger, int step)
{
    // options 保存单方总时长的全部可选值。
    int options[] = { 5 * 60, 10 * 60, 15 * 60, 30 * 60 };
    // oldseconds 保存调整前的单方总秒数。
    int oldseconds = time_get_total_seconds();
    // index 保存当前单方总时长在可选列表中的下标。
    int index = game_total_time_index(oldseconds) + step;
    if (index < 0)
    {
        index = 0;
    }
    if (index > 3)
    {
        index = 3;
    }

    time_set_total_seconds(options[index]);
    state.status = game_setup_status(state);
    if (oldseconds == time_get_total_seconds())
    {
        logger_write(logger, L"调整单方时长无效：已经到边界 " + std::to_wstring(oldseconds / 60) + L"分钟");
    }
    else
    {
        logger_write(logger, L"调整单方时长：" + std::to_wstring(oldseconds / 60) + L"分钟 -> " +
            std::to_wstring(time_get_total_seconds() / 60) + L"分钟");
    }
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
    if (state.robotcount > state.playercount)
    {
        state.robotcount = state.playercount;
    }

    state.status = game_setup_status(state);
    if (oldcount == state.playercount)
    {
        logger_write(logger, L"调整人数无效：已经到边界 " + std::to_wstring(state.playercount));
    }
    else
    {
        logger_write(logger, L"调整人数：" + std::to_wstring(oldcount) +
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
        logger_write(logger, L"调整棋子数量无效：已经到边界 " + std::to_wstring(state.piececount));
    }
    else
    {
        logger_write(logger, L"调整每人棋子数量：" + std::to_wstring(oldcount) +
            L" -> " + std::to_wstring(state.piececount));
    }
}

// 这个函数调整初始设置阶段的机器人人数。
static void game_adjust_robot_count(gamestate& state, loggerdata& logger, int step)
{
    // oldcount 保存调整前的机器人人数。
    int oldcount = state.robotcount;
    state.robotcount += step;
    if (state.robotcount < 0)
    {
        state.robotcount = 0;
    }
    if (state.robotcount > state.playercount)
    {
        state.robotcount = state.playercount;
    }

    state.status = game_setup_status(state);
    if (oldcount == state.robotcount)
    {
        logger_write(logger, L"调整机器人数量无效：已经到边界 " + std::to_wstring(state.robotcount));
    }
    else
    {
        logger_write(logger, L"调整机器人数量：" + std::to_wstring(oldcount) +
            L" -> " + std::to_wstring(state.robotcount));
    }
}

// 这个函数调整设置页当前选中项目的数值。
static void game_adjust_selected_setup_value(gamestate& state, loggerdata& logger, int step)
{
    if (state.setupindex == 0)
    {
        game_adjust_step_time(state, logger, step);
    }
    else if (state.setupindex == 1)
    {
        game_adjust_total_time(state, logger, step);
    }
    else if (state.setupindex == 2)
    {
        game_adjust_player_count(state, logger, step);
    }
    else if (state.setupindex == 3)
    {
        game_adjust_piece_count(state, logger, step);
    }
    else
    {
        game_adjust_robot_count(state, logger, step);
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
        game_adjust_selected_setup_value(state, logger, -1);
        return true;
    }
    if (key == keyboard_key_right && extended)
    {
        game_adjust_selected_setup_value(state, logger, 1);
        return true;
    }
    if (key == keyboard_key_up && extended)
    {
        game_select_setup_index(state, logger, state.setupindex - 1);
        return true;
    }
    if (key == keyboard_key_down && extended)
    {
        game_select_setup_index(state, logger, state.setupindex + 1);
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
        game_back(state, board, logger);
        return;
    }
    if (game_process_setup_key(state, board, logger, key, extended))
    {
        return;
    }
    if (game_current_is_robot(state))
    {
        state.status = L"当前是机器人回合，数字键走棋无效，请等待机器人自动行动。";
        logger_write(logger, L"键盘无效操作：机器人回合按键");
        return;
    }
    keyboard_handle_key(board, state, logger, key, scancode, extended);
}

// 这个函数处理键盘字符消息。
static void game_process_char(gamestate& state, const boarddata& board, loggerdata& logger, wchar_t keychar)
{
    if (game_current_is_robot(state))
    {
        return;
    }
    keyboard_handle_char(board, state, logger, keychar);
}

// 这个函数处理机器人回合。
static void game_process_robot_turn(const boarddata& board, gamestate& state, loggerdata& logger)
{
    if (!robot_is_turn(state))
    {
        return;
    }

    // playerindex 保存当前机器人玩家。
    int playerindex = game_current_player(state);
    if (playerindex < 0)
    {
        return;
    }

    // action 保存机器人本帧需要展示或执行的动作。
    robotaction action = robot_update(board, state);
    if (action.kind == robot_action_select_piece && action.decision.valid)
    {
        state.selectedpiece = action.decision.pieceindex;
        state.robotpreviewtarget = -1;
        state.movetargets = board_get_targets(board, state, state.selectedpiece);
        state.phase = phase_select_target;
        state.status = L"机器人已选中棋子，正在决定落点，剩余" +
            std::to_wstring(action.remainingseconds) + L"秒。";
        logger_write(logger, L"机器人玩家" + std::to_wstring(state.players[playerindex].id) +
            L"选中棋子，孔位" + std::to_wstring(state.pieces[state.selectedpiece].pointid));
        return;
    }

    if (action.kind == robot_action_select_target && action.decision.valid)
    {
        state.robotpreviewtarget = action.decision.targetid;
        state.status = L"机器人已决定落点，剩余" + std::to_wstring(action.remainingseconds) +
            L"秒，将在5秒内移动。";
        logger_write(logger, L"机器人玩家" + std::to_wstring(state.players[playerindex].id) +
            L"决定落点，孔位" + std::to_wstring(action.decision.targetid));
        return;
    }

    if (action.kind == robot_action_move && action.decision.valid)
    {
        logger_write(logger, L"机器人玩家" + std::to_wstring(state.players[playerindex].id) +
            L"在5秒内完成移动");
        game_apply_move(board, state, logger, action.decision.pieceindex, action.decision.targetid, L"机器人");
        robot_reset();
        return;
    }

    if (action.kind == robot_action_skip)
    {
        logger_write(logger, L"机器人玩家" + std::to_wstring(state.players[playerindex].id) +
            L"没有可移动棋子，本回合跳过");
        time_finish_turn();
        game_next_turn(state);
        robot_reset();
        return;
    }

    if (state.robotpreviewtarget >= 0)
    {
        state.status = L"机器人已决定落点，剩余" + std::to_wstring(action.remainingseconds) +
            L"秒，将在5秒内移动。";
    }
    else if (state.selectedpiece >= 0)
    {
        state.status = L"机器人已选中棋子，正在决定落点，剩余" +
            std::to_wstring(action.remainingseconds) + L"秒。";
    }
    else
    {
        state.status = L"机器人决策中，剩余" + std::to_wstring(action.remainingseconds) +
            L"秒，将在5秒内移动。";
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
        time_update(board, state, logger);
        game_process_robot_turn(board, state, logger);
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
