#include <algorithm>
#include "Core/Intersect.h"

namespace cyb
{
    void AxisAlignedBox::CreateFromHalfWidth(const XMFLOAT3& center, const XMFLOAT3& halfwidth)
    {
        m_min = XMFLOAT3(center.x - halfwidth.x, center.y - halfwidth.y, center.z - halfwidth.z);
        m_max = XMFLOAT3(center.x + halfwidth.x, center.y + halfwidth.y, center.z + halfwidth.z);
    }

    AxisAlignedBox AxisAlignedBox::Transform(const XMMATRIX& mat) const
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
        case 0: return m_min;
        case 1: return XMFLOAT3(m_min.x, m_max.y, m_min.z);
        case 2: return XMFLOAT3(m_min.x, m_max.y, m_max.z);
        case 3: return XMFLOAT3(m_min.x, m_min.y, m_max.z);
        case 4: return XMFLOAT3(m_max.x, m_min.y, m_min.z);
        case 5: return XMFLOAT3(m_max.x, m_max.y, m_min.z);
        case 6: return m_max;
        case 7: return XMFLOAT3(m_max.x, m_min.y, m_max.z);
        default: break;
        }

        assert(0);
        return XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    XMFLOAT3 AxisAlignedBox::GetCenter() const
    {
        return XMFLOAT3((m_min.x + m_max.x) * 0.5f, (m_min.y + m_max.y) * 0.5f, (m_min.z + m_max.z) * 0.5f);
    }

    XMFLOAT3 AxisAlignedBox::GetHalfWidth() const
    {
        const XMFLOAT3 center = GetCenter();
        return XMFLOAT3(abs(m_max.x - center.x), abs(m_max.y - center.y), abs(m_max.z - center.z));
    }

    XMMATRIX AxisAlignedBox::GetAsBoxMatrix() const
    {
        const XMFLOAT3 S = GetHalfWidth();
        const XMFLOAT3 P = GetCenter();

        return XMMatrixScaling(S.x, S.y, S.z) * XMMatrixTranslation(P.x, P.y, P.z);
    }

    bool AxisAlignedBox::IntersectPoint(const XMFLOAT3& p) const
    {
        bool intersects = (p.x <= m_max.x) ||
            (p.x >= m_min.x) ||
            (p.y <= m_max.y) ||
            (p.y >= m_min.y) ||
            (p.z <= m_max.z) ||
            (p.z >= m_min.z);

        return intersects;
    }

    Ray::Ray(const XMVECTOR& origin, const XMVECTOR& direction)
    {
        XMStoreFloat3(&m_origin, origin);
        XMStoreFloat3(&m_direction, direction);
        XMStoreFloat3(&m_invDirection, XMVectorReciprocal(direction));
    }

    bool Ray::IntersectBoundingBox(const AxisAlignedBox& aabb) const
    {
        if (aabb.IntersectPoint(m_origin))
            return true;

        XMFLOAT3 min = aabb.GetMin();
        XMFLOAT3 max = aabb.GetMax();

        float tx1 = (min.x - m_origin.x) * m_invDirection.x;
        float tx2 = (max.x - m_origin.x) * m_invDirection.x;

        float tmin = std::min(tx1, tx2);
        float tmax = std::max(tx1, tx2);

        float ty1 = (min.y - m_origin.y) * m_invDirection.y;
        float ty2 = (max.y - m_origin.y) * m_invDirection.y;

        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));

        float tz1 = (min.z - m_origin.z) * m_invDirection.z;
        float tz2 = (max.z - m_origin.z) * m_invDirection.z;

        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));

        return tmax >= tmin;
    }

    void Frustum::Create(const XMMATRIX& viewProjection)
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
        XMVECTOR min = XMLoadFloat3(&aabb.GetMin());
        XMVECTOR max = XMLoadFloat3(&aabb.GetMax());
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

