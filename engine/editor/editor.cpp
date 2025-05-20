#define IMGUI_DEFINE_MATH_OPERATORS
#include <stack>
#include <numeric>
#include <format>
#include "core/cvar.h"
#include "core/logger.h"
#include "core/timer.h"
#include "core/filesystem.h"
#include "core/mathlib.h"
#include "graphics/renderer.h"
#include "graphics/model_import.h"
#include "systems/event_system.h"
#include "systems/profiler.h"
#include "editor/editor.h"
#include "editor/imgui_backend.h"
#include "editor/undo_manager.h"
#include "editor/icons_font_awesome6.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "imgui_stdlib.h"
#include "ImGuizmo.h"
#include "editor/widgets.h"
#include "editor/terrain_generator.h"

using namespace std::string_literals;

// filters are passed to FileDialog function as std::string, thus we 
// need to use string literals to avoid them of being stripped
#define FILE_FILTER_ALL             "All Files (*.*)\0*.*\0"s
#define FILE_FILTER_SCENE           "Cyb Scene Data Files (*.csd)\0*.csd\0"s
#define FILE_FILTER_GLTF            "GLTF Files (*.gltf; *.glb)\0*.gltf;*.glb\0"s
#define FILE_FILTER_IMPORT_MODEL    FILE_FILTER_GLTF FILE_FILTER_SCENE FILE_FILTER_ALL

namespace cyb::editor
{
    bool initialized = false;
    bool fullscreenEnabled = false; // FIXME: initial value has to be synced with Application::fullscreenEnabled

    CVar* r_vsync = nullptr;
    CVar* r_debugObjectAABB = nullptr;
    CVar* r_debugLightSources = nullptr;

    Resource import_icon;
    Resource delete_icon;
    Resource light_icon;
    Resource editor_icon_select;
    Resource translate_icon;
    Resource rotate_icon;
    Resource scale_icon;

    graphics::Texture rtSelectionOutline;

    ImGuizmo::OPERATION guizmo_operation = ImGuizmo::TRANSLATE;
    bool guizmo_world_mode = true;
    SceneGraphView scenegraphView;
    std::vector<VideoMode> videoModeList;

    // fps counter data
    double deltatimes[100] = {};
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
        filter.WindowContent("##filter");

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

    void InspectAABBComponent(const spatial::AxisAlignedBox* aabb)
    {
        XMFLOAT3 min = {};
        XMFLOAT3 max = {};
        XMStoreFloat3(&min, aabb->GetMin());
        XMStoreFloat3(&max, aabb->GetMax());
        ImGui::Text("Min: [%.2f, %.2f, %.2f]", min.x, min.y, min.z);
        ImGui::Text("Max: [%.2f, %.2f, %.2f]", max.x, max.y, max.z);
        ImGui::Text("Width: %.2fm", max.x - min.x);
        ImGui::Text("Height: %.2fm", max.y - min.y);
        ImGui::Text("Depth: %.2fm", max.z - min.z);
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
        ui::ScopedStyleColor colorGuard(ImGuiCol_Header, ImVec4(0.38f, 0.58f, 0.71f, 0.94f));

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

            if (ImGui::CollapsingHeader(ICON_FA_PALETTE " Materials *"))
            {
                ImGui::Indent();
                const uint32_t materialIndex = SelectAndGetMaterialIndexForMesh(scene.meshes[meshIndex]);
                ImGui::Separator();
                InspectMaterialComponent(&scene.materials[materialIndex]);
                ImGui::Unindent();
            }
        }

        InspectComponent<scene::MeshComponent>(ICON_FA_DICE_D6" Mesh", scene.meshes, InspectMeshComponent, entityID, false);
        InspectComponent<scene::MaterialComponent>(ICON_FA_PALETTE " Material", scene.materials, InspectMaterialComponent, entityID, false);
        InspectComponent<scene::LightComponent>(ICON_FA_LIGHTBULB " Light", scene.lights, InspectLightComponent, entityID, true);
        InspectComponent<scene::TransformComponent>(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Transform", scene.transforms, InspectTransformComponent, entityID, false);
        InspectComponent<spatial::AxisAlignedBox>(ICON_FA_EXPAND " AABB##edit_object_aabb", scene.aabb_objects, InspectAABBComponent, entityID, false);
        InspectComponent<spatial::AxisAlignedBox>(ICON_FA_EXPAND " AABB##edit_light_aabb", scene.aabb_lights, InspectAABBComponent, entityID, false);
        InspectComponent<scene::HierarchyComponent>(ICON_FA_CODE_FORK " Hierarchy", scene.hierarchy, InspectHierarchyComponent, entityID, false);
        InspectComponent<scene::AnimationComponent>(ICON_FA_ROUTE " Animation", scene.animations, InspectAnimationComponent, entityID, false);
        InspectComponent<scene::WeatherComponent>(ICON_FA_CLOUD_RAIN " Weather", scene.weathers, InspectWeatherComponent, entityID, true);
    }

    //------------------------------------------------------------------------------

    void ToolWindow::Draw()
    {
        if (IsHidden())
            return;

        if (ImGui::Begin(GetWindowTitle(), &m_isVisible, m_windowFlags))
            WindowContent();
            
        ImGui::End();
    }

    class Tool_Profiler : public ToolWindow
    {
    public:
        Tool_Profiler(const std::string& title) :
            ToolWindow(title)
        {
        }

        virtual void WindowContent() override
        {
            const auto& profilerContext = profiler::GetContext();
            const auto& cpuFrame = profilerContext.entries.find(profilerContext.cpuFrame);
            const auto& gpuFrame = profilerContext.entries.find(profilerContext.gpuFrame);

            ImGui::Text("Frame counter: %llu", graphics::GetDevice()->GetFrameCount());
            ImGui::Text("Average FPS (over %zu frames): %d", CountOf(deltatimes), avgFps);
            graphics::GraphicsDevice::MemoryUsage vram = graphics::GetDevice()->GetMemoryUsage();
            ImGui::Text("VRAM usage: %lluMB / %lluMB", vram.usage / 1024 / 1024, vram.budget / 1024 / 1024);

            ImGui::BeginTable("CPU/GPU Profiling", 2, ImGuiTableFlags_Borders);
            ImGui::TableNextColumn();
            ui::ScopedStyleColor cpuFrameLineColor(ImGuiCol_PlotLines, ImColor(255, 0, 0));
            const std::string cpuOverlayText = std::format("CPU Frame: {:.1f}ms", cpuFrame->second.time);
            ImGui::SetNextItemWidth(-1);
            ImGui::PlotLines("##CPUFrame", profilerContext.cpuFrameGraph, profiler::FRAME_GRAPH_ENTRIES, 0, cpuOverlayText.c_str(), 0.0f, 16.0f, ImVec2(0, 100));
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
            ui::ScopedStyleColor gpuFrameLineColor(ImGuiCol_PlotLines, ImColor(0, 0, 255));
            const std::string gpuOverlayText = std::format("GPU Frame: {:.1f}ms", gpuFrame->second.time);
            ImGui::SetNextItemWidth(-1);
            ImGui::PlotLines("##GPUFrame", profilerContext.gpuFrameGraph, profiler::FRAME_GRAPH_ENTRIES, 0, gpuOverlayText.c_str(), 0.0f, 16.0f, ImVec2(0, 100));
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

    class Tool_LogDisplay : public ToolWindow
    {
    public:
        Tool_LogDisplay(const std::string& title) :
            ToolWindow(title)
        {
            logger::RegisterOutputModule<logger::OutputModule_StringBuffer>(&m_textBuffer);
        }

        void WindowContent() override
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(m_textBuffer.c_str());
            ImGui::PopTextWrapPos();

            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }

    private:
        std::string m_textBuffer;
    };

    //------------------------------------------------------------------------------

    class Tool_ContentBrowser : public ToolWindow
    {
    public:
        Tool_ContentBrowser(const std::string& title) :
            ToolWindow(title)
        {
        }

        virtual void WindowContent() override
        {
            const scene::Scene& scene = scene::GetScene();

            ImGui::Text("Meshes:");
            for (size_t i = 0; i < scene.meshes.Size(); ++i)
            {
                const ecs::Entity entityID = scene.meshes.GetEntity(i);
                const std::string& name = scene.names.GetComponent(entityID)->name;

                ImGui::Text("%s\n", name.c_str());
            }

            ImGui::Separator();

            ImGui::Text("Materials:");
            for (size_t i = 0; i < scene.materials.Size(); ++i)
            {
                const ecs::Entity entityID = scene.materials.GetEntity(i);
                const std::string& name = scene.names.GetComponent(entityID)->name;

                ImGui::Text("%s [%u]\n", name.c_str(), entityID);
            }
        }
    };

    //------------------------------------------------------------------------------

    class Tool_CVarViewer : public ToolWindow
    {
    public:
        Tool_CVarViewer(const std::string& title) :
            ToolWindow(title)
        {
        }

        virtual void WindowContent() override
        {
            if (ImGui::BeginTable("CVars", 4, ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Description");
                ImGui::TableHeadersRow();

                const auto& registry = cvar_system::GetRegistry();

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

    class Tool_TerrainGeneration : public ToolWindow
    {
    public:
        TerrainGenerator generator;

        Tool_TerrainGeneration(const std::string& windowTitle) :
            ToolWindow(windowTitle, ImGuiWindowFlags_MenuBar)
        {
        }

        virtual void WindowContent() override
        {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z))
                ui::GetUndoManager().Undo();
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y))
                ui::GetUndoManager().Redo();
            generator.DrawGui(scenegraphView.GetSelectedEntity());
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
        filesystem::OpenDialog(FILE_FILTER_SCENE, [] (std::string filename) {
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
    void OpenDialog_ImportModel(const std::string filter)
    {
        filesystem::OpenDialog(filter, [] (std::string filename) {
            eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=] (uint64_t) {
                const std::string extension = filesystem::GetExtension(filename);
                if (filesystem::HasExtension(filename, "csb"))
                {
                    //scene::LoadModel(filename);
                    CYB_WARNING("OpenDialog_ImportModel: Loading .csb file from here is currently not working");
                }
                else if (filesystem::HasExtension(filename, "glb") || filesystem::HasExtension(filename, "gltf"))
                {
                    ecs::Entity entity = renderer::ImportModel_GLTF(filename, scene::GetScene());
                    scenegraphView.SetSelectedEntity(entity);
                }
            });
        });
    }

    void OpenDialog_SaveAs()
    {
        filesystem::SaveDialog(FILE_FILTER_SCENE, [] (std::string filename) {
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
            scene::GetScene().RemoveEntity(scenegraphView.GetSelectedEntity(), true);
            scenegraphView.SetSelectedEntity(ecs::INVALID_ENTITY);
        });
    }

    //------------------------------------------------------------------------------
    // Editor main window
    //------------------------------------------------------------------------------

    void DrawMenuBar()
    {
        if (ImGui::BeginMenuBar())
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
                    if (ImGui::MenuItem("Model (.gltf/.glb/.csd)"))
                        OpenDialog_ImportModel(FILE_FILTER_IMPORT_MODEL);

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

                if (ImGui::BeginMenu("Add"))
                {
                    if (ImGui::MenuItem("Directional Light"))
                        CreateDirectionalLight();
                    if (ImGui::MenuItem("Point Light"))
                        CreatePointLight();
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Detach from parent"))
                    scene::GetScene().ComponentDetach(scenegraphView.GetSelectedEntity());
                if (ImGui::MenuItem("Delete", "Del"))
                    DeleteSelectedEntity();
                ImGui::MenuItem("Duplicate (!!)", "CTRL+D");

                ImGui::Separator();

                if (ImGui::MenuItem("Reload Shaders"))
                    renderer::ReloadShaders();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Renderer"))
            {
                if (ImGui::BeginMenu("Debug"))
                {
                    bool temp = r_debugObjectAABB->GetValue<bool>();
                    if (ImGui::Checkbox("Draw Object AABB", &temp))
                        r_debugObjectAABB->SetValue(temp);
                    temp = r_debugLightSources->GetValue<bool>();
                    if (ImGui::Checkbox("Draw Lightsources", &temp))
                        r_debugLightSources->SetValue(temp);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Resolution"))
                {
                    for (size_t i = 0; i < videoModeList.size(); i++)
                    {
                        VideoMode& mode = videoModeList[i];
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

                bool vsyncEnabled = r_vsync->GetValue<bool>();
                if (ImGui::Checkbox("VSync", &vsyncEnabled))
                    r_vsync->SetValue(vsyncEnabled);

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

            ImGui::EndMenuBar();
        }
    }

    static bool DrawIconButton(
        const std::string& strId,
        const graphics::Texture& texture,
        const std::string& tooltip,
        bool is_selected = false,
        ImVec2 size = ImVec2(24, 24))
    {
        if (is_selected)
        {
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
            ImGui::PushStyleColor(ImGuiCol_Button, color);
        }
        bool clicked = ImGui::ImageButton(strId.c_str(), (ImTextureID)&texture, size);
        if (is_selected)
            ImGui::PopStyleColor(1);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip.c_str());

        return clicked;
    }

    void DrawIconBar()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
        if (DrawIconButton("#Import", import_icon.GetTexture(), "Import a 3D model to the scene"))
            OpenDialog_ImportModel(FILE_FILTER_IMPORT_MODEL);

        ImGui::SameLine();
        if (DrawIconButton("##AddLight", light_icon.GetTexture(), "Add a pointlight to the scene"))
            CreatePointLight();

        ImGui::SameLine();
        if (DrawIconButton("##Delete", delete_icon.GetTexture(), "Delete the selected entity"))
            DeleteSelectedEntity();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();
        if (DrawIconButton("##Translate", translate_icon.GetTexture(), "Translate mode", guizmo_operation & ImGuizmo::TRANSLATE))
            guizmo_operation = ImGuizmo::TRANSLATE;

        ImGui::SameLine();
        if (DrawIconButton("##Rotate", rotate_icon.GetTexture(), "Rotate mode", guizmo_operation & ImGuizmo::ROTATE))
            guizmo_operation = ImGuizmo::ROTATE;

        ImGui::SameLine();
        if (DrawIconButton("##Scale", scale_icon.GetTexture(), "Scale mode", guizmo_operation & ImGuizmo::SCALEU))
            guizmo_operation = ImGuizmo::SCALEU;

        ImGui::PopStyleVar();
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

        XMFLOAT4X4 world = {};
        if (transform)
            world = transform->world;

        const bool isEnabled = transform != nullptr;
        ImGuizmo::Enable(isEnabled);
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        ImGuizmo::MODE mode = guizmo_world_mode ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        ImGuizmo::Manipulate(
            &camera.view._11,
            &camera.projection._11,
            guizmo_operation,
            mode,
            &world._11);

        if (ImGuizmo::IsUsing() && isEnabled)
        {
            if (!isUsingGizmo)
            {
                ui::GetUndoManager().EmplaceAction<ui::ModifyValue<scene::TransformComponent>>(windowID, transform);
            }

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

    static spatial::Ray GetPickRay(float cursorX, float cursorY)
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
        return spatial::Ray(lineStart, rayDirection);
    }

    //------------------------------------------------------------------------------
    // PUBLIC API
    //------------------------------------------------------------------------------

    void ResizeBuffers(const XMUINT2& internalResolution)
    {
        graphics::GraphicsDevice* device = graphics::GetDevice();

        graphics::TextureDesc desc{};
        desc.width = internalResolution.x;
        desc.height = internalResolution.y;
        desc.format = graphics::Format::R8_Unorm;
        desc.bindFlags = graphics::BindFlags::RenderTargetBit | graphics::BindFlags::ShaderResourceBit;
        device->CreateTexture(&desc, nullptr, &rtSelectionOutline);
        device->SetName(&rtSelectionOutline, "rtSelectionOutline");
    }

    void Initialize()
    {
        // Attach built-in tools
        AttachToolToMenu(std::make_unique<Tool_TerrainGeneration>("Terrain Generator"));
        AttachToolToMenu(std::make_unique<Tool_ContentBrowser>("Scene Content Browser"));
        AttachToolToMenu(std::make_unique<Tool_Profiler>("Profiler"));
        AttachToolToMenu(std::make_unique<Tool_CVarViewer>("CVar viewer"));
        AttachToolToMenu(std::make_unique<Tool_LogDisplay>("Backlog"));

        // Icons rendered by ImGui need's to be flipped manually at loadtime
        import_icon = resourcemanager::LoadFile("textures/editor/import.png");
        delete_icon = resourcemanager::LoadFile("textures/editor/delete.png");
        light_icon = resourcemanager::LoadFile("textures/editor/add.png");
        editor_icon_select = resourcemanager::LoadFile("textures/editor/select.png");
        translate_icon = resourcemanager::LoadFile("textures/editor/move.png");
        rotate_icon = resourcemanager::LoadFile("textures/editor/rotate.png");
        scale_icon = resourcemanager::LoadFile("textures/editor/resize.png");

        r_vsync = cvar_system::Find("r_vsync");
        r_debugObjectAABB = cvar_system::Find("r_debugObjectAABB");
        r_debugLightSources = cvar_system::Find("r_debugLightSources");

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
#endif

        // Grab available fullscreen resolutions
        GetVideoModesForDisplay(videoModeList, 0);

        initialized = true;
    }

    void Update()
    {
        if (!initialized)
            return;

        ImGuizmo::BeginFrame();

        ImGuiID gizmoWindowID = 0;
        if (ImGui::Begin("CybEngine Editor", 0, ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, 0, ImGuiInputFlags_RouteGlobalLow))
                ui::GetUndoManager().Undo();
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, 0, ImGuiInputFlags_RouteGlobalLow))
                ui::GetUndoManager().Redo();
            if (ImGui::Shortcut(ImGuiKey_Delete, 0, ImGuiInputFlags_RouteGlobalLow))
                DeleteSelectedEntity();

            // use windowID for gizmo undo commands
            gizmoWindowID = ImGui::GetCurrentWindow()->ID;

            DrawMenuBar();
            DrawIconBar();

            ImGui::Text("Scene Hierarchy:");
            ImGui::BeginChild("Scene hierarchy", ImVec2(0, 300), ImGuiChildFlags_Border);
            scenegraphView.GenerateView();
            scenegraphView.WindowContent();
            ImGui::EndChild();

            ImGui::Text("Components:");
            const float componentChildHeight = math::Max(300.0f, ImGui::GetContentRegionAvail().y);
            ImGui::BeginChild("Components", ImVec2(0, componentChildHeight), ImGuiChildFlags_Border);
            EditEntityComponents(scenegraphView.GetSelectedEntity());
            ImGui::EndChild();
        }
        ImGui::End();

        // Only draw gizmo with a valid entity containing transform component
        if (scene::GetScene().transforms.GetComponent(scenegraphView.GetSelectedEntity()))
            DrawGizmo(gizmoWindowID);

        // Pick on left mouse click
        ImGuiIO& io = ImGui::GetIO();
        bool isMouseIn3DView = !io.WantCaptureMouse && !ImGuizmo::IsOver();
        if (isMouseIn3DView && io.MouseClicked[0])
        {
            const scene::Scene& scene = scene::GetScene();
            spatial::Ray pick_ray = GetPickRay(io.MousePos.x, io.MousePos.y);
            scene::PickResult pick_result = scene::Pick(scene, pick_ray);

            // Enable mouse picking on lightsources only if they are being drawn
            if (r_debugLightSources->GetValue<bool>())
            {
                for (size_t i = 0; i < scene.lights.Size(); ++i)
                {
                    ecs::Entity entity = scene.lights.GetEntity(i);
                    const scene::TransformComponent& transform = *scene.transforms.GetComponent(entity);

                    XMVECTOR disV = XMVector3LinePointDistance(pick_ray.GetOrigin(), pick_ray.GetOrigin() + pick_ray.GetDirection(), transform.GetPositionV());
                    float dis = XMVectorGetX(disV);
                    const XMFLOAT3& pos = transform.GetPosition();
                    if (dis > 0.01f && dis < math::Distance(XMLoadFloat3(&pos), pick_ray.GetOrigin()) * 0.05f && dis < pick_result.distance)
                    {
                        pick_result = scene::PickResult();
                        pick_result.entity = entity;
                        pick_result.distance = dis;
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

    void Render(const graphics::Texture* rtDepthStencil, graphics::CommandList cmd)
    {
        graphics::GraphicsDevice* device = graphics::GetDevice();
        graphics::RenderPassImage rp[] = {
            graphics::RenderPassImage::RenderTarget(
                &rtSelectionOutline,
                graphics::RenderPassImage::LoadOp::Clear),
            graphics::RenderPassImage::DepthStencil(
                rtDepthStencil,
                graphics::RenderPassImage::LoadOp::Load,
                graphics::RenderPassImage::StoreOp::Store)};
        /*
        device->BeginRenderPass(rp, _countof(rp), cmd);

        renderer::ImageParams params = renderer::ImageParams::DefaultFullscreen();
        renderer::DrawImage(nullptr, params, cmd);
        device->EndRenderPass(cmd);
        */
    }

    void UpdateFPSCounter(double dt)
    {
        deltatimes[fpsAgvCounter++ % _countof(deltatimes)] = dt;
        if (fpsAgvCounter > _countof(deltatimes))
        {
            double avgTime = std::accumulate(std::begin(deltatimes), std::end(deltatimes), 0.0) / _countof(deltatimes);
            avgFps = (uint32_t)std::round(1.0 / avgTime);
        }
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