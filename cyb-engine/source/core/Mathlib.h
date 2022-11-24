#pragma once
#include "core/base-types.h"
#include <cmath>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace cyb::math
{
    constexpr XMFLOAT4X4 IdentityMatrix = XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    constexpr XMFLOAT3 ZeroFloat3 = XMFLOAT3(0.0f, 0.0f, 0.0f);
    constexpr float M_PI = 3.14159265358979323846f;
    constexpr float M_2PI = 6.283185307f;
    constexpr float M_PIDIV2 = 1.570796327f;
    constexpr float M_PIDIV4 = 0.785398163f;
    constexpr float RAD2DEG = (180.f / M_PI);
    constexpr float DEG2RAD = (M_PI / 180.f);

    static const XMVECTOR ZeroVector = XMVectorSet(0, 0, 0, 0);

    inline uint64_t TruncToInt(double value)
    {
        return (uint64_t)value;
    }

    static int32_t TruncToInt(float value)
    {
        return (int32_t)value;
    }

    // Returns highest of 2 values
    template<class T>
    constexpr T Max(const T a, const T b)
    {
        return a > b ? a : b;
    }

    // Returns lowest of 2 values
    template<class T>
    constexpr T Min(const T a, const T b)
    {
        return a < b ? a : b;
    }

    template <typename Tfloat>
    constexpr int Floor(Tfloat f)
    { 
        return f >= 0 ? (int)f : (int)f - 1; 
    }

    template <typename Tfloat>
    constexpr Tfloat Abs(Tfloat f) 
    { 
        return f < 0 ? -f : f; 
    }

    constexpr XMFLOAT3 Max(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return XMFLOAT3(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z));
    }

    constexpr XMFLOAT3 Min(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return XMFLOAT3(Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z));
    }

    constexpr float Saturate(float x) 
    { 
        return Min(Max(x, 0.0f), 1.0f);
    }

    constexpr uint32_t GetNextPowerOfTwo(uint32_t x)
    {
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return ++x;
    }
    constexpr uint64_t GetNextPowerOfTwo(uint64_t x)
    {
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32u;
        return ++x;
    }

    constexpr uint32_t GetNextDivisible(uint32_t num, uint32_t divisor)
    {
        int bits = num & (divisor - 1);
        if (bits == 0)
        {
            return num;
        }

        return num + (divisor - bits);
    }

    constexpr float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    constexpr float CubicLerp(float a, float b, float c, float d, float t)
    {
        float p = (d - c) - (a - b);
        return t * t * t * p + t * t * ((a - b) - p) + t * (c - a) + b;
    }

    constexpr float InterpHermiteFunc(float t) 
    { 
        return t * t * (3 - 2 * t); 
    }

    constexpr float InterpQuinticFunc(float t) 
    { 
        return t * t * t * (t * (t * 6 - 15) + 10); 
    }

    inline float XM_CALLCONV Distance(const XMVECTOR& v1, const XMVECTOR& v2)
    {
        return XMVectorGetX(XMVector3Length(v1 - v2));
    }

    inline float XM_CALLCONV Distance(const XMFLOAT3& v1, const XMFLOAT3& v2)
    {
        XMVECTOR vector1 = XMLoadFloat3(&v1);
        XMVECTOR vector2 = XMLoadFloat3(&v2);
        return Distance(vector1, vector2);
    }

    inline uint32_t StoreColor_RGB(const XMFLOAT3& color)
    {
        uint32_t retval = 0;

        retval |= (uint32_t)((uint8_t)(Saturate(color.x) * 255.0f) << 0);
        retval |= (uint32_t)((uint8_t)(Saturate(color.y) * 255.0f) << 8);
        retval |= (uint32_t)((uint8_t)(Saturate(color.z) * 255.0f) << 16);

        return retval;
    }
    inline uint32_t StoreColor_RGBA(const XMFLOAT4& color)
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
    inline bool XM_CALLCONV RayTriangleIntersects(FXMVECTOR Origin, FXMVECTOR Direction, FXMVECTOR V0, GXMVECTOR V1, HXMVECTOR V2, float& Dist, XMFLOAT2& bary)
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