#pragma once
#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.hpp>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

class ShaderCompiler
{
public:
    static std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::ate);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = file.tellg();

        std::string result;
        result.resize(fileSize);

        file.seekg(0);
        file.read(result.data(), fileSize);
        file.close();
        return result;
    }

    static std::vector<uint32_t> CompileFromFile(const std::filesystem::path& path, shaderc_shader_kind kind)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(ReadFile(path), kind, path.c_str(), options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            spdlog::error("Shader compilation failed: {}", result.GetErrorMessage());
            return {};
        }


        return {result.begin(), result.end()};
    }
};
