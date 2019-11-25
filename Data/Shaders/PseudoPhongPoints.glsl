-- Vertex

#version 430 core

#include "VertexAttributeNames.glsl"

layout(location = 0) in vec3 vertexPosition;
layout(location = 3) in float VERTEX_ATTRIBUTE;

out VertexData
{
    vec3 pointPosition;
    float pointAttribute;
};

void main()
{
    pointPosition = vertexPosition;
    pointAttribute = VERTEX_ATTRIBUTE;
}


-- Geometry

#version 430 core

layout(points) in;

layout(triangle_strip, max_vertices = 4) out;
uniform vec3 cameraPosition;

uniform float radius;

in VertexData
{
    vec3 pointPosition;
    float pointAttribute;
} v_in[];

out vec3 fragmentNormal;
out vec3 fragmentPositionWorld;
out vec3 worldSpaceSplatCenter;
out vec3 screenSpacePosition;
out vec2 fragmentTextureCoords;
out float fragmentAttribute;

void main()
{
    vec3 pointCoords = (mMatrix * vec4(v_in[0].pointPosition, 1.0)).xyz;
    float attribueValue = v_in[0].pointAttribute;

    vec3 quadNormal = normalize(cameraPosition - pointCoords);
    vec3 vertexPosition;

    //radius = 10;

    worldSpaceSplatCenter = pointCoords;

//    vec3 right = cross(quadNormal, vec3(0, 1, 0));
//    vec3 top = cross(quadNormal, right);

    mat4 vMatrixInv = inverse(vMatrix);
    vec3 right = (vMatrixInv * vec4(vec3(1, 0, 0), 0.0)).xyz;
    vec3 top = (vMatrixInv * vec4(vec3(0, 1, 0), 0.0)).xyz;

    vec3 splatCenter = pointCoords + quadNormal * radius;

    vertexPosition = splatCenter + radius * (right - top);
    fragmentNormal = quadNormal;
    fragmentPositionWorld = vertexPosition;
    screenSpacePosition = (vMatrix * vec4(vertexPosition, 1.0)).xyz;
    fragmentTextureCoords = vec2(1, 0);
    fragmentAttribute = attribueValue;
    gl_Position = pMatrix * vMatrix * vec4(vertexPosition, 1.0);
    EmitVertex();

    vertexPosition = splatCenter + radius* 2 * (right + top);
    fragmentNormal = quadNormal;
    fragmentPositionWorld = vertexPosition;
    screenSpacePosition = (vMatrix * vec4(vertexPosition, 1.0)).xyz;
    fragmentTextureCoords = vec2(1, 1);
    gl_Position = pMatrix * vMatrix * vec4(vertexPosition, 1.0);
    EmitVertex();

    vertexPosition = splatCenter + radius* 2 * (-right - top);
    fragmentNormal = quadNormal;
    fragmentPositionWorld = vertexPosition;
    screenSpacePosition = (vMatrix * vec4(vertexPosition, 1.0)).xyz;
    fragmentTextureCoords = vec2(0, 0);
    fragmentAttribute = attribueValue;
    gl_Position = pMatrix * vMatrix * vec4(vertexPosition, 1.0);
    EmitVertex();

    vertexPosition = splatCenter + radius * 2 * (-right + top);
    fragmentNormal = quadNormal;
    fragmentPositionWorld = vertexPosition;
    screenSpacePosition = (vMatrix * vec4(vertexPosition, 1.0)).xyz;
    fragmentTextureCoords = vec2(0, 1);
    fragmentAttribute = attribueValue;
    gl_Position = pMatrix * vMatrix * vec4(vertexPosition, 1.0);
    EmitVertex();

    EndPrimitive();
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition;
in vec3 worldSpaceSplatCenter;

#if !defined(DIRECT_BLIT_GATHER) || defined(SHADOW_MAPPING_MOMENTS_GENERATE)
#include OIT_GATHER_HEADER
#endif

in vec3 fragmentNormal;
in vec3 fragmentPositionWorld;
in vec2 fragmentTextureCoords;
in float fragmentAttribute;

#ifdef DIRECT_BLIT_GATHER
out vec4 fragColor;
#endif


#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"

#define M_PI 3.14159265358979323846

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
uniform vec3 cameraPosition; // world space
uniform float aoFactorGlobal = 1.0f;
uniform float shadowFactorGlobal = 1.0f;


uniform float minCriterionValue = 0.0;
uniform float maxCriterionValue = 1.0;
uniform bool transparencyMapping = true;

uniform float radius;

// Color of the object
uniform vec4 colorGlobal;


// Transfer function color lookup table
uniform sampler1D transferFunctionTexture;

bool raySphereIntersection(in vec3 origin, in vec3 dir, in vec3 center, in float r, out float t)
{
    t = 0;
    vec3 OC = origin - center;

    float a = 1;
    float b = 2 * dot(dir, OC);
    float c = dot(OC, OC) - r * r;

    float root = b * b - 4 * a * c;

    // no solution
    if (root < 0)
    {
        return false;
    }
    // one solution
    if (root == 0)
    {
        t = (-b) / (2 * a);
        return true;
    } else
    // multiple solutions
    {
        float t1 = (sqrt(root) - b) / (2 * a);
        float t2 = (-b - sqrt(root)) / (2 * a);

        t = min(t1, t2);
        return true;
    }
}

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
    float occlusionFactor = getAmbientOcclusionFactor(vec4(fragmentPositionWorld, 1.0));
    float shadowFactor = getShadowFactor(vec4(fragmentPositionWorld, 1.0));
    occlusionFactor = mix(1.0f, occlusionFactor, aoFactorGlobal);
    shadowFactor = mix(1.0f, shadowFactor, shadowFactorGlobal);
#else
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;
#endif

    vec4 colorAttribute = transferFunction(fragmentAttribute);

    vec3 origin = cameraPosition;
    vec3 dir = normalize(fragmentPositionWorld - cameraPosition);
    vec3 center = worldSpaceSplatCenter;
    float tRay = 0;

    bool isIntersect = raySphereIntersection(origin, dir, center, radius, tRay);
    vec3 spherePos = fragmentPositionWorld;
    vec3 sphereNormal = fragmentNormal;

    if (isIntersect)
    {
        spherePos = origin + tRay * dir;
        sphereNormal = normalize(spherePos - center);
    }
    else
    {
        discard;
    }

    float opacity = colorAttribute.a;

    #if REFLECTION_MODEL == 0 // PSEUDO_PHONG_LIGHTING
    const vec3 lightColor = vec3(1,1,1);
    const vec3 ambientColor = colorAttribute.rgb;
    const vec3 diffuseColor = colorAttribute.rgb;

    const float kA = 0.4 * occlusionFactor * shadowFactor;
    const vec3 Ia = kA * ambientColor;
    const float kD = 0.5;
    const float kS = 0.1;
    const float s = 10;

    const vec3 n = normalize(sphereNormal);
    const vec3 v = normalize(cameraPosition - spherePos);
//    const vec3 l = normalize(lightDirection);
    const vec3 l = normalize(v);
    const vec3 h = normalize(v + l);

//    #ifdef CONVECTION_ROLLS
//    vec3 t = normalize(cross(vec3(0, 0, 1), n));
//    #else
//    vec3 t = normalize(cross(vec3(0, 0, 1), n));
//    #endif

    vec3 Id = kD * clamp(abs(dot(n, l)), 0.0, 1.0) * diffuseColor;
    vec3 Is = kS * pow(clamp(abs(dot(n, h)), 0.0, 1.0), s) * lightColor;

//    float haloParameter = 1;
//    float angle1 = abs( dot( v, n));
//    float angle2 = abs( dot( v, normalize(t))) * 0.7;
//    float halo = mix(1.0f,((angle1)+(angle2)) , haloParameter);

    vec3 colorShading = Ia + Id + Is;

//    colorShading *= clamp(halo, 0, 1) * clamp(halo, 0, 1);
    #elif REFLECTION_MODEL == 1 // COMBINED_SHADOW_MAP_AND_AO
    vec3 colorShading = vec3(occlusionFactor * shadowFactor);
    #elif REFLECTION_MODEL == 2 // LOCAL_SHADOW_MAP_OCCLUSION
    vec3 colorShading = vec3(shadowFactor);
    #elif REFLECTION_MODEL == 3 // AMBIENT_OCCLUSION_FACTOR
    vec3 colorShading = vec3(occlusionFactor);
    #elif REFLECTION_MODEL == 4 // NO_LIGHTING
    vec3 colorShading = colorAttribute.rgb;
    vec2 centerOffset = 2.0 * fragmentTextureCoords - vec2(1.0, 1.0);
    float invCenterDistance = 1.0 - clamp(length(centerOffset), 0.0, 1.0);
    opacity *= sqrt(invCenterDistance);

    #endif
    vec4 color = vec4(colorShading, opacity);

    //color.rgb = fragmentNormal;

    if (!transparencyMapping) {
        color.a = colorGlobal.a;
    }

    // Radial weight function
//    vec2 centerOffset = 2.0 * fragmentTextureCoords - vec2(1.0, 1.0);
//    float invCenterDistance = 1.0 - clamp(length(centerOffset), 0.0, 1.0);
//    color.a *= sqrt(invCenterDistance);

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
