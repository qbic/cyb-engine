#pragma once
#include <vector>
#include "core/noise.h"

namespace cyb::editor {

    enum class StrataOp {
        None,
        SharpSub,
        SharpAdd,
        Quantize,
        Smooth
    };
     
    enum class MixOp {
        Add,
        Sub,
        Mul,
        Lerp
    };

    float ApplyStrata(float value, StrataOp op, float strength);
    float ApplyMix(float valueA, float valueB, MixOp op, float strength);

    struct HeightmapGenerator {
        struct Input {
            cyb::noise::Parameters noise;
            StrataOp strataOp = StrataOp::Smooth;
            float strata = 5.0f;
            float exponent = 1.0;

            // mixing is done between current, and next
            // input (if any) in the input list
            MixOp mixOp = MixOp::Lerp;
            float mixing = 0;               // t value for lerp
        };

        std::vector<Input> inputList;

        bool lockedMinMax = true;
        mutable float minHeight = std::numeric_limits<float>::max();
        mutable float maxHeight = std::numeric_limits<float>::min();

        void UnlockMinMax();
        void LockMinMax();
        [[nodiscard]] bool IsMinMaxLocked() const { return lockedMinMax; }
        [[nodiscard]] bool IsMinMaxValid() const { return maxHeight > minHeight; }
        [[nodiscard]] float GetValue(float x, float y) const;
        [[nodiscard]] float GetHeightAt(const XMINT2& p) const;
        [[nodiscard]] std::pair<XMINT2, float> FindCandidate(uint32_t width, uint32_t height, const XMINT2& offset, const XMINT2& p0, const XMINT2& p1, const XMINT2& p2) const;
    };

    [[nodiscard]] std::vector<HeightmapGenerator::Input> GetDefaultInputs();

    namespace detail {
        class HeightmapImageSampler {
        public:
            HeightmapImageSampler(const HeightmapGenerator& generator) : m_generator(generator) {}
            float Get(uint32_t x, uint32_t y) const {
                return m_generator.GetHeightAt(XMINT2(x, y));
            }

        private:
            const HeightmapGenerator& m_generator;
        };

        class  HeightmapImageSamplerNormalized {
        public:
            HeightmapImageSamplerNormalized(const HeightmapGenerator& generator) :
                m_generator(generator),
                m_invHeight(1.0f / (generator.maxHeight - generator.minHeight)) {
            }
            float Get(uint32_t x, uint32_t y) const {
                return m_generator.GetHeightAt(XMINT2(x, y)) * m_invHeight;
            }

        private:
            const HeightmapGenerator& m_generator;
            float m_invHeight;
        };

        template <class T>
        class CreateHeightmapImage {
        public:
            CreateHeightmapImage() = delete;
            CreateHeightmapImage(std::vector<float>& heightmap, int width, int height, const HeightmapGenerator& generator, int offsetX, int offsetY, float freqScale) {
                const T sampler(generator);
                heightmap.clear();
                heightmap.resize(width * height);
                for (int32_t y = 0; y < height; y++) {
                    for (int32_t x = 0; x < width; x++) {
                        const float scale = (1.0f / (float)width) * freqScale;
                        const int sampleX = (int)std::round((float)(x + offsetX) * scale);
                        const int sampleY = (int)std::round((float)(y + offsetY) * scale);

                        float& point = heightmap[y * width + x];
                        point = sampler.Get(sampleX, sampleY);
                    }
                }
            }
            ~CreateHeightmapImage() = default;
        };
    }

    inline void CreateHeightmapImage(std::vector<float>& heightmap, int width, int height, const HeightmapGenerator& generator, int offsetX, int offsetY, float freqScale = 1.0f) {
        detail::CreateHeightmapImage<detail::HeightmapImageSampler>(heightmap, width, height, generator, offsetX, offsetY, freqScale);
    }

    inline void CreateHeightmapImageNormalized(std::vector<float>& heightmap, int width, int height, const HeightmapGenerator& generator, int offsetX, int offsetY, float freqScale = 1.0f) {
        // minHeight and maxHeight needs to be set in HeightmapGenerator before
        // creating a normalized heightmap, this can be done manually or by 
        // generating metadata with CreateHeightmapImage() between 
        // HeightmapGenerator::UnlockMinMax() and HeightmapGenerator::LockMinMax()
        assert(generator.IsMinMaxValid());
        assert(generator.IsMinMaxLocked());
        detail::CreateHeightmapImage<detail::HeightmapImageSamplerNormalized>(heightmap, width, height, generator, offsetX, offsetY, freqScale);
    }

    class HeightmapTriangulator
    {
    public:
        HeightmapTriangulator(const HeightmapGenerator* heightmap, uint32_t width, uint32_t height, const XMINT2& offset) :
            m_Heightmap(heightmap),
            m_Offset(offset),
            m_Width(width),
            m_Height(height)
        {
        }

        void Run(const float maxError, const uint32_t maxTriangles, const uint32_t maxPoints);

        uint32_t NumPoints() const { return (uint32_t)m_Points.size(); }
        uint32_t NumTriangles() const { return (uint32_t)m_queue.size(); }
        float Error() const { return m_Errors[m_queue[0]]; }

        std::vector<XMFLOAT3> GetPoints() const;
        std::vector<XMINT3> GetTriangles() const;

    private:
        void Flush();
        void Step();
        uint32_t AddPoint(const XMINT2 point);
        int AddTriangle(
            const int a, const int b, const int c,
            const int ab, const int bc, const int ca,
            int32_t e);
        void Legalize(const int a);
        void QueuePush(const int t);
        int QueuePop();
        int QueuePopBack();
        void QueueRemove(const int t);
        bool QueueLess(const size_t i, const size_t j) const;
        void QueueSwap(const size_t i, const size_t j);
        void QueueUp(const size_t j0);
        bool QueueDown(const size_t i0, const size_t n);

        const HeightmapGenerator* m_Heightmap;
        XMINT2 m_Offset;
        uint32_t m_Width;
        uint32_t m_Height;
        std::vector<XMINT2> m_Points;
        std::vector<uint32_t> m_triangles;      // triangle indexes
        std::vector<int> m_Halfedges;
        std::vector<XMINT2> m_Candidates;
        std::vector<float> m_Errors;

        std::vector<size_t> m_queueIndexes;
        std::vector<uint32_t> m_queue;
        std::vector<uint32_t> m_pending;
    };
}
