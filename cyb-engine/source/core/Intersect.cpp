#include "core/intersect.h"
#include <algorithm>

namespace cyb::math
{
    AxisAlignedBox AxisAlignedBox::TransformBy(const XMMATRIX& mat) const
    {
        XMFLOAT3 corners[8];
        for (uint32_t i = 0; i < 8; ++i)
        {
            XMFLOAT3 C = GetCorner(i);
            XMVECTOR P = XMVector3Transform(XMLoadFloat3(&C), mat);
            XMStoreFloat3(&corners[i], P);
        }

        XMFLOAT3 min = corners[0];
        XMFLOAT3 max = corners[6];
        for (uint32_t i = 0; i < 8; ++i)
        {
            min = math::Min(min, corners[i]);
            max = math::Max(max, corners[i]);
        }

        return AxisAlignedBox(min, max);
    }

    XMFLOAT3 AxisAlignedBox::GetCorner(uint32_t i) const
    {
        switch (i)
        {
        case 0: return min;
        case 1: return XMFLOAT3(min.x, max.y, min.z);
        case 2: return XMFLOAT3(min.x, max.y, max.z);
        case 3: return XMFLOAT3(min.x, min.y, max.z);
        case 4: return XMFLOAT3(max.x, min.y, min.z);
        case 5: return XMFLOAT3(max.x, max.y, min.z);
        case 6: return max;
        case 7: return XMFLOAT3(max.x, min.y, max.z);
        default: break;
        }

        assert(0);
        return XMFLOAT3(0.0f, 0.0f, 0.0f);
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

    AxisAlignedBox AABBFromHalfWidth(const XMFLOAT3& center, const XMFLOAT3& halfwidth)
    {
        AxisAlignedBox newBox;
        newBox.min = XMFLOAT3(center.x - halfwidth.x, center.y - halfwidth.y, center.z - halfwidth.z);
        newBox.max = XMFLOAT3(center.x + halfwidth.x, center.y + halfwidth.y, center.z + halfwidth.z);
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

