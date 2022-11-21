#include <fstream>
#include "core/logger.h"
#include "Core/Timer.h"
#include "Systems/Scene.h"
#include "Graphics/ModelImport.h"

//#define DEBUG_3DS_LOADER    1

#ifdef DEBUG_3DS_LOADER
#define LocalDebugPrintf    DebugPrintf
#else
#define LocalDebugPrintf(...)
#endif

namespace cyb::renderer::import3ds
{
    enum ChunkID
    {
        // Primary chunk, at the beginning of each file
        CHUNKID_PRIMARY = 0x4D4D,

        // PRIMARY sub chunks
        CHUNKID_EDIT3DS = 0x3D3D,
        CHUNKID_VERSION = 0x0002,
        CHUNKID_MESHVERSION = 0x3D3E,
        CHUNKID_EDITKEYFRAME = 0xB000,

        // EDIT3DS sub chunks
        CHUNKID_EDITMATERIAL = 0xAFFF,
        CHUNKID_EDITOBJECT = 0x4000,

        // EDITMATERIAL sub chunks
        CHUNKID_MATNAME = 0xA000,
        CHUNKID_MATAMBIENT = 0xA010,
        CHUNKID_MATDIFFUSE = 0xA020,
        CHUNKID_MATSPECULAR = 0xA030,
        CHUNKID_MATSHININESS = 0xA040,
        CHUNKID_MATMAP = 0xA200,
        CHUNKID_MATMAPFILE = 0xA300,

        // EDITOBJECT sub chunks
        CHUNKID_OBJTRIMESH = 0x4100,

        // OBJTRIMESH sub chunks
        CHUNKID_TRIVERT = 0x4110,
        CHUNKID_TRIFACE = 0x4120,
        CHUNKID_TRIFACEMAT = 0x4130,
        CHUNKID_TRIUV = 0x4140,
        CHUNKID_TRISMOOTH = 0x4150,
        CHUNKID_TRILOCAL = 0x4160,

        // EIDTKEYFRAME sub chunks
        CHUNKID_KFMESH = 0xB002,
        CHUNKID_KFHEIRARCHY = 0xB030,
        CHUNKID_KFNAME = 0xB010,

        // Color chunk types
        CHUNKID_COL_RGB = 0x0010,
        CHUNKID_COL_TRU = 0x0011,
        CHUNKID_COL_UNK = 0x0013
    };

    struct Material
    {
        char            name[128];
        float           ambient[4];
        float           diffuse[4];
        float           specular[4];
        float           shininess;
    };

    struct MeshSubSet
    {
        char                    name[128] = { 0 };
        char                    material[128] = { 0 };
        std::vector<XMFLOAT3>   vertexes;
        std::vector<uint16_t>   indexes;
    };

    struct Model
    {
        uint32_t                version = 0;
        std::vector<Material>   materials;
        std::vector<MeshSubSet> meshes;
    };

    struct Chunk
    {
        uint16_t        id;
        uint32_t        length;
        const uint8_t* chunkBegin;
        const uint8_t* chunkEnd;
    };

    static Chunk ReadChunk(const uint8_t* buffer)
    {
        Chunk chunk;
        chunk.id = *(uint16_t*)(buffer);
        chunk.length = *(uint32_t*)(buffer + 2);
        chunk.chunkBegin = buffer + 6;
        chunk.chunkEnd = buffer + (size_t)chunk.length;
        return chunk;
    }

    static ptrdiff_t ReadString(char* dest, const ptrdiff_t maxLength, const uint8_t* buffer)
    {
        const uint8_t* bufferStart = buffer;

        do
        {
            if ((buffer - bufferStart) < maxLength)
                *dest++ = (char)*buffer;
        } while (*buffer++ != '\0');

        return buffer - bufferStart;
    }

    //==========================
    // TRIVERT (0x4110) CHUNK
    //==========================
    static void ParseChunk_TriVert(MeshSubSet& mesh, const Chunk* triVert)
    {
        const uint16_t numVertexes = *(uint16_t*)(triVert->chunkBegin);
        const float* vertexPtr = (float*)(triVert->chunkBegin + 2);

        LocalDebugPrintf("Parsing %d vertexes\n", numVertexes);

        mesh.vertexes.reserve(numVertexes);
        for (uint16_t i = 0; i < numVertexes; i++)
        {
            const float x = *vertexPtr++;
            const float y = *vertexPtr++;
            const float z = *vertexPtr++;
            mesh.vertexes.push_back(XMFLOAT3(x, y, z));
            //LocalDebugPrintf("%d: x=%f y=%f z=%f\n", i, x, y, z);
        }
    }

    //==========================
    // TRIFACEMAT (0x4130) CHUNK
    //==========================
    static void ParseChunk_TriFaceMat(MeshSubSet& mesh, const Chunk* triFaceMat)
    {
        ReadString(mesh.material, _countof(mesh.material), triFaceMat->chunkBegin);
    }

    //==========================
    // TRIFACE (0x4120) CHUNK
    //==========================
    static void ParseChunk_TriFace(MeshSubSet& mesh, const Chunk* triFace)
    {
        const uint16_t numTris = *(uint16_t*)(triFace->chunkBegin);
        const uint32_t numIndexes = numTris * 3;
        const uint16_t* indexPtr = (uint16_t*)(triFace->chunkBegin + 2);

        LocalDebugPrintf("Parsing %d triangles\n", numTris);

        mesh.indexes.reserve(numIndexes);
        for (uint32_t i = 0; i < numIndexes; i++)
        {
            mesh.indexes.push_back(*indexPtr++);
        }

        for (const uint8_t* chunkPos = (uint8_t*)indexPtr; chunkPos < triFace->chunkEnd;)
        {
            const Chunk subChunk = ReadChunk(chunkPos);

            switch (subChunk.id)
            {
            case CHUNKID_TRIFACEMAT:
                ParseChunk_TriFaceMat(mesh, &subChunk);
                break;

            default:
                LocalDebugPrintf("Unhandled chunk id=0x%x length=%d\n", subChunk.id, subChunk.length);
                break;
            }

            chunkPos = subChunk.chunkEnd;
        }
    }

    //==========================
    // OBJTRIMESH (0x4100) CHUNK
    //==========================
    static void ParseChunk_ObjTriMesh(MeshSubSet& mesh, const Chunk* objTriMesh)
    {
        for (const uint8_t* chunkPos = objTriMesh->chunkBegin; chunkPos < objTriMesh->chunkEnd;)
        {
            const Chunk subChunk = ReadChunk(chunkPos);

            switch (subChunk.id)
            {
            case CHUNKID_TRIVERT:
                ParseChunk_TriVert(mesh, &subChunk);
                break;

            case CHUNKID_TRIFACE:
                ParseChunk_TriFace(mesh, &subChunk);
                break;

            default:
                LocalDebugPrintf("Unhandled chunk id=0x%x length=%d\n", subChunk.id, subChunk.length);
                break;
            }

            chunkPos = subChunk.chunkEnd;
        }
    }

    //==========================
    // EDITOBJECT (0x4000) CHUNK
    //==========================
    static void  ParseChunk_EditObject(MeshSubSet& mesh, const Chunk* editObject)
    {
        // read object name
        const uint8_t* chunkPos = editObject->chunkBegin;
        chunkPos += ReadString(mesh.name, _countof(mesh.name), chunkPos);

        while (chunkPos < editObject->chunkEnd)
        {
            const Chunk subChunk = ReadChunk(chunkPos);

            switch (subChunk.id)
            {
            case CHUNKID_OBJTRIMESH:
                ParseChunk_ObjTriMesh(mesh, &subChunk);
                break;

            default:
                LocalDebugPrintf("Unhandled chunk id=0x%x length=%d\n", subChunk.id, subChunk.length);
                break;
            }

            chunkPos = subChunk.chunkEnd;
        }

        LocalDebugPrintf("Added object name=%s numVerts=%d numTris=%d material=%s\n", mesh.name, mesh.vertexes.size(), mesh.indexes.size() / 3, mesh->material.name);
    }

    //==========================
    // COLOR CHUNK
    //==========================
    static void ParseChunk_Color(float* rgba, const Chunk* color)
    {
        for (const uint8_t* chunkPos = color->chunkBegin; chunkPos < color->chunkEnd;)
        {
            const Chunk colorFormatChunk = ReadChunk(chunkPos);

            switch (colorFormatChunk.id)
            {
            case CHUNKID_COL_TRU:
                for (int i = 0; i < 3; ++i)
                    rgba[i] = 1.0f * colorFormatChunk.chunkBegin[i] / 255.0f;
                rgba[3] = 1.0f;
                //LocalDebugPrintf("COL_TRU chunk color=[ %f, %f, %f ] length=%d\n",  rgba[0], rgba[1], rgba[2], chunk.length);
                break;

            default:
                LocalDebugPrintf("Unknown color format id=0x%x length=%d\n", colorFormatChunk.id, colorFormatChunk.length);
                break;
            }

            chunkPos = colorFormatChunk.chunkEnd;
        }
    }

    //==========================
    // EDITMATERIAL (0xAFFF) CHUNK
    //==========================
    static void ParseChunk_EditMaterial(Material& mat, const Chunk* editMateral)
    {
        for (const uint8_t* chunkPos = editMateral->chunkBegin; chunkPos < editMateral->chunkEnd;)
        {
            const Chunk chunk = ReadChunk(chunkPos);

            switch (chunk.id)
            {
            case CHUNKID_MATNAME:
                LocalDebugPrintf("MATNAME chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                ReadString(mat.name, _countof(mat.name), chunk.chunkBegin);
                break;

            case CHUNKID_MATAMBIENT:
                LocalDebugPrintf("MATAMBIENT chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                ParseChunk_Color(mat.ambient, &chunk);
                break;

            case CHUNKID_MATDIFFUSE:
                LocalDebugPrintf("MATDIFFUSE chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                ParseChunk_Color(mat.diffuse, &chunk);
                break;

            case CHUNKID_MATSPECULAR:
                LocalDebugPrintf("MATSPECULAR chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                ParseChunk_Color(mat.specular, &chunk);
                break;

            default:
                LocalDebugPrintf("Unhandled chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                break;
            }

            chunkPos = chunk.chunkEnd;
        }

        LocalDebugPrintf("Added material name=%s\n", mat.name);
    }


    //==========================
    // EDIT3DS (0x3D3D) CHUNK
    //==========================
    static void ParseChunk_Edit(Model& model, const Chunk* edit)
    {
        for (const uint8_t* chunkPos = edit->chunkBegin; chunkPos < edit->chunkEnd;)
        {
            const Chunk chunk = ReadChunk(chunkPos);

            switch (chunk.id)
            {
            case CHUNKID_EDITOBJECT:
                LocalDebugPrintf("EDITOBJECT chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                model.meshes.emplace_back();
                ParseChunk_EditObject(model.meshes.back(), &chunk);
                break;

            case CHUNKID_EDITMATERIAL:
                LocalDebugPrintf("EDITMATERIAL chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                model.materials.emplace_back();
                ParseChunk_EditMaterial(model.materials.back(), &chunk);
                break;

            default:
                LocalDebugPrintf("Unhandled chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                break;
            }

            chunkPos = chunk.chunkEnd;
        }
    }

    //==========================
    // PRIMARY (0x4D4D) CHUNK
    //==========================
    static void ParseChunk_Main(Model& model, const Chunk* primary)
    {
        for (const uint8_t* chunkPos = primary->chunkBegin; chunkPos < primary->chunkEnd;)
        {
            const Chunk chunk = ReadChunk(chunkPos);

            switch (chunk.id)
            {
            case CHUNKID_VERSION:
                LocalDebugPrintf("VERSION chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                model.version = *(uint32_t*)chunk.chunkBegin;
                break;

            case CHUNKID_EDIT3DS:
                LocalDebugPrintf("EDIT3DS chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                ParseChunk_Edit(model, &chunk);
                break;

            default:
                LocalDebugPrintf("Unhandled chunk id=0x%x length=%d\n", chunk.id, chunk.length);
                break;
            }

            chunkPos = chunk.chunkEnd;
        }
    }

    bool Model_Load3DS(Model& model, const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            CYB_ERROR("Failed to open file (filename={0}): {1}\n", filename, strerror(errno));
            return false;
        }

        size_t dataSize = file.tellg();
        file.seekg(0, file.beg);
        char* fileContent = new char[dataSize];
        file.read(fileContent, dataSize);
        file.close();

        const Chunk primaryChunk = ReadChunk((uint8_t*)fileContent);
        if (primaryChunk.id != CHUNKID_PRIMARY)
        {
            delete[] fileContent;
            CYB_ERROR("Failed to load 3ds (filename={0}: Bad file magic", filename);
            return false;
        }

        memset(&model, 0, sizeof(model));
        ParseChunk_Main(model, &primaryChunk);
        delete[] fileContent;

        return true;
    }

#undef DEBUG_3DS_LOADER
}

namespace cyb::renderer
{
    void Convert3DSToScene(const import3ds::Model& model, scene::Scene& scene)
    {
        // Import model materials
        for (const auto& sourceMaterial : model.materials)
        {
            ecs::Entity materialEntity = scene.CreateMaterial(sourceMaterial.name);
            scene::MaterialComponent* material = scene.materials.GetComponent(materialEntity);
            material->baseColor = XMFLOAT4(sourceMaterial.diffuse);
        }

        // Import geometry
        for (const auto& sourceSubMesh : model.meshes)
        {
            ecs::Entity meshEntity = scene.CreateMesh(std::string(sourceSubMesh.name) + "_mesh");
            scene::MeshComponent* destMesh = scene.meshes.GetComponent(meshEntity);
            destMesh;
            //BackLog::Post("Importing submesh: %s", scene.names.GetComponent(meshEntity)->name.c_str());
        }
    }

    void ImportModel_3DS(const std::string& filename, scene::Scene& scene)
    {
        Timer timer;

        timer.Record();
        import3ds::Model model = {};
        if (!import3ds::Model_Load3DS(model, filename.c_str()))
        {
            return;
        }

        Convert3DSToScene(model, scene);

        CYB_TRACE("Imported model (filename={} in {:.2f}ms", filename, timer.ElapsedMilliseconds());
    }
}