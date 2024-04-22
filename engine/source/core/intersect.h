#pragma once
#include "core/mathlib.h"

namespace cyb::spatial
{
    struct AxisAlignedBox
    {
        AxisAlignedBox();
        AxisAlignedBox(const XMFLOAT3& min, const XMFLOAT3& max);
        AxisAlignedBox(float min, float max);

        AxisAlignedBox Transform(const XMMATRIX& transform) const;
        XMMATRIX GetAsBoxMatrix() const;
        bool IsInside(const XMFLOAT3& p) const;

        XMFLOAT3 min;
        XMFLOAT3 max;
    };

    struct Ray
    {
        Ray() = default;
        Ray(const XMVECTOR& inOrigin, const XMVECTOR& inDirection);
        
        bool IntersectsBoundingBox(const AxisAlignedBox& aabb) const;

        XMFLOAT3 origin;
        XMFLOAT3 direction;
        XMFLOAT3 invDirection;
    };

    struct Frustum
    {
        Frustum() = default;
        Frustum(const XMMATRIX& viewProjection);

        bool IntersectsBoundingBox(const AxisAlignedBox& aabb) const;

        XMFLOAT4 planes[6];
    };
}
