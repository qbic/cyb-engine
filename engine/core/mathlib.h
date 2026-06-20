#pragma once
#include <concepts>
#include <type_traits>
#include <algorithm>
#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;

namespace cyb
{
    inline constexpr float PI = 3.141592653589793f;
    inline constexpr XMFLOAT3 g_float3Zero { 0, 0, 0 };
    inline constexpr XMFLOAT3 g_float3One  { 1, 1, 1 };
    inline constexpr XMFLOAT3 g_float3OneX { 1, 0, 0 };
    inline constexpr XMFLOAT3 g_float3OneY { 0, 1, 0 };
    inline constexpr XMFLOAT3 g_float3OneZ { 0, 0, 1 };
    inline constexpr XMFLOAT4 g_float4OneW { 0, 0, 0, 1 };

    // Check if a type is an arithmetic type (integral or floating point, but not bool).
    template <typename T>
    concept arithmetic = (std::integral<std::remove_cvref_t<T>> || std::floating_point<std::remove_cvref_t<T>>) &&
                         !std::same_as<std::remove_cvref_t<T>, bool>;

    /** A 2D vector with arithmetic components. */
    template <arithmetic T>
    struct TVec2
    {
        T x{};
        T y{};

        TVec2() = default;
        constexpr TVec2(T _x, T _y) noexcept : x(_x), y(_y) {}
        constexpr TVec2(T v) noexcept : x(v), y(v) {}
        constexpr TVec2(const TVec2& other)  noexcept = default;
        constexpr TVec2& operator=(const TVec2& other) noexcept = default;

        [[nodiscard]] constexpr TVec2 operator+(const TVec2& rhs) const noexcept { return TVec2{ x + rhs.x, y + rhs.y }; }
        [[nodiscard]] constexpr TVec2 operator-(const TVec2& rhs) const noexcept { return TVec2{ x - rhs.x, y - rhs.y }; }
        [[nodiscard]] constexpr TVec2 operator*(T s) const noexcept { return { x * s, y * s }; }
        [[nodiscard]] constexpr TVec2 operator/(T s) const noexcept { return { x / s, y / s }; }
        constexpr TVec2& operator+=(const TVec2& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
        constexpr TVec2& operator-=(const TVec2& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
        constexpr TVec2& operator*=(T s) noexcept { x *= s; y *= s; return *this; }
        constexpr TVec2& operator/=(T s) noexcept { x /= s; y /= s; return *this; }
        constexpr auto operator<=>(const TVec2&) const = default;

        [[nodiscard]] constexpr TVec2 ComponentMin(const TVec2& rhs) const noexcept { return TVec2{ std::min(x, rhs.x), std::min(y, rhs.y) }; }
        [[nodiscard]] constexpr TVec2 ComponentMax(const TVec2& rhs) const noexcept { return TVec2{ std::max(x, rhs.x), std::max(y, rhs.y) }; }

        [[nodiscard]] T Cross(const TVec2& rhs) const noexcept { return x * rhs.y - y * rhs.x; }
    };

    using Vec2 = TVec2<float>;
    using DVec2 = TVec2<double>;
    using IVec2 = TVec2<int32_t>;
    using UVec2 = TVec2<uint32_t>;

    /**
     * Check if a point is inside the circumcircle of a triangle.
     *
     * @param a First vertex of the triangle.
     * @param b Second vertex of the triangle.
     * @param c Third vertex of the triangle.
     * @param point The point to test.
     * @return true if the point is inside the circumcircle, false otherwise.
     */
    template <arithmetic T>
    [[nodiscard]] inline bool IsPointInCircumcircle(
        const TVec2<T>& a,
        const TVec2<T>& b,
        const TVec2<T>& c,
        const TVec2<T>& point) noexcept
    {
        const T ax = a.x - point.x;
        const T ay = a.y - point.y;
        const T bx = b.x - point.x;
        const T by = b.y - point.y;
        const T cx = c.x - point.x;
        const T cy = c.y - point.y;
        const T det =
            (ax * ax + ay * ay) * (bx * cy - cx * by) -
            (bx * bx + by * by) * (ax * cy - cx * ay) +
            (cx * cx + cy * cy) * (ax * by - bx * ay);
        return det < T(0);
    }

    /** Check if three points are collinear. */
    template <arithmetic T>
    [[nodiscard]] bool IsPointsCollinear(const TVec2<T>& p0, const TVec2<T>& p1, const TVec2<T>& p2) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            const T area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
            const T scale = std::max(
                std::max(std::abs(p1.x - p0.x), std::abs(p1.y - p0.y)),
                std::max(std::abs(p2.x - p0.x), std::abs(p2.y - p0.y)));
            return std::abs(area) <= std::numeric_limits<T>::epsilon() * scale * scale;
        }
        else
            return (p1.y - p0.y) * (p2.x - p1.x) == (p2.y - p1.y) * (p1.x - p0.x);
    }

    /** Convert angle from degrees to radians. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T ToRadians(const T degrees) noexcept
    {
        return (degrees * PI) / T(180);
    }

    /** Convert angle from radians to degrees. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T ToDegrees(const T radians) noexcept
    {
        return (radians * T(180)) / PI;
    }

    /** Clamp num to min 0, max 1. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T Saturate(T x) noexcept
    {
        return std::min(std::max(x, T(0)), T(1));
    }

    /** Round up to the next power of two. */
    template <std::integral T>
    [[nodiscard]] constexpr T NextPowerOfTwo(T x) noexcept
    {
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        if constexpr (sizeof(x) == 8)
            x |= x >> 32u;
        return ++x;
    }

    /** Round up to the next multiple of divisor. Divisor must be a power of two. */
    [[nodiscard]] constexpr uint32_t NextDivisible(uint32_t num, uint32_t divisor) noexcept
    {
        int bits = num & (divisor - 1);
        if (bits == 0)
            return num;

        return num + (divisor - bits);
    }

    /** Linear interpolation between a and b by t. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T Lerp(const T a, const T b, const T t) noexcept
    {
        return a + (b - a) * t;
    }

    /** Cubic interpolation between a, b, c, and d by t. */
    template <std::floating_point T>
    [[nodiscard]] constexpr float CubicLerp(T a, T b, T c, T d, T t) noexcept
    {
        const T p{ (d - c) - (a - b) };
        return t * t * t * p + t * t * ((a - b) - p) + t * (c - a) + b;
    }

    /** Cubic smoothstep function. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T CubicSmoothstep(T t) noexcept
    {
        return t * t * (T(3.0) - T(2.0) * t);
    }

    /** Quintic smoothstep function. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T QuinticSmoothstep(T t) noexcept
    {
        return t * t * t * (t * (t * T(6.0) - T(15.0)) + T(10.0));
    }

    /** Septic smoothstep function. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T SepticSmoothstep(T t) noexcept
    {
        const T t2 = t * t;
        const T t3 = t2 * t;
        const T t4 = t2 * t2;
        return t4 * (T(35.0) - T(84.0) * t + T(70.0) * t2 - T(20.0) * t3);
    }

    [[nodiscard]] inline float XM_CALLCONV Distance(const XMVECTOR& v1, const XMVECTOR& v2) noexcept
    {
        return XMVectorGetX(XMVector3Length(XMVectorSubtract(v1, v2)));
    }

    [[nodiscard]] inline float XM_CALLCONV Distance(const XMFLOAT3& v1, const XMFLOAT3& v2) noexcept
    {
        return Distance(XMLoadFloat3(&v1), XMLoadFloat3(&v2));
    }

    [[nodiscard]] inline uint32_t StoreColor_RGB(const XMFLOAT3& color) noexcept
    {
        return (uint32_t)((uint8_t)(Saturate(color.x) * 255.0f) << 0) |
               (uint32_t)((uint8_t)(Saturate(color.y) * 255.0f) << 8) |
               (uint32_t)((uint8_t)(Saturate(color.z) * 255.0f) << 16);
    }

    [[nodiscard]] inline uint32_t StoreColor_RGBA(const XMFLOAT4& color) noexcept
    {
        uint32_t retval = 0;

        retval |= (uint32_t)((uint8_t)(Saturate(color.x) * 255.0f) << 0);
        retval |= (uint32_t)((uint8_t)(Saturate(color.y) * 255.0f) << 8);
        retval |= (uint32_t)((uint8_t)(Saturate(color.z) * 255.0f) << 16);
        retval |= (uint32_t)((uint8_t)(Saturate(color.w) * 255.0f) << 24);

        return retval;
    }

    /*
     * Compute the intersection of a ray (Origin, Direction) with a triangle
     * (V0, V1, V2).  Return true if there is an intersection and also set *pDist
     * to the distance along the ray to the intersection.
     *
     * The algorithm is based on Moller, Tomas and Trumbore, "Fast, Minimum Storage
     * Ray-Triangle Intersection", Journal of Graphics Tools, vol. 2, no. 1,
     * pp 21-28, 1997.
     */
    [[nodiscard]] inline bool XM_CALLCONV RayTriangleIntersects(FXMVECTOR Origin, FXMVECTOR Direction, FXMVECTOR V0, GXMVECTOR V1, HXMVECTOR V2, float& Dist, XMFLOAT2& bary) noexcept
    {
        XMVECTOR Zero = XMVectorZero();

        XMVECTOR e1 = XMVectorSubtract(V1, V0);
        XMVECTOR e2 = XMVectorSubtract(V2, V0);

        // p = Direction ^ e2;
        XMVECTOR p = XMVector3Cross(Direction, e2);

        // det = e1 * p;
        XMVECTOR det = XMVector3Dot(e1, p);

        XMVECTOR u, v, t;

        if (XMVector3GreaterOrEqual(det, g_RayEpsilon))
        {
            // Determinate is positive (front side of the triangle).
            XMVECTOR s = XMVectorSubtract(Origin, V0);

            // u = s * p;
            u = XMVector3Dot(s, p);

            XMVECTOR NoIntersection = XMVectorLess(u, Zero);
            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(u, det));

            // q = s ^ e1;
            XMVECTOR q = XMVector3Cross(s, e1);

            // v = Direction * q;
            v = XMVector3Dot(Direction, q);

            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(v, Zero));
            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(XMVectorAdd(u, v), det));

            // t = e2 * q;
            t = XMVector3Dot(e2, q);

            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(t, Zero));

            if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt()))
            {
                Dist = 0.f;
                return false;
            }
        }
        else if (XMVector3LessOrEqual(det, g_RayNegEpsilon))
        {
            // Determinate is negative (back side of the triangle).
            XMVECTOR s = XMVectorSubtract(Origin, V0);

            // u = s * p;
            u = XMVector3Dot(s, p);

            XMVECTOR NoIntersection = XMVectorGreater(u, Zero);
            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(u, det));

            // q = s ^ e1;
            XMVECTOR q = XMVector3Cross(s, e1);

            // v = Direction * q;
            v = XMVector3Dot(Direction, q);

            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(v, Zero));
            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(XMVectorAdd(u, v), det));

            // t = e2 * q;
            t = XMVector3Dot(e2, q);

            NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(t, Zero));

            if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt()))
            {
                Dist = 0.f;
                return false;
            }
        }
        else
        {
            // Parallel ray.
            Dist = 0.f;
            return false;
        }

        t = XMVectorDivide(t, det);

        const XMVECTOR invdet = XMVectorReciprocal(det);
        XMStoreFloat(&bary.x, u * invdet);
        XMStoreFloat(&bary.y, v * invdet);

        // Store the x-component to *pDist
        XMStoreFloat(&Dist, t);

        return true;
    }
}