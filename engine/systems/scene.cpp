#include <variant>
#include "core/cvar.h"
#include "core/logger.h"
#include "systems/profiler.h"
#include "systems/scene.h"

using namespace cyb::rhi;

namespace cyb::scene
{
    CVar<uint32_t> r_sceneSubtaskGroupsize("r_sceneSubtaskGroupsize", 64, CVarFlag::RendererBit, "Groupsize for multithreaded scene update tasks");

    void TransformComponent::SetDirty(bool value)
    {
        SetFlag(flags, Flags::DirtyBit, value);
    }

    bool TransformComponent::IsDirty() const
    {
        return HasFlag(flags, Flags::DirtyBit);
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
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, world);
        XMStoreFloat3(&scale_local, S);
        XMStoreFloat4(&rotation_local, R);
        XMStoreFloat3(&translation_local, T);

        SetDirty(true);
    }

    void TransformComponent::MatrixTransform(const XMMATRIX& matrix)
    {
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, GetLocalMatrix() * matrix);
        XMStoreFloat3(&scale_local, S);
        XMStoreFloat4(&rotation_local, R);
        XMStoreFloat3(&translation_local, T);

        SetDirty(true);
    }

    void TransformComponent::UpdateTransform()
    {
        if (!IsDirty())
            return;

        world = GetLocalMatrix();
        SetDirty(false);
    }

    void TransformComponent::UpdateTransformParented(const TransformComponent& parent)
    {
        world = GetLocalMatrix() * parent.world;
    }

    void TransformComponent::Translate(const XMFLOAT3& value)
    {
        translation_local.x += value.x;
        translation_local.y += value.y;
        translation_local.z += value.z;
        SetDirty();
    }

    void TransformComponent::RotateRollPitchYaw(const XMFLOAT3& value)
    {
        XMVECTOR quat = XMLoadFloat4(&rotation_local);
        XMVECTOR x = XMQuaternionRotationRollPitchYaw(value.x, 0, 0);
        XMVECTOR y = XMQuaternionRotationRollPitchYaw(0, value.y, 0);
        XMVECTOR z = XMQuaternionRotationRollPitchYaw(0, 0, value.z);

        quat = XMQuaternionMultiply(x, quat);
        quat = XMQuaternionMultiply(quat, y);
        quat = XMQuaternionMultiply(z, quat);
        quat = XMQuaternionNormalize(quat);

        XMStoreFloat4(&rotation_local, quat);
        SetDirty();
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
            desc.usage = rhi::BufferUsage::IndexBufferBit;

            bool result = device->CreateBuffer(&desc, indices.data(), &index_buffer);
            assert(result == true);
        }

        aabb.Invalidate();

        // vertex_buffer_pos - POSITION + NORMAL
        {
            std::vector<Vertex_Pos> vertices(vertex_positions.size());
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                const XMFLOAT3& pos = vertex_positions[i];
                XMFLOAT3 nor = vertex_normals.empty() ? XMFLOAT3(1, 1, 1) : vertex_normals[i];
                XMStoreFloat3(&nor, XMVector3Normalize(XMLoadFloat3(&nor)));
                vertices[i].Set(pos, nor);

                aabb.GrowPoint(pos);
            }

            rhi::GPUBufferDesc desc;
            desc.size   = uint32_t(sizeof(Vertex_Pos) * vertices.size());
            desc.usage  = rhi::BufferUsage::VertexBufferBit;
            bool result = device->CreateBuffer(&desc, vertices.data(), &vertex_buffer_pos);
            assert(result == true);
        }

        // vertex_buffer_col - COLOR
        if (!vertex_colors.empty())
        {
            rhi::GPUBufferDesc desc;
            desc.size = uint32_t(sizeof(uint32_t) * vertex_colors.size());
            desc.usage = rhi::BufferUsage::VertexBufferBit;
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
        projection = XMMatrixPerspectiveFovLH(ToRadians(fov), aspect, zFarPlane, zNearPlane);

        const XMVECTOR camPos = XMLoadFloat3(&pos);
        const XMVECTOR camDir = XMLoadFloat3(&target);
        const XMVECTOR camUp  = XMLoadFloat3(&up);
        view = XMMatrixLookToLH(camPos, camDir, camUp);
        VP = XMMatrixMultiply(view, projection);

        invProjection = XMMatrixInverse(nullptr, projection);
        invView       = XMMatrixInverse(nullptr, view);
        invVP         = XMMatrixInverse(nullptr, VP);

        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, view);
        invRotation = XMMatrixInverse(nullptr, XMMatrixRotationQuaternion(R));

        frustum = Frustum{ VP };
    }

    void CameraComponent::TransformCamera(const TransformComponent& transform)
    {
        XMVECTOR S, R, T;
        XMMatrixDecompose(&S, &R, &T, transform.world);

        const XMMATRIX rotation = XMMatrixRotationQuaternion(R);
        const XMVECTOR camPos = T;
        const XMVECTOR camDir = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotation);
        const XMVECTOR camUp  = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotation);
        
        XMStoreFloat3(&pos, camPos);
        XMStoreFloat3(&target, camDir);
        XMStoreFloat3(&up, camUp);
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
            XMMATRIX B = XMMatrixInverse(nullptr, parent->world);
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

    uint32_t Scene::GetMeshUseCount(ecs::Entity meshID) const
    {
        if (meshID == ecs::INVALID_ENTITY || !meshes.Contains(meshID))
            return 0;

        uint32_t useCount{ 0 };
        for (size_t i = 0; i < objects.Size(); ++i)
        {
            if (objects[i].meshID == meshID)
                useCount++;
        }

        return useCount;
    }

    uint32_t Scene::GetMaterialUseCount(ecs::Entity materialID) const
    {
        if (materialID == ecs::INVALID_ENTITY || !materials.Contains(materialID))
            return 0;

        uint32_t useCount{ 0 };
        for (size_t i = 0; i < meshes.Size(); ++i)
        {
            for (auto& subset : meshes[i].subsets)
            {
                if (subset.materialID == materialID)
                    useCount++;
            }
        }

        return useCount;
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
        jobsystem::Wait(ctx);

        // update systems that only depends on local transform
        RunHierarchyUpdateSystem(ctx);
        RunMeshUpdateSystem(ctx);
        jobsystem::Wait(ctx);

        // update systems that is dependent on world transform
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
        animations.Clear();
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
        animations.Merge(other.animations);
        weathers.Merge(other.weathers);

        aabb_objects.insert(aabb_objects.end(), other.aabb_objects.begin(), other.aabb_objects.end());
        aabb_lights.insert(aabb_lights.end(), other.aabb_lights.begin(), other.aabb_lights.end());
    }

    void Scene::RemoveEntity(ecs::Entity entity, bool recursive, bool removeLinkedEntities)
    {
        // Create a list of all the child entities.
        std::vector<ecs::Entity> childList;
        for (size_t i = 0; i < hierarchy.Size(); ++i)
        {
            const HierarchyComponent& hier = hierarchy[i];
            if (hier.parentID == entity)
            {
                ecs::Entity child = hierarchy.GetEntity(i);
                childList.push_back(child);
            }
        }

        // Remove all linked entities
        if (removeLinkedEntities)
        {
            std::vector<ecs::Entity> deleteList;
            const scene::ObjectComponent* object = objects.GetComponent(entity);
            if (object != nullptr && GetMeshUseCount(object->meshID) == 1)
                deleteList.push_back(object->meshID);

            const scene::MeshComponent* mesh = meshes.GetComponent(entity);
            if (mesh != nullptr)
            {
                for (const auto& subset : mesh->subsets)
                {
                    if (GetMaterialUseCount(subset.materialID) == 1)
                        deleteList.push_back(subset.materialID);
                }
            }

            for (auto ent : deleteList)
                RemoveEntity(ent, recursive, removeLinkedEntities);
        }


        // If we're deleting recursivly, remove all children, else
        // we just detach them so we don't leave them parentless.
        for (auto& child : childList)
        {
            if (recursive)
                RemoveEntity(child, true, removeLinkedEntities);
            else
                ComponentDetach(child);
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
        animations.Remove(entity);
        weathers.Remove(entity);
    }

    void Scene::RemoveUnusedEntities()
    {
        // Remove unused meshes.
        for (size_t i = 0; i < meshes.Size(); ++i)
        {
            ecs::Entity meshID = meshes.GetEntity(i);
            if (GetMeshUseCount(meshID) == 0)
                RemoveEntity(meshID, false, false);
        }

        // Remove unused materials.
        for (size_t i = 0; i < materials.Size(); ++i)
        {
            ecs::Entity materialID = materials.GetEntity(i);
            if (GetMaterialUseCount(materialID) == 0)
                RemoveEntity(materialID, false, false);
        }
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
        jobsystem::Dispatch(ctx, (uint32_t)transforms.Size(), r_sceneSubtaskGroupsize.GetValue(), [&] (jobsystem::JobArgs args) {
            TransformComponent& transform = transforms[args.jobIndex];
            transform.UpdateTransform();
        });
    }

    void Scene::RunHierarchyUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)hierarchy.Size(), r_sceneSubtaskGroupsize.GetValue(), [&] (jobsystem::JobArgs args) {
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
                transform->world = worldMatrix;
        });
    }

    void Scene::RunMeshUpdateSystem(jobsystem::Context& ctx)
    {
        jobsystem::Dispatch(ctx, (uint32_t)meshes.Size(), r_sceneSubtaskGroupsize.GetValue(), [&] (jobsystem::JobArgs args) {
            ecs::Entity entity = meshes.GetEntity(args.jobIndex);
            MeshComponent& mesh = meshes[args.jobIndex];

            for (auto& subset : mesh.subsets)
            {
                if (subset.materialID != ecs::INVALID_ENTITY && materials.Contains(subset.materialID))
                    subset.materialIndex = (uint32_t)materials.GetIndex(subset.materialID);
                else
                    subset.materialIndex = 0;
            }
        });
    }

    void Scene::RunObjectUpdateSystem(jobsystem::Context& ctx)
    {
        aabb_objects.resize(objects.Size());

        jobsystem::Dispatch(ctx, (uint32_t)objects.Size(), r_sceneSubtaskGroupsize.GetValue(), [&] (jobsystem::JobArgs args) {
            ecs::Entity entity = objects.GetEntity(args.jobIndex);
            ObjectComponent& object = objects[args.jobIndex];

            if (object.meshID != ecs::INVALID_ENTITY && meshes.Contains(object.meshID) && transforms.Contains(entity))
            {
                object.meshIndex = (uint32_t)meshes.GetIndex(object.meshID);
                const MeshComponent& mesh = meshes[object.meshIndex];

                object.transformIndex = (int32_t)transforms.GetIndex(entity);
                const TransformComponent& transform = transforms[object.transformIndex];

                AxisAlignedBox& aabb = aabb_objects[args.jobIndex];
                aabb = mesh.aabb.Transform(transform.world);
            }
        });
    }

    void Scene::RunLightUpdateSystem(jobsystem::Context& ctx)
    {
        aabb_lights.resize(lights.Size());

        jobsystem::Dispatch(ctx, (uint32_t)lights.Size(), r_sceneSubtaskGroupsize.GetValue(), [&] (jobsystem::JobArgs args) {
            LightComponent& light = lights[args.jobIndex];
            const ecs::Entity entity = lights.GetEntity(args.jobIndex);
            if (!transforms.Contains(entity))
                return;
            const TransformComponent& transform = *transforms.GetComponent(entity);
            AxisAlignedBox& aabb = aabb_lights[args.jobIndex];

            XMVECTOR S, R, T;
            XMMatrixDecompose(&S, &R, &T, transform.world);
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
        jobsystem::Dispatch(ctx, (uint32_t)cameras.Size(), r_sceneSubtaskGroupsize.GetValue(), [&] (jobsystem::JobArgs args) {
            CameraComponent& camera = cameras[args.jobIndex];
            camera.UpdateCamera();
        });
    }

    void Scene::RunAnimationUpdateSystem(jobsystem::Context& ctx)
    {
        CYB_PROFILE_CPU_SCOPE("Animation");

        jobsystem::Dispatch(ctx, (uint32_t)animations.Size(), r_sceneSubtaskGroupsize.GetValue(), [&] (jobsystem::JobArgs args) {
            AnimationComponent& animation = animations[args.jobIndex];
            if (!animation.IsPlaying())
                return;

            animation.lastUpdateTime = animation.timer;

            for (const AnimationComponent::Channel& channel : animation.channels)
            {
                assert(channel.samplerIndex < (int)animation.samplers.size());
                AnimationComponent::Sampler& sampler = animation.samplers[channel.samplerIndex];
                if (sampler.keyframeTimes.empty())
                    continue;

                const auto keyframe = std::lower_bound(sampler.keyframeTimes.begin(), sampler.keyframeTimes.end(), animation.timer);
                if (keyframe == sampler.keyframeTimes.begin() ||
                    keyframe == sampler.keyframeTimes.end())
                    continue;
                const auto rightKeyIndex = keyframe - sampler.keyframeTimes.begin();
                const auto leftKeyIndex = rightKeyIndex - 1;

                using Interpolator = std::variant<float, XMFLOAT3, XMFLOAT4>;
                Interpolator interpolator = {};

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
                        interpolator = targetTransform->translation_local;
                        break;
                    case AnimationComponent::Channel::Path::Rotation:
                        interpolator = targetTransform->rotation_local;
                        break;
                    case AnimationComponent::Channel::Path::Scale:
                        interpolator = targetTransform->scale_local;
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

                const float td = (sampler.keyframeTimes[rightKeyIndex] - sampler.keyframeTimes[leftKeyIndex]);
                const float t = (animation.timer - sampler.keyframeTimes[leftKeyIndex]) / std::max(td, FLT_EPSILON);

                // path data interpolation
                switch (sampler.mode)
                {
                case AnimationComponent::Sampler::Mode::Linear:
                    // Linear interpolation method:
                    std::visit([&] (auto& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, XMFLOAT3>)
                        {
                            assert(sampler.keyframeData.size() == sampler.keyframeTimes.size() * 3);
                            const XMFLOAT3* data = (const XMFLOAT3*)sampler.keyframeData.data();
                            XMVECTOR vLeft = XMLoadFloat3(&data[leftKeyIndex]);
                            XMVECTOR vRight = XMLoadFloat3(&data[rightKeyIndex]);
                            XMVECTOR vAnim = XMVectorLerp(vLeft, vRight, t);
                            XMStoreFloat3(&value, vAnim);
                        }
                        else if constexpr (std::is_same_v<T, XMFLOAT4>)
                        {
                            assert(sampler.keyframeData.size() == sampler.keyframeTimes.size() * 4);
                            const XMFLOAT4* data = (const XMFLOAT4*)sampler.keyframeData.data();
                            XMVECTOR vLeft = XMLoadFloat4(&data[leftKeyIndex]);
                            XMVECTOR vRight = XMLoadFloat4(&data[rightKeyIndex]);
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
                            XMStoreFloat4(&value, vAnim);
                        }
                    }, interpolator);
                    break;
                case AnimationComponent::Sampler::Mode::CubicSpline:
                    // Cubic spline interpolation method:
                    std::visit([&] (auto& value) {
                        const float t2 = t * t;
                        const float t3 = t2 * t;

                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, XMFLOAT3>)
                        {
                            assert(sampler.keyframeData.size() == sampler.keyframeTimes.size() * 3 * 3);
                            const XMFLOAT3* data = (const XMFLOAT3*)sampler.keyframeData.data();
                            XMVECTOR vLeft = XMLoadFloat3(&data[leftKeyIndex * 3 + 1]);
                            XMVECTOR vLeftTanOut = dt * XMLoadFloat3(&data[leftKeyIndex * 3 + 2]);
                            XMVECTOR vRightTanIn = dt * XMLoadFloat3(&data[rightKeyIndex * 3 + 0]);
                            XMVECTOR vRight = XMLoadFloat3(&data[rightKeyIndex * 3 + 1]);
                            XMVECTOR vAnim = (2 * t3 - 3 * t2 + 1) * vLeft + (t3 - 2 * t2 + t) * vLeftTanOut + (-2 * t3 + 3 * t2) * vRight + (t3 - t2) * vRightTanIn;
                            XMStoreFloat3(&value, vAnim);
                        }
                        else if constexpr (std::is_same_v<T, XMFLOAT4>)
                        {
                            assert(sampler.keyframeData.size() == sampler.keyframeTimes.size() * 4 * 3);
                            const XMFLOAT4* data = (const XMFLOAT4*)sampler.keyframeData.data();
                            XMVECTOR vLeft = XMLoadFloat4(&data[leftKeyIndex * 3 + 1]);
                            XMVECTOR vLeftTanOut = td * XMLoadFloat4(&data[leftKeyIndex * 3 + 2]);
                            XMVECTOR vRightTanIn = td * XMLoadFloat4(&data[rightKeyIndex * 3 + 0]);
                            XMVECTOR vRight = XMLoadFloat4(&data[rightKeyIndex * 3 + 1]);
                            XMVECTOR vAnim = (2 * t3 - 3 * t2 + 1) * vLeft + (t3 - 2 * t2 + t) * vLeftTanOut + (-2 * t3 + 3 * t2) * vRight + (t3 - t2) * vRightTanIn;
                            if (channel.path == AnimationComponent::Channel::Path::Rotation)
                            {
                                vAnim = XMQuaternionNormalize(vAnim);
                            }
                            XMStoreFloat4(&value, vAnim);
                        }
                    }, interpolator);
                    break;
                default:
                    assert(0);
                    break;
                }

                if (targetTransform != nullptr)
                {
                    targetTransform->SetDirty();

                    switch (channel.path)
                    {
                    case AnimationComponent::Channel::Path::Translation:
                    {
                        const XMVECTOR aT = XMLoadFloat3(&targetTransform->translation_local);
                        const XMVECTOR bT = XMLoadFloat3(&std::get<XMFLOAT3>(interpolator));
                        const XMVECTOR T = XMVectorLerp(aT, bT, animation.blendAmount);
                        XMStoreFloat3(&targetTransform->translation_local, T);
                    } break;
                    case AnimationComponent::Channel::Path::Rotation:
                    {
                        const XMVECTOR aR = XMLoadFloat4(&targetTransform->rotation_local);
                        const XMVECTOR bR = XMLoadFloat4(&std::get<XMFLOAT4>(interpolator));
                        const XMVECTOR R = XMQuaternionSlerp(aR, bR, animation.blendAmount);
                        XMStoreFloat4(&targetTransform->rotation_local, R);
                    } break;
                    case AnimationComponent::Channel::Path::Scale:
                    {
                        const XMVECTOR aS = XMLoadFloat3(&targetTransform->scale_local);
                        const XMVECTOR bS = XMLoadFloat3(&std::get<XMFLOAT3>(interpolator));
                        const XMVECTOR S = XMVectorLerp(aS, bS, animation.blendAmount);
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
            const XMMATRIX object_matrix = object.transformIndex >= 0 ? scene.transforms[object.transformIndex].world : XMMatrixIdentity();
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
                    if (RayTriangleIntersects(ray_origin_local, ray_direction_local, p0, p1, p2, distance, bary))
                    {
                        const XMVECTOR pos = XMVector3Transform(XMVectorAdd(ray_origin_local, ray_direction_local * distance), object_matrix);
                        distance = Distance(pos, ray_origin);

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