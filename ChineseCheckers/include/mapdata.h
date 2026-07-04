#pragma once

// 棋盘总行数，对应标准跳棋星形棋盘。
inline constexpr int map_row_count = 17;

// 棋盘单行最大孔位数。
inline constexpr int map_max_col_count = 13;

// 每一行的孔位数量，这是固定地图数据。
inline constexpr int map_row_lengths[map_row_count] = { 1, 2, 3, 4, 13, 12, 11, 10, 9, 10, 11, 12, 13, 4, 3, 2, 1 };

// 星形棋盘的角数量。
inline constexpr int map_arm_count = 6;

// 游戏窗口宽度。
inline constexpr int map_window_width = 1320;

// 游戏窗口高度。
inline constexpr int map_window_height = 860;

// 左侧棋盘区域宽度。
inline constexpr int map_left_width = 860;

// 右侧信息栏宽度。
inline constexpr int map_info_width = map_window_width - map_left_width;

// 棋盘中心横坐标。
inline constexpr double map_board_center_x = 430.0;

// 棋盘顶部纵坐标。
inline constexpr double map_board_top_y = 94.0;

// 相邻孔位横向间距。
inline constexpr double map_point_space = 40.0;

// 相邻行的纵向间距。
inline constexpr double map_row_space = 34.6410161514;

// 棋盘孔位半径。
inline constexpr int map_hole_radius = 10;

// 棋子半径。
inline constexpr int map_piece_radius = 14;

// 可落点标记半径。
inline constexpr int map_target_radius = 17;

// 右侧按钮宽度。
inline constexpr int map_button_width = 330;

// 右侧按钮高度。
inline constexpr int map_button_height = 38;

// 右侧按钮起始横坐标。
inline constexpr int map_button_x = map_left_width + 54;

// 设置页第一行按钮纵坐标。
inline constexpr int map_setup_first_button_y = 328;

// 设置页每一项之间的纵向间距。
inline constexpr int map_setup_row_gap = 70;

// 设置页按钮上方标题偏移量。
inline constexpr int map_setup_label_offset = 28;

// 设置页开始按钮纵坐标。
inline constexpr int map_setup_start_button_y = 648;

// 设置页退出按钮纵坐标。
inline constexpr int map_setup_exit_button_y = 696;

// 游戏中底部第一个按钮纵坐标。
inline constexpr int map_bottom_button_first_y = 760;

// 游戏中底部第二个按钮纵坐标。
inline constexpr int map_bottom_button_second_y = 808;

// 游戏结束时重新开始按钮纵坐标。
inline constexpr int map_game_over_restart_y = 712;
