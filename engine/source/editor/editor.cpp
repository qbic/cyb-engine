#include "core/logger.h"
#include "core/timer.h"
#include "core/profiler.h"
#include "core/helper.h"
#include "core/mathlib.h"
#include "graphics/renderer.h"
#include "graphics/model-import.h"
#include "systems/event-system.h"
#include "editor/editor.h"
#include "editor/imgui-backend.h"
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "imgui_stdlib.h"
#include "ImGuizmo.h"
#include "editor/imgui-widgets.h"
#include "editor/terrain-generator.h"
#include <stack>
#include <numeric>
#include <sstream>

namespace cyb::editor 
{
    // Pre-defined filters for open/save dialoge window
    // Using string literals solves the issue of using '\0' in std::string and 
    // keep the flixabilaty of creating long filter strings with simple +overloading
    using namespace std::string_literals;
    const std::string FILE_FILTER_ALL = "All Files (*.*)\0*.*\0"s;
    const std::string FILE_FILTER_SCENE = "CybEngine Binary Scene Files (*.cbs)\0*.cbs\0"s;
    const std::string FILE_FILTER_GLTF = "GLTF Files (*.gltf; *.glb)\0*.gltf;*.glb\0"s;
    const std::string FILE_FILTER_IMPORT_MODEL = FILE_FILTER_GLTF + FILE_FILTER_SCENE + FILE_FILTER_ALL;

    const size_t PROFILER_FRAME_COUNT = 80;

    bool initialized = false;
    bool vsync_enabled = true;      // FIXME: initial value has to be synced with SwapChainDesc::vsync
    Resource import_icon;
    Resource delete_icon;
    Resource light_icon;
    Resource editor_icon_select;
    Resource translate_icon;
    Resource rotate_icon;
    Resource scale_icon;
    ImGuizmo::OPERATION guizmo_operation = ImGuizmo::BOUNDS;
    bool guizmo_world_mode = true;
    std::vector<float> g_frameTimes;
    SceneGraphView scenegraph_view;

    // History undo/redo
    enum class HistoryOpType
    {
        TRANSFORM,
        DRAG_FLOAT,
        DRAG_INT,
        SLIDER_FLOAT,
        SLIDER_INT,
        ENTITY_CHANGE,
        INT32_CHANGE,
        None
    };
    std::vector<serializer::Archive> history;
    int history_pos = -1;

    serializer::Archive& AdvanceHistory()
    {
        history_pos++;

        while (static_cast<int>(history.size()) > history_pos)
        {
            history.pop_back();
        }

        history.emplace_back();
        return history.back();
    }

    void ConsumeHistoryOp(bool undo)
    {
        if ((undo && history_pos >= 0) || (!undo && history_pos < (int)history.size() - 1))
        {
            if (!undo)
                history_pos++;

            scene::Scene& scene = scene::GetScene();
            serializer::Archive& archive = history[history_pos];
            archive.SetAccessModeAndResetPos(serializer::Archive::kRead);

            int temp;
            archive >> temp;
            HistoryOpType op = (HistoryOpType)temp;

            switch (op)
            {
            case HistoryOpType::TRANSFORM: 
            {
                XMFLOAT4X4 delta;
                ecs::Entity entity;

                archive >> delta;
                archive >> entity;
                XMMATRIX W = XMLoadFloat4x4(&delta);
                if (undo)
                {
                    W = XMMatrixInverse(nullptr, W);
                }
                scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
                if (transform != nullptr)
                {
                    transform->MatrixTransform(W);
                }
            } break;
            case HistoryOpType::SLIDER_FLOAT:
            case HistoryOpType::DRAG_FLOAT:
            {
                float delta;
                uintptr_t ptr_addr;
                archive >> delta;
                archive >> ptr_addr;
                if (undo)
                {
                    delta = -delta;
                }
                float* value_ptr = (float*)ptr_addr;
                *value_ptr -= delta;
            } break;
            case HistoryOpType::SLIDER_INT:
            case HistoryOpType::DRAG_INT:
            {
                int32_t delta;
                uintptr_t ptr_addr;
                archive >> delta;
                archive >> ptr_addr;
                if (undo)
                {
                    delta = -delta;
                }
                uint32_t* value_ptr = (uint32_t*)ptr_addr;
                *value_ptr -= delta;
            } break;
            case HistoryOpType::ENTITY_CHANGE:
            {
                ecs::Entity new_value;
                ecs::Entity old_value;
                uintptr_t ptr_addr;
                archive >> new_value;
                archive >> old_value;
                archive >> ptr_addr;

                ecs::Entity value = undo ? old_value : new_value;
                ecs::Entity* value_ptr = (uint32_t*)ptr_addr;
                *value_ptr = value;

            } break;
            case HistoryOpType::INT32_CHANGE:
            {
                int32_t new_value;
                int32_t old_value;
                uintptr_t ptr_addr;
                archive >> new_value;
                archive >> old_value;
                archive >> ptr_addr;

                int32_t value = undo ? old_value : new_value;
                int32_t* value_ptr = (int32_t*)ptr_addr;
                *value_ptr = value;

            } break;
            default: break;
            }

            if (undo)
            {
                history_pos--;
            }
        }
    }

    namespace gui
    {
        const float DEFAULT_COLUMN_WIDTH = 100.0f;
        std::stack<float> column_width_stack;

        void PushColumnWidth(float width)
        {
            column_width_stack.push(width);
        }

        void PopColumnWidth()
        {
            column_width_stack.pop();
        }

        float ColumnWidth()
        {
            if (!column_width_stack.empty())
                return column_width_stack.top();

            return DEFAULT_COLUMN_WIDTH;
        }

        static void BeginElement(const std::string& label)
        {
            ImGui::PushID(label.c_str());

            ImGui::BeginTable(label.c_str(), 2);
            ImGui::TableSetupColumn("one", ImGuiTableColumnFlags_WidthFixed, ColumnWidth());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label.c_str());
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
        }

        static void EndElement()
        {
            ImGui::EndTable();
            ImGui::PopID();
        }

        bool CheckBox(const std::string& label, bool& value)
        {
            BeginElement(label);
            bool value_changed = ImGui::Checkbox("##FLAG", &value);
            EndElement();

            return value_changed;
        }

        template <class T>
        static bool CheckboxFlags(const std::string& label, T& flags, T flags_value)
        {
            BeginElement(label);
            bool value_changed = ImGui::CheckboxFlags("##FLAG", (unsigned int*)&flags, (unsigned int)flags_value);
            EndElement();

            return value_changed;
        }

        // Helper function to draw a float slider with label on left side.
        // The label column width is specifiled by column_width.
        bool DragFloat(
            const std::string& label,
            float& value,
            float v_speed,
            float v_min,
            float v_max,
            const char* format
        )
        {
            static float old_value = 0.0f;

            BeginElement(label);
            bool value_changed = ImGui::DragFloat("##X", &value, v_speed, v_min, v_max, format);
            if (ImGui::IsItemActivated())
                old_value = value;

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                serializer::Archive& ar = AdvanceHistory();
                ar << (int)HistoryOpType::DRAG_FLOAT;
                ar << old_value - value;
                ar << (uintptr_t)&value;
            }
            EndElement();

            return value_changed;
        }

        bool DragInt(
            const std::string& label,
            uint32_t& value,
            float v_speed,
            uint32_t v_min,
            uint32_t v_max,
            const char* format
        )
        {
            static uint32_t old_value = 0;

            BeginElement(label);
            bool value_changed = ImGui::DragInt("##X", (int *)&value, v_speed, v_min, v_max, format);
            if (ImGui::IsItemActivated())
                old_value = value;

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                serializer::Archive& ar = AdvanceHistory();
                ar << (int)HistoryOpType::DRAG_INT;
                ar << old_value - value;
                ar << (uintptr_t)&value;
            }
            EndElement();

            return value_changed;
        }

        bool SliderFloat(const std::string& label, float& value, float v_min, float v_max)
        {
            static float old_value = 0.0f;

            BeginElement(label);
            bool value_changed = ImGui::SliderFloat("##X", &value, v_min, v_max, "%.2f");
            if (ImGui::IsItemActivated())
                old_value = value;

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                serializer::Archive& ar = AdvanceHistory();
                ar << (int)HistoryOpType::SLIDER_FLOAT;
                ar << old_value - value;
                ar << (uintptr_t)&value;
            }
            EndElement();

            return value_changed;
        }

        bool SliderInt(const std::string& label, uint32_t& value, uint32_t v_min, uint32_t v_max, const char* format)
        {
            static uint32_t old_value = 0;

            BeginElement(label);
            bool value_changed = ImGui::SliderInt("##X", (int *)&value, v_min, v_max, format);
            if (ImGui::IsItemActivated())
                old_value = value;

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                serializer::Archive& ar = AdvanceHistory();
                ar << (int)HistoryOpType::SLIDER_INT;
                ar << old_value - value;
                ar << (uintptr_t)&value;
            }
            EndElement();

            return value_changed;
        }

        static bool Float3Edit(
            const std::string& label,
            XMFLOAT3& values,
            float reset_value = 0.0f)
        {
            bool value_changed = false;

            BeginElement(label);

            ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

            const float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
            const ImVec2 button_size = { line_height + 3.0f, line_height };

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
            if (ImGui::Button("X", button_size))
            {
                values.x = reset_value;
                value_changed = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reset X to %.2f", reset_value);
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            value_changed |= ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
            if (ImGui::Button("Y", button_size))
            {
                values.y = reset_value;
                value_changed = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Reset Y to %.2f", reset_value);
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::SameLine();
            value_changed |= ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
            if (ImGui::Button("Z", button_size))
            {
                values.z = reset_value;
                value_changed = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Reset Z to %.2f", reset_value);
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            value_changed |= ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");

            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            EndElement();
            return value_changed;
        }

        void ColorEdit3(const std::string& label, XMFLOAT3& value)
        {
            BeginElement(label);
            ImGui::ColorEdit3("##COLOR3", &value.x, ImGuiColorEditFlags_Float);
            EndElement();
        }

        void ColorEdit4(const std::string& label, XMFLOAT4& value)
        {
            BeginElement(label);
            ImGui::ColorEdit4("##COLOR4", &value.x, ImGuiColorEditFlags_Float);
            EndElement();
        }
    }


    //------------------------------------------------------------------------------
    // Component inspectors
    //------------------------------------------------------------------------------

    void InspectNameComponent(scene::NameComponent* name_component)
    {
        ImGui::InputText("Name", &name_component->name);
    }

    void InspectTransformComponent(scene::TransformComponent* transform)
    {
        if (gui::Float3Edit("Translation", transform->translation_local))
        {
            transform->SetDirty(true);
        }

        //if (gui::Vec3Control("Rotation", transform->rotation_local.xyz))
        //{
        //    transform->SetDirty(true);
        //}

        if (gui::Float3Edit("Scale", transform->scale_local, 1.0f))
        {
            transform->SetDirty(true);
        }
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
        if (!mesh)
            return;

        scene::Scene& scene = scene::GetScene();

        // Mesh info
        ImGui::Text("Vertex positions: %u", mesh->vertex_positions.size());
        ImGui::Text("Vertex normals: %u", mesh->vertex_normals.size());
        ImGui::Text("Vertex colors: %u", mesh->vertex_colors.size());
        ImGui::Text("Index count: %u", mesh->indices.size());

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

        if (ImGui::Button("Compute Normals"))
            mesh->ComputeNormals();
    }

    void InspectMaterialComponent(scene::MaterialComponent* material)
    {
        if (!material)
            return;

        static const std::unordered_map<scene::MaterialComponent::Shadertype, std::string> shadertype_names =
        {
            { scene::MaterialComponent::Shadertype_BDRF,    "Flat BRDF" },
            { scene::MaterialComponent::Shadertype_Unlit,   "Flat Unlit" },
            { scene::MaterialComponent::Shadertype_Terrain, "Terrain (NOT IMPLEMENTED)" }
        };

        gui::ComboBox("Shader Type", material->shaderType, shadertype_names);
        gui::ColorEdit4("BaseColor", material->baseColor);
        gui::SliderFloat("Roughness", material->roughness, 0.0f, 1.0f);
        gui::SliderFloat("Metalness", material->metalness, 0.0f, 1.0f);
    }

    struct NameSortableEntityData
    {
        ecs::Entity id = ecs::InvalidEntity;
        std::string_view name;

        bool operator<(const NameSortableEntityData& a)
        {
            return name < a.name;
        }
    };

    template <typename T>
    void SelectEntityPopup(
        ecs::ComponentManager<T>& components,
        ecs::ComponentManager<scene::NameComponent>& names,
        ecs::Entity& current_entity)
    {
        static ImGuiTextFilter filter;
        const int listBoxHeightInItems = 10;

        assert(components.Size() < INT32_MAX);
        if (ImGui::ListBoxHeader("##lbhd", (int)components.Size(), listBoxHeightInItems))
        {
            std::vector<NameSortableEntityData> sorted_entities;
            for (size_t i = 0; i < components.Size(); ++i)
            {
                auto& back = sorted_entities.emplace_back();
                back.id = components.GetEntity(i);
                back.name = names.GetComponent(back.id)->name;
            }

            std::sort(sorted_entities.begin(), sorted_entities.end());
            for (auto& entity : sorted_entities)
            {
                ImGui::PushID(entity.id);
                if (filter.PassFilter(entity.name.data()))
                {
                    const std::string label = fmt::format("{}##{}", entity.name, entity.id); // ImGui needs a uniqe label for each entity
                    if (ImGui::Selectable(label.c_str(), current_entity == entity.id))
                    {
                        if (current_entity != entity.id)
                        {
                            serializer::Archive& ar = AdvanceHistory();
                            ar << (int)HistoryOpType::ENTITY_CHANGE;
                            ar << entity.id;        // new
                            ar << current_entity;   // old
                            ar << (uintptr_t)&current_entity;

                            current_entity = entity.id;
                        }

                        filter.Clear();
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::PopID();
            }
            ImGui::ListBoxFooter();
        }

        ImGui::Text("Search:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        filter.Draw("##filter");
    }

    ecs::Entity SelectAndGetMaterialForMesh(scene::MeshComponent* mesh)
    {
        scene::Scene& scene = scene::GetScene();

        std::vector<std::string_view> names;
        for (const auto& subset : mesh->subsets)
        {
            std::string_view name = scene.names.GetComponent(subset.materialID)->name.c_str();
            names.push_back(name);
        }

        static int selectedSubsetIndex = 0;
        selectedSubsetIndex = std::min(selectedSubsetIndex, (int)mesh->subsets.size() - 1);

        gui::BeginElement("Select Material");
        ImGui::ListBox("##MeshMaterials", &selectedSubsetIndex, names);
        gui::EndElement();
        ecs::Entity selectedMaterialID = mesh->subsets[selectedSubsetIndex].materialID;

        // Edit material name / select material
        scene::NameComponent* name = scene.names.GetComponent(selectedMaterialID);
        ImGui::InputText("##Material_Name", &name->name);
        ImGui::SameLine();
        if (ImGui::Button("Change##Material"))
            ImGui::OpenPopup("MaterialSelectPopup");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Link another material to the mesh");

        if (ImGui::BeginPopup("MaterialSelectPopup"))
        {
            SelectEntityPopup(scene.materials, scene.names, mesh->subsets[selectedSubsetIndex].materialID);
            ImGui::EndPopup();
        }

        return selectedMaterialID;
    }

    void InspectAABBComponent(const math::AxisAlignedBox* aabb)
    {
        const XMFLOAT3& min = aabb->min;
        const XMFLOAT3& max = aabb->max;
        ImGui::Text("Min: [%.2f, %.2f, %.2f]", min.x, min.y, min.z);
        ImGui::Text("Max: [%.2f, %.2f, %.2f]", max.x, max.y, max.z);
        ImGui::Text("Width: %.2fm", max.x - min.x);
        ImGui::Text("Height: %.2fm", max.y - min.y);
        ImGui::Text("Depth: %.2fm", max.z - min.z);
    }

    ecs::Entity SelectAndGetMeshForObject(scene::ObjectComponent* object)
    {
        scene::Scene& scene = scene::GetScene();

        // Edit mesh name / select mesh
        scene::NameComponent* name = scene.names.GetComponent(object->meshID);
        ImGui::InputText("##Mesh_Name", &name->name);
        ImGui::SameLine();
        if (ImGui::Button("Change"))
            ImGui::OpenPopup("MeshSelectPopup");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Link another mesh to the object");

        if (ImGui::BeginPopup("MeshSelectPopup"))
        {
            SelectEntityPopup(scene.meshes, scene.names, object->meshID);
            ImGui::EndPopup();
        }

        return object->meshID;
    }

    void InspectObjectComponent(scene::ObjectComponent* object)
    {
        ImGui::CheckboxFlags("Renderable (unimplemented)", (unsigned int*)&object->flags, (unsigned int)scene::ObjectComponent::Flags::RenderableBit);
        ImGui::CheckboxFlags("Cast shadow (unimplemented)", (unsigned int*)&object->flags, (unsigned int)scene::ObjectComponent::Flags::CastShadowBit);
    }

    bool InspectCameraComponent(scene::CameraComponent& camera)
    {
        bool change = ImGui::SliderFloat("Z Near Plane", &camera.zNearPlane, 0.001f, 10.0f);
        change |= ImGui::SliderFloat("Z Far Plane", &camera.zFarPlane, 10.0f, 1000.0f);
        change |= ImGui::SliderFloat("FOV", &camera.fov, 0.0f, 3.0f);
        change |= gui::Float3Edit("Position", camera.pos);
        change |= gui::Float3Edit("Target", camera.target);
        change |= gui::Float3Edit("Up", camera.up);
        return change;
    }

    void InspectLightComponent(scene::LightComponent* light)
    {
        static const std::unordered_map<scene::LightType, std::string> lightTypeNames =
        {
            { scene::LightType::Directional, "Directional" },
            { scene::LightType::Point,       "Point"       }
        };

        scene::LightType light_type = light->GetType();
        if (gui::ComboBox("Type", light_type, lightTypeNames))
            light->SetType(light_type);

        gui::ColorEdit3("Color", light->color);
        gui::DragFloat("Energy", light->energy, 0.02f);
        gui::DragFloat("Range", light->range, 1.2f, 0.0f, FLT_MAX);
        gui::CheckboxFlags("Affects scene", light->flags, scene::LightComponent::Flags::AffectsSceneBit);
        gui::CheckboxFlags("Cast shadows", light->flags, scene::LightComponent::Flags::CastShadowsBit);
    }

    void InspectWeatherComponent(scene::WeatherComponent* weather)
    {
        ImGui::ColorEdit3("Horizon Color", &weather->horizon.x);
        ImGui::ColorEdit3("Zenith Color", &weather->zenith.x);
        ImGui::DragFloat("Fog Begin", &weather->fogStart);
        ImGui::DragFloat("Fog End", &weather->fogEnd);
        ImGui::DragFloat("Fog Height", &weather->fogHeight);
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

        if (added_entities.count(entity) != 0)
            return;
        parent->children.emplace_back(parent, entity, name);
        added_entities.insert(entity);

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
        root.children.clear();
        added_entities.clear();

        const scene::Scene& scene = scene::GetScene();

        // First weather...
        {
            if (scene.weathers.Size() > 0)
            {
                ecs::Entity entity = scene.weathers.GetEntity(0);
                const char* name = "Weather";
                AddNode(&root, entity, name);
            }
        }

        // ... then groups...
        for (size_t i = 0; i < scene.groups.Size(); ++i)
        {
            ecs::Entity entity = scene.groups.GetEntity(i);
            const std::string& name = scene.names.GetComponent(entity)->name;
            AddNode(&root, entity, name);
        }

        // ... then objects...
        for (size_t i = 0; i < scene.objects.Size(); ++i)
        {
            ecs::Entity entity = scene.objects.GetEntity(i);
            const std::string& name = scene.names.GetComponent(entity)->name;
            AddNode(&root, entity, name);
        }

        // ... then all other entities containing transform components
        for (size_t i = 0; i < scene.transforms.Size(); ++i)
        {
            ecs::Entity entity = scene.transforms.GetEntity(i);
            const std::string& name = scene.names.GetComponent(entity)->name;
            AddNode(&root, entity, name);
        }
    }

    void SceneGraphView::DrawNode(const Node* node)
    {
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        node_flags |= (node->children.empty()) ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen : 0;
        node_flags |= (node->entity == selected_entity) ? ImGuiTreeNodeFlags_Selected : 0;

        bool is_open = ImGui::TreeNodeEx(node->name.data(), node_flags, node->name.data());
        if (ImGui::IsItemClicked())
            selected_entity = node->entity;

        const char* drag_and_drop_id = "SGV_TreeNode";
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(drag_and_drop_id, &node->entity, sizeof(node->entity));
            ImGui::Text("Move to parent");
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(drag_and_drop_id);
            if (payload)
            {
                assert(payload->DataSize == sizeof(ecs::Entity));
                ecs::Entity dragged_entity = *((ecs::Entity*)payload->Data);
                scene::GetScene().ComponentAttach(dragged_entity, node->entity);
            }
        }

        if (is_open)
        {
            if (ImGui::IsItemClicked())
                selected_entity = node->entity;

            if (!node->children.empty())
            {
                for (const auto& child : node->children)
                    DrawNode(&child);

                ImGui::TreePop();
            }
        }
    }

    void SceneGraphView::Draw()
    {
        for (const auto& x : root.children)
            DrawNode(&x);
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
        if (entityID == ecs::InvalidEntity)
            return;

        scene::Scene& scene = scene::GetScene();

        scene::ObjectComponent* object = scene.objects.GetComponent(entityID);
        if (object)
        {
            if (ImGui::CollapsingHeader("Object"))
            {
                ImGui::Indent();
                InspectNameComponent(scene.names.GetComponent(entityID));
                InspectObjectComponent(object);
                ImGui::Unindent();
            }

            scene::MeshComponent* mesh = nullptr;

            if (ImGui::CollapsingHeader("Mesh *"))
            {
                ImGui::Indent();
                const ecs::Entity meshID = SelectAndGetMeshForObject(object);
                ImGui::Separator();
                mesh = scene.meshes.GetComponent(meshID);
                InspectMeshComponent(mesh);
                ImGui::Unindent();
            }

            if (mesh == nullptr)
                mesh = scene.meshes.GetComponent(object->meshID);

            if (ImGui::CollapsingHeader("Materials *"))
            {
                ImGui::Indent();
                const ecs::Entity materialID = SelectAndGetMaterialForMesh(mesh);
                ImGui::Separator();
                InspectMaterialComponent(scene.materials.GetComponent(materialID));
                ImGui::Unindent();
            }
        }

        InspectComponent<scene::MeshComponent>("Mesh", scene.meshes, InspectMeshComponent, entityID, false);
        InspectComponent<scene::MaterialComponent>("Material", scene.materials, InspectMaterialComponent, entityID, false);
        InspectComponent<scene::LightComponent>("Light", scene.lights, InspectLightComponent, entityID, true);
        InspectComponent<scene::TransformComponent>("Transform", scene.transforms, InspectTransformComponent, entityID, false);
        InspectComponent<math::AxisAlignedBox>("AABB##edit_object_aabb", scene.aabb_objects, InspectAABBComponent, entityID, false);
        InspectComponent<math::AxisAlignedBox>("AABB##edit_light_aabb", scene.aabb_lights, InspectAABBComponent, entityID, false);
        InspectComponent<scene::HierarchyComponent>("Hierarchy", scene.hierarchy, InspectHierarchyComponent, entityID, false);
        InspectComponent<scene::WeatherComponent>("Weather", scene.weathers, InspectWeatherComponent, entityID, true);
    }

    //------------------------------------------------------------------------------

    bool GuiTool::PreDraw()
    {
        return ImGui::Begin(GetWindowTitle(), &show_window, ImGuiWindowFlags_HorizontalScrollbar);
    }

    void GuiTool::PostDraw()
    {
        ImGui::End();
    }

    class Tool_Profiler : public GuiTool
    {
    public:
        using GuiTool::GuiTool;
        virtual void Draw() override
        {
            ImGui::SetNextItemWidth(-1);

            const float totalFrameTime = std::accumulate(g_frameTimes.begin(), g_frameTimes.end(), 0.0f);
            std::string avgFrameTime = std::string("avg. time: ") + std::to_string(totalFrameTime / PROFILER_FRAME_COUNT);
            ImGui::PlotHistogram("##FrameTimes", &g_frameTimes[0], (uint32_t)g_frameTimes.size(), 0, avgFrameTime.c_str(), 0.0f, 0.02f, ImVec2(0, 80.0f));
            ImGui::Text("Frame counter: %u", renderer::GetDevice()->GetFrameCount());
            ImGui::Text("Avarage FPS (Over %d frames): %.1f", PROFILER_FRAME_COUNT, PROFILER_FRAME_COUNT / totalFrameTime);

            graphics::GraphicsDevice::MemoryUsage vram = renderer::GetDevice()->GetMemoryUsage();
            ImGui::Text("VRAM usage: %dMB / %dMB", vram.usage / 1024 / 1024, vram.budget / 1024 / 1024);

            // Display profiler entries sorted by their cpu time
            const auto& profiler_entries = profiler::GetData();
            float max_time = 10.0f;
            std::vector<std::pair<std::string_view, float>> sorted_entries;
            sorted_entries.reserve(profiler_entries.size());
            for (auto& it : profiler_entries)
            {
                max_time = std::max(max_time, it.second.time);
                sorted_entries.emplace_back(std::pair< std::string_view, float>(it.second.name, it.second.time));
            }

            std::sort(sorted_entries.begin(), sorted_entries.end(), [=](std::pair<std::string_view, float>& a, std::pair<std::string_view, float>& b)
                {
                    return a.second > b.second;
                });

            for (auto& it : sorted_entries)
            {
                ImGui::SetNextItemWidth(-1);
                ImGui::FilledBar(it.first.data(), it.second, 0, max_time, "{:.3f}ms");
            }
        }
    };

    //------------------------------------------------------------------------------

    class LogOutputModule_Editor : public logger::LogOutputModule
    {
    public:
        void Write(const logger::LogMessage& log) override
        {
            logStream << log.message;
        }

        static std::stringstream logStream;
    };
    std::stringstream LogOutputModule_Editor::logStream("");

    class Tool_LogDisplay : public GuiTool
    {
    public:
        using GuiTool::GuiTool;

        virtual void Draw() override
        {
            std::string text = LogOutputModule_Editor::logStream.str();
            ImGui::TextUnformatted((char*)text.c_str(), (char*)text.c_str() + text.size());

            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
    };

    //------------------------------------------------------------------------------

    class Tool_ContentBrowser : public GuiTool
    {
    public:
        using GuiTool::GuiTool;
        virtual void Draw() override
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

    // Generate a terrain mesh on the currently selected entity.
    // Params are stored in global terrain_generator_params.
    //
    // TODO: This tool feels abit wonky, having to select the entity in the
    // scene browser, and using global TerrainParameters...
    class Tool_TerrainGeneration : public GuiTool
    {
    public:
        TerrainGenerator generator;

        using GuiTool::GuiTool;
        virtual void Draw() override
        {
            generator.DrawGui(scenegraph_view.SelectedEntity());
        }
    };

    //------------------------------------------------------------------------------

    std::vector<std::unique_ptr<GuiTool>> tools;

    void AttachToolToMenu(std::unique_ptr<GuiTool>&& tool)
    {
        tools.push_back(std::move(tool));
    }

    void DrawTools()
    {
        for (auto& x : tools)
        {
            if (!x->IsShown())
                continue;

            if (x->PreDraw())
                x->Draw();
            
            x->PostDraw();
        }
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
        scenegraph_view.SelectEntity(entity);
        return entity;
    }

    // Clears the current scene and loads in a new from a selected file.
    // TODO: Add a dialog to prompt user about unsaved progress
    void OpenDialog_Open()
    {
        helper::FileDialog(helper::FileOp::OPEN, FILE_FILTER_SCENE, [](std::string filename) {
            eventsystem::Subscribe_Once(eventsystem::kEvent_ThreadSafePoint, [=](uint64_t) {
                scene::GetScene().Clear();
                scene::LoadModel(filename);
                });
            });
    }

    // Import a new model to the scene, once the loading is complete
    // it will be automaticly selected in the scene graph view.
    void OpenDialog_ImportModel(const std::string filter)
    {
        helper::FileDialog(helper::FileOp::OPEN, filter, [](std::string filename) {
            eventsystem::Subscribe_Once(eventsystem::kEvent_ThreadSafePoint, [=](uint64_t) {
                std::string extension = helper::ToUpper(helper::GetExtensionFromFileName(filename));
                if (extension.compare("CBS") == 0)
                {
                    scene::LoadModel(filename);
                }
                else if (extension.compare("GLB") == 0 || extension.compare("GLTF") == 0)
                {
                    ecs::Entity entity = renderer::ImportModel_GLTF(filename, scene::GetScene());
                    SetSceneGraphViewSelection(entity);
                }
                });
            });
    }

    void OpenDialog_SaveAs()
    {
        helper::FileDialog(helper::FileOp::SAVE, FILE_FILTER_SCENE, [](std::string filename) {
            if (helper::GetExtensionFromFileName(filename) != "cbs")
            {
                filename += ".cbs";
            }

            serializer::Archive ar;
            Timer timer;
            timer.Record();
            scene::GetScene().Serialize(ar);
            ar.SaveFile(filename);
            CYB_TRACE("Serialized and saved (filename={0}) in {1:.2f}ms", filename, timer.ElapsedMilliseconds());
            });
    }

    void SetSceneGraphViewSelection(ecs::Entity entity)
    {
        scenegraph_view.SelectEntity(entity);
    }

    static void DeleteSelectedEntity()
    {
        eventsystem::Subscribe_Once(eventsystem::kEvent_ThreadSafePoint, [=](uint64_t)
            {
                scene::GetScene().RemoveEntity(scenegraph_view.SelectedEntity());
                scenegraph_view.SelectEntity(ecs::InvalidEntity);
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
                    eventsystem::Subscribe_Once(eventsystem::kEvent_ThreadSafePoint, [=](uint64_t)
                        {
                            scene::GetScene().Clear(); 
                        });
                if (ImGui::MenuItem("Open"))
                    OpenDialog_Open();
                if (ImGui::MenuItem("Save As..."))
                    OpenDialog_SaveAs();

                ImGui::Separator();

                if (ImGui::BeginMenu("Import"))
                {
                    if (ImGui::MenuItem("Model (.gltf/.glb/.cbs)"))
                        OpenDialog_ImportModel(FILE_FILTER_IMPORT_MODEL);

                    ImGui::EndMenu();
                }

                ImGui::Separator();
                ImGui::MenuItem("Exit (!!)", "ALT+F4");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "CTRL+Z"))
                    ConsumeHistoryOp(true);
                if (ImGui::MenuItem("Redo", "CTRL+Y"))
                    ConsumeHistoryOp(false);

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
                    scene::GetScene().ComponentDetach(scenegraph_view.SelectedEntity());
                if (ImGui::MenuItem("Delete", "Del"))
                    DeleteSelectedEntity();
                ImGui::MenuItem("Duplicate (!!)", "CTRL+D");

                ImGui::Separator();

                if (ImGui::MenuItem("Reload Shaders"))
                    renderer::ReloadShaders();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debug"))
            {
                bool debug_object_aabb = renderer::GetDebugObjectAABB();
                if (ImGui::Checkbox("Draw Object AABB", &debug_object_aabb))
                    renderer::SetDebugObjectAABB(debug_object_aabb);
                bool debug_lightsources = renderer::GetDebugLightsources();
                if (ImGui::Checkbox("Draw Lightsources", &debug_lightsources))
                    renderer::SetDebugLightsources(debug_lightsources);
                bool debug_lightsources_abb = renderer::GetDebugLightsourcesAABB();
                if (ImGui::Checkbox("Draw Lightsources AABB", &debug_lightsources_abb))
                    renderer::SetDebugLightsourcesAABB(debug_lightsources_abb);

                if (ImGui::Checkbox("Enable VSync", &vsync_enabled))
                    eventsystem::FireEvent(eventsystem::kEvent_SetVSync, vsync_enabled ? 1ull : 0ull);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools"))
            {
                for (auto& x : tools)
                {
                    bool show_window = x->IsShown();
                    if (ImGui::MenuItem(x->GetWindowTitle(), NULL, &show_window))
                        x->ShowWindow(show_window);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
    }

    static bool DrawIconButton(
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
        bool clicked = ImGui::ImageButton((ImTextureID)&texture, size);
        if (is_selected)
        {
            ImGui::PopStyleColor(1);
        }

        if (ImGui::IsItemHovered()) 
        {
            ImGui::SetTooltip(tooltip.c_str());
        }

        return clicked;
    }

    void DrawIconBar()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
        if (DrawIconButton(import_icon.GetTexture(), "Import a 3D model to the scene"))
            OpenDialog_ImportModel(FILE_FILTER_IMPORT_MODEL);

        ImGui::SameLine();
        if (DrawIconButton(light_icon.GetTexture(), "Add a pointlight to the scene"))
            CreatePointLight();

        ImGui::SameLine();
        if (DrawIconButton(delete_icon.GetTexture(), "Delete the selected entity"))
        {
            DeleteSelectedEntity();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine();
        if (DrawIconButton(editor_icon_select.GetTexture(), "Select entity", guizmo_operation & ImGuizmo::BOUNDS))
            guizmo_operation = ImGuizmo::BOUNDS;

        ImGui::SameLine();
        if (DrawIconButton(translate_icon.GetTexture(), "Move the selected entity", guizmo_operation & ImGuizmo::TRANSLATE))
            guizmo_operation = ImGuizmo::TRANSLATE;

        ImGui::SameLine();
        if (DrawIconButton(rotate_icon.GetTexture(), "Rotate the selected entity", guizmo_operation & ImGuizmo::ROTATE))
            guizmo_operation = ImGuizmo::ROTATE;

        ImGui::SameLine();
        if (DrawIconButton(scale_icon.GetTexture(), "Scale the selected entity", guizmo_operation & ImGuizmo::SCALEU))
            guizmo_operation = ImGuizmo::SCALEU;

        //ImGui::SameLine();
        //ImGui::Checkbox("World mode transform", &guizmo_world_mode);

        //ImGui::SameLine();
        //ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::PopStyleVar();
    }

    static void DrawGizmo()
    {
        scene::Scene& scene = scene::GetScene();
        const scene::CameraComponent& camera = scene::GetCamera();

        const ecs::Entity entity = scenegraph_view.SelectedEntity();
        scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
        if (transform) 
        {
            XMFLOAT4X4& world_matrix = transform->world;

            const ImGuiIO& io = ImGui::GetIO();
            ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
            ImGuizmo::SetOrthographic(true);
            const ImGuizmo::MODE mode = guizmo_world_mode ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
            ImGuizmo::Manipulate(
                &camera.view._11,
                &camera.projection._11,
                guizmo_operation,
                mode,
                &world_matrix._11);

            if (ImGuizmo::IsUsing())
            {
                transform->ApplyTransform();

                // Transform to local space if parented
                const scene::HierarchyComponent* hierarchy = scene.hierarchy.GetComponent(entity);
                if (hierarchy)
                {
                    const scene::TransformComponent* parent_transform = scene.transforms.GetComponent(hierarchy->parentID);
                    if (parent_transform != nullptr)
                    {
                        transform->MatrixTransform(XMMatrixInverse(nullptr, XMLoadFloat4x4(&parent_transform->world)));
                    }
                }
            }

            if (ImGuizmo::FinishedDragging())
            {
                XMFLOAT4X4 drag_matrix;
                ImGuizmo::GetFinishedDragMatrix(&drag_matrix._11);

                serializer::Archive& ar = AdvanceHistory();
                ar << (int)HistoryOpType::TRANSFORM;
                ar << drag_matrix;
                ar << entity;
            }
        }
    }

    static math::Ray GetPickRay(float cursorX, float cursorY)
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
        return math::Ray(lineStart, rayDirection);
    }

    //------------------------------------------------------------------------------
    // PUBLIC API
    //------------------------------------------------------------------------------

    void PushFrameTime(const float t)
    {
        g_frameTimes.push_back(t);
        while (g_frameTimes.size() > PROFILER_FRAME_COUNT)
            g_frameTimes.erase(g_frameTimes.begin());
    }

    void Initialize()
    {
        logger::RegisterOutputModule(std::make_unique<LogOutputModule_Editor>());
        
        // Attach built-in tools
        AttachToolToMenu(std::make_unique<Tool_TerrainGeneration>("Terrain Generator"));
        AttachToolToMenu(std::make_unique<Tool_ContentBrowser>("Scene Content Browser"));
        AttachToolToMenu(std::make_unique<Tool_Profiler>("Profiler"));
        AttachToolToMenu(std::make_unique<Tool_LogDisplay>("Backlog"));

        // Icons rendered by ImGui need's to be flipped manually at loadtime
        import_icon = resourcemanager::Load("assets/import.png", resourcemanager::LoadFlags::FlipImageBit);
        delete_icon = resourcemanager::Load("assets/delete.png", resourcemanager::LoadFlags::FlipImageBit);
        light_icon = resourcemanager::Load("assets/add.png", resourcemanager::LoadFlags::FlipImageBit);
        editor_icon_select = resourcemanager::Load("assets/select.png", resourcemanager::LoadFlags::FlipImageBit);
        translate_icon = resourcemanager::Load("assets/move.png", resourcemanager::LoadFlags::FlipImageBit);
        rotate_icon = resourcemanager::Load("assets/rotate.png",  resourcemanager::LoadFlags::FlipImageBit);
        scale_icon = resourcemanager::Load("assets/resize.png", resourcemanager::LoadFlags::FlipImageBit);

        initialized = true;
    }

    void Update()
    {
        CYB_PROFILE_FUNCTION();
        if (!initialized)
            return;

        ImGuizmo::BeginFrame();

        ImGui::Begin("CybEngine Editor", 0, ImGuiWindowFlags_MenuBar);
        DrawMenuBar();
        DrawIconBar();

        ImGui::Text("Scene Hierarchy:");
        ImGui::BeginChild("Scene hierarchy", ImVec2(0, 300), true, 0);
        scenegraph_view.GenerateView();
        scenegraph_view.Draw();
        ImGui::EndChild();
        
        ImGui::Text("Components:");
        const float componentChildHeight = math::Max(300.0f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("Components", ImVec2(0, componentChildHeight), true);
        EditEntityComponents(scenegraph_view.SelectedEntity());
        ImGui::EndChild();

        ImGui::End();

        DrawGizmo();

        // Pick on left mouse click
        if (guizmo_operation & ImGuizmo::BOUNDS)
        {
            ImGuiIO& io = ImGui::GetIO();
            bool mouse_in_3dview = !io.WantCaptureMouse && !ImGuizmo::IsOver();
            if (mouse_in_3dview && io.MouseClicked[0])
            {
                const scene::Scene& scene = scene::GetScene();
                math::Ray pick_ray = GetPickRay(io.MousePos.x, io.MousePos.y);
                scene::PickResult pick_result = scene::Pick(scene, pick_ray);

                // Enable mouse picking on lightsources only if they are being drawn
                if (renderer::GetDebugLightsources())
                {
                    for (size_t i = 0; i < scene.lights.Size(); ++i)
                    {
                        ecs::Entity entity = scene.lights.GetEntity(i);
                        const scene::TransformComponent& transform = *scene.transforms.GetComponent(entity);

                        XMVECTOR disV = XMVector3LinePointDistance(XMLoadFloat3(&pick_ray.origin), XMLoadFloat3(&pick_ray.origin) + XMLoadFloat3(&pick_ray.direction), transform.GetPositionV());
                        float dis = XMVectorGetX(disV);
                        if (dis > 0.01f && dis < math::Distance(transform.GetPosition(), pick_ray.origin) * 0.05f && dis < pick_result.distance)
                        {
                            pick_result = scene::PickResult();
                            pick_result.entity = entity;
                            pick_result.distance = dis;
                        }
                    }
                }

                if (pick_result.entity != ecs::InvalidEntity)
                    scenegraph_view.SelectEntity(pick_result.entity);
            }
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