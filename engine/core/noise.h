// Noise code is inspired by FastNoiseLight:
// https://github.com/Auburn/FastNoiseLite#pragma
#pragma once
#include <array>

namespace cyb::noise2
{
    class NoiseNode
    {
    public:
        NoiseNode() = default;
        virtual ~NoiseNode() = default;
        virtual double GetValue(double x, double y) const = 0;
    };

    /**
     * @brief Extend NoiseNode with size N inputs.
     */
    template <typename T, size_t N>
    class NoiseNodeFixedInputs : public NoiseNode
    {
    public:
        NoiseNodeFixedInputs()
        {
            static_assert(std::is_base_of_v<NoiseNode, T>, "T must derive from NoiseNode");
        }
        virtual ~NoiseNodeFixedInputs() = default;

        constexpr T& SetInput(uint32_t index, const NoiseNode* node)
        {
            m_inputs[index] = node;
            return static_cast<T&>(*this);
        }

        const NoiseNode* GetInput(uint32_t index) const
        {
            return m_inputs[index];
        }

        uint32_t GetInputCount() const
        {
            return static_cast<uint32_t>(m_inputs.size());
        }

    private:
        std::array<const NoiseNode*, N> m_inputs{ nullptr };
    };

    /**
     * @brief Generate uniform perlin noise.
     */
    class NoiseNode_Perlin : public NoiseNode
    {
    public:
        static constexpr uint32_t MAX_OCTAVES = 20;
        
        NoiseNode_Perlin() = default;
        virtual ~NoiseNode_Perlin() = default;

        constexpr NoiseNode_Perlin& SetSeed(uint32_t seed) { m_seed = seed; return *this; }
        constexpr NoiseNode_Perlin& SetFrequency(double frequency) { m_frequency = frequency; return *this; }
        constexpr NoiseNode_Perlin& SetOctaves(uint32_t octaves) { m_octaves = std::clamp(octaves, 1u, MAX_OCTAVES); return *this; }
        constexpr NoiseNode_Perlin& SetLacunarity(double lacunarity) { m_lacunarity = lacunarity; return *this; }
        constexpr NoiseNode_Perlin& SetPersistence(double persistence) { m_persistence = persistence; return *this; }
        
        [[nodiscard]] uint32_t GetSeed() const { return m_seed; }
        [[nodiscard]] double GetFrequency() const { return m_frequency; }
        [[nodiscard]] uint32_t GetOctaves() const { return m_octaves; }
        [[nodiscard]] double GetLacunarity() const { return m_lacunarity; }
        [[nodiscard]] double GetPersistance() const { return m_persistence; }

        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        uint32_t m_seed{ 0 };
        double m_frequency{ 5.0 };
        uint32_t m_octaves{ 4 };
        double m_lacunarity{ 2.0 };
        double m_persistence{ 0.5 };
        double m_varianceCorrection{ 0.0 };
    };

    /**
     * @brief Generate cellular (worley) noise.
     */
    class NoiseNode_Cellular : public NoiseNode
    {
    public:
        static constexpr uint32_t MAX_OCTAVES = 20;

        NoiseNode_Cellular() = default;
        virtual ~NoiseNode_Cellular() = default;

        constexpr NoiseNode_Cellular& SetSeed(uint32_t seed) { m_seed = seed; return *this; }
        constexpr NoiseNode_Cellular& SetFrequency(double frequency) { m_frequency = frequency; return *this; }
        constexpr NoiseNode_Cellular& SetJitterModifier(double jitterModifier) { m_jitterModifier = jitterModifier; return *this; }
        constexpr NoiseNode_Cellular& SetOctaves(uint32_t octaves) { m_octaves = octaves; return *this; }
        constexpr NoiseNode_Cellular& SetLacunarity(double lacunarity) { m_lacunarity = lacunarity; return *this; }
        constexpr NoiseNode_Cellular& SetPersistence(double persistence) { m_persistence = persistence; return *this; }

        [[nodiscard]] uint32_t GetSeed() const { return m_seed; }
        [[nodiscard]] double GetFrequency() const { return m_frequency; }
        [[nodiscard]] double GetJitterModifier() const { return m_jitterModifier; }
        [[nodiscard]] uint32_t GetOctaves() const { return m_octaves; }
        [[nodiscard]] double GetLacunarity() const { return m_lacunarity; }
        [[nodiscard]] double GetPersistance() const { return m_persistence; }

        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        uint32_t m_seed{ 0 };
        double m_frequency{ 5.0 };
        double m_jitterModifier{ 1.0 };
        uint32_t m_octaves{ 4 };
        double m_lacunarity{ 2.0 };
        double m_persistence{ 0.5 };
    };

    /**
     * @brief Generate a constant value regardless of (x, y).
     */
    class NoiseNode_Const : public NoiseNode
    {
    public:
        NoiseNode_Const() = default;
        virtual ~NoiseNode_Const() = default;
        constexpr NoiseNode_Const& SetConstValue(double constValue) { m_constValue = constValue; return *this; }
        [[nodiscard]] double GetConstValue() const { return m_constValue; }
        [[nodiscard]] virtual double GetValue([[maybe_unused]] double x, [[maybe_unused]] double y) const override { return m_constValue; }

    private:
        double m_constValue{ 1.0 };
    };

    /**
     * @brief Blend the value from input(0) with input(1) using input(2) as alpha.
     */
    class NoiseNode_Blend : public NoiseNodeFixedInputs<NoiseNode_Blend, 2>
    {
    public:
        NoiseNode_Blend() = default;
        virtual ~NoiseNode_Blend() = default;
        constexpr NoiseNode_Blend& SetAlpha(double alpha) { m_alpha = alpha; return *this; }
        [[nodiscard]] double GetAlpha() const { return m_alpha; }
        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_alpha{ 0.5 };
    };

    /**
     * @brief Invert the value from input(0).
     */
    class NoiseNode_Invert : public NoiseNodeFixedInputs<NoiseNode_Invert, 1>
    {
    public:
        NoiseNode_Invert() = default;
        virtual ~NoiseNode_Invert() = default;
        [[nodiscard]] virtual double GetValue(double x, double y) const override;
    };

    /**
     * @brief Scale the value from input(0) and offset it with bias.
     */
    class NoiseNode_ScaleBias : public NoiseNodeFixedInputs<NoiseNode_ScaleBias, 1>
    {
    public:
        NoiseNode_ScaleBias() = default;
        virtual ~NoiseNode_ScaleBias() = default;
        constexpr NoiseNode_ScaleBias& SetScale(double scale) { m_scale = scale; return *this; }
        constexpr NoiseNode_ScaleBias& SetBias(double bias) { m_bias = bias; return *this; }
        [[nodiscard]] double GetScale() const { return m_scale; }
        [[nodiscard]] double GetBias() const { return m_bias; }
        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_scale = 1.0;
        double m_bias = 0.0;
    };

    /**
     * @brief Apply strata to the value from input(0).
     */
    class NoiseNode_Strata : public NoiseNodeFixedInputs<NoiseNode_Strata, 1>
    {
    public:
        NoiseNode_Strata() = default;
        virtual ~NoiseNode_Strata() = default;
        constexpr NoiseNode_Strata& SetStrata(double strata) { m_strata = strata; return *this; }
        [[nodiscard]] double GetStrata() const { return m_strata; }
        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_strata{ 5.0 };
    };

    /**
     * @brief Select the value from input(0) or input(1) using input(2) as control.
     */
    class NoiseNode_Select : public NoiseNodeFixedInputs<NoiseNode_Select, 3>
    {
    public:
        NoiseNode_Select() = default;
        virtual ~NoiseNode_Select() = default;

        constexpr NoiseNode_Select& SetThreshold(double threshold) { m_threshold = threshold; return *this; }
        constexpr NoiseNode_Select& SetEdgeFalloff(double edgeFalloff) { m_edgeFalloff = edgeFalloff; return *this; }
        [[nodiscard]] double GetThreshold() const { return m_threshold; }
        [[nodiscard]] double GetEdgeFalloff() const { return m_edgeFalloff; }

        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_threshold{ 0.5 };
        double m_edgeFalloff{ 0.0 };
    };

    struct NoiseImageDimensions
    {
        uint32_t width{ 0 };
        uint32_t height{ 0 };
    };

    struct NoiseImageOffset
    {
        int32_t x{ 0 };
        int32_t y{ 0 };
    };

    class NoiseImage
    {
    public:
        struct Color
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };

        NoiseImage() = default;
        NoiseImage(const NoiseImageDimensions& size);
        
        uint32_t GetWidth() const { return m_size.width; }
        uint32_t GetHeight() const { return m_size.height; }
        uint32_t GetStride() const { return m_stride; }

        Color* GetPtr(uint32_t row);
        Color* GetPtr(uint32_t x, uint32_t y);
        const Color* GetConstPtr(uint32_t row) const;
        const Color* GetConstPtr(uint32_t x, uint32_t y) const;

        size_t GetMemoryUsage() const;

    private:
        NoiseImageDimensions m_size;
        uint32_t m_stride{ 0 };
        std::unique_ptr<Color[]> m_image;
    };

    struct NoiseImageDesc
    {
        noise2::NoiseNode* input{ nullptr };
        NoiseImageDimensions size;
        NoiseImageOffset offset;
        double freqScale{ 1.0 };
    };

    void RenderNoiseImageRows(NoiseImage& image,
                              const NoiseImageDesc& desc,
                              uint32_t rowStart, uint32_t rowCount);
    std::shared_ptr<NoiseImage> RenderNoiseImage(const NoiseImageDesc& desc);
}

namespace cyb::noise
{
    enum class Type
    {
        Perlin,
        Cellular
    };

    enum class CellularReturn
    {
        CellValue,
        Distance,
        Distance2,
        Distance2Add,
        Distance2Sub,
        Distance2Mul,
        Distance2Div
    };

    struct Parameters
    {
        Type type = Type::Perlin;
        uint32_t seed = 0;              // noise function seed value
        float frequency = 5.5f;         // noise function frequency
        uint32_t octaves = 4;           // fractal brownian motion (FBM) octaves
        float lacunarity = 2.0f;        // frequency increase per octave
        float gain = 0.5f;              // amplification increase per octave
        CellularReturn cellularReturnType = CellularReturn::Distance;
        float cellularJitterModifier = 1.0f;
    };

    class Generator
    {
    public:
        Generator(const Parameters& params_) noexcept;

        [[nodiscard]] float GetValue(float x, float y) const noexcept;
        [[nodiscard]] const Parameters& GetParams() const noexcept { return params; }

    private:
        void CalculateFractalBounding() noexcept;
        [[nodiscard]] float GetNoiseSingle(uint32_t seed, float x, float y) const noexcept;
        [[nodiscard]] float SinglePerlin(uint32_t seed, float x, float y) const noexcept;
        [[nodiscard]] float SingleCellular(uint32_t seed, float x, float y) const noexcept;

    private:
        Parameters params;
        float fractalBounding;
    };
}
