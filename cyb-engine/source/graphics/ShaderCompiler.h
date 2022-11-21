#pragma once
#include "Core/EnumFlags.h"
#include "Graphics/GraphicsDevice.h"

namespace cyb::graphics
{
    enum class ShaderCompilerFlags
    {
        NONE = 0,
        DISABLE_OPTIMAZATION = 1 << 0,
        GENERATE_DEBUG_INFO = 1 << 1
    };
    CYB_ENABLE_BITMASK_OPERATORS(ShaderCompilerFlags);

    enum class ShaderValidationErrorCode
    {
        NO_ERROR,
        NOT_MULTIPLE_OF_4,
        INVALID_MAGIC
    };

    struct ShaderValidationResult
    {
        ShaderValidationErrorCode code = ShaderValidationErrorCode::NO_ERROR;
        std::string error_message;
    };

    struct ShaderCompilerInput
    {
        ShaderCompilerFlags flags = ShaderCompilerFlags::NONE;
        ShaderFormat format = ShaderFormat::NONE;
        ShaderStage stage = ShaderStage::COUNT;
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
