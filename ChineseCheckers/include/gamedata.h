#pragma once

#include <graphics.h>

#include <fstream>
#include <string>
#include <vector>

enum phasekind
{
    phase_player_count,
    phase_piece_count,
    phase_select_piece,
    phase_select_target,
    phase_game_over
};

enum buttonkind
{
    button_none = 0,
    button_back = 1,
    button_player_count = 2,
    button_piece_count = 3,
    button_player_minus = 4,
    button_restart = 5,
    button_exit = 6,
    button_player_plus = 7,
    button_start_setup = 8,
    button_step_time = 9,
    button_total_time = 10,
    button_setup_focus = 11,
    button_step_minus = 12,
    button_step_plus = 13,
    button_total_minus = 14,
    button_total_plus = 15,
    button_piece_minus = 16,
    button_piece_plus = 17,
    button_robot_minus = 18,
    button_robot_plus = 19
};

struct jumpdata
{
    int overid; // 被跳过的孔位编号。
    int toid; // 跳跃落点孔位编号。
};

struct pointdata
{
    int id; // 孔位编号。
    int row; // 孔位所在行。
    int col; // 孔位所在列。
    double x; // 孔位绘制横坐标。
    double y; // 孔位绘制纵坐标。
    std::vector<int> nearids; // 一步可达的相邻孔位编号。
};

struct boarddata
{
    std::vector<pointdata> points; // 全部棋盘孔位。
    int idmap[17][13]; // 行列到孔位编号的映射，空位置为 -1。
    std::vector<std::vector<jumpdata>> jumps; // 每个孔位的跳跃边。
};

struct piecedata
{
    int owner; // 棋子所属玩家下标。
    int pointid; // 棋子当前所在孔位编号。
};

struct playerdata
{
    int id; // 玩家显示编号。
    int arm; // 玩家起始三角区编号。
    int targetarm; // 玩家停车区三角区编号。
    COLORREF color; // 玩家棋子颜色。
    std::wstring colorname; // 玩家颜色名称。
    std::vector<int> pieceids; // 玩家拥有的棋子下标。
    std::vector<int> targetids; // 玩家停车区孔位编号。
    bool lost; // 玩家是否已经超时判负。
    bool isrobot; // 玩家是否由机器人控制。
    bool finished; // 玩家是否已经完成全部棋子进停车区。
    int rank; // 玩家当前排名，尚未确定为 0。
    int lossprogress; // 玩家判负瞬间已经进入最终区域的棋子数量。
    int lossorder; // 玩家判负顺序，数值越小表示越早判负。
    std::wstring lossreason; // 玩家判负原因，未判负时为空。
};

struct buttondata
{
    int left; // 按钮左边界。
    int top; // 按钮上边界。
    int right; // 按钮右边界。
    int bottom; // 按钮下边界。
    int code; // 按钮操作类型。
    int value; // 按钮携带的数值。
    bool enabled; // 按钮是否可点击。
    bool selected; // 按钮所在设置项是否被选中。
    std::wstring text; // 按钮显示文字。
};

struct movedata
{
    int playerindex; // 移动玩家下标。
    int pieceindex; // 被移动棋子下标。
    int fromid; // 移动前孔位编号。
    int toid; // 移动后孔位编号。
    int orderindex; // 移动前回合顺序下标。
};

struct gamestate
{
    phasekind phase; // 当前游戏阶段。
    int playercount; // 参加游戏的人数。
    int piececount; // 每个玩家的棋子数量。
    int robotcount; // 参加游戏的机器人人数。
    int setupindex; // 设置页当前选中的项目，0步时、1总时、2人数、3棋子数、4机器人。
    int currentorderindex; // 当前回合在出发顺序中的下标。
    int selectedpiece; // 当前选中的棋子下标，未选中为 -1。
    int robotpreviewtarget; // 机器人或人类已经选择并正在等待确认的落点，未展示为 -1。
    int winnerindex; // 获胜玩家下标，未获胜为 -1。
    int nextlossorder; // 下一名判负玩家使用的顺序编号。
    bool running; // 游戏主循环是否继续运行。
    std::vector<playerdata> players; // 所有玩家数据。
    std::vector<piecedata> pieces; // 所有棋子数据。
    std::vector<int> turnorder; // 按玩家编号决定的行动顺序。
    std::vector<int> movetargets; // 当前选中棋子的可落点。
    std::vector<buttondata> buttons; // 当前界面按钮列表。
    std::vector<movedata> history; // 已完成移动的撤销记录。
    std::vector<int> ranking; // 正常完成或最后胜出的玩家顺序，不包含判负玩家。
    int lastfinished; // 最近完成游戏的玩家下标，没有则为 -1。
    std::wstring status; // 当前状态提示。
};

// 这个函数返回游戏阶段的中文名称。
std::wstring gamedata_phase_text(phasekind phase);

// 这个函数返回三角区的中文名称。
std::wstring gamedata_arm_text(int arm);
