#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

#ifdef __cplusplus
// c++ application-side types
#include <DirectXMath.h>
using namespace DirectX;

// Defining glsl builtin types instead of creating new types with typedef.
// This enables us to undef them at the end of file and not having glsl's
// builtin types clutter the global namespace.
#define vec2 XMFLOAT2
#define vec3 XMFLOAT3
#define vec4 XMFLOAT4
#define mat4 XMFLOAT4X4

#define GETBINDSLOT(name) __CBUFFERBINDSLOT__##name##__
#define CONSTANTBUFFER(name, slot) static const int GETBINDSLOT(name) = slot; struct alignas(16) name
#define CONSTANTBUFFER_NAME(name) 
#define PUSHBUFFER(name) struct alignas(16) name
#define PUSHBUFFER_NAME(name) 
#define PADDING_LINE2(num, line) float pading_##line[num];
#define PADDING_LINE(num, line) PADDING_LINE2(num, line)
#define PADDING(num) PADDING_LINE(num, __LINE__)
namespace cyb::renderer
{
#else
// glsl shader-side types
#define CONSTANTBUFFER(blockname, slot) layout(std140, binding = slot) uniform blockname
#define CONSTANTBUFFER_NAME(name) name
#define PUSHBUFFER(blockname) layout(push_constant, std140) uniform blockname
#define PUSHBUFFER_NAME(name) name
#define PADDING(num)
#endif

#define CBSLOT_FRAME                        0
#define CBSLOT_CAMERA                       2
#define CBSLOT_MISC                         3
#define CBSLOT_MATERIAL                     4
#define CBSLOT_IMAGE                        5

#define SHADER_MAX_LIGHTSOURCES             64
#define LIGHTSOURCE_TYPE_DIRECTIONAL        0
#define LIGHTSOURCE_TYPE_POINT              1

struct LightSource
{
    vec4 position;
    vec4 direction;
    vec4 color;
    int type;
    float energy;
    float range;
    PADDING(1)
};

CONSTANTBUFFER(FrameConstants, CBSLOT_FRAME)
{
    vec3 horizon;
    float time;                 // game runtime in ms

    vec3 zenith;
    float gamma;

    vec4 fog;                   // fog [start, end, height, 1/(end-start)]

    float cloudiness;           // [0..1]
    float cloudTurbulence;
    float cloudHeight;
    float windSpeed;

    int numLights;
    int mostImportantLightIndex;
    int drawSun;
    PADDING(1)

    LightSource lights[SHADER_MAX_LIGHTSOURCES];
} CONSTANTBUFFER_NAME(cbFrame);

CONSTANTBUFFER(CameraConstants, CBSLOT_CAMERA)
{
    mat4 proj;
    mat4 view;
    mat4 vp;                    // view * proj
    mat4 inv_proj;
    mat4 inv_view;
    mat4 inv_vp;
    vec4 pos;
} CONSTANTBUFFER_NAME(camera);

CONSTANTBUFFER(MaterialCB, CBSLOT_MATERIAL)
{
    vec4 baseColor;
    float roughness;
    float metalness;
    PADDING(2)
} CONSTANTBUFFER_NAME(cbMaterial);

#define IMAGE_FULLSCREEN_BIT (1 << 3)

CONSTANTBUFFER(ImageConstants, CBSLOT_IMAGE)
{
    int flags;
    PADDING(3)
        vec4 corners[4];
} CONSTANTBUFFER_NAME(image);

CONSTANTBUFFER(MiscCB, CBSLOT_MISC)
{
    mat4 g_xModelMatrix;
    mat4 g_xTransform;                   // model * view * proj
};

PUSHBUFFER(PostProcess)
{
    vec4 param0;
    vec4 param1;
} PUSHBUFFER_NAME(postprocess);

#ifdef __cplusplus
} // namespace cyb::graphics
// clean up c++ namespace
#undef vec2
#undef vec3 
#undef vec4 
#undef mat4 

#undef GETBINDSLOT
#undef CONSTANTBUFFER
#undef CONSTANTBUFFER_NAME
#undef PUSHBUFFER
#undef PUSHBUFFER_NAME
#undef PADDING_LINE2
#undef PADDING_LINE
#undef PADDING
#endif // __cplusplus

#endif // SHADER_INTEROP_H