#include "main.h"

#include "game.h"

// 这个函数作为 main 的转接入口，调用游戏公共入口。
int mainentry()
{
    return game_run();
}

// 这个函数是程序入口。
int main()
{
    return mainentry();
}
