#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <windows.h>

// 这个函数把宽字符串转成 utf-8 字符串，保证中文日志可以直接阅读。
static std::string logger_to_utf8(const std::wstring& text)
{
    // size 保存 utf-8 字节数。
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        return std::string();
    }

    // result 保存转换后的 utf-8 内容，先为结尾空字符预留空间。
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}

// 这个函数生成当前时间对应的文件名。
static std::wstring logger_make_filename()
{
    // now 保存当前系统时间。
    auto now = std::chrono::system_clock::now();
    // timevalue 保存可格式化的时间值。
    std::time_t timevalue = std::chrono::system_clock::to_time_t(now);
    // local 保存本地时间结构。
    std::tm local{};
    localtime_s(&local, &timevalue);

    // output 保存时间格式化结果。
    std::wstringstream output;
    output << std::put_time(&local, L"%Y%m%d_%H%M%S") << L".txt";
    return output.str();
}

// 这个函数生成日志行前面的时间戳。
static std::wstring logger_make_time()
{
    // now 保存当前系统时间。
    auto now = std::chrono::system_clock::now();
    // timevalue 保存可格式化的时间值。
    std::time_t timevalue = std::chrono::system_clock::to_time_t(now);
    // local 保存本地时间结构。
    std::tm local{};
    localtime_s(&local, &timevalue);

    // output 保存时间格式化结果。
    std::wstringstream output;
    output << std::put_time(&local, L"%H:%M:%S");
    return output.str();
}

// 这个函数创建 result 文件夹并打开按时间命名的日志文件。
void logger_open(loggerdata& logger)
{
    logger.opened = false;
    std::filesystem::path folder = std::filesystem::current_path() / L"result"; // folder 是日志文件夹。
    std::filesystem::create_directories(folder);

    logger.path = folder / logger_make_filename();
    logger.stream.open(logger.path, std::ios::binary);
    if (logger.stream.is_open())
    {
        logger.opened = true;
        const unsigned char bom[] = { 0xef, 0xbb, 0xbf }; // bom 让记事本按 utf-8 打开。
        logger.stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
        logger_write(logger, L"游戏日志创建");
    }
}

// 这个函数把一条游戏操作写入日志文件。
void logger_write(loggerdata& logger, const std::wstring& text)
{
    if (!logger.opened)
    {
        return;
    }

    // line 保存带时间戳的一整行日志。
    std::wstring line = L"[" + logger_make_time() + L"] " + text + L"\n";
    logger.stream << logger_to_utf8(line);
    logger.stream.flush();
}

// 这个函数关闭日志文件。
void logger_close(loggerdata& logger)
{
    if (logger.opened)
    {
        logger_write(logger, L"游戏日志关闭");
        logger.stream.close();
        logger.opened = false;
    }
}

// 这个函数返回日志路径的宽字符串。
std::wstring logger_path_text(const loggerdata& logger)
{
    return logger.path.wstring();
}
