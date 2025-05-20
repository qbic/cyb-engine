#pragma once
#include "core/serializer.h"
#include "core/mathlib.h"

namespace cyb
{
    class AxisAlignedBox
    {
    public:
        AxisAlignedBox();
        explicit AxisAlignedBox(const XMFLOAT3& min, const XMFLOAT3& max);
        explicit AxisAlignedBox(const XMVECTOR& min, const XMVECTOR& max);

        void Invalidate();

        void Set(const XMFLOAT3& boxMin, const XMFLOAT3& boxMax);
        void SetMin(const XMFLOAT3& boxMin);
        void SetMax(const XMFLOAT3& boxMax);
        void SetAsSphere(const XMFLOAT3& center, float radius);

        void GrowPoint(const XMFLOAT3& point);
        void GrowAABB(const AxisAlignedBox& box);

        [[nodiscard]] const XMVECTOR& GetMin() const;
        [[nodiscard]] const XMVECTOR& GetMax() const;
        [[nodiscard]] const XMVECTOR GetCenter() const;
        [[nodiscard]] const XMVECTOR GetExtent() const;

        [[nodiscard]] AxisAlignedBox Transform(const XMMATRIX& transform) const;
        [[nodiscard]] XMMATRIX GetAsBoxMatrix() const;

        [[nodiscard]] bool ContainsPoint(const XMVECTOR& p) const;

        void Serialize(Serializer& s);

    private:
        XMVECTOR m_min;
        XMVECTOR m_max;
    };

    class Ray
    {
    public:
        Ray() = default;
        explicit Ray(const XMVECTOR& origin, const XMVECTOR& direction);

        [[nodiscard]] const XMVECTOR GetOrigin() const;
        [[nodiscard]] const XMVECTOR GetDirection() const;
        [[nodiscard]] bool IntersectsBoundingBox(const AxisAlignedBox& aabb) const;

    private:
        XMVECTOR m_origin;
        XMVECTOR m_direction;
        XMVECTOR m_invDirection;
    };

    struct Frustum
    {
        Frustum() = default;
        Frustum(const XMMATRIX& viewProjection);

        [[nodiscard]] bool IntersectsBoundingBox(const AxisAlignedBox& aabb) const;

        XMFLOAT4 planes[6] = {};
    };
}
