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
    static shaderc_shader_kind _ConvertShaderKind(ShaderStage stage)
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
    ShaderValidationResult ValidateShaderSPIRV(const uint32_t* data, size_t size)
    {
        constexpr uint32_t SpvMagicNumber = 0x07230203;
        ShaderValidationResult result = {};

        if ((size % 4) != 0)
        {
            result.code = ShaderValidationErrorCode::NotMultipleOf4;
            result.error_message = "SPIR - V shader size not multiple of 4!";
            return result;
        }

        const uint32_t magic = data[0];
        if (magic != SpvMagicNumber)
        {
            result.code = ShaderValidationErrorCode::InvalidMagic;
            result.error_message = "Shader has invalid magic number!";
            return result;
        }

        return result;
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
            size_t include_depth)
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
        void ReleaseInclude(shaderc_include_result* data)
        {
            delete static_cast<ReadFileResult*>(data->user_data);
            delete data;
        }
    };

    bool CompileShader(const ShaderCompilerInput* input, ShaderCompilerOutput* output)
    {
        assert(input != nullptr);
        assert(input->format != ShaderFormat::None);
        assert(input->stage != ShaderStage::Count);
        assert(output != nullptr);

        Timer timer;
        *output = ShaderCompilerOutput();

        shaderc::CompileOptions options;
        options.SetIncluder(std::make_unique<CompileShaderIncluder>());
        if (HasFlag(input->flags, ShaderCompilerFlags::GenerateDebugInfoBit))
            options.SetGenerateDebugInfo();
        if (!HasFlag(input->flags, ShaderCompilerFlags::NoOptimizationBit))
            options.SetOptimizationLevel(shaderc_optimization_level_size);

        shaderc::Compiler compiler;
        const shaderc_shader_kind kind = _ConvertShaderKind(input->stage);
        const std::string shadersource = std::string((const char*)input->shadersource, input->shadersize);
        shaderc::PreprocessedSourceCompilationResult pre_result = compiler.PreprocessGlsl(
            shadersource, kind, input->name.c_str(), options);
        if (pre_result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            output->error_message = pre_result.GetErrorMessage();
            return false;
        }
        const std::string preprocessed_source(pre_result.begin());
        
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(preprocessed_source.c_str(), kind, input->name.c_str(), options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            output->error_message = module.GetErrorMessage();
            return false;
        }

        auto internal_state = std::make_shared<std::vector<uint32_t>>(module.cbegin(), module.cend());
        output->internal_state = internal_state;
        output->shaderdata = (uint8_t*)internal_state.get()->data();
        output->shadersize = sizeof(uint32_t) * internal_state.get()->size();
        output->shaderhash = hash::String(preprocessed_source.c_str());

        CYB_TRACE("Compiled GLSL -> SPIR-V (filename={0}) in {1:.2f}ms", input->name, timer.ElapsedMilliseconds());
        return true;
    }
}