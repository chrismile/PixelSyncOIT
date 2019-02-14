#ifdef SHADOW_MAPPING_STANDARD
#include "ShadowMapping.glsl"
#elif defined(SHADOW_MAPPING_MOMENTS) && !defined(SHADOW_MAPPING_MOMENTS_GENERATE)
#include "MomentShadowMapping.glsl"
#else
float getShadowFactor(vec4 worldPosition)
{
    return 1.0;
}
#endif