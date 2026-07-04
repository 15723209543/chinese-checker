#pragma once

#include <vector>

struct analysisplayerdata
{
    int playerindex; // 玩家下标。
    int score; // 当前局势评分，最低为 0。
    int probability; // 折算后的胜利概率百分比。
};

struct analysisdata
{
    std::vector<analysisplayerdata> players; // 所有玩家的分析结果。
};

// 基础分数。
inline constexpr int analysis_base_score = 100;

// 过河最高加分，影响强于配合。
inline constexpr int analysis_cross_bonus = 28;

// 自己棋子配合跳跃加分。
inline constexpr int analysis_cooperate_bonus = 16;

// 给其他人提供桥梁扣分。
inline constexpr int analysis_bridge_penalty = 10;

// 到达终点加分，影响最强。
inline constexpr int analysis_target_bonus = 60;
