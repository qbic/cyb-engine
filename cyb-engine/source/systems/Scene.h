#pragma once
#include "core/serializer.h"
#include "core/intersect.h"
#include "core/enum-flags.h"
#include "systems/ecs.h"
#include "systems/job-system.h"
#include "graphics/renderer.h"

namespace cyb::scene
{
    struct NameComponent
    {
        std::string name;

        inline void operator=(const std::string& str) { name = str; }
    };

    struct TransformComponent
    {
        enum
        {
            kDirtyBit = (1 << 0),
        };

        uint32_t flags = kDirtyBit;
        XMFLOAT3 scale_local = XMFLOAT3(1, 1, 1);
        XMFLOAT4 rotation_local = XMFLOAT4(0, 0, 0, 1);     // quaternion rotation
        XMFLOAT3 translation_local = XMFLOAT3(0, 0, 0);

        // Non-serialized data
        XMFLOAT4X4 world = math::kIdentityMatrix;

        inline void SetDirty(bool value = true) { if (value) { flags |= kDirtyBit; } else { flags &= ~kDirtyBit; } }
        inline bool IsDirty() const { return flags & kDirtyBit; }

        XMFLOAT3 GetPosition() const;
        XMFLOAT4 GetRotation() const;
        XMFLOAT3 GetScale() const;
        XMVECTOR GetPositionV() const;
        XMVECTOR GetRotationV() const;
        XMVECTOR GetScaleV() const;

        XMMATRIX GetLocalMatrix() const;

        // Apply world matrix to local space, overwriting scale, rotation & translation
        void ApplyTransform();

        void MatrixTransform(const XMFLOAT4X4& matrix);
        void MatrixTransform(const XMMATRIX& matrix);
        
        void UpdateTransform();
        void UpdateTransformParented(const TransformComponent& parent);
        void Translate(const XMFLOAT3& value);
        void RotateRollPitchYaw(const XMFLOAT3& value);
    };

    struct GroupComponent
    {
    };

    struct HierarchyComponent
    {
        ecs::Entity parentID = ecs::kInvalidEntity;
    };

    struct MaterialComponent
    {
        enum
        {
            kDirtyBit = (1 << 0),
            kUseVertexColorsBit = (1 << 1)
        };

        enum Shadertype
        {
            kShadertype_BDRF,
            kShadertype_Unlit,
            kShadertype_Terrain,
            kShadertype_Count
        };

        uint32_t flags = 0;
        Shadertype shaderType = kShadertype_BDRF;
        XMFLOAT4 baseColor = XMFLOAT4(1, 1, 1, 1);
        float roughness = 0.2f;
        float metalness = 0.0f;

        inline void SetUseVertexColors(bool value) { if (value) { flags |= kUseVertexColorsBit; } else { flags &= ~kUseVertexColorsBit; } }
        inline bool IsUsingVertexColors() const { return flags & kUseVertexColorsBit; }
    };

    struct MeshComponent
    {
        std::vector<XMFLOAT3> vertex_positions;
        std::vector<XMFLOAT3> vertex_normals;
        std::vector<uint32_t> vertex_colors;
        std::vector<uint32_t> indices;

        struct MeshSubset
        {
            ecs::Entity materialID = ecs::kInvalidEntity;
            uint32_t indexOffset = 0;
            uint32_t indexCount = 0;
        };
        std::vector<MeshSubset> subsets;

        // Non-serialized data
        AxisAlignedBox aabb;
        graphics::GPUBuffer vertex_buffer_pos;
        graphics::GPUBuffer vertex_buffer_col;
        graphics::GPUBuffer index_buffer;
        graphics::GPUBuffer vertexBuffer;

        void Clear();                       // Clear vertex and index data. GPUBuffer's will be left untouched
        void CreateRenderData();
        void ComputeNormals();

        // Internal format for vertex_buffer_pos
        struct Vertex_Pos
        {
            static constexpr graphics::Format kFormat = graphics::Format::kR32G32B32A32_Float;
            XMFLOAT3 pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
            uint32_t normal = 0;

            void Set(const XMFLOAT3& _pos, const XMFLOAT3& _nor);
            void SetNormal(const XMFLOAT3& _nor);
            XMFLOAT3 GetNormal() const;
        };

        // Internal format for vertex_buffer_col
        struct Vertex_Col
        {
            static constexpr graphics::Format kFormat = graphics::Format::kR8G8B8A8_Unorm;
            uint32_t color = 0;
        };
    };

    struct ObjectComponent
    {
        enum
        {
            kRenderableBit = (1 << 0),
            kCastShadowBit = (1 << 1)
        };

        uint32_t flags = kRenderableBit | kCastShadowBit;
        ecs::Entity meshID = ecs::kInvalidEntity;

        // Non-serialized data
        int32_t transformIndex = -1;        // Only valid for a single frame
    };

    // NOTE: Theese need to be synced with the LIGHTSOURCE_TYPE_*  defines in Shader_Interop.h
    enum class LightType
    {
        kDirectional,
        kPoint
    };

    struct LightComponent
    {
        enum
        {
            kCastShadowsBit     = (1 << 0),
            kAffectsSceneBit    = (1 << 1),
            kDefaultFlags = kAffectsSceneBit
        };

        uint32_t flags = kDefaultFlags;
        XMFLOAT3 color = XMFLOAT3(1.0f, 1.0f, 1.0f);
        LightType type = LightType::kPoint;
        float energy = 1.0f;
        float range = 10.0f;

        // Non-serialized data
        AxisAlignedBox aabb;

        inline void SetffectingSceney(bool value) { if (value) { flags |= kAffectsSceneBit; } else { flags &= ~kAffectsSceneBit; } }
        inline bool IsAffectingScene() const { return flags & kAffectsSceneBit; }
        inline void SetType(LightType value) { type = value; }
        inline LightType GetType() const { return type; }
        void UpdateLight();
    };

    struct WeatherComponent
    {
        XMFLOAT3 horizon = XMFLOAT3(1, 1, 1);
        XMFLOAT3 zenith = XMFLOAT3(0, 0, 0);
        float fogStart = 100.0f;
        float fogEnd = 1000.0f;
        float fogHeight = 0.0f;
    };

    struct CameraComponent
    {
        float aspect = 1.0f;
        float zNearPlane = 0.001f;
        float zFarPlane = 800.0f;
        float fov = math::M_PI / 3.0f;

        XMFLOAT3 pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 target = XMFLOAT3(0.0f, 0.0f, 1.0f);
        XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);

        // Non-serialized data
        XMFLOAT3X3 rotation;
        XMFLOAT4X4 view, projection, VP;
        XMFLOAT4X4 inv_view, inv_projection, inv_VP;
        Frustum frustum;

        XMMATRIX GetViewProjection() const { return XMLoadFloat4x4(&VP); }
        void CreatePerspective(float newAspect, float newNear, float newFar, float newFOV = math::M_PI / 3.0f);
        void UpdateCamera();
        void TransformCamera(const TransformComponent& transform);
    };

    struct Scene
    {
        ecs::ComponentManager<NameComponent> names;
        ecs::ComponentManager<TransformComponent> transforms;
        ecs::ComponentManager<GroupComponent> groups;
        ecs::ComponentManager<HierarchyComponent> hierarchy;
        ecs::ComponentManager<MaterialComponent> materials;
        ecs::ComponentManager<MeshComponent> meshes;
        ecs::ComponentManager<ObjectComponent> objects;
        ecs::ComponentManager<AxisAlignedBox> aabb_objects;
        ecs::ComponentManager<LightComponent> lights;
        ecs::ComponentManager<AxisAlignedBox> aabb_lights;
        ecs::ComponentManager<CameraComponent> cameras;
        ecs::ComponentManager<WeatherComponent> weathers;

        WeatherComponent active_weather;   // weathers[0] copy

        void Update(float dt);
        void Clear();
        void merge(Scene& other);
        void RemoveEntity(ecs::Entity entity);

        ecs::Entity CreateGroup(const std::string& name);
        ecs::Entity CreateMaterial(const std::string& name);
        ecs::Entity CreateMesh(const std::string& name);
        ecs::Entity CreateObject(const std::string& name);
        ecs::Entity CreateLight(
            const std::string& name,
            const XMFLOAT3& position,
            const XMFLOAT3& color,
            float energy,
            float range,
            LightType type = LightType::kPoint
        );
        ecs::Entity CreateCamera(
            const std::string& name,
            float aspect,
            float nearPlane = 0.01f,
            float farPlane = 1000.0f,
            float fov = math::M_PIDIV4
        );

        ecs::Entity FindMaterial(const std::string& search_value);

        // Hierarchy functions
        void ComponentAttach(ecs::Entity entity, ecs::Entity parent);
        void ComponentDetach(ecs::Entity entity);

        void Serialize(serializer::Archive& ar);

        void RunTransformUpdateSystem(jobsystem::Context& ctx);
        void RunHierarchyUpdateSystem();
        void RunObjectUpdateSystem(jobsystem::Context& ctx);
        void RunLightUpdateSystem(jobsystem::Context& ctx);
        void RunCameraUpdateSystem(jobsystem::Context& ctx);
        void RunWeatherUpdateSystem(jobsystem::Context& ctx);
    };

    // Getter to the global scene
    inline Scene& GetScene() 
    {
        static Scene scene;
        return scene;
    }

    // Getter to the global camera
    inline CameraComponent& GetCamera()
    {
        static CameraComponent camera;
        return camera;
    }

    void LoadModel(const std::string& filename);
    void LoadModel(Scene& scene, const std::string& filename);

    struct PickResult
    {
        ecs::Entity entity = ecs::kInvalidEntity;
        XMFLOAT3 position = XMFLOAT3(0, 0, 0);
        float distance = FLT_MAX;
    };

    PickResult Pick(const Scene& scene, const Ray& ray);

    // Scene component serializers:
    void SerializeComponent(NameComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& state);
    void SerializeComponent(TransformComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(GroupComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(HierarchyComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(MaterialComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(MeshComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(ObjectComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(LightComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(CameraComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(WeatherComponent& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);
    void SerializeComponent(AxisAlignedBox& x, serializer::Archive& ar, ecs::SerializeEntityContext& serialize);

}