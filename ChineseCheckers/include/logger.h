#pragma once

#include <filesystem>
#include <fstream>
#include <string>

struct loggerdata
{
    std::filesystem::path path; // 日志文件完整路径。
    std::ofstream stream; // 日志输出流。
    bool opened; // 日志是否已经打开。
};

// 这个函数创建 result 文件夹并打开按时间命名的日志文件。
void logger_open(loggerdata& logger);

// 这个函数把一条游戏操作写入日志文件。
void logger_write(loggerdata& logger, const std::wstring& text);

// 这个函数关闭日志文件。
void logger_close(loggerdata& logger);

// 这个函数返回日志路径的宽字符串。
std::wstring logger_path_text(const loggerdata& logger);
