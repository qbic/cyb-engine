#include "core/profiler.h"
#include "core/logger.h"
#include "systems/scene.h"

namespace cyb::scene
{
    void TransformComponent::SetDirty(bool value)
    {
        if (value)
            flags |= Flags::DirtyBit;
        else
            flags &= ~Flags::DirtyBit;
    }

    bool TransformComponent::IsDirty() const
    {
        return HasFlag(flags, Flags::DirtyBit);
    }

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
        XMVECTOR S = XMLoadFloat3(&scale_local);
        XMVECTOR R = XMLoadFloat4(&rotation_local);
        XMVECTOR T = XMLoadFloat3(&translation_local);
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

    void MaterialComponent::SetUseVertexColors(bool value)
    {
        if (value)
            flags |= Flags::UseVertexColorsBit;
        else
            flags &= ~Flags::UseVertexColorsBit;
    }

    bool MaterialComponent::IsUsingVertexColors() const
    {
        return HasFlag(flags, Flags::UseVertexColorsBit);
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
        graphics::GraphicsDevice* device = graphics::GetDevice();

        // Create index buffer gpu data
        {
            graphics::GPUBufferDesc desc;
            desc.size = uint32_t(sizeof(uint32_t) * indices.size());
            desc.usage = graphics::MemoryAccess::Default;
            desc.bindFlags = graphics::BindFlags::IndexBufferBit;

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
            desc.usage = graphics::MemoryAccess::Default;
            desc.size = uint32_t(sizeof(Vertex_Pos) * _vertices.size());
            desc.bindFlags = graphics::BindFlags::VertexBufferBit;
            bool result = device->CreateBuffer(&desc, _vertices.data(), &vertex_buffer_pos);
            assert(result == true);
        }

        // vertex_buffer_col - COLOR
        if (!vertex_colors.empty())
        {
            graphics::GPUBufferDesc desc;
            desc.usage = graphics::MemoryAccess::Default;
            desc.size = uint32_t(sizeof(uint32_t) * vertex_colors.size());
            desc.bindFlags = graphics::BindFlags::VertexBufferBit;
            bool result = device->CreateBuffer(&desc, vertex_colors.data(), &vertex_buffer_col);
            assert(result == true);
        }

        aabb = math::AxisAlignedBox(_min, _max);
    }

    void MeshComponent::ComputeHardNormals()
    {
        std::vector<XMFLOAT3> newPositions;
        std::vector<XMFLOAT3> newNormals;
        std::vector<uint32_t> newColors;
        std::vector<uint32_t> newIndices;

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
            XMVECTOR N = XMVector3Normalize(XMVector3Cross(U, V));

            XMFLOAT3 normal;
            XMStoreFloat3(&normal, N);

            newPositions.push_back(p0);
            newPositions.push_back(p1);
            newPositions.push_back(p2);

            newNormals.push_back(normal);
            newNormals.push_back(normal);
            newNormals.push_back(normal);

            if (!vertex_colors.empty())
            {
                newColors.push_back(vertex_colors[i0]);
                newColors.push_back(vertex_colors[i1]);
                newColors.push_back(vertex_colors[i2]);
            }

            newIndices.push_back(static_cast<uint32_t>(newIndices.size()));
            newIndices.push_back(static_cast<uint32_t>(newIndices.size()));
            newIndices.push_back(static_cast<uint32_t>(newIndices.size()));
        }

        // For hard surface normals, we created a new mesh in the previous loop through faces, so swap data:
        vertex_positions = newPositions;
        vertex_normals = newNormals;
        vertex_colors = newColors;
        indices = newIndices;

        CreateRenderData();
    }

    void MeshComponent::ComputeSmoothNormals()
    {
        std::vector<XMFLOAT3> newNormals;
        newNormals.resize(vertex_positions.size());

        for (size_t face = 0; face < indices.size() / 3; face++)
        {
            const uint32_t i0 = indices[face * 3 + 0];
            const uint32_t i1 = indices[face * 3 + 1];
            const uint32_t i2 = indices[face * 3 + 2];

            const XMFLOAT3& p0 = vertex_positions[i0];
            const XMFLOAT3& p1 = vertex_positions[i1];
            const XMFLOAT3& p2 = vertex_positions[i2];

            const XMVECTOR U = XMLoadFloat3(&p2) - XMLoadFloat3(&p0);
            const XMVECTOR V = XMLoadFloat3(&p1) - XMLoadFloat3(&p0);
            const XMVECTOR N = XMVector3Normalize(XMVector3Cross(U, V));

            XMFLOAT3 normal;
            XMStoreFloat3(&normal, N);

            newNormals[i0].x += normal.x;
            newNormals[i0].y += normal.y;
            newNormals[i0].z += normal.z;

            newNormals[i1].x += normal.x;
            newNormals[i1].y += normal.y;
            newNormals[i1].z += normal.z;

            newNormals[i2].x += normal.x;
            newNormals[i2].y += normal.y;
            newNormals[i2].z += normal.z;
        }

        vertex_normals = newNormals;
        CreateRenderData(); // <- Normals will be normalized here
    }

    void MeshComponent::Vertex_Pos::Set(const XMFLOAT3& pos, const XMFLOAT3& norm)
    {
        this->pos = pos;
        this->normal = EncodeNormal(norm);
    }

    uint32_t MeshComponent::Vertex_Pos::EncodeNormal(const XMFLOAT3& norm) const
    {
        uint32_t n = 0xFF000000;
        n |= (uint32_t)((norm.x * 0.5f + 0.5f) * 255.0f) << 0;
        n |= (uint32_t)((norm.y * 0.5f + 0.5f) * 255.0f) << 8;
        n |= (uint32_t)((norm.z * 0.5f + 0.5f) * 255.0f) << 16;
        return n;
    }

    XMFLOAT3 MeshComponent::Vertex_Pos::DecodeNormal() const
    {
        XMFLOAT3 norm(0, 0, 0);
        norm.x = (float)((normal >> 0) & 0x000000FF) / 255.0f * 2.0f - 1.0f;
        norm.y = (float)((normal >> 8) & 0x000000FF) / 255.0f * 2.0f - 1.0f;
        norm.z = (float)((normal >> 16) & 0x000000FF) / 255.0f * 2.0f - 1.0f;
        return norm;
    }

    uint32_t MeshComponent::Vertex_Pos::DecodeMaterialIndex() const
    {
        return (normal >> 24) & 0x000000FF;
    }

    void LightComponent::SetffectingSceney(bool value)
    {
        if (value)
            flags |= Flags::AffectsSceneBit;
        else
            flags &= ~Flags::AffectsSceneBit;
    }

    bool LightComponent::IsAffectingScene() const
    {
        return HasFlag(flags, Flags::AffectsSceneBit);
    }

    void LightComponent::UpdateLight()
    {
        // Skip directional lights as they affects the whole scene and 
        // doesen't really have a AABB.
        if (type == LightType::Directional)
        {
            return;
        }

        const float halfRange = range * 0.5f;
        aabb = math::AxisAlignedBox(-halfRange, halfRange);
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
        XMMATRIX _P = XMMatrixPerspectiveFovLH(math::ToRadians(fov), aspect, zFarPlane, zNearPlane);

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

        frustum = math::Frustum(_VP);
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

        return ecs::INVALID_ENTITY;
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
            return;

        TransformComponent* transform = transforms.GetComponent(entity);
        transform->ApplyTransform();
        hierarchy.Remove(entity);
    }

    void Scene::Update([[maybe_unused]] double dt)
    {
        jobsystem::Context ctx;

        // Run update systems with no dependency
        RunTransformUpdateSystem(ctx);
        RunWeatherUpdateSystem(ctx);

        // Run update systems that only depends on local transform
        jobsystem::Wait(ctx);
        RunHierarchyUpdateSystem(ctx);

        // Run update systems that is dependent on world transform
        jobsystem::Wait(ctx);
        RunObjectUpdateSystem(ctx);
        RunLightUpdateSystem(ctx);
        RunCameraUpdateSystem(ctx);
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

    void Scene::RemoveEntity(ecs::Entity entity, bool recursive)
    {
        if (recursive)
        {
            std::vector<ecs::Entity> removeList;
            for (size_t i = 0; i < hierarchy.Size(); ++i)
            {
                const HierarchyComponent& hier = hierarchy[i];
                if (hier.parentID == entity)
                {
                    ecs::Entity child = hierarchy.GetEntity(i);
                    removeList.push_back(child);
                }
            }
            for (auto& child : removeList)
            {
                RemoveEntity(child, true);
            }
        }

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

    void Scene::Serialize(serializer::Archive& archive)
    {
        ecs::SerializeEntityContext serialize;

        names.Serialize(archive, serialize);
        transforms.Serialize(archive, serialize);
        if (archive.GetVersion() >= 4)
            groups.Serialize(archive, serialize);
        hierarchy.Serialize(archive, serialize);
        materials.Serialize(archive, serialize);
        meshes.Serialize(archive, serialize);
        objects.Serialize(archive, serialize);
        aabb_objects.Serialize(archive, serialize);
        lights.Serialize(archive, serialize);
        aabb_lights.Serialize(archive, serialize);
        cameras.Serialize(archive, serialize);
        weathers.Serialize(archive, serialize);
    }

    const uint32_t SMALL_SUBTASK_GROUPSIZE = 64;

    void Scene::RunTransformUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)transforms.Size(), SMALL_SUBTASK_GROUPSIZE, [&](jobsystem::JobArgs args)
            {
                TransformComponent& transform = transforms[args.jobIndex];
                transform.UpdateTransform();
            });
    }

    void Scene::RunHierarchyUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)hierarchy.Size(), SMALL_SUBTASK_GROUPSIZE, [&](jobsystem::JobArgs args)
            {
                const HierarchyComponent& hier = hierarchy[args.jobIndex];
                ecs::Entity entity = hierarchy.GetEntity(args.jobIndex);
                TransformComponent* transform = transforms.GetComponent(entity);
                
                XMMATRIX worldMatrix = {};
                if (transform != nullptr)
                {
                    worldMatrix = transform->GetLocalMatrix();
                }

                ecs::Entity parentID = hier.parentID;
                while (parentID != ecs::INVALID_ENTITY)
                {
                    const TransformComponent* transformParent = transforms.GetComponent(parentID);

                    if (transform != nullptr && transformParent != nullptr)
                        worldMatrix *= transformParent->GetLocalMatrix();

                    const HierarchyComponent* hierRecursive = hierarchy.GetComponent(parentID);
                    if (hierRecursive != nullptr)
                    {
                        parentID = hierRecursive->parentID;
                    }
                    else
                    {
                        parentID = ecs::INVALID_ENTITY;
                    }
                }

                if (transform != nullptr)
                {
                    XMStoreFloat4x4(&transform->world, worldMatrix);
                }
            });
    }

    void Scene::RunObjectUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Execute(ctx, [&](jobsystem::JobArgs)
            {
                for (size_t i = 0; i < objects.Size(); ++i)
                {
                    ObjectComponent& object = objects[i];
                    math::AxisAlignedBox& aabb = aabb_objects[i];

                    aabb = math::AxisAlignedBox();

                    if (object.meshID != ecs::INVALID_ENTITY)
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
        jobsystem::Dispatch(ctx, (uint32_t)lights.Size(), SMALL_SUBTASK_GROUPSIZE, [&](jobsystem::JobArgs args)
            {
                const ecs::Entity entity = lights.GetEntity(args.jobIndex);
                const TransformComponent* transform = transforms.GetComponent(entity);

                LightComponent& light = lights[args.jobIndex];
                light.UpdateLight();

                math::AxisAlignedBox& aabb = aabb_lights[args.jobIndex];
                aabb = light.aabb.Transform(XMLoadFloat4x4(&transform->world));
            });
    }

    void Scene::RunCameraUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)cameras.Size(), SMALL_SUBTASK_GROUPSIZE, [&](jobsystem::JobArgs args)
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

    bool LoadModel(const std::string& filename)
    {
        Scene scene;
        if (!LoadModel(scene, filename))
            return false;

        GetScene().merge(scene);
        return true;
    }

    bool LoadModel(Scene& scene, const std::string& filename)
    {
        Timer timer;
        timer.Record();
        serializer::Archive ar(filename);
        if (!ar.IsOpen())
            return false;

        scene.Serialize(ar);
        CYB_TRACE("Loaded scene (filename={0}) in {1:.2f}ms", filename, timer.ElapsedMilliseconds());
        return true;
    }

    PickResult Pick(const Scene& scene, const math::Ray& ray)
    {
        CYB_TIMED_FUNCTION();

        PickResult result;
        const XMVECTOR ray_origin = XMLoadFloat3(&ray.origin);
        const XMVECTOR ray_direction = XMVector3Normalize(XMLoadFloat3(&ray.direction));

        for (size_t i = 0; i < scene.aabb_objects.Size(); ++i)
        {
            const math::AxisAlignedBox& aabb = scene.aabb_objects[i];

            if (!ray.IntersectsBoundingBox(aabb))
                continue;

            const ObjectComponent& object = scene.objects[i];
            if (object.meshID == ecs::INVALID_ENTITY)
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
}

// Scene component serializers
namespace cyb::ecs
{
    void SerializeComponent(scene::NameComponent& x, serializer::Archive& archive, SerializeEntityContext& entitySerializer)
    {
        if (archive.IsReadMode())
        {
            archive >> x.name;
        }
        else
        {
            archive << x.name;
        }
    }

    void SerializeComponent(scene::TransformComponent& x, serializer::Archive& archive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (archive.IsReadMode())
        {
            archive.UnsafeRead(x.flags);
            archive >> x.scale_local;
            archive >> x.rotation_local;
            archive >> x.translation_local;

            x.SetDirty();
            jobsystem::Execute(entitySerializer.ctx, [&x](jobsystem::JobArgs) { x.UpdateTransform(); });
        }
        else
        {
            archive << (uint32_t)x.flags;
            archive << x.scale_local;
            archive << x.rotation_local;
            archive << x.translation_local;
        }
    }

    void SerializeComponent(scene::GroupComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
    }

    void SerializeComponent(scene::HierarchyComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        ecs::SerializeEntity(x.parentID, arhive, entitySerializer);
    }

    void SerializeComponent(scene::MaterialComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (arhive.IsReadMode())
        {
            arhive.UnsafeRead(x.flags);
            if (arhive.GetVersion() >= 4)
                arhive >> (uint32_t&)x.shaderType;
            arhive >> x.baseColor;
            arhive >> x.roughness;
            arhive >> x.metalness;
        }
        else
        {
            arhive << (uint32_t)x.flags;
            if (arhive.GetVersion() >= 4)
                arhive << (uint32_t&)x.shaderType;
            arhive << x.baseColor;
            arhive << x.roughness;
            arhive << x.metalness;
        }
    }

    void SerializeComponent(scene::MeshComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (arhive.IsReadMode())
        {
            size_t subsetCount;
            arhive >> subsetCount;
            x.subsets.resize(subsetCount);
            for (size_t i = 0; i < subsetCount; ++i)
            {
                ecs::SerializeEntity(x.subsets[i].materialID, arhive, entitySerializer);
                arhive >> x.subsets[i].indexOffset;
                arhive >> x.subsets[i].indexCount;
            }

            arhive >> x.vertex_positions;
            arhive >> x.vertex_normals;
            arhive >> x.vertex_colors;
            arhive >> x.indices;

            jobsystem::Execute(entitySerializer.ctx, [&x](jobsystem::JobArgs) { x.CreateRenderData(); });
        }
        else
        {
            arhive << x.subsets.size();
            for (size_t i = 0; i < x.subsets.size(); ++i)
            {
                ecs::SerializeEntity(x.subsets[i].materialID, arhive, entitySerializer);
                arhive << x.subsets[i].indexOffset;
                arhive << x.subsets[i].indexCount;
            }

            arhive << x.vertex_positions;
            arhive << x.vertex_normals;
            arhive << x.vertex_colors;
            arhive << x.indices;
        }
    }

    void SerializeComponent(scene::ObjectComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (arhive.IsReadMode())
        {
            arhive.UnsafeRead(x.flags);
            ecs::SerializeEntity(x.meshID, arhive, entitySerializer);
        }
        else
        {
            arhive << (uint32_t)x.flags;
            ecs::SerializeEntity(x.meshID, arhive, entitySerializer);
        }
    }

    void SerializeComponent(scene::LightComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (arhive.IsReadMode())
        {
            arhive >> (uint32_t&)x.flags;
            arhive >> x.color;
            arhive >> (uint32_t&)x.type;
            arhive >> x.energy;
            arhive >> x.range;
        }
        else
        {
            arhive << (uint32_t)x.flags;
            arhive << x.color;
            arhive << (uint32_t&)x.type;
            arhive << x.energy;
            arhive << x.range;
        }
    }

    void SerializeComponent(scene::CameraComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (arhive.IsReadMode())
        {
            arhive >> x.aspect;
            arhive >> x.zNearPlane;
            arhive >> x.zFarPlane;
            arhive >> x.fov;
            arhive >> x.pos;
            arhive >> x.target;
            arhive >> x.up;
        }
        else
        {
            arhive << x.aspect;
            arhive << x.zNearPlane;
            arhive << x.zFarPlane;
            arhive << x.fov;
            arhive << x.pos;
            arhive << x.target;
            arhive << x.up;
        }
    }

    void SerializeComponent(scene::WeatherComponent& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (arhive.IsReadMode())
        {
            arhive >> x.horizon;
            arhive >> x.zenith;
        }
        else
        {
            arhive << x.horizon;
            arhive << x.zenith;
        }
    }

    void SerializeComponent(math::AxisAlignedBox& x, serializer::Archive& arhive, ecs::SerializeEntityContext& entitySerializer)
    {
        if (arhive.IsReadMode())
        {
            XMFLOAT3 min;
            XMFLOAT3 max;
            arhive >> min;
            arhive >> max;
            x = math::AxisAlignedBox(min, max);
        }
        else
        {
            arhive << x.min;
            arhive << x.max;
        }
    }
}