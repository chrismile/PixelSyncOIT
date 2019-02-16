#ifdef VOXEL_SSAO

// Convert points in world space to voxel space (voxel grid at range (0, 0, 0) to (1, 1, 1)).
uniform mat4 worldSpaceToVoxelTextureSpace;

uniform sampler3D aoTexture;

float getAmbientOcclusionTermVoxelGrid(vec3 posWorld)
{
    vec3 texCoord = (worldSpaceToVoxelTextureSpace * vec4(posWorld, 1.0)).xyz;
    return texture(aoTexture, texCoord).x;
}

#endif