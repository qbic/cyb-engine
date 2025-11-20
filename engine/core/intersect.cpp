#include <algorithm>
#include "core/intersect.h"

namespace cyb
{
    AxisAlignedBox::AxisAlignedBox()
    {
        m_min = XMVectorZero();
        m_max = XMVectorZero();
    }

    AxisAlignedBox::AxisAlignedBox(const XMVECTOR& min, const XMVECTOR& max) :
        m_min(min),
        m_max(max)
    {
    }

    AxisAlignedBox::AxisAlignedBox(const XMFLOAT3& min, const XMFLOAT3& max)
    {
        m_min = XMLoadFloat3(&min);
        m_max = XMLoadFloat3(&max);
    }

    void AxisAlignedBox::Invalidate()
    {
        m_min = XMVectorReplicate(FLT_MAX);
        m_max = -m_min;
    }

    void AxisAlignedBox::Set(const XMFLOAT3& boxMin, const XMFLOAT3& boxMax)
    {
        m_min = XMLoadFloat3(&boxMin);
        m_max = XMLoadFloat3(&boxMax);
    }

    void AxisAlignedBox::SetMin(const XMFLOAT3& boxMin)
    {
        m_min = XMLoadFloat3(&boxMin);
    }

    void AxisAlignedBox::SetMax(const XMFLOAT3& boxMax)
    {
        m_max = XMLoadFloat3(&boxMax);
    }

    void AxisAlignedBox::SetFromSphere(const XMFLOAT3& center, float radius)
    {
        XMVECTOR C = XMLoadFloat3(&center);
        XMVECTOR R = XMVectorReplicate(radius);
        m_min = XMVectorSubtract(C, R);
        m_max = XMVectorAdd(C, R);
    }

    void AxisAlignedBox::GrowPoint(const XMFLOAT3& point)
    {
        XMVECTOR p = XMLoadFloat3(&point);
        m_min = XMVectorMin(m_min, p);
        m_max = XMVectorMax(m_max, p);
    }

    void AxisAlignedBox::GrowAABB(const AxisAlignedBox& box)
    {
        m_max = XMVectorMax(m_max, box.m_max);
        m_min = XMVectorMin(m_min, box.m_min);
    }

    const XMVECTOR& AxisAlignedBox::GetMin() const
    {
        return m_min;
    }

    const XMVECTOR& AxisAlignedBox::GetMax() const
    {
        return m_max;
    }

    const XMVECTOR AxisAlignedBox::GetCenter() const
    {
        return XMVectorMultiply(XMVectorAdd(m_max, m_min), XMVectorReplicate(0.5f));
    }

    const XMVECTOR AxisAlignedBox::GetExtent() const
    {
        return XMVectorMultiply(XMVectorSubtract(m_max, m_min), XMVectorReplicate(0.5f));
    }

    AxisAlignedBox AxisAlignedBox::Transform(const XMMATRIX& transform) const
    {
        const XMVECTOR& m0 = transform.r[0];
        const XMVECTOR& m1 = transform.r[1];
        const XMVECTOR& m2 = transform.r[2];
        const XMVECTOR& m3 = transform.r[3];

        const XMVECTOR vCenter = GetCenter();
        const XMVECTOR vExtents = GetExtent();

        XMVECTOR vNewCenter = XMVectorMultiply(XMVectorReplicate(XMVectorGetX(vCenter)), m0);
        vNewCenter = XMVectorMultiplyAdd(XMVectorReplicate(XMVectorGetY(vCenter)), m1, vNewCenter);
        vNewCenter = XMVectorMultiplyAdd(XMVectorReplicate(XMVectorGetZ(vCenter)), m2, vNewCenter);
        vNewCenter = XMVectorAdd(vNewCenter, m3);

        XMVECTOR vNewExtents = XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetX(vExtents)), m0));
        vNewExtents = XMVectorAdd(vNewExtents, XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetY(vExtents)), m1)));
        vNewExtents = XMVectorAdd(vNewExtents, XMVectorAbs(XMVectorMultiply(XMVectorReplicate(XMVectorGetZ(vExtents)), m2)));

        const XMVECTOR vNewMin = XMVectorSubtract(vNewCenter, vNewExtents);
        const XMVECTOR vNewMax = XMVectorAdd(vNewCenter, vNewExtents);
        return AxisAlignedBox{ vNewMin, vNewMax };
    }

    XMMATRIX AxisAlignedBox::GetAsBoxMatrix() const
    {
        const XMVECTOR C = GetCenter();
        const XMVECTOR E = GetExtent();;
        return XMMatrixScalingFromVector(E) * XMMatrixTranslationFromVector(C);
    }

    bool AxisAlignedBox::ContainsPoint(const XMVECTOR& p) const
    {
        return XMVector3GreaterOrEqual(p, m_min) && XMVector3LessOrEqual(p, m_max);
    }

    void AxisAlignedBox::Serialize(Serializer& s)
    {
        // serialize minmax vectors as XMFLOAT3
        XMFLOAT3 min;
        XMFLOAT3 max;
        XMStoreFloat3(&min, m_min);
        XMStoreFloat3(&max, m_max);
        s.Serialize(min);
        s.Serialize(max);

        // update local data if we are reading
        if (s.IsReading())
            Set(min, max);
    }

    Ray::Ray(const XMVECTOR& origin, const XMVECTOR& direction)
    {
        m_origin = origin;
        m_direction = direction;
        m_invDirection = XMVectorReciprocal(direction);
    }

    const XMVECTOR Ray::GetOrigin() const
    {
        return m_origin;
    }

    const XMVECTOR Ray::GetDirection() const
    {
        return m_direction;
    }

    bool Ray::IntersectsBoundingBox(const AxisAlignedBox& aabb) const
    {
        if (aabb.ContainsPoint(m_origin))
            return true;

        const XMVECTOR t1 = XMVectorMultiply(XMVectorSubtract(aabb.GetMin(), m_origin), m_invDirection);
        const XMVECTOR t2 = XMVectorMultiply(XMVectorSubtract(aabb.GetMax(), m_origin), m_invDirection);
        XMVECTOR tmin = XMVectorMin(t1, t2);
        XMVECTOR tmax = XMVectorMax(t1, t2);

        tmin = XMVectorMax(tmin, XMVectorSplatY(tmin));  // x = max(x,y)
        tmin = XMVectorMax(tmin, XMVectorSplatZ(tmin));  // x = max(max(x,y),z)
        tmax = XMVectorMin(tmax, XMVectorSplatY(tmax));  // x = min(x,y)
        tmax = XMVectorMin(tmax, XMVectorSplatZ(tmax));  // x = min(min(x,y),z)

        // if ( t_min > t_max ) return false;
        XMVECTOR NoIntersection = XMVectorGreater(XMVectorSplatX(tmin), XMVectorSplatX(tmax));

        // if ( t_max < 0.0f ) return false;
        NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(XMVectorSplatX(tmax), XMVectorZero()));
        return DirectX::Internal::XMVector3AnyTrue(NoIntersection) == 0;
    }

    Frustum::Frustum(const XMMATRIX& viewProjection)
    {
        const XMMATRIX mat = XMMatrixTranspose(viewProjection);

        planes[0] = XMPlaneNormalize(mat.r[3] + mat.r[0]); // left
        planes[1] = XMPlaneNormalize(mat.r[3] - mat.r[0]); // right
        planes[2] = XMPlaneNormalize(mat.r[3] + mat.r[1]); // bottom
        planes[3] = XMPlaneNormalize(mat.r[3] - mat.r[1]); // top
        planes[5] = XMPlaneNormalize(mat.r[3] + mat.r[2]); // near
        planes[4] = XMPlaneNormalize(mat.r[3] - mat.r[2]); // far
    }

    bool Frustum::IntersectsBoundingBox(const AxisAlignedBox& aabb) const
    {
        const XMVECTOR min = aabb.GetMin();
        const XMVECTOR max = aabb.GetMax();
        const XMVECTOR zero = XMVectorZero();

        for (size_t p = 0; p < 6; ++p)
        {
            const XMVECTOR plane = planes[p];
            XMVECTOR lt = XMVectorLess(plane, zero);
            XMVECTOR furthestFromPlane = XMVectorSelect(max, min, lt);
            if (XMVectorGetX(XMPlaneDotCoord(plane, furthestFromPlane)) < 0.0f)
                return false;
        }

        return true;
    }
} // namespace cyb

