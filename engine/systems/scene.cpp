#include "core/cvar.h"
#include "core/logger.h"
#include "systems/profiler.h"
#include "systems/scene.h"

using namespace cyb::rhi;

namespace cyb::scene
{
    CVar r_sceneSubtaskGroupsize("r_sceneSubtaskGroupsize", 64u, CVarFlag::RendererBit, "Groupsize for multithreaded scene update tasks");

    void TransformComponent::SetDirty(bool value)
    {
        SetFlag(flags, Flags::DirtyBit, value);
    }

    bool TransformComponent::IsDirty() const
    {
        return HasFlag(flags, Flags::DirtyBit);
    }

    XMFLOAT3 TransformComponent::GetPosition() const noexcept
    {
        return *((XMFLOAT3*)&world._41);
    }

    XMFLOAT4 TransformComponent::GetRotation() const noexcept
    {
        XMFLOAT4 rotation;
        XMStoreFloat4(&rotation, GetRotationV());
        return rotation;
    }

    XMFLOAT3 TransformComponent::GetScale() const noexcept
    {
        XMFLOAT3 scale;
        XMStoreFloat3(&scale, GetScaleV());
        return scale;
    }

    XMVECTOR TransformComponent::GetPositionV() const noexcept
    {
        return XMLoadFloat3((XMFLOAT3*)&world._41);
    }

    XMVECTOR TransformComponent::GetRotationV() const noexcept
    {
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&world));
        return R;
    }

    XMVECTOR TransformComponent::GetScaleV() const noexcept
    {
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&world));
        return S;
    }

    XMMATRIX TransformComponent::GetLocalMatrix() const noexcept
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
        if (!IsDirty())
            return;

        SetDirty(false);
        XMStoreFloat4x4(&world, GetLocalMatrix());
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
        SetFlag(flags, Flags::UseVertexColorsBit, value);
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
        rhi::GraphicsDevice* device = rhi::GetDevice();

        // create index buffer gpu data
        {
            rhi::GPUBufferDesc desc;
            desc.size = uint32_t(sizeof(uint32_t) * indices.size());
            desc.bindFlags = rhi::BindFlags::IndexBufferBit;

            bool result = device->CreateBuffer(&desc, indices.data(), &index_buffer);
            assert(result == true);
        }

        aabb.Invalidate();

        // vertex_buffer_pos - POSITION + NORMAL
        {
            std::vector<Vertex_Pos> _vertices(vertex_positions.size());
            for (size_t i = 0; i < _vertices.size(); ++i)
            {
                const XMFLOAT3& pos = vertex_positions[i];
                XMFLOAT3 nor = vertex_normals.empty() ? XMFLOAT3(1, 1, 1) : vertex_normals[i];
                XMStoreFloat3(&nor, XMVector3Normalize(XMLoadFloat3(&nor)));
                _vertices[i].Set(pos, nor);

                aabb.GrowPoint(pos);
            }

            rhi::GPUBufferDesc desc;
            desc.size = uint32_t(sizeof(Vertex_Pos) * _vertices.size());
            desc.bindFlags = rhi::BindFlags::VertexBufferBit;
            bool result = device->CreateBuffer(&desc, _vertices.data(), &vertex_buffer_pos);
            assert(result == true);
        }

        // vertex_buffer_col - COLOR
        if (!vertex_colors.empty())
        {
            rhi::GPUBufferDesc desc;
            desc.size = uint32_t(sizeof(uint32_t) * vertex_colors.size());
            desc.bindFlags = rhi::BindFlags::VertexBufferBit;
            bool result = device->CreateBuffer(&desc, vertex_colors.data(), &vertex_buffer_col);
            assert(result == true);
        }
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

        // for hard surface normals, we created a new mesh in the previous loop through 
        // faces, so swap data
        vertex_positions = newPositions;
        vertex_normals = newNormals;
        vertex_colors = newColors;
        indices = newIndices;
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

        // normals will be normalized when creating render data
        vertex_normals = newNormals;
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

    void ObjectComponent::SetUserStencilRef(uint8_t value)
    {
        assert(value < 16);
        userStencilRef = value & 0x0F;
    }

    void LightComponent::SetAffectingScene(bool value)
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
        // using reverse z-buffer: zNear > zFar
        const XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(math::ToRadians(fov), aspect, zFarPlane, zNearPlane);

        const XMVECTOR cameraEye = XMLoadFloat3(&pos);
        const XMVECTOR cameraDirection = XMLoadFloat3(&target);
        const XMVECTOR cameraUp = XMLoadFloat3(&up);
        const XMMATRIX viewMatrix = XMMatrixLookToLH(cameraEye, cameraDirection, cameraUp);
        
        XMStoreFloat4x4(&view, viewMatrix);
        XMStoreFloat4x4(&projection, projectionMatrix);
        
        XMStoreFloat4x4(&inv_view, XMMatrixInverse(nullptr, viewMatrix));
        XMStoreFloat4x4(&inv_projection, XMMatrixInverse(nullptr, projectionMatrix));

        const XMMATRIX viewProjectionMatrix = XMMatrixMultiply(viewMatrix, projectionMatrix);
        XMStoreFloat4x4(&VP, viewProjectionMatrix);
        XMStoreFloat4x4(&inv_VP, XMMatrixInverse(nullptr, viewProjectionMatrix));

        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, viewMatrix);
        const XMMATRIX _Rot = XMMatrixRotationQuaternion(R);
        XMStoreFloat3x3(&rotation, _Rot);

        frustum = Frustum(viewProjectionMatrix);
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

    AnimationComponent::Channel::PathDataType AnimationComponent::Channel::GetPathDataType() const
    {
        switch (path)
        {
        case Path::Translation:
            return PathDataType::Float3;
        case Path::Rotation:
            return PathDataType::Float4;
        case Path::Scale:
            return PathDataType::Float3;
        default:
            assert(0);
            break;
        }

        return PathDataType::Float;
    }

    ecs::Entity Scene::CreateGroup(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity, name);
        transforms.Create(entity);
        groups.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateMaterial(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity, name);
        materials.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateMesh(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity, name);
        meshes.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateObject(const std::string& name)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity, name);
        transforms.Create(entity);
        objects.Create(entity);
        return entity;
    }

    ecs::Entity Scene::CreateLight(
        const std::string& name,
        const XMFLOAT3& position,
        const XMFLOAT3& color,
        float energy,
        float range,
        LightType type)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity, name);

        TransformComponent& transform = transforms.Create(entity);
        transform.Translate(position);
        transform.UpdateTransform();

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
        float fov)
    {
        ecs::Entity entity = ecs::CreateEntity();
        names.Create(entity, name);
        transforms.Create(entity);
        cameras.Create(entity, aspect, nearPlane, farPlane, fov);
        return entity;
    }

    void Scene::ComponentAttach(ecs::Entity entity, ecs::Entity parentEntity)
    {
        assert(entity != parentEntity);

        if (hierarchy.Contains(entity))
            ComponentDetach(entity);

        HierarchyComponent& component = hierarchy.Create(entity);
        component.parentID = parentEntity;

        const TransformComponent* parent = transforms.GetComponent(parentEntity);
        TransformComponent* child = transforms.GetComponent(entity);
        if (parent != nullptr && child != nullptr)
        {
            // move child to parent's local space
            XMMATRIX B = XMMatrixInverse(nullptr, XMLoadFloat4x4(&parent->world));
            child->MatrixTransform(B);
            child->UpdateTransform();

            child->UpdateTransformParented(*parent);
        }
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
        this->dt = dt;
        this->time += dt;

        jobsystem::Context ctx;

        // update systems with no dependency
        RunAnimationUpdateSystem(ctx);      // waits on ctx before returning
        RunTransformUpdateSystem(ctx);
        RunWeatherUpdateSystem(ctx);

        // update systems that only depends on local transform
        jobsystem::Wait(ctx);
        RunHierarchyUpdateSystem(ctx);
        RunMeshUpdateSystem(ctx);

        // update systems that is dependent on world transform
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
        lights.Clear();
        cameras.Clear();
        weathers.Clear();

        aabb_objects.clear();
        aabb_lights.clear();
    }

    void Scene::Merge(Scene& other)
    {
        names.Merge(other.names);
        transforms.Merge(other.transforms);
        groups.Merge(other.groups);
        hierarchy.Merge(other.hierarchy);
        materials.Merge(other.materials);
        meshes.Merge(other.meshes);
        objects.Merge(other.objects);
        lights.Merge(other.lights);
        cameras.Merge(other.cameras);
        weathers.Merge(other.weathers);

        aabb_objects.insert(aabb_objects.end(), other.aabb_objects.begin(), other.aabb_objects.end());
        aabb_lights.insert(aabb_lights.end(), other.aabb_lights.begin(), other.aabb_lights.end());
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
        lights.Remove(entity);
        cameras.Remove(entity);
        weathers.Remove(entity);
    }

    void Scene::Serialize(Serializer& ser)
    {
        constexpr uint64_t LEAST_SUPPORTED_VERSION = 4;
        ecs::SceneSerializeContext context;

        context.archiveVersion = ser.GetVersion();
        if (context.archiveVersion < LEAST_SUPPORTED_VERSION)
        {
            CYB_ERROR("Unsupported archive version (version={} LeastUupportedVersion={})", context.archiveVersion, LEAST_SUPPORTED_VERSION);
            return;
        }
        if (context.archiveVersion > ARCHIVE_VERSION)
        {
            CYB_ERROR("Corrupt archive version (version={})", context.archiveVersion);
            return;
        }
        CYB_CWARNING(context.archiveVersion < ARCHIVE_VERSION, "Old (but supported) archive version (version={} currentVersion={})", context.archiveVersion, ARCHIVE_VERSION);

        if (ser.IsReading())
            Clear();

        names.Serialize(ser, context);
        transforms.Serialize(ser, context);
        groups.Serialize(ser, context);
        hierarchy.Serialize(ser, context);
        materials.Serialize(ser, context);
        meshes.Serialize(ser, context);
        objects.Serialize(ser, context);
        lights.Serialize(ser, context);
        cameras.Serialize(ser, context);
        weathers.Serialize(ser, context);
    }

    void Scene::RunTransformUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)transforms.Size(), r_sceneSubtaskGroupsize.GetValue<uint32_t>(), [&] (jobsystem::JobArgs args) {
            TransformComponent& transform = transforms[args.jobIndex];
            transform.UpdateTransform();
        });
    }

    void Scene::RunHierarchyUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)hierarchy.Size(), r_sceneSubtaskGroupsize.GetValue<uint32_t>(), [&] (jobsystem::JobArgs args) {
            const HierarchyComponent& hier = hierarchy[args.jobIndex];
            ecs::Entity entity = hierarchy.GetEntity(args.jobIndex);
            TransformComponent* transform = transforms.GetComponent(entity);

            XMMATRIX worldMatrix = {};
            if (transform != nullptr)
                worldMatrix = transform->GetLocalMatrix();

            ecs::Entity parentID = hier.parentID;
            while (parentID != ecs::INVALID_ENTITY)
            {
                const TransformComponent* transformParent = transforms.GetComponent(parentID);

                if (transform != nullptr && transformParent != nullptr)
                    worldMatrix *= transformParent->GetLocalMatrix();

                const HierarchyComponent* hierRecursive = hierarchy.GetComponent(parentID);
                if (hierRecursive != nullptr)
                    parentID = hierRecursive->parentID;
                else
                    parentID = ecs::INVALID_ENTITY;
            }

            if (transform != nullptr)
                XMStoreFloat4x4(&transform->world, worldMatrix);
        });
    }

    void Scene::RunMeshUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)meshes.Size(), r_sceneSubtaskGroupsize.GetValue<uint32_t>(), [&] (jobsystem::JobArgs args) {
            ecs::Entity entity = meshes.GetEntity(args.jobIndex);
            MeshComponent& mesh = meshes[args.jobIndex];

            for (auto& subset : mesh.subsets)
            {
                const MaterialComponent* material = materials.GetComponent(subset.materialID);
                if (material != nullptr)
                {
                    subset.materialIndex = (uint32_t)materials.GetIndex(subset.materialID);
                }
                else
                {
                    subset.materialIndex = 0;
                }
            }
        });
    }

    void Scene::RunObjectUpdateSystem(jobsystem::Context& ctx)
    {
        aabb_objects.resize(objects.Size());

        jobsystem::Dispatch(ctx, (uint32_t)objects.Size(), r_sceneSubtaskGroupsize.GetValue<uint32_t>(), [&] (jobsystem::JobArgs args) {
            ecs::Entity entity = objects.GetEntity(args.jobIndex);
            ObjectComponent& object = objects[args.jobIndex];

            if (object.meshID != ecs::INVALID_ENTITY && meshes.Contains(object.meshID) && transforms.Contains(entity))
            {
                object.meshIndex = (uint32_t)meshes.GetIndex(object.meshID);
                const MeshComponent& mesh = meshes[object.meshIndex];

                object.transformIndex = (int32_t)transforms.GetIndex(entity);
                const TransformComponent& transform = transforms[object.transformIndex];

                AxisAlignedBox& aabb = aabb_objects[args.jobIndex];
                aabb = mesh.aabb.Transform(XMLoadFloat4x4(&transform.world));
            }
        });
    }

    void Scene::RunLightUpdateSystem(jobsystem::Context& ctx)
    {
        aabb_lights.resize(lights.Size());

        jobsystem::Dispatch(ctx, (uint32_t)lights.Size(), r_sceneSubtaskGroupsize.GetValue<uint32_t>(), [&] (jobsystem::JobArgs args) {
            LightComponent& light = lights[args.jobIndex];
            const ecs::Entity entity = lights.GetEntity(args.jobIndex);
            if (!transforms.Contains(entity))
                return;
            const TransformComponent& transform = *transforms.GetComponent(entity);
            AxisAlignedBox& aabb = aabb_lights[args.jobIndex];

            XMMATRIX W = XMLoadFloat4x4(&transform.world);
            XMVECTOR S, R, T;
            XMMatrixDecompose(&S, &R, &T, W);
            XMStoreFloat3(&light.position, T);

            switch (light.type)
            {
            case LightType::Point:
                aabb.SetFromSphere(light.position, light.range * 0.5f);
                break;
            case LightType::Directional:
            default:
                break;
            }
        });
    }

    void Scene::RunCameraUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)cameras.Size(), r_sceneSubtaskGroupsize.GetValue<uint32_t>(), [&] (jobsystem::JobArgs args) {
            CameraComponent& camera = cameras[args.jobIndex];
            camera.UpdateCamera();
        });
    }

    void Scene::RunAnimationUpdateSystem(jobsystem::Context& ctx)
    {
        CYB_PROFILE_CPU_SCOPE("Animation");

        jobsystem::Dispatch(ctx, (uint32_t)animations.Size(), r_sceneSubtaskGroupsize.GetValue<uint32_t>(), [&] (jobsystem::JobArgs args) {
            AnimationComponent& animation = animations[args.jobIndex];
            if (!animation.IsPlaying())
                return;

            animation.lastUpdateTime = animation.timer;

            for (const AnimationComponent::Channel& channel : animation.channels)
            {
                assert(channel.samplerIndex < (int)animation.samplers.size());
                const AnimationComponent::Sampler& sampler = animation.samplers[channel.samplerIndex];
                if (sampler.keyframeTimes.empty())
                    continue;

                const AnimationComponent::Channel::PathDataType pathDataType = channel.GetPathDataType();

                float timeFirst = std::numeric_limits<float>::max();
                float timeLast = std::numeric_limits<float>::min();

                float timeLeft = std::numeric_limits<float>::min();
                float timeRight = std::numeric_limits<float>::max();
                int keyLeft = 0, keyRight = 0;

                // search for usable keyframes
                for (int k = 0; k < (int)sampler.keyframeTimes.size(); ++k)
                {
                    const float time = sampler.keyframeTimes[k];

                    timeFirst = std::min(timeFirst, time);
                    timeLast = std::max(timeLast, time);

                    if (time <= animation.timer && time > timeLeft)
                    {
                        timeLeft = time;
                        keyLeft = k;
                    }
                    if (time >= animation.timer && time < timeRight)
                    {
                        timeRight = time;
                        keyRight = k;
                    }
                }
                timeLeft = std::max(timeLeft, timeFirst);
                timeRight = std::max(timeRight, timeLast);

                const float left = sampler.keyframeTimes[keyLeft];
                const float right = sampler.keyframeTimes[keyRight];

                union Interpolator
                {
                    XMFLOAT4 f4;
                    XMFLOAT3 f3;
                    XMFLOAT2 f2;
                    float f;
                } interpolator = {};

                TransformComponent* targetTransform = nullptr;
                if (channel.path == AnimationComponent::Channel::Path::Translation||
                    channel.path == AnimationComponent::Channel::Path::Rotation ||
                    channel.path == AnimationComponent::Channel::Path::Scale)
                {
                    targetTransform = transforms.GetComponent(channel.target);
                    if (targetTransform == nullptr)
                        continue;

                    switch (channel.path)
                    {
                    case AnimationComponent::Channel::Path::Translation:
                        interpolator.f3 = targetTransform->translation_local;
                        break;
                    case AnimationComponent::Channel::Path::Rotation:
                        interpolator.f4 = targetTransform->rotation_local;
                        break;
                    case AnimationComponent::Channel::Path::Scale:
                        interpolator.f3 = targetTransform->scale_local;
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    assert(0);
                    continue;
                }

                // path data interpolation
                switch (sampler.mode)
                {
                case AnimationComponent::Sampler::Mode::Linear:
                {
                    // Linear interpolation method:
                    float t = 0.0f;
                    if (keyLeft != keyRight)
                        t = math::Saturate(animation.timer - left) / (right - left);

                    switch (pathDataType)
                    {
                    case AnimationComponent::Channel::PathDataType::Float3:
                    {
                        assert(sampler.keyframeData.size() == sampler.keyframeTimes.size() * 3);
                        const XMFLOAT3* data = (const XMFLOAT3*)sampler.keyframeData.data();
                        XMVECTOR vLeft = XMLoadFloat3(&data[keyLeft]);
                        XMVECTOR vRight = XMLoadFloat3(&data[keyRight]);
                        XMVECTOR vAnim = XMVectorLerp(vLeft, vRight, t);
                        XMStoreFloat3(&interpolator.f3, vAnim);
                    } break;
                    case AnimationComponent::Channel::PathDataType::Float4:
                    {
                        assert(sampler.keyframeData.size() == sampler.keyframeTimes.size() * 4);
                        const XMFLOAT4* data = (const XMFLOAT4*)sampler.keyframeData.data();
                        XMVECTOR vLeft = XMLoadFloat4(&data[keyLeft]);
                        XMVECTOR vRight = XMLoadFloat4(&data[keyRight]);
                        XMVECTOR vAnim;
                        if (channel.path == AnimationComponent::Channel::Path::Rotation)
                        {
                            vAnim = XMQuaternionSlerp(vLeft, vRight, t);
                            vAnim = XMQuaternionNormalize(vAnim);
                        }
                        else
                        {
                            vAnim = XMVectorLerp(vLeft, vRight, t);
                        }
                        XMStoreFloat4(&interpolator.f4, vAnim);
                    } break;
                    default:
                        assert(0);
                        break;
                    }
                } break;
                default:
                    assert(0);
                    break;
                }

                // The interpolated raw values will be blended on top of component values:
                const float t = animation.blendAmount;

                if (targetTransform != nullptr)
                {
                    targetTransform->SetDirty();

                    switch (channel.path)
                    {
                    case AnimationComponent::Channel::Path::Translation:
                    {
                        const XMVECTOR aT = XMLoadFloat3(&targetTransform->translation_local);
                        const XMVECTOR bT = XMLoadFloat3(&interpolator.f3);
                        const XMVECTOR T = XMVectorLerp(aT, bT, t);
                        XMStoreFloat3(&targetTransform->translation_local, T);
                    } break;
                    case AnimationComponent::Channel::Path::Rotation:
                    {
                        const XMVECTOR aR = XMLoadFloat4(&targetTransform->rotation_local);
                        const XMVECTOR bR = XMLoadFloat4(&interpolator.f4);
                        const XMVECTOR R = XMQuaternionSlerp(aR, bR, t);
                        XMStoreFloat4(&targetTransform->rotation_local, R);
                    } break;
                    case AnimationComponent::Channel::Path::Scale:
                    {
                        const XMVECTOR aS = XMLoadFloat3(&targetTransform->scale_local);
                        const XMVECTOR bS = XMLoadFloat3(&interpolator.f3);
                        const XMVECTOR S = XMVectorLerp(aS, bS, t);
                        XMStoreFloat3(&targetTransform->scale_local, S);
                    } break;
                    }
                }
            }

            const bool forward = animation.speed > 0;
            const bool timerBeyondEnd = animation.timer > animation.end;
            const bool timerBeforeStart = animation.timer < animation.start;
            if ((forward && timerBeyondEnd) || (!forward && timerBeforeStart))
            {
                if (animation.IsLooped())
                {
                    animation.timer = forward ? animation.start : animation.end;
                }
                else if (animation.IsPingPong())
                {
                    animation.timer = forward ? animation.end : animation.start;
                    animation.speed = -animation.speed;
                }
                else
                {
                    animation.timer = forward ? animation.end : animation.start;
                    animation.Pause();
                }
            }

            if (animation.IsPlaying())
                animation.timer += dt * animation.speed;
        });

        jobsystem::Wait(ctx);
    }

    void Scene::RunWeatherUpdateSystem(jobsystem::Context& /* ctx */)
    {
        if (weathers.Size() > 0)
            weather = weathers[0];
    }

    PickResult Pick(const Scene& scene, const Ray& ray)
    {
        CYB_TIMED_FUNCTION();

        PickResult result;
        const XMVECTOR ray_origin = ray.GetOrigin();
        const XMVECTOR ray_direction = XMVector3Normalize(ray.GetDirection());

        for (size_t objectIndex = 0; objectIndex < scene.objects.Size(); ++objectIndex)
        {
            const AxisAlignedBox& aabb = scene.aabb_objects[objectIndex];

            if (!ray.IntersectsBoundingBox(aabb))
                continue;

            const ObjectComponent& object = scene.objects[objectIndex];
            if (object.meshID == ecs::INVALID_ENTITY)
                continue;

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
                            result.entity = scene.objects.GetEntity(objectIndex);
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
    void SerializeComponent(scene::NameComponent& x, Serializer& ser, SceneSerializeContext& context)
    {
        ser.Serialize(x.name);
    }

    void SerializeComponent(scene::TransformComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        ser.Serialize((uint32_t&)x.flags);
        ser.Serialize(x.scale_local);
        ser.Serialize(x.rotation_local);
        ser.Serialize(x.translation_local);

        if (ser.IsReading())
        {
            jobsystem::Execute(context.ctx, [&x] (jobsystem::JobArgs) {
                x.SetDirty();
                x.UpdateTransform();
            });
        }
    }

    void SerializeComponent(scene::GroupComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
    }

    void SerializeComponent(scene::HierarchyComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        ecs::SerializeEntity(x.parentID, ser, context);
    }

    void SerializeComponent(scene::MaterialComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        ser.Serialize((uint32_t&)x.flags);
        ser.Serialize((uint32_t&)x.shaderType);
        ser.Serialize(x.baseColor);
        ser.Serialize(x.roughness);
        ser.Serialize(x.metalness);
    }

    void SerializeComponent(scene::MeshComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        size_t subsetCount = x.subsets.size();
        ser.Serialize(subsetCount);
        x.subsets.resize(subsetCount);
        for (size_t i = 0; i < subsetCount; ++i)
        {
            ecs::SerializeEntity(x.subsets[i].materialID, ser, context);
            ser.Serialize(x.subsets[i].indexOffset);
            ser.Serialize(x.subsets[i].indexCount);
        }

        ser.Serialize(x.vertex_positions);
        ser.Serialize(x.vertex_normals);
        ser.Serialize(x.vertex_colors);
        ser.Serialize(x.indices);

        if (ser.IsReading())
        {
            jobsystem::Execute(context.ctx, [&x] (jobsystem::JobArgs) { x.CreateRenderData(); });
        }
    }

    void SerializeComponent(scene::ObjectComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        ser.Serialize((uint32_t&)x.flags);
        ecs::SerializeEntity(x.meshID, ser, context);
    }

    void SerializeComponent(scene::LightComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        ser.Serialize((uint32_t&)x.flags);
        ser.Serialize(x.color);
        ser.Serialize((uint32_t&)x.type);
        ser.Serialize(x.energy);
        ser.Serialize(x.range);
    }

    void SerializeComponent(scene::CameraComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        ser.Serialize(x.aspect);
        ser.Serialize(x.zNearPlane);
        ser.Serialize(x.zFarPlane);
        ser.Serialize(x.fov);
        ser.Serialize(x.pos);
        ser.Serialize(x.target);
        ser.Serialize(x.up);
    }

    void SerializeComponent(scene::WeatherComponent& x, Serializer& ser, ecs::SceneSerializeContext& context)
    {
        ser.Serialize(x.horizon);
        ser.Serialize(x.zenith);
    }
}