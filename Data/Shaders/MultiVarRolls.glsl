-- Vertex

#version 430 core

#include "MultiVarGlobalVertexInput.glsl"
#include "MultiVarGlobalVariables.glsl"

out VertexData
{
    vec3 vPosition;// Position in world space
    vec3 vNormal;// Orientation normal along line in world space
    vec3 vTangent;// Tangent of line in world space
    int vVariableID; // variable index (for alternating variable indices per line curve)
    int vLineID;// number of line
    int vElementID;// number of line element (original line vertex index)
    int vElementNextID; // number of next line element (original next line vertex index)
    float vElementInterpolant; // curve parameter t along curve between element and next element
    int vInstanceID; // current instance ID for multi-instancing rendering
    vec4 lineVariable;
};
//} GeomOut; // does not work

void main()
{
    int varID = int(multiVariable.w);
    const int lineID = int(variableDesc.y);
    const int elementID = int(variableDesc.x);

    vPosition = vertexPosition;
    vNormal = normalize(vertexLineNormal);
    vTangent = normalize(vertexLineTangent);

    vVariableID = varID;
    vLineID = lineID;
    vElementID = elementID;
    vInstanceID = gl_InstanceID;

    vElementNextID = int(variableDesc.z);
    vElementInterpolant = variableDesc.w;

    lineVariable = multiVariable;
}

-- Geometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

#include "MultiVarGlobalVariables.glsl"

//#define NUM_SEGMENTS 10
#include "MultiVarGeometryUtils.glsl"

// Input from vertex buffer
in VertexData
{
    vec3 vPosition;// Position in world space
    vec3 vNormal;// Orientation normal along line in world space
    vec3 vTangent;// Tangent of line in world space
    int vVariableID; // variable index (for alternating variable indices per line curve)
    int vLineID;// number of line
    int vElementID;// number of line element (original line vertex index)
    int vElementNextID; // number of next line element (original next line vertex index)
    float vElementInterpolant; // curve parameter t along curve between element and next element
    int vInstanceID; // current instance ID for multi-instancing rendering
    vec4 lineVariable;
} vertexOutput[];

in int gl_PrimitiveIDIn;

// Output to fragments
out vec3 fragWorldPos;
out vec3 fragNormal;
out vec3 fragTangent;
out vec3 screenSpacePosition; // screen space position for depth in view space (to sort for buckets...)
out vec2 fragTexCoord;
// "Rolls"-specfic outputs
flat out int fragVariableID;
flat out float fragVariableValue;


// "Rolls" specific uniforms
#if !defined(NUM_CIRCLE_POINTS_PER_INSTANCE)
    #define NUM_CIRCLE_POINTS_PER_INSTANCE 3
#endif
#if !defined(NUM_INSTANCES)
    #define NUM_INSTANCES 12
#endif


void createPartialTubeSegments(inout vec3 positions[NUM_CIRCLE_POINTS_PER_INSTANCE],
                                inout vec3 normals[NUM_CIRCLE_POINTS_PER_INSTANCE],
                                in vec3 center, in vec3 normal,
                                in vec3 tangent, in float curRadius, in int varID)
{
    float theta = 2.0 * 3.1415926 / float(NUM_INSTANCES);
    float tangetialFactor = tan(theta); // opposite / adjacent
    float radialFactor = cos(theta); // adjacent / hypotenuse

    // Set position to the offset for ribbons (and rolls for varying radius)
    vec2 position = vec2(curRadius, 0.0);

    for (int i = 0; i < varID; i++) {
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }

    vec3 binormal = cross(tangent, normal);
    mat3 matFrame = mat3(normal, binormal, tangent);

    // Subdivide theta in number of segments per instance
    theta /= float(NUM_CIRCLE_POINTS_PER_INSTANCE - 1);
    tangetialFactor = tan(theta); // opposite / adjacent
    radialFactor = cos(theta); // adjacent / hypotenuse

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE; i++) {
        vec3 pointOnCricle = matFrame * vec3(position, 0.0);
        positions[i] = pointOnCricle + center;
        normals[i] = normalize(pointOnCricle);

        // Add the tangent vector and correct the position using the radial factor.
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }
}

void drawTangentLid(in vec3 p0, in vec3 p1, in vec3 p2)
{
    gl_Position = mvpMatrix * vec4(p0, 1.0);
    fragWorldPos = (mMatrix * vec4(p0, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(p0, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(p1, 1.0);
    fragWorldPos = (mMatrix * vec4(p1, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(p1, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(p2, 1.0);
    fragWorldPos = (mMatrix * vec4(p2, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(p2, 1.0)).xyz;
    EmitVertex();

    EndPrimitive();
}

void drawPartialCircleLids(inout vec3 circlePointsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE],
                            inout vec3 vertexNormalsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE],
                            inout vec3 circlePointsNext[NUM_CIRCLE_POINTS_PER_INSTANCE],
                            inout vec3 vertexNormalsNext[NUM_CIRCLE_POINTS_PER_INSTANCE],
                            in vec3 centerCurrent, in vec3 centerNext, in vec3 tangent)
{
    vec3 pointCurFirst = circlePointsCurrent[0];
    vec3 pointNextFirst = circlePointsNext[0];

    vec3 pointCurLast = circlePointsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE - 1];
    vec3 pointNextLast = circlePointsNext[NUM_CIRCLE_POINTS_PER_INSTANCE - 1];

    // 1) First half
    gl_Position = mvpMatrix * vec4(pointCurFirst, 1.0);
    fragNormal = normalize(cross(vertexNormalsCurrent[0], normalize(tangent)));
    fragTangent = normalize(cross(tangent, fragNormal));
    fragWorldPos = (mMatrix * vec4(pointCurFirst, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(pointCurFirst, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(pointNextFirst, 1.0);
    fragWorldPos = (mMatrix * vec4(pointNextFirst, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(pointNextFirst, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerCurrent, 1.0);
    fragWorldPos = (mMatrix * vec4(centerCurrent, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(centerCurrent, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerNext, 1.0);
    fragWorldPos = (mMatrix * vec4(centerNext, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(centerNext, 1.0)).xyz;
    EmitVertex();

    // 2) Second half
    gl_Position = mvpMatrix * vec4(pointCurLast, 1.0);
    fragNormal = normalize(cross(normalize(tangent), vertexNormalsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE - 1]));
    fragTangent = normalize(cross(tangent, fragNormal)); //! TODO compute actual tangent
    fragWorldPos = (mMatrix * vec4(pointCurLast, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(pointCurLast, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerCurrent, 1.0);
    fragWorldPos = (mMatrix * vec4(centerCurrent, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(centerCurrent, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(pointNextLast, 1.0);
    fragWorldPos = (mMatrix * vec4(pointNextLast, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(pointNextLast, 1.0)).xyz;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(centerNext, 1.0);
    fragWorldPos = (mMatrix * vec4(centerNext, 1.0)).xyz;
    screenSpacePosition = (vMatrix * mMatrix * vec4(centerNext, 1.0)).xyz;
    EmitVertex();

    EndPrimitive();
}


void main()
{
    vec3 currentPoint = (mMatrix * vec4(vertexOutput[0].vPosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(vertexOutput[1].vPosition, 1.0)).xyz;

    vec3 circlePointsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE];
    vec3 circlePointsNext[NUM_CIRCLE_POINTS_PER_INSTANCE];

    vec3 vertexNormalsCurrent[NUM_CIRCLE_POINTS_PER_INSTANCE];
    vec3 vertexNormalsNext[NUM_CIRCLE_POINTS_PER_INSTANCE];

    vec3 normalCurrent = vertexOutput[0].vNormal;
    vec3 tangentCurrent = vertexOutput[0].vTangent;
    vec3 binormalCurrent = cross(tangentCurrent, normalCurrent);
    vec3 normalNext = vertexOutput[1].vNormal;
    vec3 tangentNext = vertexOutput[1].vTangent;
    vec3 binormalNext = cross(tangentNext, normalNext);

    vec3 tangent = normalize(nextPoint - currentPoint);

    // 1) Sample variables at each tube roll
    const int instanceID = vertexOutput[0].vInstanceID;
    const int varID = vertexOutput[0].vVariableID;
    const int elementID = vertexOutput[0].vElementID;
    const int lineID = vertexOutput[0].vLineID;

    float variableValue = 0;
    vec2 variableMinMax = vec2(0);

//    vec4 varInfo = vertexOutput[0].lineVariable;

    if (varID >= 0)
    {
        sampleVariableFromLineSSBO(lineID, varID, elementID, variableValue, variableMinMax);
        // Normalize value
        variableValue = (variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
//        variableValue = (varInfo.x - varInfo.y) / (varInfo.z - varInfo.y);
    }

    // 2.1) Radius mapping
    float curRadius = radius * instanceID * 0.1;

    if (varID < 0)
    {
        curRadius *= 0.55;
    }

    // 2) Create tube circle vertices for current and next point
    createPartialTubeSegments(circlePointsCurrent, vertexNormalsCurrent, currentPoint,
    normalCurrent, tangentCurrent, curRadius, instanceID);
    createPartialTubeSegments(circlePointsNext, vertexNormalsNext, nextPoint,
    normalNext, tangentNext, curRadius, instanceID);


    // 3) Draw Tube Front Sides
    fragVariableValue = variableValue;
    fragVariableID = varID;

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++)
    {
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        fragNormal = vertexNormalsCurrent[i];
        fragTangent = tangentCurrent;
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        fragNormal = vertexNormalsCurrent[iN];
        fragTangent = tangentCurrent;
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        fragNormal = vertexNormalsNext[i];
        fragTangent = tangentNext;
        fragWorldPos = (mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        fragNormal = vertexNormalsNext[iN];
        fragTangent = tangentNext;
        fragWorldPos = (mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        EmitVertex();

        EndPrimitive();
    }

    // Render lids

    // 3) Ŕender lids
    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++) {
        fragNormal = normalize(-tangent);
        //! TODO compute tangent
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];

        drawTangentLid(segmentPointCurrent0, currentPoint, segmentPointCurrent1);
    }

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++) {
        fragNormal = normalize(tangent);
        //! TODO compute tangent
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsNext[iN];

        drawTangentLid(segmentPointCurrent0, segmentPointCurrent1, nextPoint);
    }

    // 3) Render partial circle lids
    drawPartialCircleLids(circlePointsCurrent, vertexNormalsCurrent,
                            circlePointsNext, vertexNormalsNext,
                            currentPoint, nextPoint, tangent);
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition; // Required for transparency rendering techniques

in vec3 fragWorldPos;
in vec3 fragNormal;
in vec3 fragTangent;
in vec2 fragTexCoord;
// "Rolls"-specfic inputs
flat in int fragVariableID;
flat in float fragVariableValue;

#if !defined(DIRECT_BLIT_GATHER) || defined(SHADOW_MAPPING_MOMENTS_GENERATE)
    #include OIT_GATHER_HEADER
#endif


#ifdef DIRECT_BLIT_GATHER
    out vec4 fragColor;
#endif

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
uniform vec3 cameraPosition; // world space

#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"
#include "MultiVarGlobalVariables.glsl"
#include "MultiVarShadingUtils.glsl"


void main()
{

    // 1) Determine variable color
    vec4 surfaceColor = determineColor(fragVariableID, fragVariableValue);
    // 1.1) Draw black separators between single stripes.
//    if (separatorWidth > 0)
//    {
//        drawSeparatorBetweenStripes(surfaceColor, varFraction, separatorWidth);
//    }

    ////////////
    // 2) Phong Lighting
    float occlusionFactor = 1.0f;
    float shadowFactor = 1.0f;

    vec4 color = computePhongLighting(surfaceColor, occlusionFactor, shadowFactor,
    fragWorldPos, fragNormal, fragTangent);

    if (color.a < 1.0/255.0) { discard; }

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