-- Vertex

#version 430 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexTexcoord;

out vec2 fragTexcoord;

void main()
{
    fragTexcoord = vertexTexcoord;
	gl_Position = vec4(vertexPosition, 1.0);
}

-- Fragment

#version 430 core

#define KERNEL_SIZE 64

// Values used are proposed by https://learnopengl.com/Advanced-Lighting/SSAO
uniform vec3 samples[KERNEL_SIZE];
uniform float radius = 0.05;
uniform float bias = 0.025;

uniform sampler2D gPositionTexture;
uniform sampler2D gNormalTexture;
uniform sampler2D rotationVectorTexture;

in vec2 fragTexcoord;

layout (location = 0) out float fragColor;

void main()
{
    vec3 fragPosition = texture(gPositionTexture, fragTexcoord).xyz;
    float fragDepth = fragPosition.z;
    vec3 normal = normalize(texture(gNormalTexture, fragTexcoord).rgb);

    vec2 rotationVecTexScale = textureSize(gPositionTexture, 0) / textureSize(rotationVectorTexture, 0);
    vec2 rotationVecSamplePos = fragTexcoord * rotationVecTexScale.xy;
    vec3 rotationVec = normalize(texture(rotationVectorTexture, rotationVecSamplePos).xyz);

    // Create basis change matrix converting tangent space to view space
    vec3 tangent = normalize(rotationVec - normal * dot(rotationVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbnMat = mat3(tangent, bitangent, normal);

    // Compute occlusion factor as occlusion average over all kernel samples
    float occlusion = 0.0;
    for (int i = 0; i < KERNEL_SIZE; i++) {
        // Convert sample position from tangent space to view space
        vec3 sampleViewSpace = fragPosition + tbnMat * samples[i] * radius;

        // Apply projection matrix to view space sample to get position in clip space
        vec4 screenSpacePosition = vec4(sampleViewSpace, 1.0);
        screenSpacePosition = pMatrix * screenSpacePosition;
        screenSpacePosition.xyz /= screenSpacePosition.w;
        screenSpacePosition.xyz = screenSpacePosition.xyz * 0.5 + 0.5; // From NDC [-1,1] to [0,1]

        // Get depth at sample position (of kernel sample)
        float sampleDepth = texture(gPositionTexture, screenSpacePosition.xy).z;

        // Range check: Make sure only depth differences in the radius contribute to occlusion
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragDepth - sampleDepth));

        // Check if the sample contributes to occlusion
        occlusion += (sampleDepth >= sampleViewSpace.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    fragColor = 1.0 - (occlusion / KERNEL_SIZE);
}
