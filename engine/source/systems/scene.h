#pragma once
#include "core/serializer.h"
#include "core/intersect.h"
#include "core/enum_flags.h"
#include "systems/ecs.h"
#include "graphics/renderer.h"

namespace cyb::scene {

    struct NameComponent {
        std::string name;

        NameComponent() = default;
        NameComponent(const std::string& name_) : name(name_) {}
    };

    struct TransformComponent {
        enum class Flags {
            None     = 0,
            DirtyBit = (1 << 0),
        };

        Flags flags = Flags::DirtyBit;
        XMFLOAT3 scale_local = math::VECTOR_IDENTITY;
        XMFLOAT4 rotation_local = math::QUATERNION_IDENTITY;  // quaternion rotation
        XMFLOAT3 translation_local = math::VECTOR_ZERO;

        // non-serialized data
        XMFLOAT4X4 world = math::MATRIX_IDENTITY;

        void SetDirty(bool value = true);
        [[nodiscard]] bool IsDirty() const;

        [[nodiscard]] XMFLOAT3 GetPosition() const noexcept;
        [[nodiscard]] XMFLOAT4 GetRotation() const noexcept;
        [[nodiscard]] XMFLOAT3 GetScale() const noexcept;
        [[nodiscard]] XMVECTOR GetPositionV() const noexcept;
        [[nodiscard]] XMVECTOR GetRotationV() const noexcept;
        [[nodiscard]] XMVECTOR GetScaleV() const noexcept;
        [[nodiscard]] XMMATRIX GetLocalMatrix() const noexcept;

        // apply world matrix to local space, overwriting scale, rotation & translation
        void ApplyTransform();

        void MatrixTransform(const XMFLOAT4X4& matrix);
        void MatrixTransform(const XMMATRIX& matrix);
        
        void UpdateTransform();
        void UpdateTransformParented(const TransformComponent& parent);
        void Translate(const XMFLOAT3& value);
        void RotateRollPitchYaw(const XMFLOAT3& value);
    };
    CYB_ENABLE_BITMASK_OPERATORS(TransformComponent::Flags);

    struct GroupComponent {
    };

    struct HierarchyComponent {
        ecs::Entity parentID = ecs::INVALID_ENTITY;
    };

    struct MaterialComponent {
        enum class Flags : uint32_t {
            None                = 0,
            DirtyBit            = (1 << 0),
            UseVertexColorsBit  = (1 << 1)
        };

        enum Shadertype {
            Shadertype_BDRF,
            Shadertype_Disney_BDRF,
            Shadertype_Unlit,
            Shadertype_Terrain,
            Shadertype_Count
        };

        Flags flags = Flags::None;
        Shadertype shaderType = Shadertype_BDRF;
        XMFLOAT4 baseColor = XMFLOAT4(1, 1, 1, 1);
        float roughness = 0.2f;
        float metalness = 0.0f;

        void SetUseVertexColors(bool value);
        bool IsUsingVertexColors() const;
    };
    CYB_ENABLE_BITMASK_OPERATORS(MaterialComponent::Flags);

    struct MeshComponent {
        std::vector<XMFLOAT3> vertex_positions;
        std::vector<XMFLOAT3> vertex_normals;
        std::vector<uint32_t> vertex_colors;
        std::vector<uint32_t> indices;

        struct MeshSubset {
            ecs::Entity materialID = ecs::INVALID_ENTITY;
            uint32_t indexOffset = 0;
            uint32_t indexCount = 0;
        };
        std::vector<MeshSubset> subsets;

        // non-serialized data
        spatial::AxisAlignedBox aabb;
        graphics::GPUBuffer vertex_buffer_pos;
        graphics::GPUBuffer vertex_buffer_col;
        graphics::GPUBuffer index_buffer;
        graphics::GPUBuffer vertexBuffer;

        // clear vertex and index data. GPUBuffer's will be left untouched
        void Clear();
        void CreateRenderData();
        void ComputeHardNormals();
        void ComputeSmoothNormals();

        // internal format for vertex_buffer_pos
        //      0: positions
        //      12: normal (normalized & encoded)
        struct Vertex_Pos {
            static constexpr graphics::Format FORMAT = graphics::Format::R32G32B32A32_Float;
            XMFLOAT3 pos = math::VECTOR_ZERO;
            uint32_t normal = 0;

            void Set(const XMFLOAT3& pos, const XMFLOAT3& norm);
            [[nodiscard]] uint32_t EncodeNormal(const XMFLOAT3& norm) const;
            [[nodiscard]] XMFLOAT3 DecodeNormal() const;
            [[nodiscard]] uint32_t DecodeMaterialIndex() const;
        };

        // internal format for vertex_buffer_col
        struct Vertex_Col {
            static constexpr graphics::Format FORMAT = graphics::Format::R8G8B8A8_Unorm;
            uint32_t color = 0;
        };
    };

    struct ObjectComponent {
        enum class Flags : uint32_t {
            None            = 0,
            RenderableBit   = (1 << 0),
            CastShadowBit   = (1 << 1),
            DefaultFlags    = RenderableBit | CastShadowBit
        };

        Flags flags = Flags::DefaultFlags;
        ecs::Entity meshID = ecs::INVALID_ENTITY;

        // non-serialized data
        int32_t transformIndex = -1;        // only valid for a single frame
    };
    CYB_ENABLE_BITMASK_OPERATORS(ObjectComponent::Flags);

    // NOTE: must be precisely mapped to LIGHTSOURCE_TYPE_ defs
    enum class LightType {
        Directional = LIGHTSOURCE_TYPE_DIRECTIONAL,
        Point = LIGHTSOURCE_TYPE_POINT
    };

    struct LightComponent {
        enum class Flags : uint32_t {
            CastShadowsBit  = (1 << 0),
            AffectsSceneBit = (1 << 1),
            DefaultFlags    = AffectsSceneBit
        };

        Flags flags = Flags::DefaultFlags;
        XMFLOAT3 color = XMFLOAT3(1.0f, 1.0f, 1.0f);
        LightType type = LightType::Point;
        float energy = 1.0f;
        float range = 10.0f;

        // non-serialized data
        spatial::AxisAlignedBox aabb;          // non-transformed localspace box

        void SetffectingSceney(bool value);
        bool IsAffectingScene() const;
        void SetType(LightType value) { type = value; }
        LightType GetType() const { return type; }
        void UpdateLight();
    };
    CYB_ENABLE_BITMASK_OPERATORS(LightComponent::Flags);

    struct WeatherComponent {
        XMFLOAT3 horizon = XMFLOAT3(1, 1, 1);
        XMFLOAT3 zenith = XMFLOAT3(0, 0, 0);
        bool drawSun = true;
        float fogStart = 650.0f;
        float fogEnd = 1000.0f;
        float fogHeight = 0.0f;

        float cloudiness = 0.6f;
        float cloudTurbulence = 0.9f;
        float cloudHeight = 500.0f;
        float windSpeed = 10.0f;
    };

    struct CameraComponent {
        float aspect = 1.0f;
        float zNearPlane = 0.001f;
        float zFarPlane = 800.0f;
        float fov = 90.0f;      // field of view in degrees

        XMFLOAT3 pos = math::VECTOR_ZERO;
        XMFLOAT3 target = math::VECTOR_FORWARD;
        XMFLOAT3 up = math::VECTOR_UP;

        // non-serialized data
        XMFLOAT3X3 rotation = {};
        XMFLOAT4X4 view = {}, projection = {}, VP = {};
        XMFLOAT4X4 inv_view = {}, inv_projection = {}, inv_VP= {};
        spatial::Frustum frustum;

        CameraComponent() = default;
        CameraComponent(float newAspect, float newNear, float newFar, float newFOV) { CreatePerspective(newAspect, newNear, newFar, newFOV); }
        XMMATRIX GetViewProjection() const { return XMLoadFloat4x4(&VP); }
        void CreatePerspective(float newAspect, float newNear, float newFar, float newFOV);
        void UpdateCamera();
        void TransformCamera(const TransformComponent& transform);
    };

    struct Scene {
        ecs::ComponentManager<NameComponent> names;
        ecs::ComponentManager<TransformComponent> transforms;
        ecs::ComponentManager<GroupComponent> groups;
        ecs::ComponentManager<HierarchyComponent> hierarchy;
        ecs::ComponentManager<MaterialComponent> materials;
        ecs::ComponentManager<MeshComponent> meshes;
        ecs::ComponentManager<ObjectComponent> objects;
        ecs::ComponentManager<spatial::AxisAlignedBox> aabb_objects;
        ecs::ComponentManager<LightComponent> lights;
        ecs::ComponentManager<spatial::AxisAlignedBox> aabb_lights;
        ecs::ComponentManager<CameraComponent> cameras;
        ecs::ComponentManager<WeatherComponent> weathers;

        WeatherComponent active_weather;   // weathers[0] copy

        void Update(double dt);
        void Clear();
        void Merge(Scene& other);

        // NOTE:
        // be careful removing non-recursive mode sence it might leave
        // child entities without parent
        void RemoveEntity(ecs::Entity entity, bool recursive);

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
            LightType type = LightType::Point);
        ecs::Entity CreateCamera(
            const std::string& name,
            float aspect,
            float nearPlane,
            float farPlane,
            float fov);

        // create a hierarchy component to the entity and attaches it to parent
        // world position will not be changed
        void ComponentAttach(ecs::Entity entity, ecs::Entity parent);

        // remove the hierarchy compoennt from entity and update transform,
        // world position will not be changed
        void ComponentDetach(ecs::Entity entity);

        void Serialize(Serializer& ser);

        void RunTransformUpdateSystem(jobsystem::Context& ctx);
        void RunHierarchyUpdateSystem(jobsystem::Context& ctx);
        void RunObjectUpdateSystem(jobsystem::Context& ctx);
        void RunLightUpdateSystem(jobsystem::Context& ctx);
        void RunCameraUpdateSystem(jobsystem::Context& ctx);
        void RunWeatherUpdateSystem(jobsystem::Context& ctx);
    };

    // getter to the global scene
    inline Scene& GetScene()  {
        static Scene scene;
        return scene;
    }

    // getter to the global camera
    inline CameraComponent& GetCamera() {
        static CameraComponent camera;
        return camera;
    }

    struct PickResult {
        ecs::Entity entity = ecs::INVALID_ENTITY;
        XMFLOAT3 position = math::VECTOR_ZERO;
        float distance = FLT_MAX;
    };

    PickResult Pick(const Scene& scene, const spatial::Ray& ray);
}

// scene component serializers
namespace cyb::ecs {
    void SerializeComponent(scene::NameComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::TransformComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::GroupComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::HierarchyComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::MaterialComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::MeshComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::ObjectComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::LightComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::CameraComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(scene::WeatherComponent& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
    void SerializeComponent(spatial::AxisAlignedBox& x, Serializer& ser, ecs::SceneSerializeContext& entitySerializer);
}