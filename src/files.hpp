#pragma once
#include <filesystem>

class Files
{
public:
    static void Init(int argc, char* argv[])
    {
        s_local = std::filesystem::path(argv[0]).remove_filename();
    }

    static std::filesystem::path Local(const std::filesystem::path& path)
    {
        return s_local / path;
    }
private:
    static std::filesystem::path s_local;
};
