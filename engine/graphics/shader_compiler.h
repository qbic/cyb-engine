#pragma once
#include <expected>
#include <span>
#include "core/enum_flags.h"
#include "graphics/device.h"

namespace cyb::rhi
{
    enum class ShaderCompilerFlags
    {
        None                    = 0,
        OptimizeForSpeedBit     = BIT(1),
        OptimizeForSizeBit      = BIT(2),
        GenerateDebugInfoBit    = BIT(3)
    };
    CYB_ENABLE_BITMASK_OPERATORS(ShaderCompilerFlags);

    struct CompileShaderDesc
    {
        ShaderCompilerFlags flags{ ShaderCompilerFlags::None };
        ShaderType stage{ ShaderType::Count };
        std::string name{};             //!< Should be the filename if the source is a file.
        std::span<uint8_t> source{};    //!< Shader sourcecode data.
    };

    struct ShaderCompilerOutput
    {
        std::vector<uint8_t> shader{};  //!< Compiled SPIR-V shader data.
        uint64_t hash{ 0 };             //!< Hash computed from preprocessed shader.
    };

    /**
     * @brief Do some quick validation checks on a SPIR-V shader.
     * @return nullopt if shader passed validation, otherwise a
     *         string describing the validation error.
     */
    std::optional<std::string> ValidateShaderSPIRV(std::span<uint32_t> shader);

    /**
     * @brief Compile a GLSL shader into SPIR-V.
     * @return A valid ShaderCompilerOutput if successful, otherwise a string containing error message.
     */
    std::expected<ShaderCompilerOutput, std::string> CompileShader(const CompileShaderDesc& input);
}
