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
inline constexpr int map_window_width = 1120;

// 游戏窗口高度。
inline constexpr int map_window_height = 760;

// 左侧棋盘区域宽度。
inline constexpr int map_left_width = 760;

// 右侧信息栏宽度。
inline constexpr int map_info_width = map_window_width - map_left_width;

// 棋盘中心横坐标。
inline constexpr double map_board_center_x = 380.0;

// 棋盘顶部纵坐标。
inline constexpr double map_board_top_y = 82.0;

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
inline constexpr int map_button_width = 230;

// 右侧按钮高度。
inline constexpr int map_button_height = 38;

// 右侧按钮起始横坐标。
inline constexpr int map_button_x = 822;
