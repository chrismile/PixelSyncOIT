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

#if !defined(NUM_INSTANCES)
    #define NUM_INSTANCES 10
#endif

#if !defined(NUM_CIRCLE_POINTS_PER_INSTANCE)
    #define NUM_CIRCLE_POINTS_PER_INSTANCE 3
#endif

#include "MultiVarGlobalVariables.glsl"

layout(lines, invocations = NUM_INSTANCES) in;
layout(triangle_strip, max_vertices = 128) out;



//#define NUM_SEGMENTS 10


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
out float fragBorderInterpolant;
//flat out int fragVariableNextID;
flat out float fragVariableNextValue;
out float fragElementInterpolant;

#include "MultiVarGeometryUtils.glsl"

// "Rolls" specific uniforms
uniform bool mapTubeDiameter;


float computeRadius(in int lineID, in int varID, in int elementID, in int elementNextID,
                    in float minRadius, in float maxRadius, in float interpolant)
{
    float curRadius = minRadius;
    float nextRadius = minRadius;

    vec2 lineVarMinMax = vec2(0);
    float variableValueOrig = 0;
    sampleVariableFromLineSSBO(lineID, varID, elementID , variableValueOrig, lineVarMinMax);
    sampleVariableDistributionFromLineSSBO(lineID, varID, lineVarMinMax);
    if (lineVarMinMax.x != lineVarMinMax.y)
    {
        float interpolant = (variableValueOrig - lineVarMinMax.x) / (lineVarMinMax.y - lineVarMinMax.x);
        //                interpolant = max(0.0, min(1.0, interpolant));
        curRadius = mix(minRadius, maxRadius, interpolant);
    }

    sampleVariableFromLineSSBO(lineID, varID, elementNextID , variableValueOrig, lineVarMinMax);
    sampleVariableDistributionFromLineSSBO(lineID, varID, lineVarMinMax);
//            float variableNextValue = (variableNextValueOrig - variableNextMinMax.x) / (variableNextMinMax.y - variableNextMinMax.x);
    if (lineVarMinMax.x != lineVarMinMax.y)
    {
        float interpolant = (variableValueOrig - lineVarMinMax.x) / (lineVarMinMax.y - lineVarMinMax.x);
        //                interpolant = max(0.0, min(1.0, interpolant));
        nextRadius = mix(minRadius, maxRadius, interpolant);
    }

    return  mix(curRadius, nextRadius, interpolant);
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
    const int instanceID = gl_InvocationID;//vertexOutput[0].vInstanceID;
    const int varID = instanceID % numVariables; // for stripes
    const int elementID = vertexOutput[0].vElementID;
    const int lineID = vertexOutput[0].vLineID;

    float variableValueOrig = 0;
    float variableValue = 0;
    vec2 variableMinMax = vec2(0);

    //    vec4 varInfo = vertexOutput[0].lineVariable;

    if (varID >= 0)
    {
        sampleVariableFromLineSSBO(lineID, varID, elementID, variableValueOrig, variableMinMax);
        // Normalize value
        variableValue = (variableValueOrig - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
        //        variableValue = (varInfo.x - varInfo.y) / (varInfo.z - varInfo.y);
    }

    // 2.1) Radius mapping
    float minRadius = 0.5 * radius;
    float curRadius = radius;
    float nextRadius = radius;

    if (mapTubeDiameter)
    {
        curRadius = minRadius;
        nextRadius = minRadius;
        if (varID >= 0)
        {
            curRadius = computeRadius(lineID, varID, elementID, vertexOutput[0].vElementNextID,
                                      minRadius, radius, vertexOutput[0].vElementInterpolant);
//            nextRadius = curRadius;
            nextRadius = computeRadius(lineID, varID, vertexOutput[1].vElementID, vertexOutput[1].vElementNextID,
                                      minRadius, radius, vertexOutput[1].vElementInterpolant);

//            vec2 lineVarMinMax = vec2(0);
//            sampleVariableDistributionFromLineSSBO(lineID, varID, lineVarMinMax);
//            if (lineVarMinMax.x != lineVarMinMax.y)
//            {
//                float interpolant = (variableValueOrig - lineVarMinMax.x) / (lineVarMinMax.y - lineVarMinMax.x);
//                //                interpolant = max(0.0, min(1.0, interpolant));
//                curRadius = mix(minRadius, radius, interpolant);
//            }
//
//            int elementNextID = vertexOutput[0].vElementNextID;
//            float variableNextValueOrig = 0;
//            vec2 variableNextMinMax = vec2(0);
//
//            sampleVariableFromLineSSBO(lineID, varID, elementNextID, variableNextValueOrig, variableNextMinMax);
////            float variableNextValue = (variableNextValueOrig - variableNextMinMax.x) / (variableNextMinMax.y - variableNextMinMax.x);
//            if (lineVarMinMax.x != lineVarMinMax.y)
//            {
//                float interpolant = (variableNextValueOrig - lineVarMinMax.x) / (lineVarMinMax.y - lineVarMinMax.x);
//                //                interpolant = max(0.0, min(1.0, interpolant));
//                nextRadius = mix(minRadius, radius, interpolant);
//            }
//
//            float radiusInterpolantCurrent = vertexOutput[0].vElementInterpolant;
//
//            if (vertexOutput[1].vElementInterpolant < vertexOutput[0].vElementInterpolant)
//            {
//                radiusInterpolantCurrent = 1.0;
//            }
//
//            curRadius = mix(curRadius, nextRadius, radiusInterpolantCurrent);
        }
    }
    else
    {
        if (varID < 0)
        {
            curRadius = minRadius;
            nextRadius = minRadius;
        }
    }

    // 2) Create tube circle vertices for current and next point
    createPartialTubeSegments(circlePointsCurrent, vertexNormalsCurrent, currentPoint,
                                normalCurrent, tangentCurrent, curRadius, instanceID);
    createPartialTubeSegments(circlePointsNext, vertexNormalsNext, nextPoint,
                                normalNext, tangentNext, nextRadius, instanceID);


    // 3) Draw Tube Front Sides
    fragVariableValue = variableValue;
    fragVariableID = varID;

    float interpIncrement = 1.0 / (NUM_CIRCLE_POINTS_PER_INSTANCE - 1);
    float curInterpolant = 0.0f;

    for (int i = 0; i < NUM_CIRCLE_POINTS_PER_INSTANCE - 1; i++)
    {
        int iN = (i + 1) % NUM_CIRCLE_POINTS_PER_INSTANCE;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        ////////////////////////
        // For linear interpolation: define next element and interpolant on curve
        int elementNextID = vertexOutput[0].vElementNextID;

//        float variableNextValueOrig = 0;
        float variableNextValue = 0;

        if (elementNextID >= 0)
        {
            sampleVariableFromLineSSBO(lineID, varID, elementNextID, variableNextValue, variableMinMax);
            // Normalize value
            fragVariableNextValue = (variableNextValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
        }
//        fragVariableNextID = elementNextID;
        fragElementInterpolant = vertexOutput[0].vElementInterpolant;
        ////////////////////////

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        fragTangent = normalize(segmentPointNext0 - segmentPointCurrent0);//tangentCurrent;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsCurrent[i], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal));//vertexNormalsCurrent[i];
        } else {
            fragNormal = vertexNormalsCurrent[i];
        }
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        fragBorderInterpolant = curInterpolant;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        fragTangent = normalize(segmentPointNext1 - segmentPointCurrent1);//tangentCurrent;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsCurrent[iN], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal) + vertexNormalsCurrent[iN]);//vertexNormalsCurrent[iN];
        } else {
            fragNormal = vertexNormalsCurrent[iN];
        }
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        fragBorderInterpolant = curInterpolant + interpIncrement;
        EmitVertex();

        ////////////////////////
        // For linear interpolation: define next element and interpolant on curve
        elementNextID = vertexOutput[0].vElementNextID;

        if (vertexOutput[1].vElementInterpolant < vertexOutput[0].vElementInterpolant)
        {
            fragElementInterpolant = 1.0f;
//            fragElementNextID = int(vertexOutput[0].vElementNextID);
        }
        else
        {
//            fragElementNextID = int(vertexOutput[1].vElementNextID);
            fragElementInterpolant = vertexOutput[1].vElementInterpolant;
        }

//        fragVariableNextID = elementNextID;
        if (elementNextID >= 0)
        {
            sampleVariableFromLineSSBO(lineID, varID, elementNextID, variableNextValue, variableMinMax);
            // Normalize value
            fragVariableNextValue = (variableNextValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
        }
        ////////////////////////

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        fragTangent = normalize(segmentPointNext0 - segmentPointCurrent0);//tangentNext;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsNext[i], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal) + vertexNormalsNext[i]);//vertexNormalsNext[i];
        } else {
            fragNormal = vertexNormalsNext[i];
        }
            fragWorldPos = (mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        fragBorderInterpolant = curInterpolant;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        fragTangent = normalize(segmentPointNext1 - segmentPointCurrent1);//tangentNext;
        if (mapTubeDiameter) {
            vec3 intermediateBinormal = normalize(cross(vertexNormalsNext[iN], fragTangent));
            fragNormal = normalize(cross(fragTangent, intermediateBinormal) + vertexNormalsNext[iN]);//vertexNormalsNext[iN];
        } else {
            fragNormal = vertexNormalsNext[iN];
        }
        fragWorldPos = (mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        fragBorderInterpolant = curInterpolant + interpIncrement;
        EmitVertex();

        EndPrimitive();

        curInterpolant += interpIncrement;
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
in float fragBorderInterpolant;
//flat in int fragVariableNextID;
flat in float fragVariableNextValue;
in float fragElementInterpolant;

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

// "Rolls"-specific uniforms
uniform float separatorWidth;

void main()
{

    // 1) Determine variable color
//    vec4 surfaceColor = determineColor(fragVariableID, fragVariableValue);
    vec4 surfaceColor = determineColorLinearInterpolate(fragVariableID, fragVariableValue,
                                                        fragVariableNextValue, fragElementInterpolant);
    // 1.1) Draw black separators between single stripes.
    float varFraction = fragBorderInterpolant;
    if (separatorWidth > 0)
    {
        drawSeparatorBetweenStripes(surfaceColor, varFraction, separatorWidth);
    }

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