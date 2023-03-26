#pragma once
#include "core/mathlib.h"

namespace cyb::math
{
    struct AxisAlignedBox
    {
        XMFLOAT3 min;
        XMFLOAT3 max;

        explicit AxisAlignedBox()
        {
            min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
            max = XMFLOAT3(-FLT_MAX, -FLT_MAX, FLT_MAX);
        }

        AxisAlignedBox(const XMFLOAT3& inMin, const XMFLOAT3& inMax) :
            min(inMin),
            max(inMax)
        {
        }

        AxisAlignedBox TransformBy(const XMMATRIX& mat) const;
        XMMATRIX GetAsBoxMatrix() const;
        bool IsInside(const XMFLOAT3& p) const;
    };

    // Utility function to construct a new AABB from position and half width.
    AxisAlignedBox AABBFromHalfWidth(const XMFLOAT3& origin, const XMFLOAT3& extent);

    struct Ray
    {
        explicit Ray(const XMVECTOR& inOrigin, const XMVECTOR& inDirection);
        bool IntersectBoundingBox(const AxisAlignedBox& aabb) const;

        XMFLOAT3 origin;
        XMFLOAT3 direction;
        XMFLOAT3 invDirection;
    };

    struct Frustum
    {
        explicit Frustum() { }
        explicit Frustum(const XMMATRIX& viewProjection);
        bool IntersectBoundingBox(const AxisAlignedBox& aabb) const;

        XMFLOAT4 planes[6];
    };
}