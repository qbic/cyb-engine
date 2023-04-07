#include "core/intersect.h"
#include <algorithm>

namespace cyb::math
{
    AxisAlignedBox::AxisAlignedBox() :
        min(FLT_MAX, FLT_MAX, FLT_MAX),
        max(-FLT_MAX, -FLT_MAX, -FLT_MAX)
    {
    }

    AxisAlignedBox::AxisAlignedBox(const XMFLOAT3& min, const XMFLOAT3& max) :
        min(min),
        max(max)
    {
    }

    AxisAlignedBox::AxisAlignedBox(float min, float max) :
        min(min, min, min),
        max(max, max, max)
    {
    }

    AxisAlignedBox AxisAlignedBox::Transform(const XMMATRIX& transform) const
    {
        const XMVECTOR vMin = XMLoadFloat3(&min);
        const XMVECTOR vMax = XMLoadFloat3(&max);

        const XMVECTOR& m0 = transform.r[0];
        const XMVECTOR& m1 = transform.r[1];
        const XMVECTOR& m2 = transform.r[2];
        const XMVECTOR& m3 = transform.r[3];

        const XMVECTOR vHalf = XMVectorReplicate(0.5f);
        const XMVECTOR vCenter = XMVectorMultiply(XMVectorAdd(vMax, vMin), vHalf);
        const XMVECTOR vExtents = XMVectorMultiply(XMVectorSubtract(vMax, vMin), vHalf);

        XMVECTOR vNewCenter = XMVectorMultiply(XMVectorReplicate(XMVectorGetX(vCenter)), m0);
        vNewCenter = XMVectorMultiplyAdd(XMVectorReplicate(XMVectorGetY(vCenter)), m1, vNewCenter);
        vNewCenter = XMVectorMultiplyAdd(XMVectorReplicate(XMVectorGetZ(vCenter)), m2, vNewCenter);
        vNewCenter = XMVectorAdd(vNewCenter, m3);

        XMVECTOR vNewExtents = XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetX(vExtents)), m0));
        vNewExtents = XMVectorAdd(vNewExtents, XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetY(vExtents)), m1)));
        vNewExtents = XMVectorAdd(vNewExtents, XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetZ(vExtents)), m2)));

        const XMVECTOR vNewMin = XMVectorSubtract(vNewCenter, vNewExtents);
        const XMVECTOR vNewMax = XMVectorAdd(vNewCenter, vNewExtents);

        AxisAlignedBox newAABB;
        XMStoreFloat3(&newAABB.min, vNewMin);
        XMStoreFloat3(&newAABB.max, vNewMax);
        return newAABB;
    }

    XMMATRIX AxisAlignedBox::GetAsBoxMatrix() const
    {
        const XMVECTOR vMin = XMLoadFloat3(&min);
        const XMVECTOR vMax = XMLoadFloat3(&max);

        const XMVECTOR vHalf = XMVectorReplicate(0.5f);
        const XMVECTOR vCenter = XMVectorMultiply(XMVectorAdd(vMax, vMin), vHalf);
        const XMVECTOR vExtents = XMVectorAbs(XMVectorSubtract(vMax, vCenter));
        return XMMatrixScalingFromVector(vExtents) * XMMatrixTranslationFromVector(vCenter);
    }

    bool AxisAlignedBox::IsInside(const XMFLOAT3& p) const
    {
        return (p.x <= max.x) && (p.x >= min.x) && 
               (p.y <= max.y) && (p.y >= min.y) && 
               (p.z <= max.z) && (p.z >= min.z);
    }

    AxisAlignedBox AABBFromHalfWidth(const XMFLOAT3& origin, const XMFLOAT3& extent)
    {
        AxisAlignedBox newBox;
        newBox.min = XMFLOAT3(origin.x - extent.x, origin.y - extent.y, origin.z - extent.z);
        newBox.max = XMFLOAT3(origin.x + extent.x, origin.y + extent.y, origin.z + extent.z);
        return newBox;
    }

    Ray::Ray(const XMVECTOR& inOrigin, const XMVECTOR& inDirection)
    {
        XMStoreFloat3(&origin, inOrigin);
        XMStoreFloat3(&direction, inDirection);
        XMStoreFloat3(&invDirection, XMVectorReciprocal(inDirection));
    }

    bool Ray::IntersectsBoundingBox(const AxisAlignedBox& aabb) const
    {
        if (aabb.IsInside(origin))
            return true;

        const XMFLOAT3& min = aabb.min;
        const XMFLOAT3& max = aabb.max;

        float tx1 = (min.x - origin.x) * invDirection.x;
        float tx2 = (max.x - origin.x) * invDirection.x;

        float tmin = std::min(tx1, tx2);
        float tmax = std::max(tx1, tx2);

        float ty1 = (min.y - origin.y) * invDirection.y;
        float ty2 = (max.y - origin.y) * invDirection.y;

        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));

        float tz1 = (min.z - origin.z) * invDirection.z;
        float tz2 = (max.z - origin.z) * invDirection.z;

        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));

        return tmax >= tmin;
    }

    Frustum::Frustum(const XMMATRIX& viewProjection)
    {
        const XMMATRIX mat = XMMatrixTranspose(viewProjection);

        // Near plane: 
        XMStoreFloat4(&planes[0], XMPlaneNormalize(mat.r[2]));

        // Far plane
        XMStoreFloat4(&planes[1], XMPlaneNormalize(mat.r[3] - mat.r[2]));

        // Left plane:
        XMStoreFloat4(&planes[2], XMPlaneNormalize(mat.r[3] + mat.r[0]));

        // Right plane:
        XMStoreFloat4(&planes[3], XMPlaneNormalize(mat.r[3] - mat.r[0]));

        // Top plane:
        XMStoreFloat4(&planes[4], XMPlaneNormalize(mat.r[3] - mat.r[1]));

        // Bottom plane:
        XMStoreFloat4(&planes[5], XMPlaneNormalize(mat.r[3] + mat.r[1]));
    }

    bool Frustum::IntersectsBoundingBox(const AxisAlignedBox& aabb) const
    {
        XMVECTOR min = XMLoadFloat3(&aabb.min);
        XMVECTOR max = XMLoadFloat3(&aabb.max);
        XMVECTOR zero = XMVectorZero();

        for (size_t p = 0; p < 6; ++p)
        {
            XMVECTOR plane = XMLoadFloat4(&planes[p]);
            XMVECTOR lt = XMVectorLess(plane, zero);
            XMVECTOR furthestFromPlane = XMVectorSelect(max, min, lt);
            if (XMVectorGetX(XMPlaneDotCoord(plane, furthestFromPlane)) < 0.0f)
                return false;
        }

        return true;
    }
}

