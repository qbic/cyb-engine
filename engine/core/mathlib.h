#pragma once
#include <concepts>
#include <type_traits>
#include <algorithm>
#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;

namespace cyb
{
    inline constexpr float g_PI = 3.141592653589793f;
    inline constexpr XMFLOAT3 g_float3Zero { 0, 0, 0 };
    inline constexpr XMFLOAT3 g_float3One  { 1, 1, 1 };
    inline constexpr XMFLOAT3 g_float3OneX { 1, 0, 0 };
    inline constexpr XMFLOAT3 g_float3OneY { 0, 1, 0 };
    inline constexpr XMFLOAT3 g_float3OneZ { 0, 0, 1 };
    inline constexpr XMFLOAT4 g_float4OneW { 0, 0, 0, 1 };

    template <typename T>
    concept IsArithmetic = std::is_arithmetic_v<std::remove_cvref_t<T>>;

    template <typename T>
    concept IsFloatingPoint = std::is_floating_point_v<std::remove_cvref_t<T>>;

    template <typename T>
    concept IsInteger = std::integral<std::remove_cvref_t<T>>;

    template <typename T>
    concept IsVector2Compatable = requires(T t) {
        { t.x } -> IsArithmetic;
        { t.y } -> IsArithmetic;
        requires std::same_as<decltype(t.x), decltype(t.y)>;
    };

    template <IsArithmetic T>
    struct Vector2
    {
        T x{};
        T y{};

        Vector2() = default;
        constexpr Vector2(T _x, T _y) noexcept : x(_x), y(_y) {}

        constexpr Vector2 operator+(const Vector2& rhs) const noexcept { return Vector2{ x + rhs.x, y + rhs.y }; }
        constexpr Vector2 operator-(const Vector2& rhs) const noexcept { return Vector2{ x - rhs.x, y - rhs.y }; }
        constexpr Vector2 operator*(T s) const noexcept { return { x * s, y * s }; }
        constexpr Vector2 operator/(T s) const noexcept { return { x / s, y / s }; }

        constexpr Vector2& operator+=(const Vector2& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
        constexpr Vector2& operator-=(const Vector2& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
        constexpr Vector2& operator*=(T s) noexcept { x *= s; y *= s; return *this; }
        constexpr Vector2& operator/=(T s) noexcept { x /= s; y /= s; return *this; }

        constexpr auto operator<=>(const Vector2&) const = default;
    };

    using Vector2f = Vector2<float>;
    using Vector2i = Vector2<int32_t>;

    /**
     * @brief Component-wise minimum [x, y] of two 2D vectors.
     */
    template<IsVector2Compatable T>
    [[nodiscard]] constexpr T Min(const T& a, const T& b) noexcept
    {
        return T(std::min(a.x, b.x), std::min(a.y, b.y));
    }

    /**
     * @brief Component-wise maximum [x, y] of two 2D vectors.
     */
    template<IsVector2Compatable T>
    [[nodiscard]] constexpr T Max(const T& a, const T& b) noexcept
    {
        return T(std::max(a.x, b.x), std::max(a.y, b.y));
    }

    /**
     * @brief Check if a point is inside the circumcircle of a triangle.
     *
     * @param a First vertex of the triangle.
     * @param b Second vertex of the triangle.
     * @param c Third vertex of the triangle.
     * @param point The point to test.
     * @return true if the point is inside the circumcircle, false otherwise.
     */
    template <IsArithmetic T>
    [[nodiscard]] inline bool IsPointInCircumcircle(
        const Vector2<T>& a,
        const Vector2<T>& b,
        const Vector2<T>& c,
        const Vector2<T>& point) noexcept
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

    /**
     * @brief Convert angle from degrees to radians.
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr T ToRadians(const T degrees) noexcept
    {
        return (degrees * g_PI) / T(180);
    }

    /**
     * @brief Convert angle from radians to degrees.
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr T ToDegrees(const T radians) noexcept
    {
        return (radians * T(180)) / g_PI;
    }

    /**
     * @brief Get the lowest value of two numbers.
     */
    template<IsArithmetic T>
    [[nodiscard]] constexpr T Min(const T a, const T b) noexcept
    {
        return a < b ? a : b;
    }

    /**
     * @brief Get the highest value of two numbers.
     */
    template<IsArithmetic T>
    [[nodiscard]] constexpr T Max(const T a, const T b) noexcept
    {
        return a > b ? a : b;
    }

    /**
     * @brief Get the largest integer number not larger than num.
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr int Floor(T num) noexcept
    {
        return num >= 0 ? (int)num : (int)num - 1;
    }

    /**
     * @brief Get the nearest integer value for num.
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr int Round(T num) noexcept
    {
        return num >= 0 ? (int)(num + 0.5f) : (int)(num - 0.5f);
    }

    /**
     * @brief Get the num ensuring it's within the low and high boundaries.
     */
    template <IsArithmetic T>
    [[nodiscard]] constexpr T Clamp(T num, T low, T high)
    {
        return Min(Max(num, low), high);
    }

    /**
     * @brief Clamp num to min 0, max 1.
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr T Saturate(T x) noexcept
    {
        return Clamp(x, T(0), T(1));
    }

    template <IsInteger T>
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

    [[nodiscard]] constexpr uint32_t NextDivisible(uint32_t num, uint32_t divisor) noexcept
    {
        int bits = num & (divisor - 1);
        if (bits == 0)
            return num;

        return num + (divisor - bits);
    }

    template <IsFloatingPoint T>
    [[nodiscard]] constexpr T Lerp(const T a, const T b, const T t) noexcept
    {
        return a + (b - a) * t;
    }

    [[nodiscard]] constexpr float CubicLerp(float a, float b, float c, float d, float t) noexcept
    {
        float p = (d - c) - (a - b);
        return t * t * t * p + t * t * ((a - b) - p) + t * (c - a) + b;
    }

    /**
     * @brief 3rd-degree Hermite-style smoothstep
     *
     * f(t) = t^2 * (3 - 2 t)
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr T CubicSmoothStep(T t) noexcept
    {
        return t * t * (T(3.0) - T(2.0) * t);
    }

    /**
     * @brief 5th-degree Hermite-style smoothstep
     *
     * f(t) = t^3 * (t * (6 t - 15) + 10)
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr T QuinticSmoothStep(T t) noexcept
    {
        return t * t * t * (t * (t * T(6.0) - T(15.0)) + T(10.0));
    }

    /**
     * @brief 7th-degree Hermite-style smoothstep
     *
     * f(t) = t^4 * (35 - 84 t + 70 t^2 - 20 t^3)
     */
    template <IsFloatingPoint T>
    [[nodiscard]] constexpr T SepticSmoothStep(T t) noexcept
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