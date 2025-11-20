#pragma once
#include <mutex>
#include "core/serializer.h"
#include "core/intersect.h"
#include "core/enum_flags.h"
#include "systems/ecs.h"
#include "graphics/renderer.h"

namespace cyb::scene {

struct NameComponent
{
    std::string name;

    NameComponent() = default;
    NameComponent(const std::string& name_) : name(name_) {}
};

struct alignas(16) TransformComponent
{
    enum class Flags
    {
        None     = 0,
        DirtyBit = BIT(0),
    };

    Flags flags{ Flags::DirtyBit };
    XMFLOAT3 scale_local{ g_float3One };
    XMFLOAT4 rotation_local{ g_float4OneW };    // quaternion rotation
    XMFLOAT3 translation_local{ g_float3Zero };

    // non-serialized data
    XMMATRIX world{};

    void SetDirty(bool value = true);
    [[nodiscard]] bool IsDirty() const;

    [[nodiscard]] XMMATRIX GetLocalMatrix() const noexcept;

    // apply world matrix to local space, overwriting scale, rotation & translation
    void ApplyTransform();

    void MatrixTransform(const XMMATRIX& matrix);

    void UpdateTransform();
    void UpdateTransformParented(const TransformComponent& parent);
    void Translate(const XMFLOAT3& value);
    void RotateRollPitchYaw(const XMFLOAT3& value);
};
CYB_ENABLE_BITMASK_OPERATORS(TransformComponent::Flags);

struct GroupComponent
{
};

struct alignas(16) HierarchyComponent
{
    ecs::Entity parentID{ ecs::INVALID_ENTITY };
};

struct MaterialComponent
{
    enum class Flags : uint32_t
    {
        None               = 0,
        DirtyBit           = (1 << 0),
        UseVertexColorsBit = (1 << 1)
    };

    enum Shadertype
    {
        Shadertype_BDRF,
        Shadertype_Disney_BDRF,
        Shadertype_Unlit,
        Shadertype_Terrain,
        Shadertype_Count
    };

    Flags flags{ Flags::None };
    Shadertype shaderType{ Shadertype_BDRF };
    XMFLOAT4 baseColor{ 1, 1, 1, 1 };
    float roughness{ 0.2f };
    float metalness{ 0.0f };

    void SetUseVertexColors(bool value);
    bool IsUsingVertexColors() const;
};
CYB_ENABLE_BITMASK_OPERATORS(MaterialComponent::Flags);

struct alignas(16) MeshComponent
{
    std::vector<XMFLOAT3> vertex_positions;
    std::vector<XMFLOAT3> vertex_normals;
    std::vector<uint32_t> vertex_colors;
    std::vector<uint32_t> indices;

    struct MeshSubset
    {
        ecs::Entity materialID{ ecs::INVALID_ENTITY };
        uint32_t indexOffset{ 0 };
        uint32_t indexCount{ 0 };

        // non-serialized attributes:
        uint32_t materialIndex{ 0 };
    };
    std::vector<MeshSubset> subsets;

    // non-serialized data
    AxisAlignedBox aabb;
    rhi::GPUBuffer vertex_buffer_pos;
    rhi::GPUBuffer vertex_buffer_col;
    rhi::GPUBuffer index_buffer;
    rhi::GPUBuffer vertexBuffer;

    // clear vertex and index data. GPUBuffer's will be left untouched
    void Clear();
    void CreateRenderData();
    void ComputeHardNormals();
    void ComputeSmoothNormals();

    // internal format for vertex_buffer_pos
    //      0: positions
    //      12: normal (normalized & encoded)
    struct Vertex_Pos
    {
        static constexpr rhi::Format FORMAT{ rhi::Format::RGBA32_FLOAT };
        XMFLOAT3 pos{ g_float3Zero };
        uint32_t normal{ 0 };

        void Set(const XMFLOAT3& pos, const XMFLOAT3& norm);
        [[nodiscard]] uint32_t EncodeNormal(const XMFLOAT3& norm) const;
        [[nodiscard]] XMFLOAT3 DecodeNormal() const;
        [[nodiscard]] uint32_t DecodeMaterialIndex() const;
    };

    // internal format for vertex_buffer_col
    struct Vertex_Col
    {
        static constexpr rhi::Format FORMAT{ rhi::Format::RGBA8_UNORM };
        uint32_t color{ 0 };
    };
};

struct alignas(16) ObjectComponent
{
    enum class Flags : uint32_t
    {
        None          = 0,
        RenderableBit = BIT(0),
        CastShadowBit = BIT(1),
        DefaultFlags  = RenderableBit | CastShadowBit
    };

    Flags flags{ Flags::DefaultFlags };
    ecs::Entity meshID{ ecs::INVALID_ENTITY };
    uint8_t userStencilRef{ 0 };

    // user stencil value can be in range [0, 15]
    void SetUserStencilRef(uint8_t value);

    // non-serialized data
    uint32_t meshIndex{ ~0u };
    int32_t transformIndex{ -1 };        // only valid for a single frame
};
CYB_ENABLE_BITMASK_OPERATORS(ObjectComponent::Flags);

// NOTE: must be precisely mapped to LIGHTSOURCE_TYPE_ defs
enum class LightType
{
    Directional = LIGHTSOURCE_TYPE_DIRECTIONAL,
    Point = LIGHTSOURCE_TYPE_POINT
};

struct alignas(16) LightComponent
{
    enum class Flags : uint32_t
    {
        CastShadowsBit  = BIT(0),
        AffectsSceneBit = BIT(1),
        DefaultFlags    = AffectsSceneBit
    };

    Flags flags{ Flags::DefaultFlags };
    XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
    LightType type{ LightType::Point };
    float energy{ 1.0f };
    float range{ 10.0f };

    // non-serialized data
    XMFLOAT3 position{};

    void SetAffectingScene(bool value);
    bool IsAffectingScene() const;
    void SetType(LightType value) { type = value; }
    LightType GetType() const { return type; }
};
CYB_ENABLE_BITMASK_OPERATORS(LightComponent::Flags);

struct WeatherComponent
{
    XMFLOAT3 horizon{ 1, 1, 1 };
    XMFLOAT3 zenith{ 0, 0, 0 };
    bool drawSun{ true };
    float fogStart{ 650.0f };
    float fogEnd{ 1000.0f };
    float fogHeight{ 0.0f };

    float cloudiness{ 0.6f };
    float cloudTurbulence{ 0.9f };
    float cloudHeight{ 500.0f };
    float windSpeed{ 10.0f };

    // non-serialized attributes:
    uint32_t mostImportantLightIndex{ 0 };
};

struct alignas(16) CameraComponent
{
    float aspect{ 1.0f };
    float zNearPlane{ 0.001f };
    float zFarPlane{ 800.0f };
    float fov{ 90.0f };         // field of view in degrees

    XMFLOAT3 pos{ g_float3Zero };
    XMFLOAT3 target{ g_float3OneZ };
    XMFLOAT3 up{ g_float3OneY };

    // non-serialized data
    XMMATRIX invRotation{};
    XMMATRIX projection{}, invProjection{};
    XMMATRIX view{}, invView{};
    XMMATRIX VP{}, invVP{};
    Frustum frustum;

    CameraComponent() = default;
    CameraComponent(float newAspect, float newNear, float newFar, float newFOV) { CreatePerspective(newAspect, newNear, newFar, newFOV); }
    void CreatePerspective(float newAspect, float newNear, float newFar, float newFOV);
    void UpdateCamera();
    void TransformCamera(const TransformComponent& transform);
};

struct AnimationComponent
{
    enum class Flag : uint32_t
    {
        Empty    = 0,
        Playing  = BIT(0),
        Looped   = BIT(1),
        PingPong = BIT(2)
    };

    struct Channel
    {
        enum class Path : uint32_t
        {
            Unknown,
            Translation,
            Rotation,
            Scale,
            Weights
        };

        ecs::Entity target{ ecs::INVALID_ENTITY };
        int samplerIndex{ -1 };
        Path path{ Path::Unknown };
    };

    struct Sampler
    {
        enum class Mode : uint32_t
        {
            Linear,
            Step,
            CubicSpline
        };

        Mode mode{ Mode::Linear };
        std::vector<float> keyframeTimes;
        std::vector<float> keyframeData;
    };

    Flag flags{ Flag::Looped };
    float start{ 0.0f };
    float end{ 0.0f };
    float timer{ 0.0f };
    float blendAmount{ 1.0f };
    float speed{ 1.0f };

    std::vector<Channel> channels;
    std::vector<Sampler> samplers;

    // non-serialized attributes:
    float lastUpdateTime{ 0.0f };

    [[nodiscard]] constexpr bool IsPlaying() const { return HasFlag(flags, Flag::Playing); }
    [[nodiscard]] constexpr bool IsLooped() const { return HasFlag(flags, Flag::Looped); }
    [[nodiscard]] constexpr bool IsPingPong() const { return HasFlag(flags, Flag::PingPong); }
    [[nodiscard]] constexpr float GetLength() const { return end - start; }
    [[nodiscard]] constexpr bool IsEnded() const { return timer >= end; }

    constexpr void Play() { SetFlag(flags, Flag::Playing, true); }
    constexpr void Pause() { SetFlag(flags, Flag::Playing, false); }
    constexpr void Stop() { Pause(); timer = 0.0f; lastUpdateTime = timer; }
    constexpr void SetLooped(bool value) { SetFlag(flags, Flag::Looped, value); if (value) SetFlag(flags, Flag::PingPong, false); }
    constexpr void SetPingPong(bool value) { SetFlag(flags, Flag::PingPong, value); if (value) SetFlag(flags, Flag::Looped, false); }
    void SetPlayOnce() { SetFlag(flags, Flag::Looped, false); SetFlag(flags, Flag::PingPong, false); }
};
CYB_ENABLE_BITMASK_OPERATORS(AnimationComponent::Flag);

struct Scene
{
    ecs::ComponentManager<NameComponent> names;
    ecs::ComponentManager<TransformComponent> transforms;
    ecs::ComponentManager<GroupComponent> groups;
    ecs::ComponentManager<HierarchyComponent> hierarchy;
    ecs::ComponentManager<MaterialComponent> materials;
    ecs::ComponentManager<MeshComponent> meshes;
    ecs::ComponentManager<ObjectComponent> objects;
    ecs::ComponentManager<LightComponent> lights;
    ecs::ComponentManager<CameraComponent> cameras;
    ecs::ComponentManager<AnimationComponent> animations;
    ecs::ComponentManager<WeatherComponent> weathers;

    WeatherComponent weather;   // weathers[0] copy

    // non-serialized attributes:
    float dt{ 0.0f };
    float time{ 0.0f };
    std::mutex lock;

    // AABB culling streams:
    std::vector<AxisAlignedBox> aabb_objects;
    std::vector<AxisAlignedBox> aabb_lights;

    void Update(double dt);
    void Clear();
    void Merge(Scene& other);

    /**
     * @brief Remove an entity and all it's components from the scene.
     *
     * @param recursive Set true to remove all child entities.
     * @param removeLinkedEntities Set true to remove unused linked entites (object meshes, and mesh materials).
     */
    void RemoveEntity(ecs::Entity entity, bool recursive, bool removeLinkedEntities);

    void RemoveUnusedEntities();

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

    // remove the hierarchy component from entity and update transform,
    // world position will not be changed
    void ComponentDetach(ecs::Entity entity);

    /**
     * @brief Get number of objects currently using a mash in scene.
     */
    uint32_t GetMeshUseCount(ecs::Entity meshID) const;

    /**
     * @brief Get number of sub-meshes currently using a material in scene.
     */
    uint32_t GetMaterialUseCount(ecs::Entity materialID) const;

    void Serialize(Serializer& ser);

    void RunTransformUpdateSystem(jobsystem::Context& ctx);
    void RunHierarchyUpdateSystem(jobsystem::Context& ctx);
    void RunMeshUpdateSystem(jobsystem::Context& ctx);
    void RunObjectUpdateSystem(jobsystem::Context& ctx);
    void RunLightUpdateSystem(jobsystem::Context& ctx);
    void RunCameraUpdateSystem(jobsystem::Context& ctx);
    void RunAnimationUpdateSystem(jobsystem::Context& ctx);
    void RunWeatherUpdateSystem(jobsystem::Context& ctx);
};

// getter to the global scene
inline Scene& GetScene()
{
    static Scene scene;
    return scene;
}

// getter to the global camera
inline CameraComponent& GetCamera()
{
    static CameraComponent camera;
    return camera;
}

struct PickResult
{
    ecs::Entity entity{ ecs::INVALID_ENTITY };
    XMFLOAT3 position{ g_float3Zero };
    float distance{ FLT_MAX };
};

PickResult Pick(const Scene& scene, const Ray& ray);

} // namespace cyb::scene

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

} // cyb::ecs