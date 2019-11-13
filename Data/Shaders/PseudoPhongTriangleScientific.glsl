-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

out vec3 fragmentNormal;
out vec3 fragmentPositonLocal;
out vec3 fragmentPositonWorld;
out vec3 screenSpacePosition;

void main()
{
    fragmentNormal = vertexNormal;
    fragmentPositonLocal = (vec4(vertexPosition, 1.0)).xyz;
    fragmentPositonWorld = (mMatrix * vec4(vertexPosition, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(vertexPosition, 1.0)).xyz;
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition;

#if !defined(DIRECT_BLIT_GATHER) || defined(SHADOW_MAPPING_MOMENTS_GENERATE)
#include OIT_GATHER_HEADER
#endif

in vec3 fragmentNormal;
in vec3 fragmentPositonLocal;
in vec3 fragmentPositonWorld;

#if defined(DIRECT_BLIT_GATHER) && !defined(SHADOW_MAPPING_MOMENTS_GENERATE)
out vec4 fragColor;
#endif


#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
uniform vec3 cameraPosition; // world space
uniform float aoFactorGlobal = 1.0f;
uniform float shadowFactorGlobal = 1.0f;


uniform float minCriterionValue = 0.0;
uniform float maxCriterionValue = 1.0;
uniform bool transparencyMapping = true;

// Color of the object
uniform vec4 colorGlobal;


// Transfer function color lookup table
uniform sampler1D transferFunctionTexture;

vec4 transferFunction(float attr)
{
    // Transfer to range [0,1]
    float posFloat = clamp((attr - minCriterionValue) / (maxCriterionValue - minCriterionValue), 0.0, 1.0);
    // Look up the color value
    return texture(transferFunctionTexture, posFloat);
}

void main()
{
#if !defined(SHADOW_MAP_GENERATE) && !defined(SHADOW_MAPPING_MOMENTS_GENERATE)
    float occlusionFactor = getAmbientOcclusionFactor(vec4(fragmentPositonWorld, 1.0));
    float shadowFactor = getShadowFactor(vec4(fragmentPositonWorld, 1.0));
    occlusionFactor = mix(1.0f, occlusionFactor, aoFactorGlobal);
    shadowFactor = mix(1.0f, shadowFactor, shadowFactorGlobal);
#else
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;
#endif

//    vec4 colorAttribute = transferFunction(fragmentAttribute);

    // Blinn-Phong Shading
    const vec3 lightColor = vec3(1,1,1);
    const vec3 ambientColor = vec3(0.5, fragmentPositonWorld.g, fragmentPositonWorld.b);
    const vec3 diffuseColor = ambientColor;
    vec3 phongColor = vec3(0);

    const float kA = 0.2 * occlusionFactor * shadowFactor;
    const vec3 Ia = kA * ambientColor;
    const float kD = 0.7;
    const float kS = 0.1;
    const float s = 10;

    const vec3 n = normalize(fragmentNormal);
    const vec3 v = normalize(cameraPosition - fragmentPositonWorld);
    const vec3 l = v;//normalize(lightDirection);
    const vec3 h = normalize(v + l);

    vec3 Id = kD * clamp(abs(dot(n, l)), 0.0, 1.0) * diffuseColor;
    vec3 Is = kS * pow(clamp(abs(dot(n, h)), 0.0, 1.0), s) * lightColor;

    phongColor = Ia + Id + Is;

    float tempAlpha = 0.3;
    vec4 color = vec4(phongColor, tempAlpha);

    #if REFLECTION_MODEL == 1 // COMBINED_SHADOW_MAP_AND_AO
    color.rgb = vec3(occlusionFactor * shadowFactor);
    #elif REFLECTION_MODEL == 2 // LOCAL_SHADOW_MAP_OCCLUSION
    color.rgb = vec3(shadowFactor);
    #elif REFLECTION_MODEL == 3 // AMBIENT_OCCLUSION_FACTOR
    color.rgb = vec3(occlusionFactor);
    #elif REFLECTION_MODEL == 4 // NO_LIGHTING
    color.rgb = diffuseColor;
    #endif


#if defined(DIRECT_BLIT_GATHER) && !defined(SHADOW_MAPPING_MOMENTS_GENERATE)
    // Direct rendering
    fragColor = color;
#else
#if defined(REQUIRE_INVOCATION_INTERLOCK) && !defined(TEST_NO_INVOCATION_INTERLOCK)
    // Area of mutual exclusion for fragments mapping to the same pixel
    beginInvocationInterlockARB();
    gatherFragment(color);
    endInvocationInterlockARB();
#else
    gatherFragment(color);
#endif
#endif
}
