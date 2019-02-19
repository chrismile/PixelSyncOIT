-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
#ifdef COLOR_ARRAY
layout(location = 2) in vec4 vertexColor;
#endif
// Color of the object
//uniform vec4 colorGlobal;

out vec4 fragmentColor;
out vec3 fragmentNormal;
out vec3 fragmentPositonWorld;
out vec3 screenSpacePosition;

void main()
{
#ifdef COLOR_ARRAY
    //fragmentColor = vec4(vertexColor.rgb, vertexColor.a * colorGlobal.a);
    fragmentColor = vertexColor;
#else
    fragmentColor = vec4(1.0);//colorGlobal;
#endif
    fragmentNormal = vertexNormal;
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

in vec4 fragmentColor;
in vec3 fragmentNormal;
in vec3 fragmentPositonWorld;

#ifdef DIRECT_BLIT_GATHER
out vec4 fragColor;
#endif


#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
uniform vec3 cameraPosition; // world space

uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float specularExponent;
uniform float opacity;

void main()
{
    float occlusionFactor = getAmbientOcclusionFactor(vec4(fragmentPositonWorld, 1.0));
    float shadowFactor = getShadowFactor(vec4(fragmentPositonWorld, 1.0));

#ifdef COLOR_ARRAY
    vec3 diffuseColor = fragmentColor.rgb; // TODO opacity
#endif

    // Pseudo Phong shading
    vec3 ambientShading = ambientColor * 0.1 * occlusionFactor * shadowFactor;
    vec3 diffuseShading = diffuseColor * clamp(dot(fragmentNormal, lightDirection)/2.0+0.75
            * occlusionFactor * shadowFactor, 0.0, 1.0);
    vec3 specularShading = specularColor * specularExponent * 0.00001; // In order not to get an unused warning
    vec4 color = vec4(ambientShading + diffuseShading + specularShading, opacity * fragmentColor.a); // TODO opacity
    //color = vec4(vec3(occlusionFactor), 1.0);
    //color.rgb = vec3(occlusionFactor);
/*#ifdef USE_SSAO
    color = vec4(vec3(occlusionFactor, 0.0, 0.0), 1.0);
#endif*/

#ifdef DIRECT_BLIT_GATHER
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
