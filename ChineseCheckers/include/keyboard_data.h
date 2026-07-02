#pragma once

// 主键盘数字 0 的键值。
inline constexpr int keyboard_key_digit_0 = 0x30;

// 主键盘数字 9 的键值。
inline constexpr int keyboard_key_digit_9 = 0x39;

// 小键盘数字 0 的键值。
inline constexpr int keyboard_key_numpad_0 = 0x60;

// 小键盘数字 9 的键值。
inline constexpr int keyboard_key_numpad_9 = 0x69;

// ESC 键的键值。
inline constexpr int keyboard_key_escape = 0x1b;

// 回车键的键值。
inline constexpr int keyboard_key_enter = 0x0d;

// Delete 键的键值。
inline constexpr int keyboard_key_delete = 0x2e;

// Insert 键的键值，小键盘 NumLock 关闭时数字 0 会产生这个值。
inline constexpr int keyboard_key_insert = 0x2d;

// End 键的键值，小键盘 NumLock 关闭时数字 1 会产生这个值。
inline constexpr int keyboard_key_end = 0x23;

// PageDown 键的键值，小键盘 NumLock 关闭时数字 3 会产生这个值。
inline constexpr int keyboard_key_pagedown = 0x22;

// Left 键的键值，小键盘 NumLock 关闭时数字 4 会产生这个值。
inline constexpr int keyboard_key_left = 0x25;

// Clear 键的键值，小键盘 NumLock 关闭时数字 5 会产生这个值。
inline constexpr int keyboard_key_clear = 0x0c;

// Right 键的键值，小键盘 NumLock 关闭时数字 6 会产生这个值。
inline constexpr int keyboard_key_right = 0x27;

// Home 键的键值，小键盘 NumLock 关闭时数字 7 会产生这个值。
inline constexpr int keyboard_key_home = 0x24;

// Up 键的键值，小键盘 NumLock 关闭时数字 8 会产生这个值。
inline constexpr int keyboard_key_up = 0x26;

// Down 键的键值，小键盘 NumLock 关闭时数字 2 会产生这个值。
inline constexpr int keyboard_key_down = 0x28;

// PageUp 键的键值，小键盘 NumLock 关闭时数字 9 会产生这个值。
inline constexpr int keyboard_key_pageup = 0x21;

// 棋子编号最小值，按用户要求从 0 开始。
inline constexpr int keyboard_piece_min = 0;

// 棋子编号最大值，一个数字键最多直接选择到 9。
inline constexpr int keyboard_piece_max = 9;

// 落点返回编号，选择落点阶段按 0 返回上一步。
inline constexpr int keyboard_target_back = 0;

// 落点编号最小值，按用户要求从 1 开始。
inline constexpr int keyboard_target_min = 1;

// 落点编号最大值，一个数字键最多直接选择到 9。
inline constexpr int keyboard_target_max = 9;
