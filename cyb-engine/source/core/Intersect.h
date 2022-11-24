#pragma once
#include "Core/Mathlib.h"

namespace cyb
{
    struct AxisAlignedBox
    {
        XMFLOAT3 min;
        XMFLOAT3 max;

        explicit AxisAlignedBox()
        {
            min = max = math::ZeroFloat3;
        }

        AxisAlignedBox(const XMFLOAT3& inMin, const XMFLOAT3& inMax) :
            min(inMin),
            max(inMax)
        {
        }

        void CreateFromHalfWidth(const XMFLOAT3& center, const XMFLOAT3& halfWidth);
        AxisAlignedBox TransformBy(const XMMATRIX& mat) const;
        XMFLOAT3 GetCorner(uint32_t i) const;
        XMFLOAT3 GetCenter() const;
        XMFLOAT3 GetHalfWidth() const;
        XMMATRIX GetAsBoxMatrix() const;
        bool IsInside(const XMFLOAT3& p) const;
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