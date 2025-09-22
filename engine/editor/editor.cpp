#define IMGUI_DEFINE_MATH_OPERATORS
#include <numeric>
#include <limits>
#include <format>
#include <functional>
#include "core/cvar.h"
#include "core/hash.h"
#include "core/logger.h"
#include "core/timer.h"
#include "core/filesystem.h"
#include "core/mathlib.h"
#include "graphics/renderer.h"
#include "graphics/model_import.h"
#include "systems/event_system.h"
#include "systems/profiler.h"
#include "editor/editor.h"
#include "editor/undo_manager.h"
#include "editor/icons_font_awesome6.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "ImGuizmo.h"
#include "editor/widgets.h"
#include "editor/terrain_generator.h"

using namespace std::string_literals;

extern ImFont* imguiBigFont;    // from imgui_backend.cpp

// filters are passed to FileDialog function as std::string, thus we 
// need to use string literals to avoid them of being stripped
const auto FILE_FILTER_ALL = "All Files (*.*)\0*.*\0"s;
const auto FILE_FILTER_SCD = "CybSceneData (*.csd)\0*.csd\0"s;
const auto FILE_FILTER_GLTF = "glTF 2.0 (*.gltf; *.glb)\0*.gltf;*.glb\0"s;
const auto FILE_FILTER_IMPORT_MODEL = FILE_FILTER_GLTF + FILE_FILTER_SCD + FILE_FILTER_ALL;

namespace cyb::editor
{
    CVar<bool> e_autoremoveLinkedEntities{ "e_autoremoveLinkedEntities", true, CVarFlag::GuiBit, "On entity delete, also delete linked entities that isn't beeing used." };
    CVar<bool> e_recursiveDelete{ "e_recursiveDelete", true, CVarFlag::GuiBit, "On entity delete, also delete all of the child entities." };
    
    bool initialized = false;
    bool fullscreenEnabled = false; // FIXME: initial value has to be synced with Application::fullscreenEnabled
    bool displayCubeView = false;
    CVar<bool>* r_vsync = nullptr;
    CVar<bool>* r_debugObjectAABB = nullptr;
    CVar<bool>* r_debugLightSources = nullptr;

    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
    SceneGraphView scenegraphView;
    std::vector<VideoModeInfo> videoModeList;

    // fps counter data
    std::array<double, 100> deltatimes{};
    uint32_t fpsAgvCounter = 0;
    uint32_t avgFps = 0;
    
    //------------------------------------------------------------------------------
    // Component inspectors
    //------------------------------------------------------------------------------

    void InspectNameComponent(scene::NameComponent* name_component)
    {
        ImGui::InputText("Name", &name_component->name);
    }

    void InspectTransformComponent(scene::TransformComponent* transform)
    {
        ui::EditTransformComponent("Translation", &transform->translation_local.x, transform);
        ui::EditTransformComponent("Scale", &transform->scale_local.x, transform);
    }

    void InspectHierarchyComponent(scene::HierarchyComponent* hierarchy)
    {
        scene::Scene& scene = scene::GetScene();
        const scene::NameComponent* name = scene.names.GetComponent(hierarchy->parentID);
        if (!name)
        {
            ImGui::Text("Parent: (no name) entityID=%u", hierarchy->parentID);
            return;
        }

        ImGui::Text("Parent: %s", name->name.c_str());
    }

    void InspectMeshComponent(scene::MeshComponent* mesh)
    {
        scene::Scene& scene = scene::GetScene();

        // Mesh info
        ImGui::Text("Vertex positions: %zu", mesh->vertex_positions.size());
        ImGui::Text("Vertex normals: %zu", mesh->vertex_normals.size());
        ImGui::Text("Vertex colors: %zu", mesh->vertex_colors.size());
        ImGui::Text("Index count: %zu", mesh->indices.size());

        ImGui::Spacing();
        ImGui::TextUnformatted("Mesh Subset Info:");
        ImGui::BeginTable("Subset Info", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp);
        ImGui::TableSetupColumn("Subset");
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("IndexCount");
        ImGui::TableSetupColumn("Material");
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();
        for (uint32_t i = 0; i < mesh->subsets.size(); ++i)
        {
            const auto& subset = mesh->subsets[i];
            const std::string& material_name = scene.names.GetComponent(subset.materialID)->name;

            ImGui::Text("%d", i); ImGui::TableNextColumn();
            ImGui::Text("%u", subset.indexOffset); ImGui::TableNextColumn();
            ImGui::Text("%u", subset.indexCount); ImGui::TableNextColumn();
            ImGui::Text("%s", material_name.c_str()); ImGui::TableNextColumn();
        }

        ImGui::EndTable();

        if (ImGui::Button("Compute Smooth Normals"))
        {
            mesh->ComputeSmoothNormals();
            mesh->CreateRenderData();
        }

        if (ImGui::Button("Compute Hard Normals"))
        {
            mesh->ComputeHardNormals();
            mesh->CreateRenderData();
        }

        ImGui::SameLine();
        ui::InfoIcon("This will duplicate any shared vertices and\npossibly create additional mesh geometry");
    }

    void InspectMaterialComponent(scene::MaterialComponent* material)
    {
        if (!material)
            return;

        static const std::unordered_map<scene::MaterialComponent::Shadertype, std::string> shadertype_names =
        {
            { scene::MaterialComponent::Shadertype_BDRF,    "Flat BRDF" },
            { scene::MaterialComponent::Shadertype_Disney_BDRF, "Flat Disney BRDF" },
            { scene::MaterialComponent::Shadertype_Unlit,   "Flat Unlit" },
            { scene::MaterialComponent::Shadertype_Terrain, "Terrain (NOT IMPLEMENTED)" }
        };

        ui::ComboBox("Shader Type", material->shaderType, shadertype_names);
        ui::ColorEdit4("BaseColor", &material->baseColor.x);
        ui::SliderFloat("Roughness", &material->roughness, nullptr, 0.0f, 1.0f);
        ui::SliderFloat("Metalness", &material->metalness, nullptr, 0.0f, 1.0f);
    }

    struct SortableNameEntityData
    {
        ecs::Entity id = ecs::INVALID_ENTITY;
        std::string_view name;

        bool operator<(const SortableNameEntityData& a)
        {
            return name < a.name;
        }
    };

    template <typename T>
    [[nodiscard]] ecs::Entity SelectEntityPopup(
        const ecs::ComponentManager<T>& components,
        const ecs::ComponentManager<scene::NameComponent>& names,
        const ecs::Entity currentEntity)
    {
        assert(components.Size() < INT32_MAX);
        static ImGuiTextFilter filter;
        ecs::Entity selectedEntity = ecs::INVALID_ENTITY;

        ImGui::Text(ICON_FA_MAGNIFYING_GLASS "Search:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        //filter.WindowContent("##filter");

        if (ImGui::BeginListBox("##BeginListBox"))
        {
            std::vector<SortableNameEntityData> sortedEntities;
            for (size_t i = 0; i < components.Size(); ++i)
            {
                auto& back = sortedEntities.emplace_back();
                back.id = components.GetEntity(i);
                back.name = names.GetComponent(back.id)->name;
            }

            std::sort(sortedEntities.begin(), sortedEntities.end());
            for (auto& entity : sortedEntities)
            {
                ImGui::PushID(entity.id);
                if (filter.PassFilter(entity.name.data()))
                {
                    // Create a uniqe label for each entity
                    const std::string label = std::format("{}##{}", entity.name, entity.id);

                    if (ImGui::Selectable(label.c_str(), currentEntity == entity.id))
                    {
                        selectedEntity = entity.id;
                        filter.Clear();
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }

        return selectedEntity;
    }

    [[nodiscard]] uint32_t SelectAndGetMaterialIndexForMesh(scene::MeshComponent& mesh)
    {
        scene::Scene& scene = scene::GetScene();

        std::vector<std::string_view> names;
        for (const auto& subset : mesh.subsets)
        {
            const scene::NameComponent* comp = scene.names.GetComponent(subset.materialID);
            names.push_back(comp->name);
        }

        static int selectedSubsetIndex = 0;
        selectedSubsetIndex = std::min(selectedSubsetIndex, (int)mesh.subsets.size() - 1);
        ui::ListBox("Material", &selectedSubsetIndex, names);
        ecs::Entity selectedMaterialID = mesh.subsets[selectedSubsetIndex].materialID;

        // Edit material name / select material
        scene::NameComponent* name = scene.names.GetComponent(selectedMaterialID);
        ImGui::InputText("##Material_Name", &name->name);
        ImGui::SameLine();
        if (ImGui::Button("Change##Material", ImVec2(-1, 0)))
            ImGui::OpenPopup("MaterialSelectPopup");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Link another material to the mesh");

        if (ImGui::BeginPopup("MaterialSelectPopup"))
        {
            ecs::Entity selectedID = SelectEntityPopup(scene.materials, scene.names, mesh.subsets[selectedSubsetIndex].materialID);
            if (selectedID != ecs::INVALID_ENTITY)
                mesh.subsets[selectedSubsetIndex].materialID = selectedID;

            ImGui::EndPopup();
        }

        return mesh.subsets[selectedSubsetIndex].materialIndex;
    }

    [[nodiscard]] uint32_t SelectAndGetMeshIndexForObject(scene::ObjectComponent& object)
    {
        scene::Scene& scene = scene::GetScene();

        // Edit mesh name / select mesh
        scene::NameComponent* name = scene.names.GetComponent(object.meshID);
        ImGui::InputText("##Mesh_Name", &name->name);
        ImGui::SameLine();
        if (ImGui::Button("Change"))
            ImGui::OpenPopup("MeshSelectPopup");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Link another mesh to the object");

        if (ImGui::BeginPopup("MeshSelectPopup"))
        {
            ecs::Entity selectedID = SelectEntityPopup(scene.meshes, scene.names, object.meshID);
            if (selectedID != ecs::INVALID_ENTITY)
            {
                object.meshID = selectedID;
            }
            ImGui::EndPopup();
        }

        return object.meshIndex;
    }

    void InspectObjectComponent(scene::ObjectComponent* object)
    {
        ui::CheckboxFlags("Renderable", (uint32_t*)&object->flags, (uint32_t)scene::ObjectComponent::Flags::RenderableBit, nullptr);
        ui::CheckboxFlags("Cast shadow (unimplemented)", (uint32_t*)&object->flags, (uint32_t)scene::ObjectComponent::Flags::CastShadowBit, nullptr);
    }

    void InspectCameraComponent(scene::CameraComponent& camera)
    {
        ui::SliderFloat("Z Near Plane", &camera.zNearPlane, nullptr, 0.001f, 10.0f);
        ui::SliderFloat("Z Far Plane", &camera.zFarPlane, nullptr, 10.0f, 1000.0f);
        ui::SliderFloat("FOV", &camera.fov, nullptr, 0.0f, 3.0f);
        ui::DragFloat3("Position", &camera.pos.x);
        ui::DragFloat3("Target", &camera.target.x);
        ui::DragFloat3("Up", &camera.up.x);
    }

    void InspectLightComponent(scene::LightComponent* light)
    {
        static const std::unordered_map<scene::LightType, std::string> lightTypeNames =
        {
            { scene::LightType::Directional, "Directional" },
            { scene::LightType::Point,       "Point"       }
        };

        ui::ComboBox("Type", light->type, lightTypeNames);
        ui::ColorEdit3("Color", &light->color.x);
        ui::SliderFloat("Energy", &light->energy, nullptr, 0.2f, 5.0f);
        ui::SliderFloat("Range", &light->range, nullptr, 1.0f, 300.0f);
        ui::CheckboxFlags("Affects scene", (uint32_t*)&light->flags, (uint32_t)scene::LightComponent::Flags::AffectsSceneBit, nullptr);
        ui::CheckboxFlags("Cast shadows", (uint32_t*)&light->flags, (uint32_t)scene::LightComponent::Flags::CastShadowsBit, nullptr);
    }

    void InspectAnimationComponent(scene::AnimationComponent* anim)
    {
        static float speedSlider = std::abs(anim->speed);
        constexpr ImVec2 iconButtonSize(50, 0);

        const char* loopIcon = ICON_FA_RIGHT_LONG;
        if (anim->IsLooped())
            loopIcon = ICON_FA_REPEAT;
        else if (anim->IsPingPong())
            loopIcon = ICON_FA_RIGHT_LEFT;
        if (ImGui::Button(loopIcon, iconButtonSize))
        {
            if (anim->IsLooped())
                anim->SetPingPong(true);
            else if (anim->IsPingPong())
                anim->SetPlayOnce();
            else
                anim->SetLooped(true);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", anim->IsLooped() ? "Looped" : anim->IsPingPong() ? "PingPong" : "PlayOnce");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_BACKWARD, iconButtonSize))
            anim->timer = anim->start;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Rewind");

        ImGui::SameLine();
        if (ImGui::Button(anim->IsPlaying() ? ICON_FA_PAUSE : ICON_FA_PLAY, iconButtonSize))
        {
            if (anim->IsPlaying())
            {
                anim->Pause();
            }
            else
            {
                anim->Play();
                if (!anim->IsPingPong())
                    anim->speed = speedSlider;
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s animation", anim->IsPlaying() ? "Pause" : "Play");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_STOP, iconButtonSize))
            anim->Stop();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Stop and reset animation");

        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##Animtime", &anim->timer, anim->start, anim->end);
        
        ui::SliderFloat("Speed", &speedSlider, nullptr, 0.1f, 2.0f);
        anim->speed = anim->speed >= 0 ? speedSlider : -speedSlider;
        ui::SliderFloat("Blend", &anim->blendAmount, nullptr, 0.0, 2.0);
    }

    void InspectWeatherComponent(scene::WeatherComponent* weather)
    {
        ui::ColorEdit3("Horizon Color", &weather->horizon.x);
        ui::ColorEdit3("Zenith Color", &weather->zenith.x);
        ui::Checkbox("Draw Sun", &weather->drawSun, nullptr);
        ui::DragFloat("Fog Begin", &weather->fogStart);
        ui::DragFloat("Fog End", &weather->fogEnd);
        ui::DragFloat("Fog Height", &weather->fogHeight);
        ui::SliderFloat("Cloudiness", &weather->cloudiness, nullptr, 0.0f, 1.0f);
        ui::SliderFloat("Cloud Turbulence", &weather->cloudTurbulence, nullptr, 0.0f, 10.0f);
        ui::SliderFloat("Cloud Height", &weather->cloudHeight, nullptr, 200.f, 1000.0f);
        ui::SliderFloat("Wind Speed", &weather->windSpeed, nullptr, 0.0f, 150.0f);
    }

    //------------------------------------------------------------------------------
    // Scene Graph
    //------------------------------------------------------------------------------

    void SceneGraphView::AddNode(Node* parent, ecs::Entity entity, const std::string_view& name)
    {
        const scene::Scene& scene = scene::GetScene();

        const scene::HierarchyComponent* hierarchy = scene.hierarchy.GetComponent(entity);
        if (hierarchy != nullptr)
        {
            ecs::Entity parent_id = hierarchy->parentID;
            const std::string_view parent_name = scene.names.GetComponent(parent_id)->name;
            AddNode(parent, parent_id, parent_name);
        }

        if (m_entities.count(entity) != 0)
            return;

        parent->children.emplace_back(parent, entity, name);
        m_entities.insert(entity);

        // Generate a list of all child nodes
        for (size_t i = 0; i < scene.hierarchy.Size(); ++i)
        {
            if (scene.hierarchy[i].parentID == entity)
            {
                const ecs::Entity child_entity = scene.hierarchy.GetEntity(i);
                const std::string_view child_name = scene.names.GetComponent(child_entity)->name;
                AddNode(&parent->children.back(), child_entity, child_name);
            }
        }
    }

    void SceneGraphView::GenerateView()
    {
        m_root.children.clear();
        m_entities.clear();

        const scene::Scene& scene = scene::GetScene();

        // First weather...
        if (scene.weathers.Size() > 0)
        {
            ecs::Entity entity = scene.weathers.GetEntity(0);
            const char* name = "Weather";
            AddNode(&m_root, entity, name);
        }

        auto addComponents = [&] (auto& components) {
            for (size_t i = 0; i < components.Size(); ++i)
            {
                ecs::Entity entity = components.GetEntity(i);
                const std::string& name = scene.names.GetComponent(entity)->name;
                AddNode(&m_root, entity, name);
            }
        };

        // ... then groups, objects and transforms
        addComponents(scene.groups);
        addComponents(scene.objects);
        addComponents(scene.transforms);
    }

    void SceneGraphView::DrawNode(const Node* node)
    {
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
        nodeFlags |= (node->children.empty()) ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen : 0;
        nodeFlags |= (node->entity == GetSelectedEntity()) ? ImGuiTreeNodeFlags_Selected : 0;

        const void* nodeId = (void*)((size_t)node->entity);
        bool isNodeOpen = ImGui::TreeNodeEx(nodeId, nodeFlags, "%s", node->name.data());
        if (ImGui::IsItemClicked())
            SetSelectedEntity(node->entity);

        static const char* dragDropId = "SGV_TreeNode";
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(dragDropId, &node->entity, sizeof(node->entity));
            ImGui::Text("Move to parent");
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dragDropId);
            if (payload)
            {
                assert(payload->DataSize == sizeof(ecs::Entity));
                ecs::Entity dragged_entity = *((ecs::Entity*)payload->Data);
                scene::GetScene().ComponentAttach(dragged_entity, node->entity);
            }
        }

        if (isNodeOpen)
        {
            if (ImGui::IsItemClicked())
                m_selectedEntity = node->entity;

            if (!node->children.empty())
            {
                for (const auto& child : node->children)
                    DrawNode(&child);

                ImGui::TreePop();
            }
        }
    }

    void SceneGraphView::WindowContent()
    {
        ui::ScopedStyleColor styleColor({ { ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_CheckMark) } });

        for (const auto& x : m_root.children)
            DrawNode(&x);
    }

    void SceneGraphView::SetSelectedEntity(ecs::Entity entity)
    {
        scene::Scene& scene = scene::GetScene();

        // remove stencil ref on previous selection
        if (m_selectedEntity != ecs::INVALID_ENTITY)
        {
            scene::ObjectComponent* object = scene.objects.GetComponent(m_selectedEntity);
            if (object)
                object->SetUserStencilRef(0);
        }

        m_selectedEntity = entity;

        // add stencil ref on new selection
        scene::ObjectComponent* object = scene.objects.GetComponent(m_selectedEntity);
        if (object)
            object->SetUserStencilRef(8);
    }

    // Draw a collapsing header for entity components
    template <typename T>
    inline void InspectComponent(
        const char* label,
        ecs::ComponentManager<T>& components,
        const std::function<void(T*)> inspector,
        const ecs::Entity entity,
        const bool defaultOpen)
    {
        T* component = components.GetComponent(entity);
        if (!component)
            return;

        ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        if (ImGui::CollapsingHeader(label, flags))
        {
            ImGui::Indent();
            inspector(component);
            ImGui::Unindent();
        }
    }

    void EditEntityComponents(ecs::Entity entityID)
    {
        if (entityID == ecs::INVALID_ENTITY)
            return;

        scene::Scene& scene = scene::GetScene();

        scene::ObjectComponent* object = scene.objects.GetComponent(entityID);
        if (object)
        {
            if (ImGui::CollapsingHeader(ICON_FA_TAGS " Object"))
            {
                ImGui::Indent();
                InspectNameComponent(scene.names.GetComponent(entityID));
                InspectObjectComponent(object);
                ImGui::Unindent();
            }

            uint32_t meshIndex = object->meshIndex;
            if (ImGui::CollapsingHeader(ICON_FA_DICE_D6 " Mesh *"))
            {
                ImGui::Indent();
                meshIndex = SelectAndGetMeshIndexForObject(*object);
                ImGui::Separator();
                InspectMeshComponent(&scene.meshes[meshIndex]);
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader(ICON_FA_PALETTE " Materials *", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();
                const uint32_t materialIndex = SelectAndGetMaterialIndexForMesh(scene.meshes[meshIndex]);
                ImGui::Separator();
                InspectMaterialComponent(&scene.materials[materialIndex]);
                ImGui::Unindent();
            }
        }

        InspectComponent<scene::MeshComponent>(ICON_FA_DICE_D6" Mesh", scene.meshes, InspectMeshComponent, entityID, false);
        InspectComponent<scene::MaterialComponent>(ICON_FA_PALETTE " Material", scene.materials, InspectMaterialComponent, entityID, true);
        InspectComponent<scene::LightComponent>(ICON_FA_LIGHTBULB " Light", scene.lights, InspectLightComponent, entityID, true);
        InspectComponent<scene::TransformComponent>(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Transform", scene.transforms, InspectTransformComponent, entityID, false);
        InspectComponent<scene::HierarchyComponent>(ICON_FA_CODE_FORK " Hierarchy", scene.hierarchy, InspectHierarchyComponent, entityID, false);
        InspectComponent<scene::AnimationComponent>(ICON_FA_ROUTE " Animation", scene.animations, InspectAnimationComponent, entityID, false);
        InspectComponent<scene::WeatherComponent>(ICON_FA_CLOUD_RAIN " Weather", scene.weathers, InspectWeatherComponent, entityID, true);
    }

    //------------------------------------------------------------------------------

    ecs::Entity CreateDirectionalLight()
    {
        scene::Scene& scene = scene::GetScene();
        return scene.CreateLight(
            "Light_Directional_NEW",
            XMFLOAT3(0.0f, 70.0f, 0.0f),
            XMFLOAT3(1.0f, 1.0f, 1.0f),
            1.0f, 100.0f,
            scene::LightType::Directional);
    }

    ecs::Entity CreatePointLight()
    {
        scene::Scene& scene = scene::GetScene();
        ecs::Entity entity = scene.CreateLight(
            "Light_Point_NEW",
            XMFLOAT3(0.0f, 20.0f, 0.0f),
            XMFLOAT3(1.0f, 1.0f, 1.0f),
            1.0f, 100.0f,
            scene::LightType::Point);
        scenegraphView.SetSelectedEntity(entity);
        return entity;
    }

    // Clears the current scene and loads in a new from a selected file.
    // TODO: Add a dialog to prompt user about unsaved progress
    void OpenDialog_Open()
    {
        filesystem::OpenDialog(FILE_FILTER_SCD, [] (std::string filename) {
            eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=] (uint64_t) {
                Timer timer;
                scene::Scene& scene = scene::GetScene();
                scene.Clear();

                if (!SerializeFromFile(filename, scene))
                {
                    CYB_ERROR("Failed to serialize file: {}", filename);
                    return;
                }

                CYB_INFO("Serialized scene from file (filename={0}) in {1:.2f}ms", filename, timer.ElapsedMilliseconds());
            });
        });
    }

    // Import a new model to the scene, once the loading is complete
    // it will be automaticly selected in the scene graph view.
    void OpenDialog_ImportGLTF(const std::string filter)
    {
        filesystem::OpenDialog(filter, [] (std::string filename) {
            eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=] (uint64_t) {
                const std::string extension = filesystem::GetExtension(filename);
                if (filesystem::HasExtension(filename, "glb") || filesystem::HasExtension(filename, "gltf"))
                {
                    ecs::Entity entity = renderer::ImportModel_GLTF(filename, scene::GetScene());
                    scenegraphView.SetSelectedEntity(entity);
                }
            });
        });
    }

    void OpenDialog_ImportCSD(const std::string filter)
    {
        filesystem::OpenDialog(filter, [] (std::string filename) {
            eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=] (uint64_t) {
                const std::string extension = filesystem::GetExtension(filename);
                if (filesystem::HasExtension(filename, "csd"))
                {
                    //scene::LoadModel(filename);
                    CYB_WARNING("OpenDialog_ImportCSD: Loading .csd file from here is currently not working");
                }
            });
        });
    }

    void OpenDialog_SaveAs()
    {
        filesystem::SaveDialog(FILE_FILTER_SCD, [] (std::string filename) {
            if (!filesystem::HasExtension(filename, "csd"))
                filename += ".csd";

            Timer timer;
            if (SerializeToFile(filename, scene::GetScene(), true))
                CYB_INFO("Serialized scene to file (filename={0}) in {1:.2f}ms", filename, timer.ElapsedMilliseconds());
        });
    }

    static void DeleteSelectedEntity()
    {
        eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=] (uint64_t) {
            scene::GetScene().RemoveEntity(
                scenegraphView.GetSelectedEntity(),
                e_recursiveDelete.GetValue(),
                e_autoremoveLinkedEntities.GetValue());
        });
    }

    //------------------------------------------------------------------------------

    void ToolWindow::Draw()
    {
        if (!IsVisible())
            return;

        PreDraw();
        if (ImGui::Begin(GetWindowTitle(), &m_isVisible, m_windowFlags))
            WindowContent();
        ImGui::End();
        PostDraw();
    }

    class Tool_Profiler : public ToolWindow, private NonCopyable
    {
    public:
        Tool_Profiler(const std::string& title) :
            ToolWindow(title)
        {
        }

        void WindowContent() override
        {
            const ImU32 plotCpuColor = 0xff00ffff;
            const ImU32 plotGpuColor = 0xff0000ff;
            const auto& profilerContext = profiler::GetContext();
            const auto& cpuFrame = profilerContext.entries.find(profilerContext.cpuFrame);
            const auto& gpuFrame = profilerContext.entries.find(profilerContext.gpuFrame);

            ImGui::Text("Frame counter: %llu", rhi::GetDevice()->GetFrameCount());
            ImGui::Text("Average FPS (over %zu frames): %d", deltatimes.size(), avgFps);
            rhi::GraphicsDevice::MemoryUsage vram = rhi::GetDevice()->GetMemoryUsage();
            ImGui::Text("VRAM usage: %lluMB / %lluMB", vram.usage / 1024 / 1024, vram.budget / 1024 / 1024);

            ImGui::BeginTable("CPU/GPU Profiling", 2, ImGuiTableFlags_Borders);
            ImGui::TableNextColumn();
            ui::ScopedStyleColor cpuFrameLineColor({ { ImGuiCol_PlotLines, plotCpuColor } });
            const std::string cpuOverlayText = std::format("CPU Frame: {:.1f}ms", cpuFrame->second.time);
            ImGui::SetNextItemWidth(-1);
            ImGui::PlotLines("##CPUFrame", profilerContext.cpuFrameGraph.data(), profiler::FRAME_GRAPH_ENTRIES, 0, cpuOverlayText.c_str(), 0.0f, 16.0f, ImVec2(0, 100));
            ImGui::Spacing();
            ImGui::Text("CPU Frame: %.2fms", cpuFrame->second.time);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 8);
            ImGui::Indent();
            for (auto& [entryID, entry] : profilerContext.entries)
                if (entry.IsCPUEntry() && entryID != cpuFrame->first)
                    ImGui::Text("%s: %.2fms", entry.name.c_str(), entry.time);
            ImGui::Unindent();
            ImGui::PopStyleVar();

            ImGui::TableNextColumn();
            ui::ScopedStyleColor gpuFrameLineColor({ { ImGuiCol_PlotLines, plotGpuColor } });
            const std::string gpuOverlayText = std::format("GPU Frame: {:.1f}ms", gpuFrame->second.time);
            ImGui::SetNextItemWidth(-1);
            ImGui::PlotLines("##GPUFrame", profilerContext.gpuFrameGraph.data(), profiler::FRAME_GRAPH_ENTRIES, 0, gpuOverlayText.c_str(), 0.0f, 16.0f, ImVec2(0, 100));
            ImGui::Separator();
            ImGui::Text("GPU Frame: %.2fms", gpuFrame->second.time);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 8);
            ImGui::Indent();
            for (auto& [entryID, entry] : profilerContext.entries)
            {
                if (!entry.IsCPUEntry() && entryID != gpuFrame->first)
                    ImGui::Text("%s: %.2fms", entry.name.c_str(), entry.time);
            }

            ImGui::Unindent();
            ImGui::PopStyleVar();
            ImGui::EndTable();
        }
    };

    //------------------------------------------------------------------------------

    class Tool_LogDisplay : public ToolWindow, private NonCopyable
    {
    public:
        struct LogLine
        {
            ImVec4 color;
            std::string text;
        };

        class LogModule : public logger::OutputModule
        {
        public:
            LogModule(std::vector<LogLine>& messages) :
                m_messages(messages)
            {
            }

            [[nodiscard]] ImVec4 GetMessageColor(logger::Level level)
            {
                switch (level)
                {
                case logger::Level::Trace:      return ImVec4(0.45f, 0.65f, 1.0f, 1.0f);
                case logger::Level::Info:       return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                case logger::Level::Warning:    return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                case logger::Level::Error:      return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                }

                assert(0);
                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            }

            void Write(const logger::Message& msg) override
            {
                auto& newline = m_messages.emplace_back();
                newline.text = msg.text;
                newline.color = GetMessageColor(msg.severity);
            }

        private:
            std::vector<LogLine>& m_messages;
        };

        Tool_LogDisplay(const std::string& title) :
            ToolWindow(title)
        {
            logger::RegisterOutputModule<LogModule>(m_messages);
        }

        void WindowContent() override
        {
            ImGui::PushTextWrapPos(0.0f);
            for (const auto& msg : m_messages)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, msg.color);
                ImGui::TextUnformatted(msg.text.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::PopTextWrapPos();

            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }

    private:
        std::vector<LogLine> m_messages;
    };

    //------------------------------------------------------------------------------

    enum class EntityType
    {
        Names,
        Transforms,
        Groups,
        Hierarchies,
        Materials,
        Meshes,
        Objects,
        Lights,
        Cameras,
        Animations
    };

    static const std::unordered_map<EntityType, std::string> typeSelectCombo{
            { EntityType::Names,        "Names"         },
            { EntityType::Transforms,   "Transforms"    },
            { EntityType::Groups,       "Groups"        },
            { EntityType::Hierarchies,  "Hierarchies"   },
            { EntityType::Materials,    "Materials"     },
            { EntityType::Meshes,       "Meshes"        },
            { EntityType::Objects,      "Objects"       },
            { EntityType::Lights,       "Lights"        },
            { EntityType::Cameras,      "Cameras"       },
            { EntityType::Animations,   "Animations"    }
    };

    class Tool_ContentBrowser : public ToolWindow, private NonCopyable
    {
    public:
        Tool_ContentBrowser(const std::string& title) :
            ToolWindow(title)
        {
        }

        template <class T>
        void ShowEntities(const ecs::ComponentManager<T>& components)
        {
            const scene::Scene& scene = scene::GetScene();

            if (ImGui::BeginTable("components", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Usages");
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < components.Size(); ++i)
                {
                    const ecs::Entity ID = components.GetEntity(i);

                    ImGui::TableNextColumn();
                    ImGui::Text("%u", ID);

                    ImGui::TableNextColumn();
                    const scene::NameComponent* name = scene.names.GetComponent(ID);
                    ImGui::Text(name ? name->name.c_str() : "(none)");

                    ImGui::TableNextColumn();
                    ImGui::Text("(unknown)");
                }

                ImGui::EndTable();
            }
        }

        void WindowContent() override
        {
            const scene::Scene& scene = scene::GetScene();

            ui::ComboBox("Entity Type", m_selectedEntityType, typeSelectCombo);
            ImGui::Separator();

            switch (m_selectedEntityType)
            {
            case EntityType::Names:
                ShowEntities(scene.names);
                break;
            case EntityType::Transforms:
                ShowEntities(scene.transforms);
                break;
            case EntityType::Groups:
                ShowEntities(scene.groups);
                break;
            case EntityType::Hierarchies:
                ShowEntities(scene.hierarchy);
                break;
            case EntityType::Materials:
                ShowEntities(scene.materials);
                break;
            case EntityType::Meshes:
                ShowEntities(scene.meshes);
                break;
            case EntityType::Objects:
                ShowEntities(scene.objects);
                break;
            case EntityType::Lights:
                ShowEntities(scene.lights);
                break;
            case EntityType::Cameras:
                ShowEntities(scene.cameras);
                break;
            case EntityType::Animations:
                ShowEntities(scene.animations);
                break;
            }
        }

    private:
        EntityType m_selectedEntityType = EntityType::Names;
    };

    //------------------------------------------------------------------------------

    class Tool_CVarViewer : public ToolWindow, private NonCopyable
    {
    public:
        Tool_CVarViewer(const std::string& title) :
            ToolWindow(title)
        {
        }

        void WindowContent() override
        {
            if (ImGui::BeginTable("CVars", 4, ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Description");
                ImGui::TableHeadersRow();

                const auto& registry = GetCVarRegistry();

                for (const auto& [_, cvar] : registry)
                {
                    ImGui::TableNextColumn();
                    ImGui::Text(cvar->GetName().c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text(cvar->GetTypeAsString().c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text(cvar->GetValueAsString().c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text(cvar->GetDescription().c_str());
                }

                ImGui::EndTable();
            }
        }
    };

    //------------------------------------------------------------------------------

    class Tool_TerrainGeneration : public ToolWindow, private NonCopyable
    {
    public:
        TerrainGenerator generator;

        Tool_TerrainGeneration(const std::string& windowTitle) :
            ToolWindow(windowTitle, false, ImGuiWindowFlags_MenuBar)
        {
        }

        void WindowContent() override
        {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z))
                ui::GetUndoManager().Undo();
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y))
                ui::GetUndoManager().Redo();
            generator.DrawGui(scenegraphView.GetSelectedEntity());
        }
    };

    //------------------------------------------------------------------------------

    class Tool_Scenegraph : public ToolWindow, private NonCopyable
    {
    public:
        Tool_Scenegraph(const std::string& title) :
            ToolWindow(title)
        {
        }

        void WindowContent() override
        {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, 0, ImGuiInputFlags_RouteGlobalLow))
                ui::GetUndoManager().Undo();
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, 0, ImGuiInputFlags_RouteGlobalLow))
                ui::GetUndoManager().Redo();
            if (ImGui::Shortcut(ImGuiKey_Delete, 0, ImGuiInputFlags_RouteGlobalLow))
                DeleteSelectedEntity();

            ImGui::Text("Scene Hierarchy:");
            ImGui::BeginChild("Scene hierarchy", ImVec2(0, 300), ImGuiChildFlags_Border);
            scenegraphView.GenerateView();
            scenegraphView.WindowContent();
            ImGui::EndChild();

            ImGui::Text("Components:");
            const float componentChildHeight = Max(300.0f, ImGui::GetContentRegionAvail().y);
            ImGui::BeginChild("Components", ImVec2(0, componentChildHeight), ImGuiChildFlags_Border);
            EditEntityComponents(scenegraphView.GetSelectedEntity());
            ImGui::EndChild();
        }
    };

    //------------------------------------------------------------------------------

    std::vector<std::unique_ptr<ToolWindow>> tools;

    void AttachToolToMenu(std::unique_ptr<ToolWindow> tool)
    {
        tools.push_back(std::move(tool));
    }

    void DrawTools()
    {
        for (auto& x : tools)
            x->Draw();
    }

    //------------------------------------------------------------------------------

    static constexpr int WidgetWindowFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

    class ActionButtonMenu : public ToolWindow, private NonCopyable
    {
    public:
        const ImVec2 buttonSize = ImVec2(48, 42);

        ActionButtonMenu() :
            ToolWindow("ActionButtonMenu", true, WidgetWindowFlags),
            m_windowStyleVars({
                { ImGuiStyleVar_FrameRounding,      4.0f },
                { ImGuiStyleVar_FrameBorderSize,    0.0f },
                { ImGuiStyleVar_WindowBorderSize,   0.0f },
                { ImGuiStyleVar_ItemSpacing,        ImVec2(0.0f, 12.0f) }
            }),
            m_windowStyleColors({
                { ImGuiCol_WindowBg,                0xff000000 }
            })
        {
        }

        bool Button(const std::string& text, const std::string& tooltip, bool isSelected)
        {
            if (isSelected)
            {
                const ImU32 color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
            }

            ImGui::PushFont(imguiBigFont);
            bool clicked = ImGui::Button(text.c_str(), buttonSize);
            ImGui::PopFont();

            if (isSelected)
                ImGui::PopStyleColor(3);

            if (!tooltip.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltip.c_str());

            return clicked;
        }

        void PreDraw() override
        {
            m_windowStyleVars.PushStyleVars();
            m_windowStyleColors.PushStyleColors();

            const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowPos(ImVec2(16.0f, (viewportSize.y * 0.5f) - 260.0f));
        }

        void PostDraw()
        {
            m_windowStyleVars.PopStyleVars();
            m_windowStyleColors.PopStyleColors();
        }

        void WindowContent() override
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);
            if (Button(ICON_FA_ARROW_POINTER, "Select item", m_gizmoOp == 0))
                m_gizmoOp = (ImGuizmo::OPERATION)0;
            if (Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Move selected item", m_gizmoOp & ImGuizmo::TRANSLATE))
                m_gizmoOp = ImGuizmo::TRANSLATE;
            if (Button(ICON_FA_ARROW_ROTATE_LEFT, "Rotate selected item", m_gizmoOp & ImGuizmo::ROTATE))
                m_gizmoOp = ImGuizmo::ROTATE;
            if (Button(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE, "Scale selected item", m_gizmoOp & ImGuizmo::SCALEU))
                m_gizmoOp = ImGuizmo::SCALEU;
            ImGui::PopStyleVar();
        }

        ImGuizmo::OPERATION GetSelectedGizmoOp() const { return m_gizmoOp; }

    private:
        ImGuizmo::OPERATION m_gizmoOp = ImGuizmo::TRANSLATE;
        ui::StyleVarSet m_windowStyleVars;
        ui::StyleColorSet m_windowStyleColors;
    };

    class PerformanceVisualizer : public ToolWindow, private NonCopyable
    {
    public:
        const ImVec2 plotSize = ImVec2(250, 100);
        const ImU32 plotCpuColor = 0xff00ffff;
        const ImU32 plotGpuColor = 0xff0000ff;

        PerformanceVisualizer() :
            ToolWindow("PerfVis", true, WidgetWindowFlags | ImGuiWindowFlags_NoInputs)
        {
        }

        void PreDraw() override
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

            const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowPos(ImVec2(40.0f, viewportSize.y - 200.0f));
        }

        void PostDraw() override
        {
            ImGui::PopStyleVar(1);
            ImGui::PopStyleColor(1);
        }

        void WindowContent() override
        {
            const auto& profilerContext = profiler::GetContext();
            const auto& cpuFrame = profilerContext.entries.find(profilerContext.cpuFrame);
            const auto& gpuFrame = profilerContext.entries.find(profilerContext.gpuFrame);

            std::string cpuLabel = std::format("CPU {:.1f}ms", cpuFrame->second.time);
            std::string gpuLabel = std::format("GPU {:.1f}ms", gpuFrame->second.time);
            std::array<ui::PlotLineDesc, 2> plotDesc = { {
                { cpuLabel, plotCpuColor, profilerContext.cpuFrameGraph },
                { gpuLabel, plotGpuColor, profilerContext.gpuFrameGraph }
            } };

            ImGui::SetNextItemWidth(-1);
            PlotMultiLines("##PerfVis", plotDesc, nullptr, 0.0f, 10.0f, plotSize);
        }
    };

    ActionButtonMenu actionButtonMenu;
    PerformanceVisualizer performanceVisualizer;

    //------------------------------------------------------------------------------
    // Editor main window
    //------------------------------------------------------------------------------

    void DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New"))
                    eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=] (uint64_t) {
                    scene::GetScene().Clear();
                });
                if (ImGui::MenuItem("Open"))
                    OpenDialog_Open();
                if (ImGui::MenuItem("Save As..."))
                    OpenDialog_SaveAs();

                ImGui::Separator();

                if (ImGui::BeginMenu("Import"))
                {
                    if (ImGui::MenuItem("CybSceneData (.csd)"))
                        OpenDialog_ImportCSD(FILE_FILTER_SCD);
                    if (ImGui::MenuItem("glTF 2.0 (.gltf/.glb)"))
                        OpenDialog_ImportGLTF(FILE_FILTER_GLTF);

                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "ALT+F4"))
                    Exit(0);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "CTRL+Z", false, ui::GetUndoManager().CanUndo()))
                {
                    ui::GetUndoManager().Undo();
                }

                if (ImGui::MenuItem("Redo", "CTRL+Y", false, ui::GetUndoManager().CanRedo()))
                {
                    ui::GetUndoManager().Redo();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Clear Selection", nullptr, false, scenegraphView.GetSelectedEntity() != ecs::INVALID_ENTITY))
                    scenegraphView.SetSelectedEntity(ecs::INVALID_ENTITY);
                
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Unused Entities", nullptr, false))
                    scene::GetScene().RemoveUnusedEntities();

                ImGui::Separator();

                if (ImGui::BeginMenu("Add"))
                {
                    if (ImGui::MenuItem("Directional Light"))
                        CreateDirectionalLight();
                    if (ImGui::MenuItem("Point Light"))
                        CreatePointLight();
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Detach From Parent"))
                    scene::GetScene().ComponentDetach(scenegraphView.GetSelectedEntity());
                if (ImGui::MenuItem("Delete", "Del"))
                    DeleteSelectedEntity();
                ImGui::MenuItem("Duplicate (!!)", "CTRL+D");

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                bool showWidget = actionButtonMenu.IsVisible();
                if (ImGui::MenuItem("Action Buttons", NULL, &showWidget))
                    actionButtonMenu.SetVisible(showWidget);
                showWidget = performanceVisualizer.IsVisible();
                if (ImGui::MenuItem("Performance Visualizer", NULL, &showWidget))
                    performanceVisualizer.SetVisible(showWidget);
                ImGui::MenuItem("CubeView transform", NULL, &displayCubeView);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Renderer"))
            {
                if (ImGui::BeginMenu("Debug"))
                {
                    bool temp = r_debugObjectAABB->GetValue();
                    if (ImGui::Checkbox("Draw Object AABB", &temp))
                        r_debugObjectAABB->SetValue(temp);
                    temp = r_debugLightSources->GetValue();
                    if (ImGui::Checkbox("Draw Lightsources", &temp))
                        r_debugLightSources->SetValue(temp);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Resolution"))
                {
                    for (size_t i = 0; i < videoModeList.size(); i++)
                    {
                        VideoModeInfo& mode = videoModeList[i];
                        std::string str = std::format("{}x{} {}bpp @ {}hz", mode.width, mode.height, mode.bitsPerPixel, mode.displayFrequency);
                        if (ImGui::MenuItem(str.c_str()))
                        {
                            eventsystem::FireEvent(eventsystem::Event_SetFullScreen, i);
                            fullscreenEnabled = true;
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::Checkbox("Fullscreen", &fullscreenEnabled))
                {
                }

                bool vsyncEnabled = r_vsync->GetValue();
                if (ImGui::Checkbox("VSync", &vsyncEnabled))
                    r_vsync->SetValue(vsyncEnabled);

                ImGui::Separator();

                if (ImGui::MenuItem("Reload Shaders"))
                    renderer::ReloadShaders();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                for (auto& x : tools)
                {
                    bool showWindow = x->IsVisible();
                    if (ImGui::MenuItem(x->GetWindowTitle(), NULL, &showWindow))
                        x->SetVisible(showWindow);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    // windowID is only used for recording undo commands
    static void DrawGizmo(const ImGuiID windowID)
    {
        static bool isUsingGizmo = false;
        const ImGuiIO& io = ImGui::GetIO();
        scene::Scene& scene = scene::GetScene();
        scene::CameraComponent& camera = scene::GetCamera();

        const ecs::Entity entity = scenegraphView.GetSelectedEntity();
        scene::TransformComponent* transform = scene.transforms.GetComponent(entity);

        XMFLOAT4X4 world{};
        if (transform)
            world = transform->world;

        const bool isEnabled = (transform != nullptr);
        ImGuizmo::Enable(isEnabled);
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        ImGuizmo::Manipulate(
            &camera.view._11,
            &camera.projection._11,
            gizmoOperation,
            ImGuizmo::WORLD,
            &world._11);

        if (displayCubeView)
        {
            ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
            ImGuizmo::ViewManipulate(
                &camera.view._11,
                &camera.projection._11,
                gizmoOperation,
                ImGuizmo::WORLD,
                &world._11,
                1.0f,
                ImVec2(viewportSize.x - 110.0f, 30.0f),
                ImVec2(100.0f, 100.0f),
                0x00000000);
        }

        if (ImGuizmo::IsUsing() && isEnabled)
        {
            if (!isUsingGizmo)
                ui::GetUndoManager().EmplaceAction<ui::ModifyValue<scene::TransformComponent>>(windowID, transform);

            transform->world = world;
            transform->ApplyTransform();

            // Transform to local space if parented
            const scene::HierarchyComponent* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy)
            {
                const scene::TransformComponent* parent_transform = scene.transforms.GetComponent(hierarchy->parentID);
                if (parent_transform != nullptr)
                    transform->MatrixTransform(XMMatrixInverse(nullptr, XMLoadFloat4x4(&parent_transform->world)));
            }

            isUsingGizmo = true;
        }
        else
        {
            if (isUsingGizmo)
            {
                ui::GetUndoManager().CommitIncompleteAction();
                isUsingGizmo = false;
            }
        }
    }

    static Ray GetPickRay(float cursorX, float cursorY)
    {
        const scene::CameraComponent& camera = scene::GetCamera();
        ImGuiIO& io = ImGui::GetIO();

        float screenW = io.DisplaySize.x;
        float screenH = io.DisplaySize.y;

        XMMATRIX V = XMLoadFloat4x4(&camera.view);
        XMMATRIX P = XMLoadFloat4x4(&camera.projection);
        XMMATRIX W = XMMatrixIdentity();
        XMVECTOR lineStart = XMVector3Unproject(XMVectorSet(cursorX, cursorY, 1, 1), 0, 0, screenW, screenH, 0.0f, 1.0f, P, V, W);
        XMVECTOR lineEnd = XMVector3Unproject(XMVectorSet(cursorX, cursorY, 0, 1), 0, 0, screenW, screenH, 0.0f, 1.0f, P, V, W);
        XMVECTOR rayDirection = XMVector3Normalize(XMVectorSubtract(lineEnd, lineStart));
        return Ray(lineStart, rayDirection);
    }

    //------------------------------------------------------------------------------
    // PUBLIC API
    //------------------------------------------------------------------------------

    void Initialize()
    {
        // Attach built-in tools
        AttachToolToMenu(std::make_unique<Tool_Scenegraph>("Scenegraph & Components"));
        AttachToolToMenu(std::make_unique<Tool_TerrainGeneration>("Terrain Generator"));
        AttachToolToMenu(std::make_unique<Tool_ContentBrowser>("Scene Content Browser"));
        AttachToolToMenu(std::make_unique<Tool_Profiler>("Profiler"));
        AttachToolToMenu(std::make_unique<Tool_CVarViewer>("CVar viewer"));
        AttachToolToMenu(std::make_unique<Tool_LogDisplay>("Backlog"));

        r_vsync = FindCVar<bool>(hash::String("r_vsync"));
        r_debugObjectAABB = FindCVar<bool>(hash::String("r_debugObjectAABB"));
        r_debugLightSources = FindCVar<bool>(hash::String("r_debugLightSources"));
#if 1
        // ImGuizmo style
        ImGuizmo::Style& style = ImGuizmo::GetStyle();
        style.TranslationLineThickness = 6.0f;
        style.TranslationLineArrowSize = 12.0f;
        style.RotationLineThickness = 5.0f;
        style.RotationOuterLineThickness = 6.0f;
        style.Colors[ImGuizmo::PLANE_X].w = 0.6f;
        style.Colors[ImGuizmo::PLANE_Y].w = 0.6f;
        style.Colors[ImGuizmo::PLANE_Z].w = 0.6f;
        ImGuizmo::AllowAxisFlip(false);
#endif

        // Grab available fullscreen resolutions
        GetVideoModesForDisplay(videoModeList, 0);

        initialized = true;
    }

    static void UpdateFPSCounter(double dt)
    {
        deltatimes[fpsAgvCounter++ % deltatimes.size()] = dt;
        if (fpsAgvCounter > deltatimes.size())
        {
            double avgTime = std::accumulate(deltatimes.begin(), deltatimes.end(), 0.0) / deltatimes.size();
            avgFps = (uint32_t)std::round(1.0 / avgTime);
        }
    }

    struct PerlinNode : public ui::NG_Node
    {
        PerlinNode() :
            NG_Node("Perlin Noise")
        {
            AddOutputPin<noise2::NoiseNode*>("Output", [=] () {return &noise; });
            Pos = { 100, 500 };             // REMOVE
        }

        void DisplayContent(float zoom) override
        {
            const float childWidth = 240 * zoom;
            ImGui::PushItemWidth(childWidth);

            auto onChange = [=] () { ModifiedFlag = true; };

            uint32_t iTemp = noise.GetSeed();
            if (ui::SliderInt("Seed", (int*)&iTemp, onChange, 0, std::numeric_limits<int>::max() / 2))
                noise.SetSeed(iTemp);

            float fTemp = noise.GetFrequency();
            if (ui::SliderFloat("Frequency", &fTemp, onChange, 0.1f, 8.0f))
                noise.SetFrequency(fTemp);

            iTemp = noise.GetOctaves();
            if (ui::SliderInt("Octaves", (int*)&iTemp, onChange, 1, 6))
                noise.SetOctaves(iTemp);

            fTemp = noise.GetLacunarity();
            if (ui::SliderFloat("Lacunarity", &fTemp, onChange, 0.0f, 4.0f))
                noise.SetLacunarity(fTemp);

            fTemp = noise.GetPersistance();
            if (ui::SliderFloat("Persistance", &fTemp, onChange, 0.0f, 4.0f))
                noise.SetPersistence(fTemp);

            ImGui::PopItemWidth();
        }

        noise2::NoiseNode_Perlin noise;
    };

    struct CellularNode : public ui::NG_Node
    {
        CellularNode() :
            NG_Node("Cellular Noise")
        {
            AddOutputPin<noise2::NoiseNode*>("Output", [=] () {return &noise; });
        }

        void DisplayContent(float zoom) override
        {
            const float childWidth = 240 * zoom;
            ImGui::PushItemWidth(childWidth);

            auto onChange = [=] () { ModifiedFlag = true; };

            uint32_t iTemp = noise.GetSeed();
            if (ui::SliderInt("Seed", (int*)&iTemp, onChange, 0, std::numeric_limits<int>::max() / 2))
                noise.SetSeed(iTemp);

            float fTemp = noise.GetFrequency();
            if (ui::SliderFloat("Frequency", &fTemp, onChange, 0.1f, 8.0f))
                noise.SetFrequency(fTemp);

            fTemp = noise.GetJitterModifier();
            if (ui::SliderFloat("Jitter", &fTemp, onChange, 0.0f, 2.0f))
                noise.SetJitterModifier(fTemp);

            iTemp = noise.GetOctaves();
            if (ui::SliderInt("Octaves", (int*)&iTemp, onChange, 1, 6))
                noise.SetOctaves(iTemp);

            fTemp = noise.GetLacunarity();
            if (ui::SliderFloat("Lacunarity", &fTemp, onChange, 0.0f, 4.0f))
                noise.SetLacunarity(fTemp);

            fTemp = noise.GetPersistance();
            if (ui::SliderFloat("Persistance", &fTemp, onChange, 0.0f, 4.0f))
                noise.SetPersistence(fTemp);

            ImGui::PopItemWidth();
        }

        noise2::NoiseNode_Cellular noise;
    };

    class PreviewNode : public ui::NG_Node
    {
    public:
        PreviewNode() :
            NG_Node("Preivew")
        {
            AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
                m_input = from.value_or(nullptr);
                Update();
            });

            Pos = { 600, 400 };         // REMOVE
        }

        void UpdatePreview()
        {
            if (!m_input || !ValidState)
                return;

            Timer timer;
            auto image = noise2::RenderNoiseImage(noise2::NoiseImageDesc()
                .SetInput(m_input)
                .SetSize(m_previewSize, m_previewSize)
                .SetOffset(0, 0)
                .SetFrequencyScale(m_freqScale));

            rhi::TextureDesc desc{};
            desc.width = image->GetWidth();
            desc.height = image->GetHeight();
            desc.format = rhi::Format::RGBA8_UNORM;

            rhi::SubresourceData subresourceData;
            subresourceData.mem = image->GetConstPtr(0);
            subresourceData.rowPitch = image->GetStride();

            rhi::GetDevice()->CreateTexture(&desc, &subresourceData, &m_texture);
            m_lastPreviewGenerationTime = timer.ElapsedMilliseconds();
        }

        void Update() override
        {
            if (m_autoUpdate)
                UpdatePreview();
        }

        void DisplayContent(float zoom) override
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            const float childWidth = std::max(256u, m_previewSize) * zoom;
            ImGui::PushItemWidth(childWidth);

            if (ui::Checkbox("Auto Update", &m_autoUpdate, nullptr) && m_autoUpdate)
                UpdatePreview();
            if (!m_autoUpdate && ImGui::Button("Update", ImVec2(childWidth, 0)))
                UpdatePreview();

            const float image_width = childWidth;
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const ImVec2 p1 = p0 + ImVec2(image_width, image_width);
            if (m_input && ValidState && m_texture.IsValid())
            {
                ImGui::Image((ImTextureID)&m_texture, ImVec2(image_width, image_width));
            }
            else
            {
                ImGui::ItemAdd(ImRect(p0, p1), 12);
                ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, 0xff222222);
                ImGui::SetCursorScreenPos(p0 + ImVec2(0, image_width + style.ItemSpacing.y * zoom));
            }

            ImGui::Text("Updated in %.2fms", m_lastPreviewGenerationTime);
            if (ui::DragInt("Preview Size", (int*)&m_previewSize, 1, 4, 512))
                Update();
            if (ui::SliderFloat("Freq Scale", &m_freqScale, nullptr, 1.0f, 12.0f))
                Update();

            ImGui::PopItemWidth();
        }

    private:
        bool m_autoUpdate = true;
        uint32_t m_previewSize = 128;   // Square, (width, height) = previewSize
        float m_lastPreviewGenerationTime = 0.0f;
        float m_freqScale = 8.0f;
        rhi::Texture m_texture;
        noise2::NoiseNode* m_input = nullptr;
    };

    class ScaleBiasNode : public ui::NG_Node
    {
    public:
        ScaleBiasNode() :
            NG_Node("ScaleBias")
        {
            AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
                m_scaleBias.SetInput(0, from.value_or(nullptr));
            });
            AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_scaleBias; });
        }

        void DisplayContent(float zoom) override
        {
            const float childWidth = 160 * zoom;
            ImGui::PushItemWidth(childWidth);

            auto onChange = [=] () { ModifiedFlag = true; };

            float fTemp = m_scaleBias.GetScale();
            if (ui::SliderFloat("Scale", &fTemp, onChange, 0.0f, 2.0f))
                m_scaleBias.SetScale(fTemp);
            
            fTemp = m_scaleBias.GetBias();
            if (ui::SliderFloat("Bias", &fTemp, onChange, 0.0f, 1.0f))
                m_scaleBias.SetBias(fTemp);

            ImGui::PopItemWidth();
        }

    private:
        noise2::NoiseNode_ScaleBias m_scaleBias;
    };

    class StrataNode : public ui::NG_Node
    {
    public:
        StrataNode() :
            NG_Node("Strata")
        {
            AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
                m_strata.SetInput(0, from.value_or(nullptr));
            });
            AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_strata; });

            Pos = { 300, 300 };     // REMOVE
        }

        void DisplayContent(float zoom) override
        {
            const float childWidth = 160 * zoom;
            ImGui::PushItemWidth(childWidth);

            auto onChange = [=]() { ModifiedFlag = true; };

            float fTemp = m_strata.GetStrata();
            if (ui::SliderFloat("Strata", &fTemp, onChange, 2.0f, 12.0f))
                m_strata.SetStrata(fTemp);
         
            ImGui::PopItemWidth();
        }

    private:
        noise2::NoiseNode_Strata m_strata;
    };

    class SelectNode : public ui::NG_Node
    {
    public:
        SelectNode() :
            NG_Node("Select")
        {
            AddInputPin<noise2::NoiseNode*>("Input A", [&] (std::optional<noise2::NoiseNode*> from) {
                m_select.SetInput(0, from.value_or(nullptr));
            });
            AddInputPin<noise2::NoiseNode*>("Input B", [&] (std::optional<noise2::NoiseNode*> from) {
                m_select.SetInput(1, from.value_or(nullptr));
            });
            AddInputPin<noise2::NoiseNode*>("Control", [&] (std::optional<noise2::NoiseNode*> from) {
                m_select.SetInput(2, from.value_or(nullptr));
            });
            AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_select; });
        }

        void DisplayContent(float zoom) override
        {
            const float childWidth = 160 * zoom;
            ImGui::PushItemWidth(childWidth);

            auto onChange = [=] () { ModifiedFlag = true; };

            float fTemp = m_select.GetThreshold();
            if (ui::SliderFloat("Threshold", &fTemp, onChange, 0.0f, 1.0f))
                m_select.SetThreshold(fTemp);

            fTemp = m_select.GetEdgeFalloff();
            if (ui::SliderFloat("Edge Falloff", &fTemp, onChange, 0.0f, 1.0f))
                m_select.SetEdgeFalloff(fTemp);

            ImGui::PopItemWidth();
        }

    private:
        noise2::NoiseNode_Select m_select;
    };

    void Update(bool showGui, double dt)
    {
        if (!initialized)
            return;

        UpdateFPSCounter(dt);

        // if we won't show the gui, don't bother processing it
        if (!showGui)
            return;

        ImGuizmo::BeginFrame();

        DrawMenuBar();
        actionButtonMenu.Draw();
        performanceVisualizer.Draw();
        gizmoOperation = actionButtonMenu.GetSelectedGizmoOp();

        // Create an invisible dummy window for recording actions for the
        // undo manager for the 3d viewport
        ImGuiID gizmoWindowID = 0;
        if (ImGui::Begin("##viewportDummy", 0, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings))
        {
            // use windowID for gizmo undo commands
            gizmoWindowID = ImGui::GetCurrentWindow()->ID;

            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, 0, ImGuiInputFlags_RouteGlobalLow))
                ui::GetUndoManager().Undo();
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, 0, ImGuiInputFlags_RouteGlobalLow))
                ui::GetUndoManager().Redo();
            if (ImGui::Shortcut(ImGuiKey_Delete, 0, ImGuiInputFlags_RouteGlobalLow))
                DeleteSelectedEntity();
            ImGui::End();
        }

        ///
        ///    ============== NG_* TEST CODE ==============
        ///
#if 1
        if (ImGui::Begin("Node Editor DEV", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            static ui::NG_Canvas nodeCanvas;

            // if PinCounter == 0 we haven't yet initialized
            if (nodeCanvas.Nodes.size() == 0)
            {
                nodeCanvas.Flags = ui::NG_CanvasFlags_DisplayGrid;
                //nodeCanvas.Flags |= ui::NG_CanvasFlags_DisplayState;

                auto test3 = std::make_unique<PerlinNode>();
                nodeCanvas.Nodes.push_back(std::move(test3));
                nodeCanvas.Nodes.push_back(std::make_unique<PreviewNode>());
                nodeCanvas.Nodes.push_back(std::make_unique<StrataNode>());

                nodeCanvas.Factory.Register("Perlin",    [] () { return std::make_unique<PerlinNode>();    });
                nodeCanvas.Factory.Register("Cellular",  [] () { return std::make_unique<CellularNode>();  });
                nodeCanvas.Factory.Register("ScaleBias", [] () { return std::make_unique<ScaleBiasNode>(); });
                nodeCanvas.Factory.Register("Strata",    [] () { return std::make_unique<StrataNode>();    });
                nodeCanvas.Factory.Register("Select",    [] () { return std::make_unique<SelectNode>();    });
                nodeCanvas.Factory.Register("Preview",   [] () { return std::make_unique<PreviewNode>();   });
            }


            ui::NodeGraph(nodeCanvas);
        }
        ImGui::End();
#endif

        ///
        ///
        ///
       
        // Only draw gizmo with a valid entity containing transform component
        if (scene::GetScene().transforms.GetComponent(scenegraphView.GetSelectedEntity()))
            DrawGizmo(gizmoWindowID);

        // check if mouse is in 3d view (not over any capturing gui frame)
        ImGuiIO& io = ImGui::GetIO();
        const bool isMouseIn3DView = !io.WantCaptureMouse && !ImGuizmo::IsOver(gizmoOperation);

        // clear selection with escape
        if (isMouseIn3DView && ImGui::IsKeyPressed(ImGuiKey_Escape))
            scenegraphView.SetSelectedEntity(ecs::INVALID_ENTITY);

        // Pick on left mouse click
        if (isMouseIn3DView && io.MouseClicked[0])
        {
            const scene::Scene& scene = scene::GetScene();
            Ray pick_ray = GetPickRay(io.MousePos.x, io.MousePos.y);
            scene::PickResult pick_result = scene::Pick(scene, pick_ray);

            // Enable mouse picking on lightsources only if they are being drawn
            if (r_debugLightSources->GetValue())
            {
                for (size_t i = 0; i < scene.lights.Size(); ++i)
                {
                    const auto& light = scene.lights[i];
                    const XMVECTOR lightPos = XMLoadFloat3(&light.position);
                    const float dist = XMVectorGetX(XMVector3LinePointDistance(pick_ray.GetOrigin(), pick_ray.GetOrigin() + pick_ray.GetDirection(), lightPos));
                    if (dist > 0.01f && dist < Distance(lightPos, pick_ray.GetOrigin()) * 0.05f && dist < pick_result.distance)
                    {
                        pick_result = scene::PickResult();
                        pick_result.entity = scene.lights.GetEntity(i);
                        pick_result.distance = dist;
                    }
                }
            }

            if (pick_result.entity != ecs::INVALID_ENTITY)
                scenegraphView.SetSelectedEntity(pick_result.entity);
            else
                scenegraphView.SetSelectedEntity(ecs::INVALID_ENTITY);
        }

        DrawTools();
    }

    bool WantInput()
    {
        if (ImGui::GetCurrentContext() != nullptr)
        {
            ImGuiIO& io = ImGui::GetIO();
            return io.WantCaptureMouse || io.WantCaptureKeyboard;
        }

        return false;
    }
}