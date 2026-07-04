#include "keyboard.h"

#include "analysis.h"
#include "board.h"
#include "keyboard_data.h"
#include "time.h"

#include <algorithm>

// keyboard_skip_char_number 保存已经由按键消息处理过、需要跳过的字符数字。
static int keyboard_skip_char_number = -1;

// 这个函数把键盘键值转换为 0 到 9 的数字。
static bool keyboard_key_to_number(int key, int scancode, bool extended, int& number)
{
    if (key >= keyboard_key_digit_0 && key <= keyboard_key_digit_9)
    {
        number = key - keyboard_key_digit_0;
        return true;
    }
    if (key >= keyboard_key_numpad_0 && key <= keyboard_key_numpad_9)
    {
        number = key - keyboard_key_numpad_0;
        return true;
    }

    // NumLock 关闭时，小键盘 0-9 会以 Insert/End/方向键等形式出现。
    // 只有非扩展键才按小键盘数字处理，避免真正方向键被误认为数字。
    if (!extended)
    {
        switch (key)
        {
        case keyboard_key_insert:
            number = 0;
            return true;
        case keyboard_key_end:
            number = 1;
            return true;
        case keyboard_key_down:
            number = 2;
            return true;
        case keyboard_key_pagedown:
            number = 3;
            return true;
        case keyboard_key_left:
            number = 4;
            return true;
        case keyboard_key_clear:
            number = 5;
            return true;
        case keyboard_key_right:
            number = 6;
            return true;
        case keyboard_key_home:
            number = 7;
            return true;
        case keyboard_key_up:
            number = 8;
            return true;
        case keyboard_key_pageup:
            number = 9;
            return true;
        default:
            break;
        }
    }

    number = -1;
    return false;
}

// 这个函数把字符消息转换为 0 到 9 的数字。
static bool keyboard_char_to_number(wchar_t keychar, int& number)
{
    if (keychar >= L'0' && keychar <= L'9')
    {
        number = static_cast<int>(keychar - L'0');
        return true;
    }
    number = -1;
    return false;
}

// 这个函数返回当前行动玩家下标。
static int keyboard_current_player(const gamestate& state)
{
    if (state.turnorder.empty() || state.currentorderindex < 0 ||
        state.currentorderindex >= static_cast<int>(state.turnorder.size()))
    {
        return -1;
    }
    return state.turnorder[state.currentorderindex];
}

// 这个函数进入下一名玩家回合。
static void keyboard_next_turn(gamestate& state)
{
    if (state.turnorder.empty())
    {
        return;
    }

    state.currentorderindex = (state.currentorderindex + 1) % static_cast<int>(state.turnorder.size());
    state.selectedpiece = -1;
    state.movetargets.clear();
    state.phase = phase_select_piece;

    // playerindex 保存新的当前玩家下标。
    int playerindex = keyboard_current_player(state);
    if (playerindex >= 0)
    {
        state.status = L"请玩家" + std::to_wstring(state.players[playerindex].id) +
            L"选择棋子，键盘编号已显示在棋子上。";
    }
    time_start_turn(state);
}

// 这个函数取消键盘选中的棋子并返回棋子选择阶段。
static void keyboard_cancel_target(gamestate& state, loggerdata& logger)
{
    state.selectedpiece = -1;
    state.movetargets.clear();
    state.phase = phase_select_piece;
    state.status = L"已取消棋子选择，请重新选择棋子。";
    logger_write(logger, L"键盘返回上一步：取消当前棋子选择");
}

// 这个函数用键盘编号选择当前玩家的棋子。
static void keyboard_select_piece(const boarddata& board, gamestate& state, loggerdata& logger, int number)
{
    // playerindex 保存当前玩家下标。
    int playerindex = keyboard_current_player(state);
    if (playerindex < 0)
    {
        state.status = L"当前没有行动玩家，键盘操作无效。";
        logger_write(logger, L"键盘无效操作：没有行动玩家");
        return;
    }

    const playerdata& player = state.players[playerindex];
    if (number < keyboard_piece_min || number >= static_cast<int>(player.pieceids.size()))
    {
        state.status = L"当前玩家没有编号为 " + std::to_wstring(number) + L" 的棋子。";
        logger_write(logger, L"键盘无效操作：玩家" + std::to_wstring(player.id) +
            L"选择不存在的棋子编号 " + std::to_wstring(number));
        return;
    }

    // pieceindex 保存键盘编号对应的全局棋子下标。
    int pieceindex = player.pieceids[number];
    state.movetargets = board_get_targets(board, state, pieceindex);
    if (state.movetargets.empty())
    {
        state.selectedpiece = -1;
        state.phase = phase_select_piece;
        state.status = L"编号 " + std::to_wstring(number) + L" 的棋子当前没有可走位置。";
        logger_write(logger, L"键盘选择棋子无效：玩家" + std::to_wstring(player.id) +
            L"的编号" + std::to_wstring(number) + L"棋子无可走位置");
        return;
    }

    state.selectedpiece = pieceindex;
    state.phase = phase_select_target;
    if (state.movetargets.size() > keyboard_target_max)
    {
        state.status = L"已选择编号 " + std::to_wstring(number) +
            L" 的棋子。键盘可选前 9 个落点，其他落点可用鼠标。";
    }
    else
    {
        state.status = L"已选择编号 " + std::to_wstring(number) +
            L" 的棋子，请按落点编号落地，按 0 返回。";
    }

    logger_write(logger, L"键盘选择棋子：玩家" + std::to_wstring(player.id) +
        L"选择编号" + std::to_wstring(number) + L"棋子，可落点数量 " +
        std::to_wstring(state.movetargets.size()));
}

// 这个函数用键盘编号选择当前棋子的落点。
static void keyboard_select_target(const boarddata& board, gamestate& state, loggerdata& logger, int number)
{
    if (number == keyboard_target_back)
    {
        keyboard_cancel_target(state, logger);
        return;
    }

    if (state.selectedpiece < 0)
    {
        state.status = L"还没有选中棋子，键盘落点选择无效。";
        logger_write(logger, L"键盘无效操作：未选棋子时选择落点");
        return;
    }

    if (number < keyboard_target_min || number > keyboard_target_max ||
        number > static_cast<int>(state.movetargets.size()))
    {
        state.status = L"当前没有编号为 " + std::to_wstring(number) + L" 的可走位置。";
        logger_write(logger, L"键盘无效操作：选择不存在的落点编号 " + std::to_wstring(number));
        return;
    }

    // playerindex 保存当前玩家下标。
    int playerindex = keyboard_current_player(state);
    if (playerindex < 0)
    {
        state.status = L"当前没有行动玩家，键盘操作无效。";
        logger_write(logger, L"键盘无效操作：没有行动玩家");
        return;
    }

    // fromid 保存移动前孔位编号。
    int fromid = state.pieces[state.selectedpiece].pointid;
    // toid 保存移动后孔位编号。
    int toid = state.movetargets[number - 1];

    movedata move{};
    move.playerindex = playerindex;
    move.pieceindex = state.selectedpiece;
    move.fromid = fromid;
    move.toid = toid;
    move.orderindex = state.currentorderindex;
    state.history.push_back(move);

    state.pieces[state.selectedpiece].pointid = toid;
    time_finish_turn();
    analysis_update(board, state);
    logger_write(logger, L"键盘移动棋子：玩家" + std::to_wstring(state.players[playerindex].id) +
        L"选择落点编号" + std::to_wstring(number) + L"，孔位" +
        std::to_wstring(fromid) + L" -> " + std::to_wstring(toid));

    state.selectedpiece = -1;
    state.movetargets.clear();

    if (board_player_finished(state, playerindex))
    {
        state.winnerindex = playerindex;
        state.phase = phase_game_over;
        time_stop();
        state.status = L"玩家" + std::to_wstring(state.players[playerindex].id) +
            L"全部进入停车区，获得胜利！";
        logger_write(logger, L"胜利：玩家" + std::to_wstring(state.players[playerindex].id) +
            L"全部棋子进入停车区");
        return;
    }

    keyboard_next_turn(state);
}

// 这个函数处理游戏阶段之外的数字键误触。
static void keyboard_reject_digit(gamestate& state, loggerdata& logger, int number)
{
    state.status = L"当前阶段不能用数字键走棋，本次按键无效。";
    logger_write(logger, L"键盘无效操作：阶段" + gamedata_phase_text(state.phase) +
        L"按下数字 " + std::to_wstring(number));
}

// 这个函数处理一次键盘按键，并在非法按键时保护游戏状态。
void keyboard_handle_key(const boarddata& board, gamestate& state, loggerdata& logger, int key, int scancode, bool extended)
{
    // number 保存键盘数字值。
    int number = -1;
    if (!keyboard_key_to_number(key, scancode, extended, number))
    {
        return;
    }

    keyboard_skip_char_number = number;
    if (state.phase == phase_select_piece)
    {
        keyboard_select_piece(board, state, logger, number);
    }
    else if (state.phase == phase_select_target)
    {
        keyboard_select_target(board, state, logger, number);
    }
    else
    {
        keyboard_reject_digit(state, logger, number);
    }
}

// 这个函数处理一次键盘字符消息，主要用于兼容小键盘数字。
void keyboard_handle_char(const boarddata& board, gamestate& state, loggerdata& logger, wchar_t keychar)
{
    // number 保存键盘字符对应的数字。
    int number = -1;
    if (!keyboard_char_to_number(keychar, number))
    {
        return;
    }

    if (keyboard_skip_char_number == number)
    {
        keyboard_skip_char_number = -1;
        return;
    }
    keyboard_skip_char_number = -1;

    if (state.phase == phase_select_piece)
    {
        keyboard_select_piece(board, state, logger, number);
    }
    else if (state.phase == phase_select_target)
    {
        keyboard_select_target(board, state, logger, number);
    }
    else
    {
        keyboard_reject_digit(state, logger, number);
    }
}

// 这个函数返回当前玩家全部棋子的全局棋子下标。
std::vector<int> keyboard_get_current_pieceids(const gamestate& state)
{
    // playerindex 保存当前玩家下标。
    int playerindex = keyboard_current_player(state);
    if (playerindex < 0 || playerindex >= static_cast<int>(state.players.size()))
    {
        return {};
    }
    return state.players[playerindex].pieceids;
}

// 这个函数返回棋子在当前玩家手中的键盘编号，非当前玩家棋子返回 -1。
int keyboard_get_piece_number(const gamestate& state, int pieceindex)
{
    // pieceids 保存当前玩家的棋子下标列表。
    std::vector<int> pieceids = keyboard_get_current_pieceids(state);
    for (int index = 0; index < static_cast<int>(pieceids.size()); ++index)
    {
        if (pieceids[index] == pieceindex)
        {
            return index;
        }
    }
    return -1;
}

// 这个函数返回落点的键盘编号，未编号落点返回 -1。
int keyboard_get_target_number(const gamestate& state, int pointid)
{
    // limit 保存键盘可以直接选择的落点数量。
    int targetcount = static_cast<int>(state.movetargets.size()); // targetcount 保存当前可落点数量。
    int limit = targetcount < keyboard_target_max ? targetcount : keyboard_target_max;
    for (int index = 0; index < limit; ++index)
    {
        if (state.movetargets[index] == pointid)
        {
            return index + 1;
        }
    }
    return -1;
}

// 这个函数生成右侧信息栏显示的当前棋子编号文本。
std::wstring keyboard_make_piece_text(const gamestate& state)
{
    // pieceids 保存当前玩家的棋子下标列表。
    std::vector<int> pieceids = keyboard_get_current_pieceids(state);
    if (pieceids.empty())
    {
        return L"";
    }

    // text 保存显示用编号列表。
    std::wstring text;
    for (int index = 0; index < static_cast<int>(pieceids.size()); ++index)
    {
        if (!text.empty())
        {
            text += L"  ";
        }
        text += std::to_wstring(index);
    }
    return text;
}

// 这个函数生成右侧信息栏显示的可走位置编号文本。
std::wstring keyboard_make_target_text(const gamestate& state)
{
    if (state.movetargets.empty())
    {
        return L"";
    }

    // limit 保存键盘可以直接选择的落点数量。
    int targetcount = static_cast<int>(state.movetargets.size()); // targetcount 保存当前可落点数量。
    int limit = targetcount < keyboard_target_max ? targetcount : keyboard_target_max;
    // text 保存显示用编号列表。
    std::wstring text;
    for (int index = 0; index < limit; ++index)
    {
        if (!text.empty())
        {
            text += L"  ";
        }
        text += std::to_wstring(index + 1);
    }
    return text;
}
