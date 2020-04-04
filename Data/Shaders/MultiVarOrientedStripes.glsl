-- Vertex

#version 430 core

#include "MultiVarGlobalVertexInput.glsl"
#include "MultiVarGlobalVariables.glsl"

out VertexData
{
    vec3 vPosition;// Position in world space
    vec3 vNormal;// Orientation normal along line in world space
    vec3 vTangent;// Tangent of line in world space
//    int variableID; // variable index
    uint vLineID;// number of line
    uint vElementID;// number of line element (original line vertex index)
};
//} GeomOut;



void main()
{
//    uint varID = gl_InstanceID % 6;
    const uint lineID = int(variableDesc.y);
    const uint elementID = uint(variableDesc.x);

    vPosition = vertexPosition;
    vNormal = normalize(vertexLineNormal);
    vTangent = normalize(vertexLineTangent);

//    Output.variableID = varID;
    vLineID = lineID;
    vElementID = elementID;
}

-- Geometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 64) out;

#include "MultiVarGlobalVariables.glsl"

#define NUM_SEGMENTS 14

void createTubeSegments(inout vec3 positions[NUM_SEGMENTS],
                        inout vec3 normals[NUM_SEGMENTS],
                        in vec3 center,
                        in vec3 normal,
                        in vec3 tangent,
                        in float curRadius)
{
    const float theta = 2.0 * 3.1415926 / float(NUM_SEGMENTS);
    const float tangetialFactor = tan(theta); // opposite / adjacent
    const float radialFactor = cos(theta); // adjacent / hypotenuse

    vec3 binormal = cross(tangent, normal);
    mat3 matFrame = mat3(normal, binormal, tangent);

    vec2 position = vec2(curRadius, 0.0);
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        vec3 pointOnCricle = matFrame * vec3(position, 0.0);
        positions[i] = pointOnCricle + center;
        normals[i] = normalize(pointOnCricle);

        // Add the tangent vector and correct the position using the radial factor.
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }
}

void computeAABBFromNDC(inout vec3 positions[NUM_SEGMENTS],
                        inout mat2 invMatNDC, inout vec2 bboxMin,
                        inout vec2 bboxMax, inout vec2 tangentNDC,
                        inout vec2 normalNDC, inout vec2 refPointNDC)
{
    for (int i = 0; i < NUM_SEGMENTS; i++)
    {
        vec4 pointNDC = mvpMatrix * vec4(positions[i], 1);
        pointNDC.xyz /= pointNDC.w;
        pointNDC.xy = invMatNDC * pointNDC.xy;

        bboxMin.x = min(pointNDC.x, bboxMin.x);
        bboxMax.x = max(pointNDC.x, bboxMax.x);
        bboxMin.y = min(pointNDC.y, bboxMin.y);
        bboxMax.y = max(pointNDC.y, bboxMax.y);
    }

    refPointNDC = vec2(bboxMin.x, bboxMax.y);
    normalNDC = vec2(0, bboxMin.y - bboxMax.y);
    tangentNDC = vec2(bboxMax.x - bboxMin.x, 0);
}

void computeTexCoords(inout vec2 texCoords[NUM_SEGMENTS], inout vec3 positions[NUM_SEGMENTS],
                        inout mat2 invMatNDC, inout vec2 tangentNDC, inout vec2 normalNDC,
                        inout vec2 refPointNDC)
{
    for (int i = 0; i < NUM_SEGMENTS; i++)
    {
        vec4 pointNDC = mvpMatrix * vec4(positions[i], 1);
        pointNDC.xyz /= pointNDC.w;
        pointNDC.xy = invMatNDC * pointNDC.xy;

        vec2 offsetNDC = pointNDC.xy - refPointNDC;
        texCoords[i] = vec2(dot(offsetNDC, normalize(tangentNDC)) / length(tangentNDC),
                            dot(offsetNDC, normalize(normalNDC)) / length(normalNDC));
    }
}

in VertexData
{
    vec3 vPosition;// Position in world space
    vec3 vNormal;// Orientation normal along line in world space
    vec3 vTangent;// Tangent of line in world space
//    int variableID; // variable index
    uint vLineID;// number of line
    uint vElementID;// number of line element (original line vertex index)
} vertexOutput[];

//out DataOut
//{
//    vec3 worldPos;
//    vec3 normal;
//    vec3 tangent;
//    flat vec2 texCoord;
//} fragOut;

out vec3 fragWorldPos;
out vec3 fragNormal;
out vec3 fragTangent;
out vec3 screenSpacePosition;
out vec2 fragTexCoord;

//out DataOutFlat
//{
//
//} fragOutFlat;

void main()
{
    vec3 currentPoint = (mMatrix * vec4(vertexOutput[0].vPosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(vertexOutput[1].vPosition, 1.0)).xyz;

    vec3 circlePointsCurrent[NUM_SEGMENTS];
    vec3 circlePointsNext[NUM_SEGMENTS];

    vec3 vertexNormalsCurrent[NUM_SEGMENTS];
    vec3 vertexNormalsNext[NUM_SEGMENTS];

    vec2 vertexTexCoordsCurrent[NUM_SEGMENTS];
    vec2 vertexTexCoordsNext[NUM_SEGMENTS];

    vec3 normalCurrent = vertexOutput[0].vNormal;
    vec3 tangentCurrent = vertexOutput[0].vTangent;
    vec3 binormalCurrent = cross(tangentCurrent, normalCurrent);
    vec3 normalNext = vertexOutput[1].vNormal;
    vec3 tangentNext = vertexOutput[1].vTangent;
    vec3 binormalNext = cross(tangentNext, normalNext);

    vec3 tangent = normalize(nextPoint - currentPoint);

    // 1) Create tube circle vertices for current and next point
    createTubeSegments(circlePointsCurrent, vertexNormalsCurrent, currentPoint,
                        normalCurrent, tangentCurrent, radius);
    createTubeSegments(circlePointsNext, vertexNormalsNext, nextPoint,
                        normalNext, tangentNext, radius);

    // 2) Create NDC AABB for stripe -> tube mapping
    // 2.1) Define orientation of local NDC frame-of-reference
    vec4 currentPointNDC = mvpMatrix * vec4(currentPoint, 1);
    currentPointNDC.xyz /= currentPointNDC.w;
    vec4 nextPointNDC = mvpMatrix * vec4(nextPoint, 1);
    nextPointNDC.xyz /= nextPointNDC.w;

    vec2 ndcOrientation = normalize(nextPointNDC.xy - currentPointNDC.xy);
    vec2 ndcOrientNormal = vec2(-ndcOrientation.y, ndcOrientation.x);
    // 2.2) Matrix to rotate every NDC point back to the local frame-of-reference
    // (such that tangent is aligned with the x-axis)
    mat2 invMatNDC = inverse(mat2(ndcOrientation, ndcOrientNormal));

    // 2.3) Compute AABB in NDC space
    vec2 bboxMin = vec2(10,10);
    vec2 bboxMax = vec2(-10,-10);
    vec2 tangentNDC = vec2(0);
    vec2 normalNDC = vec2(0);
    vec2 refPointNDC = vec2(0);

    computeAABBFromNDC(circlePointsCurrent, invMatNDC, bboxMin, bboxMax, tangentNDC, normalNDC, refPointNDC);
    computeAABBFromNDC(circlePointsNext, invMatNDC, bboxMin, bboxMax, tangentNDC, normalNDC, refPointNDC);

    // 3) Compute texture coordinates
    computeTexCoords(vertexTexCoordsCurrent, circlePointsCurrent,
                     invMatNDC, tangentNDC, normalNDC, refPointNDC);
    computeTexCoords(vertexTexCoordsNext, circlePointsNext,
    invMatNDC, tangentNDC, normalNDC, refPointNDC);

//    fragmentAttributeIndex = gl_PrimitiveIDIn;

    // 4) Emit the tube triangle vertices and attributes to the fragment shader
//    gl_Position = mvpMatrix * vec4(currentPoint, 1.0);
//    normal = vertexNormalsCurrent[0];
//    tangent = tangent;
//    worldPos = (mMatrix * vec4(currentPoint, 1.0)).xyz;
//    texCoord = vertexTexCoordsCurrent[0];
//    EmitVertex();
//
//    gl_Position = mvpMatrix * vec4(nextPoint, 1.0);
//    normal = vertexNormalsCurrent[0];
//    tangent = tangent;
//    worldPos = (mMatrix * vec4(nextPoint, 1.0)).xyz;
//    texCoord = vertexTexCoordsCurrent[0];
//    EmitVertex();
//
////    gl_Position = vec4(1, -1, 0, 1);//mvpMatrix * vec4(nextPoint + 10 * normalCurrent, 1.0);
////    normal = vertexNormalsCurrent[0];
////    tangent = tangent;
////    worldPos = (mMatrix * vec4(nextPoint + 10 * normalCurrent, 1.0)).xyz;
////    texCoord = vertexTexCoordsCurrent[0];
////    EmitVertex();

//    EndPrimitive();

    for (int i = 0; i < NUM_SEGMENTS; i++)
    {
        int iN = (i + 1) % NUM_SEGMENTS;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        fragNormal = vertexNormalsCurrent[i];
        fragTangent = tangent;
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsCurrent[i];
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        fragNormal = vertexNormalsCurrent[iN];
        fragTangent = tangent;
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsCurrent[iN];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        fragNormal = vertexNormalsNext[i];
        fragTangent = tangent;
        fragWorldPos = (mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsNext[i];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        fragNormal = vertexNormalsNext[iN];
        fragTangent = tangent;
        fragWorldPos = (mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsNext[iN];
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

//in DataIn
//{
//    vec3 worldPos;
//    vec3 normal;
//    vec3 tangent;
//    flat vec2 texCoord;
//} fragInput;

in vec3 fragWorldPos;
in vec3 fragNormal;
in vec3 fragTangent;
in vec2 fragTexCoord;

//in DataInFlat
//{
//
//} fragInputFlat;



#ifdef DIRECT_BLIT_GATHER
out vec4 fragColor;
#endif

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
uniform vec3 cameraPosition; // world space

#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"
#include "MultiVarGlobalVariables.glsl"

void main()
{
//    vec4 surfaceColor = vec4(floor(fragTexCoord.y * 4.0) / 10.0, 0, 0, 1);
    vec4 surfaceColor = vec4(fragTexCoord.xy, 0, 1);

    ////////////
    // Shading
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;

    const vec3 lightColor = vec3(1,1,1);
    const vec3 ambientColor = surfaceColor.rgb;
    const vec3 diffuseColor = surfaceColor.rgb;

    const float kA = 0.1 * occlusionFactor * shadowFactor;
    const vec3 Ia = kA * ambientColor;
    const float kD = 0.85;
    const float kS = 0.05;
    const float s = 10;

    const vec3 n = normalize(fragNormal);
    const vec3 v = normalize(cameraPosition - fragWorldPos);
    //    const vec3 l = normalize(lightDirection);
    const vec3 l = normalize(v);
    const vec3 h = normalize(v + l);
    vec3 t = normalize(fragTangent);
//    vec3 t = normalize(cross(vec3(0, 0, 1), n));


    vec3 Id = kD * clamp(abs(dot(n, l)), 0.0, 1.0) * diffuseColor;
    vec3 Is = kS * pow(clamp(abs(dot(n, h)), 0.0, 1.0), s) * lightColor;
    vec3 colorShading = Ia + Id + Is;

//    float haloParameter = 0.5;
//    float angle1 = abs( dot( v, n)) * 0.8;
//    float angle2 = abs( dot( v, normalize(t))) * 0.2;
//    float halo = min(1.0,mix(1.0f,angle1 + angle2,haloParameter));//((angle1)+(angle2)), haloParameter);

    vec3 hV = normalize(cross(t, v));
    vec3 vNew = normalize(cross(hV, t));

    float angle = pow(abs((dot(vNew, n))), 0.5); // 1.8 + 1.5
    float angleN = pow(abs((dot(v, n))), 0.5);
//    float EPSILON = 0.8f;
//    float coverage = 1.0 - smoothstep(1.0 - 2.0*EPSILON, 1.0, angle);

    float haloNew = min(1.0, mix(1.0f, 0.5 * angle + 0.5 * angleN, 0.9)) * 0.9 + 0.1;
    colorShading *= (haloNew) * (haloNew);

    ////////////

    vec4 color = vec4(colorShading.rgb, surfaceColor.a);
//    vec4 color = vec4(floor(fragTexCoord.y * 4.0) / 10.0, 0, 0, 1);
//    vec4 color = vec4(1, 0, 0, 1);

//    if (color.a < 1.0/255.0) { discard; }

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