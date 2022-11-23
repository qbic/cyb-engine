#ifndef RENDER_SHADER_INTEROP2_H
#define RENDER_SHADER_INTEROP2_H

#ifdef __cplusplus
// c++ application-side types
#include <DirectXMath.h>
using namespace DirectX;

// Defining glsl builtin types instead of creating new types with typedef.
// This enables us to undef them at the end of file and not having glsl's
// builtin types clutter the global namespace.
#define vec3 XMFLOAT3
#define vec4 XMFLOAT4
#define mat4 XMFLOAT4X4

#define CB_GETBINDSLOT(name) __CBUFFERBINDSLOT__##name##__
#define CBUFFER(name, slot) static const int CB_GETBINDSLOT(name) = slot; struct alignas(16) name
#define CBUFFER_NAME(name) 
#define CBPADDING_LINE2(num, line) float pading_##line[num];
#define CBPADDING_LINE(num, line) CBPADDING_LINE2(num, line)
#define CBPADDING(num) CBPADDING_LINE(num, __LINE__)
#else
// glsl shader-side types
#define CBUFFER(blockname, slot) layout(binding = slot) uniform blockname
#define CBUFFER_NAME(name) name
#define CBPADDING(num)
#endif

#define CBSLOT_FRAME                        0
#define CBSLOT_CAMERA                       2
#define CBSLOT_MISC                         3
#define CBSLOT_MATERIAL                     4
#define CBSLOT_IMAGE                        5

#define SHADER_MAX_LIGHTSOURCES             256
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
    CBPADDING(1)
};

CBUFFER(FrameCB, CBSLOT_FRAME)
{
    vec3 horizon;
    float time;                 // game runtime in ms
    vec3 zenith;
    CBPADDING(1)
    vec3 fog;                   // fog [start, end, height]
    float gamma;

    int num_lights;
    CBPADDING(3)
    LightSource lights[SHADER_MAX_LIGHTSOURCES];
} CBUFFER_NAME(frame_cb);

CBUFFER(CameraCB, CBSLOT_CAMERA)
{
    mat4 proj;
    mat4 view;
    mat4 vp;                    // view * proj
    mat4 inv_proj;
    mat4 inv_view;
    mat4 inv_vp;
    vec4 pos;
} CBUFFER_NAME(camera_cb);

CBUFFER(MaterialCB, CBSLOT_MATERIAL)
{
    vec4 base_color;
    float roughness;
    float metalness;
    CBPADDING(2)
} CBUFFER_NAME(material_cb);

#define IMAGE_FLAG_FULLSCREEN (1 << 3)

CBUFFER(ImageCB, CBSLOT_IMAGE)
{
    int flags;
    CBPADDING(3)
    vec4 corners[4];
} CBUFFER_NAME(image_cb);

CBUFFER(MiscCB, CBSLOT_MISC)
{
    mat4 g_xModelMatrix;
    mat4 g_xTransform;                   // model * view * proj
};

#ifdef __cplusplus
#undef vec3 
#undef vec4 
#undef mat4 
#endif // __cplusplus

#endif // RENDER_SHADER_INTEROP2_H