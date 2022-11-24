#pragma once
#include "Core/Mathlib.h"

enum class ForceInit
{
    ForceInit,
    ForceInitToZero
};

namespace cyb::math
{
    struct AxisAlignedBox
    {
        XMFLOAT3 min;
        XMFLOAT3 max;

        AxisAlignedBox() { }

        explicit AxisAlignedBox(ForceInit)
        {
            min = max = math::ZeroFloat3;
        }

        AxisAlignedBox(const XMFLOAT3& inMin, const XMFLOAT3& inMax) :
            min(inMin),
            max(inMax)
        {
        }

        AxisAlignedBox TransformBy(const XMMATRIX& mat) const;
        XMFLOAT3 GetCenter() const;
        XMFLOAT3 GetHalfWidth() const;
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
        explicit Frustum() = default;
        Frustum(const XMMATRIX& viewProjection);
        bool IntersectBoundingBox(const AxisAlignedBox& aabb) const;

        XMFLOAT4 planes[6];
    };
}