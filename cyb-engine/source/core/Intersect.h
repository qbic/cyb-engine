#pragma once
#include "Core/Mathlib.h"

namespace cyb
{
    class AxisAlignedBox
    {
    public:
        AxisAlignedBox(const XMFLOAT3& min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX), const XMFLOAT3& max = XMFLOAT3(FLT_MIN, FLT_MIN, FLT_MIN)) :
            m_min(min),
            m_max(max)
        {
        }

        void CreateFromHalfWidth(const XMFLOAT3& center, const XMFLOAT3& halfwidth);
        AxisAlignedBox Transform(const XMMATRIX& mat) const;
        constexpr XMFLOAT3 GetMin() const { return m_min; }
        constexpr XMFLOAT3 GetMax() const { return m_max; }
        XMFLOAT3 GetCorner(uint32_t i) const;
        XMFLOAT3 GetCenter() const;
        XMFLOAT3 GetHalfWidth() const;
        XMMATRIX GetAsBoxMatrix() const;
        bool IntersectPoint(const XMFLOAT3& p) const;

    private:
        XMFLOAT3 m_min;
        XMFLOAT3 m_max;
    };

    struct Ray
    {
        explicit Ray(const XMVECTOR& origin, const XMVECTOR& direction);
        bool IntersectBoundingBox(const AxisAlignedBox& aabb) const;
        XMFLOAT3 GetOrigin() const { return m_origin; }
        XMFLOAT3 GetDirection() const { return m_direction; }

        XMFLOAT3 m_origin;
        XMFLOAT3 m_direction;
        XMFLOAT3 m_invDirection;
    };

    class Frustum
    {
    public:
        Frustum() = default;
        void Create(const XMMATRIX& viewProjection);

        bool IntersectBoundingBox(const AxisAlignedBox& aabb) const;

    private:
        XMFLOAT4 planes[6];
    };
}