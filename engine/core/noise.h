// Noise code is inspired by FastNoiseLight:
// https://github.com/Auburn/FastNoiseLite#pragma
#pragma once
#include <vector>

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
     * @brief Base noise ode class to handle inputs.
     */
    template <typename T>
    class NoiseNodeInputs : public NoiseNode
    {
    public:
        NoiseNodeInputs(uint32_t requiredInputCount) :
            m_inputs(requiredInputCount, nullptr)
        {
        }
        virtual ~NoiseNodeInputs() = default;

        constexpr T& SetInput(uint32_t index, const NoiseNode* node)
        {
            if (index < m_inputs.size())
                m_inputs[index] = node;
            return static_cast<T&>(*this);
        }
        const NoiseNode* GetInput(uint32_t index) const
        {
            return m_inputs[index];
        }
        uint32_t GetRequiredInputCount() const
        {
            return static_cast<uint32_t>(m_inputs.size());
        }

    private:
        std::vector<const NoiseNode*> m_inputs;
    };

    class NoiseNode_Perlin : public NoiseNode
    {
    public:
        static constexpr uint32_t MAX_OCTAVES = 20;
        
        NoiseNode_Perlin() { RecalculateFractalBounding(); }
        virtual ~NoiseNode_Perlin() = default;

        constexpr NoiseNode_Perlin& SetSeed(uint32_t seed) { m_seed = seed; return *this; }
        constexpr NoiseNode_Perlin& SetFrequency(double frequency) { m_frequency = frequency; return *this; }
                  NoiseNode_Perlin& SetOctaves(uint32_t octaves);
        constexpr NoiseNode_Perlin& SetLacunarity(double lacunarity) { m_lacunarity = lacunarity; return *this; }
                  NoiseNode_Perlin& SetPersistence(double persistence);
        
        [[nodiscard]] uint32_t GetSeed() const;
        [[nodiscard]] double GetFrequency() const;
        [[nodiscard]] uint32_t GetOctaves() const;
        [[nodiscard]] double GetLacunarity() const;
        [[nodiscard]] double GetPersistance() const;
        [[nodiscard]] double GetFractalBounding() const;

        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        void RecalculateFractalBounding();

        uint32_t m_seed{ 0 };
        double m_frequency{ 5.0 };
        uint32_t m_octaves{ 4 };
        double m_lacunarity{ 2.0 };
        double m_persistence{ 0.5 };
        double m_fractalBounding{ 0.0 };
        double m_varianceCorrection{ 0.0 };
    };

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

        [[nodiscard]] uint32_t GetSeed() const;
        [[nodiscard]] double GetFrequency() const;
        [[nodiscard]] double GetJitterModifier() const;
        [[nodiscard]] uint32_t GetOctaves() const;
        [[nodiscard]] double GetLacunarity() const;
        [[nodiscard]] double GetPersistance() const;

        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        uint32_t m_seed{ 0 };
        double m_frequency{ 5.0 };
        double m_jitterModifier{ 1.0 };
        uint32_t m_octaves{ 4 };
        double m_lacunarity{ 2.0 };
        double m_persistence{ 0.5 };
    };

    class NoiseNode_Const : public NoiseNode
    {
    public:
        NoiseNode_Const() = default;
        virtual ~NoiseNode_Const() = default;
        constexpr NoiseNode_Const& SetConstValue(double constValue) { m_constValue = constValue; return *this; }
        [[nodiscard]] double GetConstValue() const;
        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_constValue{ 1.0 };
    };

    /**
     * @blend Use linear interpolation to blend two inputs.
     * 
     * Requires 3 inputs.
     * Input 0 = Value 0
     * Input 1 = Value 1
     * Input 2 = Alpha
     */
    class NoiseNode_Blend : public NoiseNodeInputs<NoiseNode_Blend>
    {
    public:
        NoiseNode_Blend() : NoiseNodeInputs(3) {}
        virtual ~NoiseNode_Blend() = default;
        [[nodiscard]] virtual double GetValue(double x, double y) const override;
    };

    class NoiseNode_ScaleBias : public NoiseNodeInputs<NoiseNode_ScaleBias>
    {
    public:
        NoiseNode_ScaleBias() : NoiseNodeInputs(1) {}
        virtual ~NoiseNode_ScaleBias() = default;
        constexpr NoiseNode_ScaleBias& SetScale(double scale) { m_scale = scale; return *this; }
        constexpr NoiseNode_ScaleBias& SetBias(double bias) { m_bias = bias; return *this; }
        [[nodiscard]] double GetScale() const;
        [[nodiscard]] double GetBias() const;
        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_scale = 1.0;
        double m_bias = 0.0;
    };

    class NoiseNode_Strata : public NoiseNodeInputs<NoiseNode_Strata>
    {
    public:
        NoiseNode_Strata() : NoiseNodeInputs(1) {}
        virtual ~NoiseNode_Strata() = default;
        constexpr NoiseNode_Strata& SetStrata(double strata) { m_strata = strata; return *this; }
        [[nodiscard]] double GetStrata() const;
        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_strata = 5.0;
    };

    /**
     * @blend Select value from two inputs using a third input with a threshold.
     *
     * Requires 3 inputs.
     * Input 0 = Value 0
     * Input 1 = Value 1
     * Input 2 = Control
     */
    class NoiseNode_Select : public NoiseNodeInputs<NoiseNode_Select>
    {
    public:
        NoiseNode_Select() : NoiseNodeInputs(3) {}
        virtual ~NoiseNode_Select() = default;

        constexpr NoiseNode_Select& SetThreshold(double threshold) { m_threshold = threshold; return *this; }
        constexpr NoiseNode_Select& SetEdgeFalloff(double edgeFalloff) { m_edgeFalloff = edgeFalloff; return *this; }
        [[nodiscard]] double GetThreshold() const;
        [[nodiscard]] double GetEdgeFalloff() const;

        [[nodiscard]] virtual double GetValue(double x, double y) const override;

    private:
        double m_threshold = 0.5;
        double m_edgeFalloff = 0.0;
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
        NoiseImage(uint32_t width, uint32_t height);
        
        void SetSize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }
        uint32_t GetStride() const { return m_stride; }

        Color* GetPtr(uint32_t row);
        Color* GetPtr(uint32_t x, uint32_t y);
        const Color* GetConstPtr(uint32_t row) const;
        const Color* GetConstPtr(uint32_t x, uint32_t y) const;

        size_t GetMemoryUsage() const;

    private:
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_stride = 0;
        std::unique_ptr<Color[]> m_image;
    };

    struct NoiseImageDesc
    {
        noise2::NoiseNode* input = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        double xOffset = 0.0;
        double yOffset = 0.0;
        double freqScale = 1.0;

        constexpr NoiseImageDesc& SetInput(noise2::NoiseNode* input) { this->input = input; return *this; }
        constexpr NoiseImageDesc& SetSize(uint32_t width, uint32_t height) { this->width = width; this->height = height; return *this; }
        constexpr NoiseImageDesc& SetOffset(double x, double y) { this->xOffset = x; this->yOffset = y; return *this; }
        constexpr NoiseImageDesc& SetFrequencyScale(float scale) { this->freqScale = scale; return *this; }
    };

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
