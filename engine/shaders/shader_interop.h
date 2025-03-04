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

#define CB_GETBINDSLOT(name) __CBUFFERBINDSLOT__##name##__
#define CBUFFER(name, slot) static const int CB_GETBINDSLOT(name) = slot; struct alignas(16) name
#define CBUFFER_NAME(name) 
#define CBPADDING_LINE2(num, line) float pading_##line[num];
#define CBPADDING_LINE(num, line) CBPADDING_LINE2(num, line)
#define CBPADDING(num) CBPADDING_LINE(num, __LINE__)
#else
// glsl shader-side types
#define CBUFFER(blockname, slot) layout(std140, binding = slot) uniform blockname
#define CBUFFER_NAME(name) name
#define CBPADDING(num)
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
    CBPADDING(1)
};

CBUFFER(FrameCB, CBSLOT_FRAME)
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
    CBPADDING(1)

    LightSource lights[SHADER_MAX_LIGHTSOURCES];
} CBUFFER_NAME(cbFrame);

CBUFFER(CameraCB, CBSLOT_CAMERA)
{
    mat4 proj;
    mat4 view;
    mat4 vp;                    // view * proj
    mat4 inv_proj;
    mat4 inv_view;
    mat4 inv_vp;
    vec4 pos;
} CBUFFER_NAME(cbCamera);

CBUFFER(MaterialCB, CBSLOT_MATERIAL)
{
    vec4 baseColor;
    float roughness;
    float metalness;
    CBPADDING(2)
} CBUFFER_NAME(cbMaterial);

#define IMAGE_FULLSCREEN_BIT (1 << 3)

CBUFFER(ImageCB, CBSLOT_IMAGE)
{
    int flags;
    CBPADDING(3)
    vec4 corners[4];
} CBUFFER_NAME(cbImage);

CBUFFER(MiscCB, CBSLOT_MISC)
{
    mat4 g_xModelMatrix;
    mat4 g_xTransform;                   // model * view * proj
};

#ifdef __cplusplus
#undef vec2
#undef vec3 
#undef vec4 
#undef mat4 
#endif // __cplusplus

#endif // SHADER_INTEROP_H