#include "core/intersect.h"
#include <algorithm>

namespace cyb::math
{
    AxisAlignedBox AxisAlignedBox::TransformBy(const XMMATRIX& mat) const
    {
        const XMVECTOR VMin = XMLoadFloat3(&min);
        const XMVECTOR VMax = XMLoadFloat3(&max);

        const XMVECTOR& m0 = mat.r[0];
        const XMVECTOR& m1 = mat.r[1];
        const XMVECTOR& m2 = mat.r[2];
        const XMVECTOR& m3 = mat.r[3];

        const XMVECTOR half = XMVectorReplicate(0.5f);
        const XMVECTOR origin = XMVectorMultiply(XMVectorAdd(VMax, VMin), half);
        const XMVECTOR extent = XMVectorMultiply(XMVectorSubtract(VMax, VMin), half);

        XMVECTOR newOrigin = XMVectorMultiply(XMVectorReplicate(XMVectorGetX(origin)), m0);
        newOrigin = XMVectorMultiplyAdd(XMVectorReplicate(XMVectorGetY(origin)), m1, newOrigin);
        newOrigin = XMVectorMultiplyAdd(XMVectorReplicate(XMVectorGetZ(origin)), m2, newOrigin);
        newOrigin = XMVectorAdd(newOrigin, m3);

        XMVECTOR newExtent = XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetX(extent)), m0));
        newExtent = XMVectorAdd(newExtent, XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetY(extent)), m1)));
        newExtent = XMVectorAdd(newExtent, XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetZ(extent)), m2)));

        const XMVECTOR newMin = XMVectorSubtract(newOrigin, newExtent);
        const XMVECTOR newMax = XMVectorAdd(newOrigin, newExtent);

        AxisAlignedBox newAABB;
        XMStoreFloat3(&newAABB.min, newMin);
        XMStoreFloat3(&newAABB.max, newMax);
        return newAABB;
    }

    XMFLOAT3 AxisAlignedBox::GetCenter() const
    {
        return XMFLOAT3((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f);
    }

    XMFLOAT3 AxisAlignedBox::GetHalfWidth() const
    {
        const XMFLOAT3 center = GetCenter();
        return XMFLOAT3(abs(max.x - center.x), abs(max.y - center.y), abs(max.z - center.z));
    }

    XMMATRIX AxisAlignedBox::GetAsBoxMatrix() const
    {
        const XMFLOAT3 S = GetHalfWidth();
        const XMFLOAT3 P = GetCenter();

        return XMMatrixScaling(S.x, S.y, S.z) * XMMatrixTranslation(P.x, P.y, P.z);
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

    bool Ray::IntersectBoundingBox(const AxisAlignedBox& aabb) const
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

    bool Frustum::IntersectBoundingBox(const AxisAlignedBox& aabb) const
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

