#include <functional>
#include "core/profiler.h"
#include "editor/heightmap.h"
#include "systems/job-system.h"
#include "editor/imgui-widgets.h"
#include "core/random.h"
namespace cyb::editor
{
    std::vector<int> BoxesForGaussian(const float sigma, const int n) {
        const float wIdeal = std::sqrt((12 * sigma * sigma / n) + 1);
        int wl = wIdeal;
        if (wl % 2 == 0) {
            wl--;
        }
        const int wu = wl + 2;

        const float mIdeal =
            (12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) /
            (-4 * wl - 4);
        const int m = std::round(mIdeal);

        std::vector<int> sizes;
        for (int i = 0; i < n; i++) {
            sizes.push_back(i < m ? wl : wu);
        }
        return sizes;
    }

    void BoxBlurH(
        std::vector<float>& src,
        std::vector<float>& dst,
        const int w, const int h, const int r)
    {
        const float m = 1.f / (r + r + 1);
        for (int i = 0; i < h; i++) {
            int ti = i * w;
            int li = ti;
            int ri = ti + r;
            float fv = src[ti];
            float lv = src[ti + w - 1];
            float val = (r + 1) * fv;
            for (int j = 0; j < r; j++) {
                val += src[ti + j];
            }
            for (int j = 0; j <= r; j++) {
                val += src[ri] - fv;
                dst[ti] = val * m;
                ri++;
                ti++;
            }
            for (int j = r + 1; j < w - r; j++) {
                val += src[ri] - src[li];
                dst[ti] = val * m;
                li++;
                ri++;
                ti++;
            }
            for (int j = w - r; j < w; j++) {
                val += lv - src[li];
                dst[ti] = val * m;
                li++;
                ti++;
            }
        }
    }

    void BoxBlurV(
        std::vector<float>& src,
        std::vector<float>& dst,
        const int w, const int h, const int r)
    {
        const float m = 1.f / (r + r + 1);
        for (int i = 0; i < w; i++) {
            int ti = i;
            int li = ti;
            int ri = ti + r * w;
            float fv = src[ti];
            float lv = src[ti + w * (h - 1)];
            float val = (r + 1) * fv;
            for (int j = 0; j < r; j++) {
                val += src[ti + j * w];
            }
            for (int j = 0; j <= r; j++) {
                val += src[ri] - fv;
                dst[ti] = val * m;
                ri += w;
                ti += w;
            }
            for (int j = r + 1; j < h - r; j++) {
                val += src[ri] - src[li];
                dst[ti] = val * m;
                li += w;
                ri += w;
                ti += w;
            }
            for (int j = h - r; j < h; j++) {
                val += lv - src[li];
                dst[ti] = val * m;
                li += w;
                ti += w;
            }
        }
    }

    void BoxBlur(
        std::vector<float>& src,
        std::vector<float>& dst,
        const int w, const int h, const int r)
    {
        dst.assign(src.begin(), src.end());
        BoxBlurH(dst, src, w, h, r);
        BoxBlurV(src, dst, w, h, r);
    }

    std::vector<float> GaussianBlur(
        const std::vector<float>& data,
        const int w, const int h, const int r)
    {
        std::vector<float> src = data;
        std::vector<float> dst(data.size());
        const std::vector<int> boxes = BoxesForGaussian(r, 3);
        BoxBlur(src, dst, w, h, (boxes[0] - 1) / 2);
        BoxBlur(dst, src, w, h, (boxes[1] - 1) / 2);
        BoxBlur(src, dst, w, h, (boxes[2] - 1) / 2);
        return dst;
    }

    float Heightmap::GetHeightAt(uint32_t x, uint32_t y) const
    {
        return image[y * desc.width + x];
    }

    float Heightmap::GetHeightAt(const XMINT2& p) const
    {
        return GetHeightAt(p.x, p.y);
    }

    std::pair<XMINT2, float> Heightmap::FindCandidate(const XMINT2 p0, const XMINT2 p1, const XMINT2 p2) const
    {
        constexpr auto edge = [](const XMINT2& a, const XMINT2& b, const XMINT2& c)
        {
            return (b.x - c.x) * (a.y - c.y) - (b.y - c.y) * (a.x - c.x);
        };

        // Triangle bounding box
        const XMINT2 bbMin = math::Min(math::Min(p0, p1), p2);
        const XMINT2 bbMax = math::Max(math::Max(p0, p1), p2);

        // Forward differencing variables
        int32_t w00 = edge(p1, p2, bbMin);
        int32_t w01 = edge(p2, p0, bbMin);
        int32_t w02 = edge(p0, p1, bbMin);
        const int32_t a01 = p1.y - p0.y;
        const int32_t b01 = p0.x - p1.x;
        const int32_t a12 = p2.y - p1.y;
        const int32_t b12 = p1.x - p2.x;
        const int32_t a20 = p0.y - p2.y;
        const int32_t b20 = p2.x - p0.x;

        // Pre-multiplied z values at vertices
        const float a = static_cast<float>(edge(p0, p1, p2));
        const float z0 = GetHeightAt(p0) / a;
        const float z1 = GetHeightAt(p1) / a;
        const float z2 = GetHeightAt(p2) / a;

        // Iterate over pixels in bounding box
        float maxError = 0;
        XMINT2 maxPoint(0, 0);
        for (int32_t y = bbMin.y; y <= bbMax.y; y++)
        {
            // compute starting offset
            int32_t dx = 0;
            if (w00 < 0 && a12 != 0)
                dx = math::Max(dx, -w00 / a12);
            if (w01 < 0 && a20 != 0)
                dx = math::Max(dx, -w01 / a20);
            if (w02 < 0 && a01 != 0)
                dx = math::Max(dx, -w02 / a01);

            int32_t w0 = w00 + a12 * dx;
            int32_t w1 = w01 + a20 * dx;
            int32_t w2 = w02 + a01 * dx;

            bool wasInside = false;

            for (int32_t x = bbMin.x + dx; x <= bbMax.x; x++)
            {
                // check if inside triangle
                if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                {
                    wasInside = true;

                    // compute z using barycentric coordinates
                    const float z = z0 * w0 + z1 * w1 + z2 * w2;
                    const float dz = math::Abs(z - GetHeightAt(x, y));
                    if (dz > maxError)
                    {
                        maxError = dz;
                        maxPoint = XMINT2(x, y);
                    }
                }
                else if (wasInside)
                {
                    break;
                }

                w0 += a12;
                w1 += a20;
                w2 += a01;
            }

            w00 += b12;
            w01 += b20;
            w02 += b01;
        }

        if ((maxPoint.x == p0.x && maxPoint.y == p0.y) ||
            (maxPoint.x == p1.x && maxPoint.y == p1.y) ||
            (maxPoint.x == p2.x && maxPoint.y == p2.y))
            maxError = 0.0f;

        return std::make_pair(maxPoint, maxError);
    }

    static void CreateNoise(jobsystem::Context& ctx, const NoiseDesc& noiseDesc, Heightmap& map)
    {
        map.image.clear();
        map.image.resize(map.desc.width * map.desc.height);

        jobsystem::Dispatch(ctx, map.desc.height, 256, [&](jobsystem::JobArgs args)
        {
            NoiseGenerator noise(noiseDesc);

            const uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < map.desc.width; ++x)
            {
                const float xs = static_cast<float>(x) / static_cast<float>(map.desc.width);
                const float ys = static_cast<float>(y) / static_cast<float>(map.desc.height);
                const size_t offset = y * map.desc.width + x;

                map.image[offset] = noise.GetNoise(xs, ys);
            }
        });
    }

    // Normalize all values in map to [0..1] range
    static void NormalizeHeightmap(jobsystem::Context& ctx, Heightmap& map)
    {
        float minH = std::numeric_limits<float>::max();
        float maxH = std::numeric_limits<float>::min();

        for (uint32_t y = 0; y < map.desc.height; ++y)
        {
            for (uint32_t x = 0; x < map.desc.width; ++x)
            {
                const size_t offset = y * map.desc.width + x;
                const float value = map.image[offset];
                minH = math::Min(minH, value);
                maxH = math::Max(maxH, value);
            }
        }

        //jobsystem::Dispatch(ctx, height, 256, [&](jobsystem::JobArgs args)
        for (uint32_t y = 0; y < map.desc.height; ++y)
        {
            const float scale = 1.0f / (maxH - minH);
            //const uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < map.desc.width; ++x)
            {
                const size_t offset = y * map.desc.width + x;
                const float value = map.image[offset];
                map.image[offset] = (value - minH) * scale;
            }
        }
    }

    void CombineMaps(jobsystem::Context& ctx, const Heightmap& inA, const Heightmap& inB, Heightmap& out, HeightmapCombineType combineType, float strength)
    {
        out.image.clear();
        out.image.resize(out.desc.width * out.desc.height);

        jobsystem::Dispatch(ctx, out.desc.height, 256, [&](jobsystem::JobArgs args)
        {
            const uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < out.desc.width; ++x)
            {
                const size_t offset = y * out.desc.width + x;
                const float valueA = inA.image[offset];
                const float valueB = inB.image[offset];

                float value = 0.0f;
                switch (combineType)
                {
                case HeightmapCombineType::Add:
                    value = valueA + valueB;
                    break;
                case HeightmapCombineType::Sub:
                    value = valueA - valueB;
                    break;
                case HeightmapCombineType::Mul:
                    value = valueA * valueB;
                    break;
                case HeightmapCombineType::Lerp:
                    value = math::Lerp(valueA, valueB, strength);
                    break;
                }

                out.image[offset] = value;
            }
        });
    }

    static void ApplyBlur(Heightmap& map, float blur)
    {
        std::vector<float> temp = GaussianBlur(map.image, map.desc.width, map.desc.height, blur);
        map.image = temp;
    }

    static void ApplyStrata(jobsystem::Context& ctx, Heightmap& map, const HeightmapDevice& device)
    {
        jobsystem::Dispatch(ctx, map.desc.height, 256, [&](jobsystem::JobArgs args)
        {
            const uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < map.desc.width; ++x)
            {
                const size_t offset = y * map.desc.width + x;
                float value = map.image[offset];

                switch (device.strataOp)
                {
                case HeightmapStrataOp::SharpSub:
                {
                    const float steps = -math::Abs(std::sin(value * device.strata * math::M_PI) * (0.1f / device.strata * math::M_PI));
                    value = (value * 0.5f + steps * 0.5f);
                } break;
                case HeightmapStrataOp::SharpAdd:
                {
                    const float steps = math::Abs(std::sin(value * device.strata * math::M_PI) * (0.1f / device.strata * math::M_PI));
                    value = (value * 0.5f + steps * 0.5f);

                } break;
                case HeightmapStrataOp::Quantize:
                {
                    const float strata = device.strata * 2.0f;
                    value = int(value * strata) * 1.0f / strata;
                } break;
                case HeightmapStrataOp::Smooth:
                {
                    const float strata = device.strata * 2.0f;
                    const float steps = std::sin(value * strata * math::M_PI) * (0.1f / strata * math::M_PI);
                    value = value * 0.5f + steps * 0.5f;
                } break;
                case HeightmapStrataOp::None:
                default:
                    break;
                }

                map.image[offset] = value;
            }
        });
    }

    void CreateColormap(jobsystem::Context& ctx, const Heightmap& height, const ImGradient& colorBand, std::vector<uint32_t>& color)
    {
        assert(height.image.size() == height.desc.width * height.desc.height);

        color.clear();
        color.resize(height.image.size());

        jobsystem::Dispatch(ctx, height.desc.height, 128, [&](jobsystem::JobArgs args)
        {
            uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < height.desc.width; ++x)
            {
                const size_t offset = (size_t)y * height.desc.height + x;
                const float h = height.image[offset];
                color[offset] = colorBand.GetColorAt(h);
            }
        });
    }

    void CreateHeightmap(const HeightmapDesc& desc, Heightmap& heightmap)
    {
        jobsystem::Context ctx;
        Heightmap buffers[2];

        heightmap.desc = desc;
        buffers[0].desc = desc;
        buffers[1].desc = desc;
        CreateNoise(ctx, desc.device1.noise, buffers[0]);
        CreateNoise(ctx, desc.device2.noise, buffers[1]);

        jobsystem::Wait(ctx);
        NormalizeHeightmap(ctx, buffers[0]);
        NormalizeHeightmap(ctx, buffers[1]);

        jobsystem::Wait(ctx);
        ApplyStrata(ctx, buffers[0], desc.device1);
        ApplyStrata(ctx, buffers[1], desc.device2);

        jobsystem::Wait(ctx);
        jobsystem::Execute(ctx, [&](jobsystem::JobArgs args) { ApplyBlur(buffers[0], desc.device1.blur * (desc.width * 0.003f)); });
        jobsystem::Execute(ctx, [&](jobsystem::JobArgs args) { ApplyBlur(buffers[1], desc.device2.blur * (desc.width * 0.003f)); });
        
        jobsystem::Wait(ctx);
        CombineMaps(ctx, buffers[0], buffers[1], heightmap, desc.combineType, desc.combineStrength);

        jobsystem::Wait(ctx);
        NormalizeHeightmap(ctx, heightmap);
        
        jobsystem::Wait(ctx);
        //ApplyErosion(heightmap, desc.iterations, 0.1f, desc.erosion);
        //CreateColormap(ctx, m_heightmap, m_biomeColorBand, m_colormap);
        //jobsystem::Wait(ctx);
    }

}