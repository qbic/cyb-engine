#include <array>
#include <sstream>
#include <filesystem>
#include <shaderc/shaderc.hpp>
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/hash.h"
#include "core/timer.h"
#include "graphics/shader_compiler.h"

namespace cyb::rhi
{
    static shaderc_shader_kind ConvertShaderKind(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::VS: return shaderc_glsl_vertex_shader;
        case ShaderStage::FS: return shaderc_glsl_fragment_shader;
        case ShaderStage::GS: return shaderc_glsl_geometry_shader;
        default: break;
        }

        assert(0 && "Invalid shader stage");
        return (shaderc_shader_kind)0;
    }

    // TODO: Vulkan SDK probably supplies a better version of this...
    // Perform some minor SPIR-V shader validation
    std::optional<std::string> ValidateShaderSPIRV(std::span<uint32_t> shader)
    {
        static constexpr uint32_t SpvMagicNumber = 0x07230203;
        const uint32_t magic = shader.data()[0];
        if (magic != SpvMagicNumber)
            return std::string("Shader has invalid magic number!");

        if ((shader.size() % 4) != 0)
            return std::string("SPIR - V shader size not multiple of 4!");

        return std::nullopt;
    }

    class CompileShaderIncluder : public shaderc::CompileOptions::IncluderInterface
    {
        struct ReadFileResult
        {
            std::string name;
            std::vector<uint8_t> content;
        };

        shaderc_include_result* GetInclude(
            const char* requested_source,
            shaderc_include_type type,
            const char* requesting_source,
            size_t include_depth) override
        {
            auto userdata = new ReadFileResult;
            const std::string fullpath = std::filesystem::path(requesting_source).parent_path().string() + "/" + requested_source;
#if 0
            // Debugging:
            CYB_TRACE("GetInclude():");
            CYB_TRACE(" requested_source={0}", fullpath);
            CYB_TRACE(" type={0}", type);
            CYB_TRACE(" requesting_source={0}", requesting_source);
            CYB_TRACE(" include_depth={0}", include_depth);
#endif
            if (filesystem::ReadFile(fullpath, userdata->content))
                userdata->name = fullpath;

            auto result = new shaderc_include_result;
            result->user_data = userdata;
            result->source_name = userdata->name.c_str();
            result->source_name_length = userdata->name.size();
            result->content = (const char*)userdata->content.data();
            result->content_length = userdata->content.size();

            return result;
        }

        // Handles shaderc_include_result_release_fn callbacks.
        void ReleaseInclude(shaderc_include_result* data) override
        {
            delete static_cast<ReadFileResult*>(data->user_data);
            delete data;
        }
    };

    std::expected<ShaderCompilerOutput, std::string> CompileShader(const CompileShaderDesc& input)
    {
        assert(input.format != ShaderFormat::None);
        assert(input.stage != ShaderStage::Count);
        assert(input.source.empty() == false);

        Timer timer;
        ShaderCompilerOutput output{};

        // Setup shaderc compile options.
        shaderc::CompileOptions options{};
        options.SetIncluder(std::make_unique<CompileShaderIncluder>());
        if (HasFlag(input.flags, ShaderCompilerFlags::OptimizeForSpeedBit))
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
        if (HasFlag(input.flags, ShaderCompilerFlags::OptimizeForSizeBit))
            options.SetOptimizationLevel(shaderc_optimization_level_size);
        if (HasFlag(input.flags, ShaderCompilerFlags::GenerateDebugInfoBit))
            options.SetGenerateDebugInfo();

        // Setup the shaderc compiler.
        shaderc::Compiler compiler;
        const shaderc_shader_kind kind = ConvertShaderKind(input.stage);
        
        // Run the preprocessor.
        const auto preprocessResult = compiler.PreprocessGlsl(
            (const char*)input.source.data(),
            input.source.size(),
            kind,
            input.name.c_str(),
            options);
        if (preprocessResult.GetCompilationStatus() != shaderc_compilation_status_success)
            return std::unexpected(preprocessResult.GetErrorMessage());
        
        // Compile the preprocessed shader source
        const auto compileResult = compiler.CompileGlslToSpv(
            preprocessResult.begin(),
            kind,
            input.name.c_str(),
            options);
        if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
            return std::unexpected(compileResult.GetErrorMessage());

        // Copy data to compuler output.
        const size_t shaderSize = sizeof(uint32_t) * (compileResult.end() - compileResult.begin());
        output.shader.resize(shaderSize);
        memcpy(output.shader.data(), compileResult.begin(), shaderSize);
        output.hash = hash::String(preprocessResult.begin());

        CYB_TRACE("Compiled GLSL -> SPIR-V (filename={0}) in {1:.2f}ms", input.name, timer.ElapsedMilliseconds());
        return output;
    }
}