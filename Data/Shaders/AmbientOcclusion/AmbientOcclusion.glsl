#ifdef USE_SSAO
uniform sampler2D ssaoTexture;
#elif defined(VOXEL_SSAO)
#include "VoxelAO.glsl"
#endif

float getAmbientOcclusionFactor(vec4 fragmentPositonWorld)
{
#ifdef USE_SSAO
    // Read ambient occlusion factor from texture
    vec2 texCoord = vec2(gl_FragCoord.xy + vec2(0.5, 0.5))/textureSize(ssaoTexture, 0);
    return texture(ssaoTexture, texCoord).r;
#elif defined(VOXEL_SSAO)
    return getAmbientOcclusionTermVoxelGrid(fragmentPositonWorld.xyz);
#else
    // No ambient occlusion
    return 1.0;
#endif
}