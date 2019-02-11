#ifdef SHADOW_MAPPING_STANDARD
#include "ShadowMapping.glsl"
#elif defined(SHADOW_MAPPING_MOMENTS)
#include "MomentShadowMapping.glsl"
#else
float getShadowFactor(vec4 worldPosition)
{
    return 1.0;
}
#endif