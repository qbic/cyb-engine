#pragma once
#include "core/enum_flags.h"
#include "graphics/device.h"

namespace cyb::rhi
{
    enum class ShaderCompilerFlags
    {
        None                    = 0,
        NoOptimizationBit       = (1 << 0),
        GenerateDebugInfoBit    = (1 << 1)
    };
    CYB_ENABLE_BITMASK_OPERATORS(ShaderCompilerFlags);

    enum class ShaderValidationErrorCode
    {
        NoError,
        NotMultipleOf4,
        InvalidMagic
    };

    struct ShaderValidationResult
    {
        ShaderValidationErrorCode code = ShaderValidationErrorCode::NoError;
        std::string error_message;
    };

    struct ShaderCompilerInput
    {
        ShaderCompilerFlags flags = ShaderCompilerFlags::None;
        ShaderFormat format = ShaderFormat::None;
        ShaderStage stage = ShaderStage::Count;
        std::string name = "shader_src";
        const uint8_t* shadersource = nullptr;
        size_t shadersize = 0;
    };

    struct ShaderCompilerOutput
    {
        std::shared_ptr<void> internal_state;
        inline bool IsValid() const { return internal_state.get() != nullptr; }

        const uint8_t* shaderdata = nullptr;
        size_t shadersize = 0;
        size_t shaderhash = 0;
        std::string error_message;
    };

    ShaderValidationResult ValidateShaderSPIRV(const uint32_t* shaderdata, size_t shadersize);
    bool CompileShader(const ShaderCompilerInput* input, ShaderCompilerOutput* output);
}
