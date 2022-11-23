#include "Core/Profiler.h"
#include "Systems/Scene.h"

namespace cyb::scene
{
    XMFLOAT3 TransformComponent::GetPosition() const
    {
        return *((XMFLOAT3*)&world._41);
    }

    XMFLOAT4 TransformComponent::GetRotation() const
    {
        XMFLOAT4 rotation;
        XMStoreFloat4(&rotation, GetRotationV());
        return rotation;
    }

    XMFLOAT3 TransformComponent::GetScale() const
    {
        XMFLOAT3 scale;
        XMStoreFloat3(&scale, GetScaleV());
        return scale;
    }

    XMVECTOR TransformComponent::GetPositionV() const
    {
        return XMLoadFloat3((XMFLOAT3*)&world._41);
    }

    XMVECTOR TransformComponent::GetRotationV() const
    {
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&world));
        return R;
    }

    XMVECTOR TransformComponent::GetScaleV() const
    {
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&world));
        return S;
    }

    XMMATRIX TransformComponent::GetLocalMatrix() const
    {
        XMVECTOR S = XMVectorSet(scale_local.x, scale_local.y, scale_local.z, 0);
        XMVECTOR R = XMVectorSet(rotation_local.x, rotation_local.y, rotation_local.z, rotation_local.w);
        XMVECTOR T = XMVectorSet(translation_local.x, translation_local.y, translation_local.z, 0);
        return XMMatrixScalingFromVector(S) *
            XMMatrixRotationQuaternion(R) *
            XMMatrixTranslationFromVector(T);
    }

    void TransformComponent::ApplyTransform()
    {
        SetDirty(true);

        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&world));
        XMStoreFloat3(&scale_local, S);
        XMStoreFloat4(&rotation_local, R);
        XMStoreFloat3(&translation_local, T);
    }

    void TransformComponent::MatrixTransform(const XMFLOAT4X4& matrix)
    {
        MatrixTransform(XMLoadFloat4x4(&matrix));
    }

    void TransformComponent::MatrixTransform(const XMMATRIX& matrix)
    {
        SetDirty(true);

        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, GetLocalMatrix() * matrix);
        XMStoreFloat3(&scale_local, S);
        XMStoreFloat4(&rotation_local, R);
        XMStoreFloat3(&translation_local, T);
    }

    void TransformComponent::UpdateTransform()
    {
        if (IsDirty())
        {
            SetDirty(false);
            XMStoreFloat4x4(&world, GetLocalMatrix());
        }
    }

    void TransformComponent::UpdateTransformParented(const TransformComponent& parent)
    {
        XMMATRIX W = GetLocalMatrix() * XMLoadFloat4x4(&parent.world);
        XMStoreFloat4x4(&world, W);
    }

    void TransformComponent::Translate(const XMFLOAT3& value)
    {
        SetDirty();
        translation_local.x += value.x;
        translation_local.y += value.y;
        translation_local.z += value.z;
    }

    void TransformComponent::RotateRollPitchYaw(const XMFLOAT3& value)
    {
        SetDirty();

        // This needs to be handled a bit differently
        XMVECTOR quat = XMLoadFloat4(&rotation_local);
        XMVECTOR x = XMQuaternionRotationRollPitchYaw(value.x, 0, 0);
        XMVECTOR y = XMQuaternionRotationRollPitchYaw(0, value.y, 0);
        XMVECTOR z = XMQuaternionRotationRollPitchYaw(0, 0, value.z);

        quat = XMQuaternionMultiply(x, quat);
        quat = XMQuaternionMultiply(quat, y);
        quat = XMQuaternionMultiply(z, quat);
        quat = XMQuaternionNormalize(quat);

        XMStoreFloat4(&rotation_local, quat);
    }

    void MeshComponent::Clear()
    {
        vertex_positions.clear();
        vertex_normals.clear();
        vertex_colors.clear();
        indices.clear();
        subsets.clear();
    }

    void MeshComponent::CreateRenderData()
    {
        graphics::GraphicsDevice* device = renderer::GetDevice();

        // Create index buffer gpu data
        {
            graphics::GPUBufferDesc desc;
            desc.size = uint32_t(sizeof(uint32_t) * indices.size());
            desc.usage = graphics::MemoryAccess::kDefault;
            desc.bindFlags = graphics::BindFlags::kIndexBufferBit;

            bool result = device->CreateBuffer(&desc, indices.data(), &index_buffer);
            assert(result == true);
        }

        XMFLOAT3 _min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
        XMFLOAT3 _max = XMFLOAT3(FLT_MIN, FLT_MIN, FLT_MIN);

        // vertex_buffer_pos - POSITION + NORMAL
        {
            std::vector<Vertex_Pos> _vertices(vertex_positions.size());
            for (size_t i = 0; i < _vertices.size(); ++i)
            {
                const XMFLOAT3& pos = vertex_positions[i];
                XMFLOAT3 nor = vertex_normals.empty() ? XMFLOAT3(1, 1, 1) : vertex_normals[i];
                XMStoreFloat3(&nor, XMVector3Normalize(XMLoadFloat3(&nor)));
                _vertices[i].Set(pos, nor);

                _min = math::Min(_min, pos);
                _max = math::Max(_max, pos);
            }

            graphics::GPUBufferDesc desc;
            desc.usage = graphics::MemoryAccess::kDefault;
            desc.size = uint32_t(sizeof(Vertex_Pos) * _vertices.size());
            desc.bindFlags = graphics::BindFlags::kVertexBufferBit;
            bool result = device->CreateBuffer(&desc, _vertices.data(), &vertex_buffer_pos);
            assert(result == true);
        }

        // vertex_buffer_col - COLOR
        if (!vertex_colors.empty())
        {
            graphics::GPUBufferDesc desc;
            desc.usage = graphics::MemoryAccess::kDefault;
            desc.size = uint32_t(sizeof(uint32_t) * vertex_colors.size());
            desc.bindFlags = graphics::BindFlags::kVertexBufferBit;
            bool result = device->CreateBuffer(&desc, vertex_colors.data(), &vertex_buffer_col);
            assert(result == true);
        }

        aabb = AxisAlignedBox(_min, _max);
    }

    void MeshComponent::ComputeNormals()
    {
        // Compute hard surface normals:
        // Right now they are always computed even before smooth setting

        std::vector<uint32_t> newIndexBuffer;
        std::vector<XMFLOAT3> newPositionsBuffer;
        std::vector<XMFLOAT3> newNormalsBuffer;
        std::vector<uint32_t> newColorsBuffer;

        for (size_t face = 0; face < indices.size() / 3; face++)
        {
            uint32_t i0 = indices[face * 3 + 0];
            uint32_t i1 = indices[face * 3 + 1];
            uint32_t i2 = indices[face * 3 + 2];

            XMFLOAT3& p0 = vertex_positions[i0];
            XMFLOAT3& p1 = vertex_positions[i1];
            XMFLOAT3& p2 = vertex_positions[i2];

            XMVECTOR U = XMLoadFloat3(&p2) - XMLoadFloat3(&p0);
            XMVECTOR V = XMLoadFloat3(&p1) - XMLoadFloat3(&p0);

            XMVECTOR N = XMVector3Cross(U, V);
            N = XMVector3Normalize(N);

            XMFLOAT3 normal;
            XMStoreFloat3(&normal, N);

            newPositionsBuffer.push_back(p0);
            newPositionsBuffer.push_back(p1);
            newPositionsBuffer.push_back(p2);

            newNormalsBuffer.push_back(normal);
            newNormalsBuffer.push_back(normal);
            newNormalsBuffer.push_back(normal);

            if (!vertex_colors.empty())
            {
                newColorsBuffer.push_back(vertex_colors[i0]);
                newColorsBuffer.push_back(vertex_colors[i1]);
                newColorsBuffer.push_back(vertex_colors[i2]);
            }

            newIndexBuffer.push_back(static_cast<uint32_t>(newIndexBuffer.size()));
            newIndexBuffer.push_back(static_cast<uint32_t>(newIndexBuffer.size()));
            newIndexBuffer.push_back(static_cast<uint32_t>(newIndexBuffer.size()));
        }

        // For hard surface normals, we created a new mesh in the previous loop through faces, so swap data:
        vertex_positions = newPositionsBuffer;
        vertex_normals = newNormalsBuffer;
        vertex_colors = newColorsBuffer;
        indices = newIndexBuffer;

        // Compute smooth surface normals:

        // 1.) Zero normals, they will be averaged later
        //for (size_t i = 0; i < vertex_normals.size(); i++)
        //{
        //    vertex_normals[i] = XMFLOAT3(0, 0, 0);
       // }

        // 2.) Find identical vertices by POSITION, accumulate face normals
#if 1
        jobsystem::Context ctx;
        jobsystem::Dispatch(ctx, (uint32_t)vertex_positions.size(), 512, [&](jobsystem::JobArgs args)
            {
                size_t i = args.jobIndex;
                XMFLOAT3& v_search_pos = vertex_positions[i];

                for (size_t ind = 0; ind < indices.size() / 3; ++ind)
                {
                    uint32_t i0 = indices[ind * 3 + 0];
                    uint32_t i1 = indices[ind * 3 + 1];
                    uint32_t i2 = indices[ind * 3 + 2];

                    XMFLOAT3& v0 = vertex_positions[i0];
                    XMFLOAT3& v1 = vertex_positions[i1];
                    XMFLOAT3& v2 = vertex_positions[i2];

                    bool match_pos0 =
                        fabs(v_search_pos.x - v0.x) < FLT_EPSILON &&
                        fabs(v_search_pos.y - v0.y) < FLT_EPSILON &&
                        fabs(v_search_pos.z - v0.z) < FLT_EPSILON;

                    bool match_pos1 =
                        fabs(v_search_pos.x - v1.x) < FLT_EPSILON &&
                        fabs(v_search_pos.y - v1.y) < FLT_EPSILON &&
                        fabs(v_search_pos.z - v1.z) < FLT_EPSILON;

                    bool match_pos2 =
                        fabs(v_search_pos.x - v2.x) < FLT_EPSILON &&
                        fabs(v_search_pos.y - v2.y) < FLT_EPSILON &&
                        fabs(v_search_pos.z - v2.z) < FLT_EPSILON;

                    if (match_pos0 || match_pos1 || match_pos2)
                    {
                        XMVECTOR U = XMLoadFloat3(&v2) - XMLoadFloat3(&v0);
                        XMVECTOR V = XMLoadFloat3(&v1) - XMLoadFloat3(&v0);

                        XMVECTOR N = XMVector3Cross(U, V);
                        N = XMVector3Normalize(N);

                        XMFLOAT3 normal;
                        XMStoreFloat3(&normal, N);

                        vertex_normals[i].x += normal.x;
                        vertex_normals[i].y += normal.y;
                        vertex_normals[i].z += normal.z;
                    }
                }
            });
        jobsystem::Wait(ctx);
#else
        for (long i = 0; i < vertex_positions.size(); i++)
        {
            XMFLOAT3& v_search_pos = vertex_positions[i];

            for (size_t ind = 0; ind < indices.size() / 3; ++ind)
            {
                uint32_t i0 = indices[ind * 3 + 0];
                uint32_t i1 = indices[ind * 3 + 1];
                uint32_t i2 = indices[ind * 3 + 2];

                XMFLOAT3& v0 = vertex_positions[i0];
                XMFLOAT3& v1 = vertex_positions[i1];
                XMFLOAT3& v2 = vertex_positions[i2];

                bool match_pos0 =
                    fabs(v_search_pos.x - v0.x) < FLT_EPSILON &&
                    fabs(v_search_pos.y - v0.y) < FLT_EPSILON &&
                    fabs(v_search_pos.z - v0.z) < FLT_EPSILON;

                bool match_pos1 =
                    fabs(v_search_pos.x - v1.x) < FLT_EPSILON &&
                    fabs(v_search_pos.y - v1.y) < FLT_EPSILON &&
                    fabs(v_search_pos.z - v1.z) < FLT_EPSILON;

                bool match_pos2 =
                    fabs(v_search_pos.x - v2.x) < FLT_EPSILON &&
                    fabs(v_search_pos.y - v2.y) < FLT_EPSILON &&
                    fabs(v_search_pos.z - v2.z) < FLT_EPSILON;

                if (match_pos0 || match_pos1 || match_pos2)
                {
                    XMVECTOR U = XMLoadFloat3(&v2) - XMLoadFloat3(&v0);
                    XMVECTOR V = XMLoadFloat3(&v1) - XMLoadFloat3(&v0);

                    XMVECTOR N = XMVector3Cross(U, V);
                    N = XMVector3Normalize(N);

                    XMFLOAT3 normal;
                    XMStoreFloat3(&normal, N);

                    vertex_normals[i].x += normal.x;
                    vertex_normals[i].y += normal.y;
                    vertex_normals[i].z += normal.z;
                }
            }
        }
#endif
        // 3.) Find duplicated vertices by POSITION and UV0 and UV1 and ATLAS and SUBSET and remove them:
        /*
        for (auto& subset : subsets)
        {
            for (uint32_t i = 0; i < subset.indexCount - 1; i++)
            {
                uint32_t ind0 = indices[subset.indexOffset + (uint32_t)i];
                const XMFLOAT3& p0 = vertex_positions[ind0];

                for (uint32_t j = i + 1; j < subset.indexCount; j++)
                {
                    uint32_t ind1 = indices[subset.indexOffset + (uint32_t)j];

                    if (ind1 == ind0)
                    {
                        continue;
                    }

                    const XMFLOAT3& p1 = vertex_positions[ind1];

                    const bool duplicated_pos =
                        fabs(p0.x - p1.x) < FLT_EPSILON &&
                        fabs(p0.y - p1.y) < FLT_EPSILON &&
                        fabs(p0.z - p1.z) < FLT_EPSILON;

                    if (duplicated_pos)
                    {
                        // Erase vertices[ind1] because it is a duplicate:
                        if (ind1 < vertex_positions.size())
                        {
                            vertex_positions.erase(vertex_positions.begin() + ind1);
                        }
                        if (ind1 < vertex_normals.size())
                        {
                            vertex_normals.erase(vertex_normals.begin() + ind1);
                        }

                        // The vertices[ind1] was removed, so each index after that needs to be updated:
                        for (auto& index : indices)
                        {
                            if (index > ind1 && index > 0)
                            {
                                index--;
                            }
                            else if (index == ind1)
                            {
                                index = ind0;
                            }
                        }
                    }
                }
            }
        }
        */
        CreateRenderData(); // <- Normals will be normalized here
    }

    void MeshComponent::Vertex_Pos::Set(const XMFLOAT3& _pos, const XMFLOAT3& _nor)
    {
        pos = _pos;
        SetNormal(_nor);
    }

    void MeshComponent::Vertex_Pos::SetNormal(const XMFLOAT3& _nor)
    {
        normal = 0xFF000000;    // w = 0xFF
        normal |= (uint32_t)((_nor.x * 0.5f + 0.5f) * 255.0f) << 0;
        normal |= (uint32_t)((_nor.y * 0.5f + 0.5f) * 255.0f) << 8;
        normal |= (uint32_t)((_nor.z * 0.5f + 0.5f) * 255.0f) << 16;
    }

    XMFLOAT3 MeshComponent::Vertex_Pos::GetNormal() const
    {
        XMFLOAT3 norm(0, 0, 0);
        norm.x = (float)((normal >> 0) & 0x000000FF) / 255.0f * 2.0f - 1.0f;
        norm.y = (float)((normal >> 8) & 0x000000FF) / 255.0f * 2.0f - 1.0f;
        norm.z = (float)((normal >> 16) & 0x000000FF) / 255.0f * 2.0f - 1.0f;
        return norm;
    }

    void LightComponent::UpdateLight()
    {
        // Skip directional lights as they affects the whole scene and 
        // doesen't really have a AABB.
        if (type == LightType::kDirectional) 
        {
            return;
        }

        aabb.CreateFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(range, range, range));
    }

    void CameraComponent::CreatePerspective(float newAspect, float newNear, float newFar, float newFOV)
    {
        aspect = newAspect;
        zNearPlane = newNear;
        zFarPlane = newFar;
        fov = newFOV;

        UpdateCamera();
    }

    void CameraComponent::UpdateCamera()
    {
        // NOTE: reverse zbuffer!
        XMMATRIX _P = XMMatrixPerspectiveFovLH(fov, aspect, zFarPlane, zNearPlane);
        
        XMVECTOR _Eye = XMLoadFloat3(&pos);
        XMVECTOR _At = XMLoadFloat3(&target);
        XMVECTOR _Up = XMLoadFloat3(&up);
        XMMATRIX _V = XMMatrixLookToLH(_Eye, _At, _Up);
        XMMATRIX _VP = XMMatrixMultiply(_V, _P);

        XMStoreFloat4x4(&view, _V);
        XMStoreFloat4x4(&VP, _VP);
        XMStoreFloat4x4(&inv_view, XMMatrixInverse(nullptr, _V));
        XMStoreFloat4x4(&inv_VP, XMMatrixInverse(nullptr, _VP));
        XMStoreFloat4x4(&projection, _P);
        XMStoreFloat4x4(&inv_projection, XMMatrixInverse(nullptr, _P));

        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, _V);
        XMMATRIX _Rot = XMMatrixRotationQuaternion(R);
        XMStoreFloat3x3(&rotation, _Rot);

        frustum.Create(_VP);
    }

    void CameraComponent::TransformCamera(const TransformComponent& transform)
    {
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&transform.world));

        XMVECTOR _Eye = T;
        XMVECTOR _At = XMVectorSet(0, 0, 1, 0);
        XMVECTOR _Up = XMVectorSet(0, 1, 0, 0);

        XMMATRIX _Rot = XMMatrixRotationQuaternion(R);
        _At = XMVector3TransformNormal(_At, _Rot);
        _Up = XMVector3TransformNormal(_Up, _Rot);
        XMStoreFloat3x3(&rotation, _Rot);

        XMMATRIX _V = XMMatrixLookToLH(_Eye, _At, _Up);
        XMStoreFloat4x4(&view, _V);

        XMStoreFloat3(&pos, _Eye);
        XMStoreFloat3(&target, _At);
        XMStoreFloat3(&up, _Up);
    }

    ecs::Entity Scene::CreateGroup(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity) = name;
        transforms.Create(entity);
        groups.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateMaterial(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity) = name;
        materials.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateMesh(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity) = name;
        meshes.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateObject(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity) = name;
        transforms.Create(entity);
        aabb_objects.Create(entity);
        objects.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateLight(
        const std::string& name,
        const XMFLOAT3& position,
        const XMFLOAT3& color,
        float energy,
        float range,
        LightType type
    )
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity) = name;

        TransformComponent& transform = transforms.Create(entity);
        transform.Translate(position);
        transform.UpdateTransform();

        aabb_lights.Create(entity);

        LightComponent& light = lights.Create(entity);
        light.energy = energy;
        light.range = range;
        light.color = color;
        light.SetType(type);

        return entity;
    }

    ecs::Entity Scene::CreateCamera(
        const std::string& name,
        float aspect,
        float nearPlane,
        float farPlane,
        float fov
    )
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity) = name;
        transforms.Create(entity);
        CameraComponent& camera = cameras.Create(entity);
        camera.CreatePerspective(aspect, nearPlane, farPlane, fov);
        return entity;
    }

    ecs::Entity Scene::FindMaterial(const std::string& search_value)
    {
        for (size_t i = 0; i < materials.Size(); ++i)
        {
            const ecs::Entity component_id = materials.GetEntity(i);
            const scene::NameComponent* name = names.GetComponent(component_id);
            if (!name)
            {
                continue;
            }

            if (name->name.compare(search_value) == 0)
            {
                return component_id;
            }
        }

        return ecs::kInvalidEntity;
    }

    void Scene::ComponentAttach(ecs::Entity entity, ecs::Entity parent)
    {
        assert(entity != parent);
        
        if (hierarchy.Contains(entity))
        {
            ComponentDetach(entity);
        }

        HierarchyComponent& component = hierarchy.Create(entity);
        component.parentID = parent;

        // Child updated immediately, so that it can be immediately be attached to afterwards:
        const TransformComponent* transform_parent = transforms.GetComponent(parent);
        TransformComponent* transform_child = transforms.GetComponent(entity);

        // Move child to parent's local space:
        XMMATRIX B = XMMatrixInverse(nullptr, XMLoadFloat4x4(&transform_parent->world));
        transform_child->MatrixTransform(B);
        transform_child->UpdateTransform();

        transform_child->UpdateTransformParented(*transform_parent);
    }

    void Scene::ComponentDetach(ecs::Entity entity)
    {
        const HierarchyComponent* parent = hierarchy.GetComponent(entity);
        if (parent == nullptr)
        {
            return;
        }

        TransformComponent* transform = transforms.GetComponent(entity);
        transform->ApplyTransform();
        hierarchy.Remove(entity);
    }

    void Scene::Update(float dt)
    {
        CYB_PROFILE_FUNCTION();

        (void)dt;   // Unused
        jobsystem::Context ctx;
        RunTransformUpdateSystem(ctx);
        jobsystem::Wait(ctx);               // Dependencies
        RunHierarchyUpdateSystem();         // Non-Threaded

        RunObjectUpdateSystem(ctx);
        RunLightUpdateSystem(ctx);
        RunCameraUpdateSystem(ctx);
        RunWeatherUpdateSystem(ctx);
        jobsystem::Wait(ctx);
    }

    void Scene::Clear()
    {
        names.Clear();
        transforms.Clear();
        groups.Clear();
        hierarchy.Clear();
        materials.Clear();
        meshes.Clear();
        objects.Clear();
        aabb_objects.Clear();
        lights.Clear();
        aabb_lights.Clear();
        cameras.Clear();
        weathers.Clear();
    }

    void Scene::merge(Scene& other)
    {
        names.Merge(other.names);
        transforms.Merge(other.transforms);
        groups.Merge(other.groups);
        hierarchy.Merge(other.hierarchy);
        materials.Merge(other.materials);
        meshes.Merge(other.meshes);
        objects.Merge(other.objects);
        aabb_objects.Merge(other.aabb_objects);
        lights.Merge(other.lights);
        aabb_lights.Merge(other.aabb_lights);
        cameras.Merge(other.cameras);
        weathers.Merge(other.weathers);
    }

    void Scene::RemoveEntity(ecs::Entity entity)
    {
        names.Remove(entity);
        transforms.Remove(entity);
        groups.Remove(entity);
        hierarchy.Remove(entity);
        materials.Remove(entity);
        meshes.Remove(entity);
        objects.Remove(entity);
        aabb_objects.Remove(entity);
        lights.Remove(entity);
        aabb_lights.Remove(entity);
        cameras.Remove(entity);
        weathers.Remove(entity);
    }

    void Scene::Serialize(serializer::Archive& ar)
    {
        ecs::SerializeEntityContext serialize;

        names.Serialize(ar, serialize);
        transforms.Serialize(ar, serialize);
        if (ar.GetVersion() >= 4)
            groups.Serialize(ar, serialize);
        hierarchy.Serialize(ar, serialize);
        materials.Serialize(ar, serialize);
        meshes.Serialize(ar, serialize);
        objects.Serialize(ar, serialize);
        aabb_objects.Serialize(ar, serialize);
        lights.Serialize(ar, serialize);
        aabb_lights.Serialize(ar, serialize);
        cameras.Serialize(ar, serialize);
        weathers.Serialize(ar, serialize);
    }

    const uint32_t small_subtask_groupsize = 64;

    void Scene::RunTransformUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)transforms.Size(), small_subtask_groupsize, [&](jobsystem::JobArgs args)
            {
                TransformComponent& transform = transforms[args.jobIndex];
                transform.UpdateTransform();
            });
    }

    void Scene::RunHierarchyUpdateSystem()
    {
        // This needs serialized execution because there are dependencies enforced by component order!
        for (size_t i = 0; i < hierarchy.Size(); ++i)
        {
            const HierarchyComponent& parentComponent = hierarchy[i];
            ecs::Entity entity = hierarchy.GetEntity(i);

            TransformComponent* transformChild = transforms.GetComponent(entity);
            const TransformComponent* transformParent = transforms.GetComponent(parentComponent.parentID);
            transformChild->UpdateTransformParented(*transformParent);
        }
    }

    void Scene::RunObjectUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Execute(ctx, [&]
            {
                for (size_t i = 0; i < objects.Size(); ++i)
                {
                    ObjectComponent& object = objects[i];
                    AxisAlignedBox& aabb = aabb_objects[i];

                    aabb = AxisAlignedBox();

                    if (object.meshID != ecs::kInvalidEntity)
                    {
                        const ecs::Entity entity = objects.GetEntity(i);
                        const MeshComponent* mesh = meshes.GetComponent(object.meshID);

                        object.transformIndex = (int32_t)transforms.GetIndex(entity);
                        const TransformComponent& transform = transforms[object.transformIndex];

                        aabb = mesh->aabb.Transform(XMLoadFloat4x4(&transform.world));
                    }
                }
            });
    }

    void Scene::RunLightUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Execute(ctx, [&]
            {
                for (size_t i = 0; i < lights.Size(); ++i)
                {
                    const ecs::Entity entity = lights.GetEntity(i);
                    const TransformComponent* transform = transforms.GetComponent(entity);

                    LightComponent& light = lights[i];
                    light.UpdateLight();

                    AxisAlignedBox& aabb = aabb_lights[i];
                    aabb = light.aabb.Transform(XMLoadFloat4x4(&transform->world));
                }
            });
    }

    void Scene::RunCameraUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)cameras.Size(), small_subtask_groupsize, [&](jobsystem::JobArgs args)
            {
                CameraComponent& camera = cameras[args.jobIndex];

                camera.UpdateCamera();
            });
    }

    void Scene::RunWeatherUpdateSystem(jobsystem::Context& /* ctx */)
    {
        if (weathers.Size() > 0)
        {
            active_weather = weathers[0];
        }
    }

    void LoadModel(const std::string& filename)
    {
        Scene scene;
        LoadModel(scene, filename);
        GetScene().merge(scene);
    }

    void LoadModel(Scene& scene, const std::string& filename)
    {
        Timer timer;
        timer.Record();
        serializer::Archive ar(filename);

        if (!ar.IsOpen())
            return;

        scene.Serialize(ar);
        CYB_TRACE("Loaded scene (filename={0}) in {1:.2f}ms", filename, timer.ElapsedMilliseconds());
    }

    PickResult Pick(const Scene& scene, const Ray& ray)
    {
        //CYB_TIMED_FUNCTION();

        PickResult result;
        const XMVECTOR ray_origin = XMLoadFloat3(&ray.m_origin);
        const XMVECTOR ray_direction = XMVector3Normalize(XMLoadFloat3(&ray.m_direction));

        for (size_t i = 0; i < scene.aabb_objects.Size(); ++i)
        {
            const AxisAlignedBox& aabb = scene.aabb_objects[i];

            if (!ray.IntersectBoundingBox(aabb))
                continue;

            const ObjectComponent& object = scene.objects[i];
            if (object.meshID == ecs::kInvalidEntity)
                continue;

            const ecs::Entity entity = scene.aabb_objects.GetEntity(i);
            const MeshComponent* mesh = scene.meshes.GetComponent(object.meshID);
            const XMMATRIX object_matrix = object.transformIndex >= 0 ? XMLoadFloat4x4(&scene.transforms[object.transformIndex].world) : XMMatrixIdentity();
            const XMMATRIX inv_object_matrix = XMMatrixInverse(nullptr, object_matrix);
            const XMVECTOR ray_origin_local = XMVector3Transform(ray_origin, inv_object_matrix);
            const XMVECTOR ray_direction_local = XMVector3Normalize(XMVector3TransformNormal(ray_direction, inv_object_matrix));

            for (auto& subset : mesh->subsets)
            {
                for (size_t j = 0; j < subset.indexCount; j += 3)
                {
                    const uint32_t i0 = mesh->indices[subset.indexOffset + j + 0];
                    const uint32_t i1 = mesh->indices[subset.indexOffset + j + 1];
                    const uint32_t i2 = mesh->indices[subset.indexOffset + j + 2];

                    const XMVECTOR p0 = XMLoadFloat3(&mesh->vertex_positions[i0]);
                    const XMVECTOR p1 = XMLoadFloat3(&mesh->vertex_positions[i1]);
                    const XMVECTOR p2 = XMLoadFloat3(&mesh->vertex_positions[i2]);

                    float distance;
                    XMFLOAT2 bary;
                    if (math::RayTriangleIntersects(ray_origin_local, ray_direction_local, p0, p1, p2, distance, bary))
                    {
                        const XMVECTOR pos = XMVector3Transform(XMVectorAdd(ray_origin_local, ray_direction_local * distance), object_matrix);
                        distance = math::Distance(pos, ray_origin);

                        if (distance < result.distance)
                        {
                            result.entity = entity;
                            XMStoreFloat3(&result.position, pos);
                            result.distance = distance;
                        }
                    }
                }
            }
        }

        return result;
    }


    /////////////////////////////////////////////////////////////////////////////////////////
    // Scene struct serializers:

    void SerializeComponent(NameComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& state)
    {
        if (ar.IsReadMode())
        {
            ar >> x.name;
        }
        else
        {
            ar << x.name;
        }
    }

    void SerializeComponent(TransformComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            ar >> x.flags;
            ar >> x.scale_local;
            ar >> x.rotation_local;
            ar >> x.translation_local;

            x.SetDirty();
            jobsystem::Execute(serialize.ctx, [&x] { x.UpdateTransform(); });
        }
        else
        {
            ar << x.flags;
            ar << x.scale_local;
            ar << x.rotation_local;
            ar << x.translation_local;
        }
    }

    void SerializeComponent(GroupComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
    }

    void SerializeComponent(HierarchyComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        ecs::SerializeEntity(ar, x.parentID, serialize);
    }

    void SerializeComponent(MaterialComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            ar >> x.flags;
            if (ar.GetVersion() >= 4)
                ar >> (uint32_t&)x.shaderType;
            ar >> x.baseColor;
            ar >> x.roughness;
            ar >> x.metalness;
        }
        else
        {
            ar << x.flags;
            if (ar.GetVersion() >= 4)
                ar << (uint32_t&)x.shaderType;
            ar << x.baseColor;
            ar << x.roughness;
            ar << x.metalness;
        }
    }

    void SerializeComponent(MeshComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            size_t subsetCount;
            ar >> subsetCount;
            x.subsets.resize(subsetCount);
            for (size_t i = 0; i < subsetCount; ++i)
            {
                ecs::SerializeEntity(ar, x.subsets[i].materialID, serialize);
                ar >> x.subsets[i].indexOffset;
                ar >> x.subsets[i].indexCount;
            }

            ar >> x.vertex_positions;
            ar >> x.vertex_normals;
            ar >> x.vertex_colors;
            ar >> x.indices;

            jobsystem::Execute(serialize.ctx, [&x] { x.CreateRenderData(); });
        }
        else
        {
            ar << x.subsets.size();
            for (size_t i = 0; i < x.subsets.size(); ++i)
            {
                ecs::SerializeEntity(ar, x.subsets[i].materialID, serialize);
                ar << x.subsets[i].indexOffset;
                ar << x.subsets[i].indexCount;
            }

            ar << x.vertex_positions;
            ar << x.vertex_normals;
            ar << x.vertex_colors;
            ar << x.indices;
        }
    }

    void SerializeComponent(ObjectComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            ar >> x.flags;
            ecs::SerializeEntity(ar, x.meshID, serialize);
        }
        else
        {
            ar << x.flags;
            ecs::SerializeEntity(ar, x.meshID, serialize);
        }
    }

    void SerializeComponent(LightComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            ar >> x.flags;
            ar >> x.color;
            ar >> (uint32_t&)x.type;
            ar >> x.energy;
            ar >> x.range;
        }
        else
        {
            ar << x.flags;
            ar << x.color;
            ar << (uint32_t&)x.type;
            ar << x.energy;
            ar << x.range;
        }
    }

    void SerializeComponent(CameraComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            ar >> x.aspect;
            ar >> x.zNearPlane;
            ar >> x.zFarPlane;
            ar >> x.fov;
            ar >> x.pos;
            ar >> x.target;
            ar >> x.up;
        }
        else
        {
            ar << x.aspect;
            ar << x.zNearPlane;
            ar << x.zFarPlane;
            ar << x.fov;
            ar << x.pos;
            ar << x.target;
            ar << x.up;
        }
    }

    void SerializeComponent(WeatherComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            ar >> x.horizon;
            ar >> x.zenith;
        }
        else
        {
            ar << x.horizon;
            ar << x.zenith;
        }
    }

    void SerializeComponent(AxisAlignedBox& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            XMFLOAT3 min;
            XMFLOAT3 max;
            ar >> min;
            ar >> max;
            x = AxisAlignedBox(min, max);
        }
        else
        {
            ar << x.GetMin();
            ar << x.GetMax();
        }
    }
}