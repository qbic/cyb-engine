#pragma once
#include <cmath>
#include <type_traits>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

[[nodiscard]] constexpr XMINT2 operator+(const XMINT2& a, const XMINT2& b) noexcept {
    return XMINT2(a.x + b.x, a.y + b.y);
}

namespace cyb::math {

    constexpr float PI = 3.14159265358979323846264338327950288f;
    constexpr XMFLOAT4X4 MATRIX_IDENTITY = XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    constexpr XMFLOAT3 VECTOR_ZERO = XMFLOAT3(0, 0, 0);
    constexpr XMFLOAT3 VECTOR_IDENTITY = XMFLOAT3(1, 1, 1);
    constexpr XMFLOAT3 VECTOR_UP = XMFLOAT3(0, 1, 0);
    constexpr XMFLOAT3 VECTOR_FORWARD = XMFLOAT3(0, 0, 1);
    constexpr XMFLOAT3 VECTOR_RIGHT = XMFLOAT3(1, 0, 0);
    constexpr XMFLOAT4 QUATERNION_IDENTITY = XMFLOAT4(0, 0, 0, 1);

    template <typename T>
    [[nodiscard]] constexpr T ToRadians(const T degrees) noexcept {
        return (degrees * PI) / T(180);
    }

    template <typename T>
    [[nodiscard]] constexpr T ToDegrees(const T radians) noexcept {
        return (radians * T(180)) / PI;
    }

    // Returns highest of 2 values
    template<class T>
    [[nodiscard]] constexpr T Max(const T a, const T b) noexcept {
        return a > b ? a : b;
    }

    // Returns lowest of 2 values
    template<class T>
    [[nodiscard]] constexpr T Min(const T a, const T b) noexcept {
        return a < b ? a : b;
    }

    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
    [[nodiscard]] constexpr int Floor(T f) noexcept {
        return f >= 0 ? (int)f : (int)f - 1; 
    }

    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
    [[nodiscard]] constexpr int Round(T f) noexcept {
        return f >= 0 ? (int)(f + 0.5f) : (int)(f - 0.5f);
    }

    [[nodiscard]] constexpr XMINT2 Min(const XMINT2& a, const XMINT2& b) noexcept {
        return XMINT2(Min(a.x, b.x), Min(a.y, b.y));
    }

    [[nodiscard]] constexpr XMINT2 Max(const XMINT2& a, const XMINT2& b) noexcept {
        return XMINT2(Max(a.x, b.x), Max(a.y, b.y));
    }

    [[nodiscard]] constexpr XMUINT2 Min(const XMUINT2& a, const XMUINT2& b) noexcept {
        return XMUINT2(Min(a.x, b.x), Min(a.y, b.y));
    }

    [[nodiscard]] constexpr XMUINT2 Max(const XMUINT2& a, const XMUINT2& b) noexcept {
        return XMUINT2(Max(a.x, b.x), Max(a.y, b.y));
    }

    [[nodiscard]] constexpr float Saturate(float x) noexcept { 
        return Min(Max(x, 0.0f), 1.0f);
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    [[nodiscard]] constexpr T GetNextPowerOfTwo(T x) noexcept {
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

    [[nodiscard]] constexpr uint32_t GetNextDivisible(uint32_t num, uint32_t divisor) noexcept {
        int bits = num & (divisor - 1);
        if (bits == 0)
            return num;

        return num + (divisor - bits);
    }

    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
    [[nodiscard]] constexpr T Lerp(const T a, const T b, const T t) noexcept {
        return a + (b - a) * t;
    }

    [[nodiscard]] constexpr float CubicLerp(float a, float b, float c, float d, float t) noexcept {
        float p = (d - c) - (a - b);
        return t * t * t * p + t * t * ((a - b) - p) + t * (c - a) + b;
    }

    [[nodiscard]] constexpr float InterpHermiteFunc(float t) noexcept {
        return t * t * (3 - 2 * t); 
    }

    [[nodiscard]] constexpr float InterpQuinticFunc(float t) noexcept { 
        return t * t * t * (t * (t * 6 - 15) + 10); 
    }

    [[nodiscard]] inline float XM_CALLCONV Distance(const XMVECTOR& v1, const XMVECTOR& v2) noexcept {
        return XMVectorGetX(XMVector3Length(v1 - v2));
    }

    [[nodiscard]] inline float XM_CALLCONV Distance(const XMFLOAT3& v1, const XMFLOAT3& v2) noexcept {
        XMVECTOR vector1 = XMLoadFloat3(&v1);
        XMVECTOR vector2 = XMLoadFloat3(&v2);
        return Distance(vector1, vector2);
    }

    [[nodiscard]] inline uint32_t StoreColor_RGB(const XMFLOAT3& color) noexcept {
        uint32_t retval = 0;

        retval |= (uint32_t)((uint8_t)(Saturate(color.x) * 255.0f) << 0);
        retval |= (uint32_t)((uint8_t)(Saturate(color.y) * 255.0f) << 8);
        retval |= (uint32_t)((uint8_t)(Saturate(color.z) * 255.0f) << 16);

        return retval;
    }

    [[nodiscard]] inline uint32_t StoreColor_RGBA(const XMFLOAT4& color) noexcept {
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
    [[nodiscard]] inline bool XM_CALLCONV RayTriangleIntersects(FXMVECTOR Origin, FXMVECTOR Direction, FXMVECTOR V0, GXMVECTOR V1, HXMVECTOR V2, float& Dist, XMFLOAT2& bary) noexcept {
        XMVECTOR Zero = XMVectorZero();

        XMVECTOR e1 = XMVectorSubtract(V1, V0);
        XMVECTOR e2 = XMVectorSubtract(V2, V0);

        // p = Direction ^ e2;
        XMVECTOR p = XMVector3Cross(Direction, e2);

        // det = e1 * p;
        XMVECTOR det = XMVector3Dot(e1, p);

        XMVECTOR u, v, t;

        if (XMVector3GreaterOrEqual(det, g_RayEpsilon)) {
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

            if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt())) {
                Dist = 0.f;
                return false;
            }
        } else if (XMVector3LessOrEqual(det, g_RayNegEpsilon)) {
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

            if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt())) {
                Dist = 0.f;
                return false;
            }
        } else {
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