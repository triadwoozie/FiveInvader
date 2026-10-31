#include <os/logger.h>
#include <cstdio>

static FILE* s_fileLog = nullptr;

void os::logger::Init()
{
    s_fileLog = fopen("/tmp/fiveinvader_runtime.log", "w");
}

void os::logger::Log(const std::string_view str, os::logger::ELogType type, const char* func)
{
    if (!s_fileLog)
    {
        s_fileLog = fopen("/tmp/fiveinvader_runtime.log", "a");
    }

    if (func)
    {
        fmt::println("[{}] {}", func, str);
        if (s_fileLog)
        {
            fprintf(s_fileLog, "[%s] %.*s\n", func, (int)str.size(), str.data());
            fflush(s_fileLog);
        }
    }
    else
    {
        fmt::println("{}", str);
        if (s_fileLog)
        {
            fprintf(s_fileLog, "%.*s\n", (int)str.size(), str.data());
            fflush(s_fileLog);
        }
    }
    fflush(stdout);
}
