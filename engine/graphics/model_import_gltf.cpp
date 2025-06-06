#include "core/logger.h"
#include "core/timer.h"
#include "graphics/model_import.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

namespace cyb::renderer
{
    struct ImportState
    {
        tinygltf::Model gltfModel;
        scene::Scene* scene = nullptr;
        std::unordered_map<size_t, ecs::Entity> entityMap;   // node -> entity
    };

    // Recursively loads nodes and resolves hierarchy
    ecs::Entity LoadNode(ImportState& state, uint32_t mesh_offset, int node_index, ecs::Entity parent)
    {
        if (node_index < 0)
            return ecs::INVALID_ENTITY;

        const auto& node = state.gltfModel.nodes[node_index];
        ecs::Entity entity = ecs::INVALID_ENTITY;

        if (node.mesh >= 0)
        {
            assert(node.mesh < (int)state.scene->meshes.Size());

            // if (node.skin < 0):  mesh instance
            // if (node.skin >= 0): armature
            if (node.skin >= 0)
            {
                CYB_WARNING("ImportGLTF: Unhandled armature skin={} name={}", node.skin, node.name);
            }
            else
            {
                entity = state.scene->CreateObject(node.name);
                scene::ObjectComponent* object = state.scene->objects.GetComponent(entity);
                object->meshID = state.scene->meshes.GetEntity(node.mesh + mesh_offset);
            }
        }

        if (entity == ecs::INVALID_ENTITY)
        {
            entity = ecs::CreateEntity();
            state.scene->transforms.Create(entity);
            state.scene->names.Create(entity, node.name);
        }

        state.entityMap[node_index] = entity;

        scene::TransformComponent* transform = state.scene->transforms.GetComponent(entity);
        if (!node.scale.empty())
            transform->scale_local = XMFLOAT3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
        if (!node.rotation.empty())
            transform->rotation_local = XMFLOAT4((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
        if (!node.translation.empty())
            transform->translation_local = XMFLOAT3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
        transform->UpdateTransform();

        if (parent != ecs::INVALID_ENTITY)
            state.scene->ComponentAttach(entity, parent);

        if (!node.children.empty())
        {
            for (int child : node.children)
                LoadNode(state, mesh_offset, child, entity);
        }

        return entity;
    }

    ecs::Entity ImportModel_GLTF(const std::string& filename, scene::Scene& scene)
    {
        Timer timer;
        timer.Record();

        ImportState state;
        state.scene = &scene;

        tinygltf::TinyGLTF gltfLoader;
        std::string errorMsg;
        std::string warningMsg;

        bool ret = gltfLoader.LoadBinaryFromFile(&state.gltfModel, &errorMsg, &warningMsg, filename);
        if (!ret)
        {
            CYB_ERROR("ImportModel_GLTF failed to load file (filename={0}): {1}", filename, errorMsg);
            return ecs::INVALID_ENTITY;
        }

        CYB_CWARNING(!warningMsg.empty(), "ImportModel_GLTF (filename={0}): {1}", filename, warningMsg);

        // Create materials
        for (const auto& x : state.gltfModel.materials)
        {
            ecs::Entity material_id = scene.CreateMaterial(x.name);
            state.entityMap.emplace(state.entityMap.size(), material_id);
            scene::MaterialComponent* material = scene.materials.GetComponent(material_id);

            const auto& baseColorFactor = x.values.find("baseColorFactor");
            if (baseColorFactor != x.values.end())
            {
                material->baseColor.x = static_cast<float>(baseColorFactor->second.ColorFactor()[0]);
                material->baseColor.y = static_cast<float>(baseColorFactor->second.ColorFactor()[1]);
                material->baseColor.z = static_cast<float>(baseColorFactor->second.ColorFactor()[2]);
                material->baseColor.w = static_cast<float>(baseColorFactor->second.ColorFactor()[3]);
            }

            const auto& roughnessFactor = x.values.find("roughnessFactor");
            if (roughnessFactor != x.values.end())
                material->roughness = static_cast<float>(roughnessFactor->second.Factor());

            const auto& metallicFactor = x.values.find("metallicFactor");
            if (metallicFactor != x.values.end())
                material->metalness = static_cast<float>(metallicFactor->second.Factor());
        }

        // Create meshes:
        uint32_t mesh_offset = static_cast<uint32_t>(scene.meshes.Size());
        for (const auto& x : state.gltfModel.meshes)
        {
            ecs::Entity meshID = scene.CreateMesh(x.name);
            scene::MeshComponent* mesh = scene.meshes.GetComponent(meshID);

            //CYB_TRACE("New mesh name={0}", x.name);

            for (auto& prim : x.primitives)
            {
                mesh->subsets.push_back(scene::MeshComponent::MeshSubset());
                scene::MeshComponent::MeshSubset& subset = mesh->subsets.back();

                // create a default material if none were provided
                if (scene.materials.Size() == 0)
                    scene.materials.Create(ecs::CreateEntity());

                subset.materialID = state.entityMap[prim.material];
                scene::MaterialComponent* material = scene.materials.GetComponent(subset.materialID);

                // Read submesh indices:
                const tinygltf::Accessor& indexAccessor = state.gltfModel.accessors[prim.indices];
                const tinygltf::BufferView& indexBufferView = state.gltfModel.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = state.gltfModel.buffers[indexBufferView.buffer];

                const int indexStride = indexAccessor.ByteStride(indexBufferView);
                const size_t indexCount = indexAccessor.count;
                const size_t indexOffset = mesh->indices.size();
                mesh->indices.resize(indexOffset + indexCount);
                subset.indexOffset = (uint32_t)indexOffset;
                subset.indexCount = (uint32_t)indexCount;

                const uint32_t vertexOffset = (uint32_t)mesh->vertex_positions.size();
                const unsigned char* indexData = indexBuffer.data.data() + indexAccessor.byteOffset + indexBufferView.byteOffset;

                // gltf uses right-handed coordinate system, so we need to remap the
                // indices to work on our left-hand system
                const size_t indexRemap[] = { 0, 2, 1 };
                for (size_t i = 0; i < indexCount; i += 3)
                {
                    switch (indexStride)
                    {
                    case 1:
                        mesh->indices[indexOffset + i + 0] = vertexOffset + indexData[i + indexRemap[0]];
                        mesh->indices[indexOffset + i + 1] = vertexOffset + indexData[i + indexRemap[1]];
                        mesh->indices[indexOffset + i + 2] = vertexOffset + indexData[i + indexRemap[2]];
                        break;
                    case 2:
                        mesh->indices[indexOffset + i + 0] = vertexOffset + ((uint16_t*)indexData)[i + indexRemap[0]];
                        mesh->indices[indexOffset + i + 1] = vertexOffset + ((uint16_t*)indexData)[i + indexRemap[1]];
                        mesh->indices[indexOffset + i + 2] = vertexOffset + ((uint16_t*)indexData)[i + indexRemap[2]];
                        break;
                    case 4:
                        mesh->indices[indexOffset + i + 0] = vertexOffset + ((uint32_t*)indexData)[i + indexRemap[0]];
                        mesh->indices[indexOffset + i + 1] = vertexOffset + ((uint32_t*)indexData)[i + indexRemap[1]];
                        mesh->indices[indexOffset + i + 2] = vertexOffset + ((uint32_t*)indexData)[i + indexRemap[2]];
                        break;
                    default:
                        assert(0 && "invalid index stride!");
                        break;
                    }
                }

                // Read submesh vertices:
                for (auto& attrib : prim.attributes)
                {
                    const std::string& attr_name = attrib.first;
                    const int attribData = attrib.second;

                    const tinygltf::Accessor& vertexAccessor = state.gltfModel.accessors[attribData];
                    const tinygltf::BufferView& vertexBufferView = state.gltfModel.bufferViews[vertexAccessor.bufferView];
                    const tinygltf::Buffer& vertexBuffer = state.gltfModel.buffers[vertexBufferView.buffer];

                    const int vertexStride = vertexAccessor.ByteStride(vertexBufferView);
                    const size_t vertexCount = vertexAccessor.count;

                    const unsigned char* vertexData = vertexBuffer.data.data() + vertexAccessor.byteOffset + vertexBufferView.byteOffset;
                    if (!attr_name.compare("POSITION"))
                    {
                        assert(vertexStride == 12);
                        mesh->vertex_positions.resize(vertexOffset + vertexCount);
                        for (size_t i = 0; i < vertexCount; ++i)
                            mesh->vertex_positions[vertexOffset + i] = ((XMFLOAT3*)vertexData)[i];
                    }
                    else if (!attr_name.compare("NORMAL"))
                    {
                        assert(vertexStride == 12);
                        mesh->vertex_normals.resize(vertexOffset + vertexCount);
                        for (size_t i = 0; i < vertexCount; ++i)
                            mesh->vertex_normals[vertexOffset + i] = ((XMFLOAT3*)vertexData)[i];
                    }
                    else if (!attr_name.compare("COLOR_0"))
                    {
                        if (material != nullptr)
                            material->SetUseVertexColors(true);

                        mesh->vertex_colors.resize(vertexOffset + vertexCount);
                        for (size_t i = 0; i < vertexCount; ++i)
                        {
                            const XMFLOAT4& color = ((XMFLOAT4*)vertexData)[i];
                            uint32_t rgba = math::StoreColor_RGBA(color);

                            mesh->vertex_colors[vertexOffset + i] = rgba;
                        }
                    }
                }
            }

            // create white vertex colors for all vertices
            if (mesh->vertex_colors.empty())
            {
                mesh->vertex_colors.resize(mesh->vertex_positions.size());
                for (auto& color : mesh->vertex_colors)
                    color = math::StoreColor_RGBA(XMFLOAT4(1, 1, 1, 1));
            }

            mesh->CreateRenderData();
        }

        // Create transform hierarchy, assign objects, meshes, armatures, cameras:
        ecs::Entity rootEntity = ecs::INVALID_ENTITY;
        const tinygltf::Scene& glftScene = state.gltfModel.scenes[state.gltfModel.defaultScene];
        for (size_t i = 0; i < glftScene.nodes.size(); i++)
            rootEntity = LoadNode(state, mesh_offset, glftScene.nodes[i], ecs::INVALID_ENTITY);

        // Create animations
        for (const auto& anim : state.gltfModel.animations)
        {
            ecs::Entity entity = ecs::CreateEntity();
            scene.names.Create(entity, anim.name);
            scene.ComponentAttach(entity, rootEntity);
            scene::AnimationComponent& animComponent = scene.animations.Create(entity);
            animComponent.samplers.resize(anim.samplers.size());
            animComponent.channels.resize(anim.channels.size());

            for (size_t i = 0; i < anim.samplers.size(); ++i)
            {
                auto& sampler = anim.samplers[i];

                if (sampler.interpolation.compare("LINEAR") == 0)
                    animComponent.samplers[i].mode = scene::AnimationComponent::Sampler::Mode::Linear;
                else if (sampler.interpolation.compare("STEP") == 0)
                    animComponent.samplers[i].mode = scene::AnimationComponent::Sampler::Mode::Step;
                else if (sampler.interpolation.compare("CUBICSPLINE") == 0)
                    animComponent.samplers[i].mode = scene::AnimationComponent::Sampler::Mode::CubicSpline;

                // sampler.input = keyframe times
                {
                    const tinygltf::Accessor& accessor = state.gltfModel.accessors[sampler.input];
                    const tinygltf::BufferView& bufferView = state.gltfModel.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = state.gltfModel.buffers[bufferView.buffer];
                    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                    int stride = accessor.ByteStride(bufferView);
                    assert(stride == 4);
                    size_t count = accessor.count;

                    std::vector<float>& keyframeTimes = animComponent.samplers[i].keyframeTimes;
                    keyframeTimes.resize(count);

                    const unsigned char* data = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

                    for (size_t j = 0; j < count; ++j)
                    {
                        const float time = ((float*)data)[j];
                        keyframeTimes[j] = time;
                        animComponent.start = std::min(animComponent.start, time);
                        animComponent.end = std::max(animComponent.end, time);
                    }
                }

                // sampler.ouput = keyframe data
                {
                    const tinygltf::Accessor& accessor = state.gltfModel.accessors[sampler.output];
                    const tinygltf::BufferView& bufferView = state.gltfModel.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = state.gltfModel.buffers[bufferView.buffer];

                    int stride = accessor.ByteStride(bufferView);
                    size_t count = accessor.count;

                    std::vector<float>& keyframeData = animComponent.samplers[i].keyframeData;
                    const unsigned char* data = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

                    switch (accessor.type)
                    {
                    case TINYGLTF_TYPE_SCALAR:
                    {
                        assert(stride == sizeof(float));
                        keyframeData.resize(count);
                        for (size_t j = 0; j < count; ++j)
                            keyframeData[j] = ((float*)data)[j];
                    } break;
                    case TINYGLTF_TYPE_VEC3:
                    {
                        assert(stride == sizeof(XMFLOAT3));
                        keyframeData.resize(count * 3);
                        for (size_t j = 0; j < count; ++j)
                            ((XMFLOAT3*)keyframeData.data())[j] = ((XMFLOAT3*)data)[j];
                    } break;
                    case TINYGLTF_TYPE_VEC4:
                    {
                        assert(stride == sizeof(XMFLOAT4));
                        keyframeData.resize(count * 4);
                        for (size_t j = 0; j < count; ++j)
                            ((XMFLOAT4*)keyframeData.data())[j] = ((XMFLOAT4*)data)[j];
                    } break;
                    default:
                        assert(0);
                        break;
                    }
                }
            }

            for (size_t i = 0; i < anim.channels.size(); ++i)
            {
                auto& channel = anim.channels[i];

                animComponent.channels[i].target = state.entityMap[channel.target_node];
                assert(channel.sampler >= 0);
                animComponent.channels[i].samplerIndex = (uint32_t)channel.sampler;

                if (!channel.target_path.compare("scale"))
                    animComponent.channels[i].path = scene::AnimationComponent::Channel::Path::Scale;
                else if (!channel.target_path.compare("rotation"))
                    animComponent.channels[i].path = scene::AnimationComponent::Channel::Path::Rotation;
                else if (!channel.target_path.compare("translation"))
                    animComponent.channels[i].path = scene::AnimationComponent::Channel::Path::Translation;
                else if (!channel.target_path.compare("weights"))
                    animComponent.channels[i].path = scene::AnimationComponent::Channel::Path::Weights;
                else
                    animComponent.channels[i].path = scene::AnimationComponent::Channel::Path::Unknown;
            }
        }

        CYB_TRACE("Imported model (filename={0}) in {1:.2f} milliseconds", filename, timer.ElapsedMilliseconds());
        return rootEntity;
    }
}