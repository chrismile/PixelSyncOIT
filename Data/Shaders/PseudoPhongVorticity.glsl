-- TriangleVertex

#version 430 core

#define CONCAT_VERTEX_ATTRIBUTE_NAME(n) vertexAttribute##n
#define vertexAttribute CONCAT_VERTEX_ATTRIBUTE_NAME(IMPORTANCE_CRITERION_INDEX)

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 3) in float vertexAttribute;

out vec3 fragmentNormal;
out vec3 fragmentPositonWorld;
out vec3 screenSpacePosition;
out float fragmentAttribute;
out float lineCurvature;
out float lineLength;

void main()
{
    fragmentNormal = vertexNormal;
    fragmentPositonWorld = (mMatrix * vec4(vertexPosition, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(vertexPosition, 1.0)).xyz;
    fragmentAttribute = vertexAttribute;
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Vertex

#version 430 core

#define CONCAT_VERTEX_ATTRIBUTE_NAME(n) vertexAttribute##n
#define vertexAttribute CONCAT_VERTEX_ATTRIBUTE_NAME(IMPORTANCE_CRITERION_INDEX)

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexLineNormal;
layout(location = 2) in vec3 vertexLineTangent;
layout(location = 3) in float vertexAttribute;

out VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttribute;
};

void main()
{
    linePosition = vertexPosition;
    lineNormal = vertexLineNormal;
    lineTangent = vertexLineTangent;
    lineAttribute = vertexAttribute;
}


-- Geometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 32) out;


uniform float radius = 0.001f;

in VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttribute;
} v_in[];

out vec3 fragmentNormal;
out vec3 fragmentPositonWorld;
out vec3 screenSpacePosition;
out float fragmentAttribute;

#define NUM_SEGMENTS 5

void main()
{
    vec3 currentPoint = v_in[0].linePosition.xyz;
    vec3 nextPoint = v_in[1].linePosition.xyz;

    vec3 circlePointsCurrent[NUM_SEGMENTS];
    vec3 circlePointsNext[NUM_SEGMENTS];
    vec3 vertexNormalsCurrent[NUM_SEGMENTS];
    vec3 vertexNormalsNext[NUM_SEGMENTS];

    vec3 normalCurrent = v_in[0].lineNormal;
    vec3 tangentCurrent = v_in[0].lineTangent;
    vec3 binormalCurrent = cross(tangentCurrent, normalCurrent);
    vec3 normalNext = v_in[1].lineNormal;
    vec3 tangentNext = v_in[1].lineTangent;
    vec3 binormalNext = cross(tangentNext, normalNext);

    mat3 tangentFrameMatrixCurrent = mat3(normalCurrent, binormalCurrent, tangentCurrent);
    mat3 tangentFrameMatrixNext = mat3(normalNext, binormalNext, tangentNext);

    const float theta = 2.0 * 3.1415926 / float(NUM_SEGMENTS);
    const float tangetialFactor = tan(theta); // opposite / adjacent
    const float radialFactor = cos(theta); // adjacent / hypotenuse

    vec2 position = vec2(radius, 0.0);
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        vec3 point2DCurrent = tangentFrameMatrixCurrent * vec3(position, 0.0);
        vec3 point2DNext = tangentFrameMatrixNext * vec3(position, 0.0);
        circlePointsCurrent[i] = point2DCurrent.xyz + currentPoint;
        circlePointsNext[i] = point2DNext.xyz + nextPoint;
        vertexNormalsCurrent[i] = normalize(circlePointsCurrent[i] - currentPoint);
        vertexNormalsNext[i] = normalize(circlePointsNext[i] - nextPoint);

        // Add the tangent vector and correct the position using the radial factor.
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }

    // Emit the tube triangle vertices
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        gl_Position = mvpMatrix * vec4(circlePointsCurrent[i], 1.0);
        fragmentNormal = vertexNormalsCurrent[i];
        fragmentPositonWorld = (mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        fragmentAttribute = v_in[0].lineAttribute;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsCurrent[(i+1)%NUM_SEGMENTS];
        fragmentPositonWorld = (mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        fragmentAttribute = v_in[0].lineAttribute;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
        fragmentNormal = vertexNormalsNext[i];
        fragmentPositonWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        fragmentAttribute = v_in[1].lineAttribute;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsNext[(i+1)%NUM_SEGMENTS];
        fragmentPositonWorld = (mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        fragmentAttribute = v_in[1].lineAttribute;
        EmitVertex();

        EndPrimitive();
    }
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition;

#if !defined(DIRECT_BLIT_GATHER) || defined(SHADOW_MAPPING_MOMENTS_GENERATE)
#include OIT_GATHER_HEADER
#endif

in vec3 fragmentNormal;
in vec3 fragmentPositonWorld;
in float fragmentAttribute;
in float lineCurvature;
in float lineLength;

#ifdef DIRECT_BLIT_GATHER
out vec4 fragColor;
#endif


#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
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

    vec4 colorAttribute = transferFunction(fragmentAttribute);

    vec3 normal = fragmentNormal;
    if (length(normal) < 0.5) {
        normal = vec3(1.0, 0.0, 0.0);
    }
    normal = normalize(fragmentNormal);

    #if REFLECTION_MODEL == 0 // PSEUDO_PHONG_LIGHTING
    vec3 colorShading = colorAttribute.rgb * clamp(dot(normal, lightDirection)/2.0
            + 0.75 * occlusionFactor * shadowFactor, 0.0, 1.0);
    #elif REFLECTION_MODEL == 1 // COMBINED_SHADOW_MAP_AND_AO
    vec3 colorShading = vec3(occlusionFactor * shadowFactor);
    #elif REFLECTION_MODEL == 2 // LOCAL_SHADOW_MAP_OCCLUSION
    vec3 colorShading = vec3(shadowFactor);
    #elif REFLECTION_MODEL == 3 // AMBIENT_OCCLUSION_FACTOR
    vec3 colorShading = vec3(occlusionFactor);
    #endif
    vec4 color = vec4(colorShading, colorAttribute.a);

    if (!transparencyMapping) {
        color.a = colorGlobal.a;
    }

    if (color.a < 1.0/255.0) {
        discard;
    }

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
