-- Vertex

#version 430 core

#include "VertexAttributeNames.glsl"

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexLineNormal;
layout(location = 2) in vec3 vertexLineTangent;
layout(location = 3) in float vertexAttribute0;
layout(location = 4) in float vertexAttribute1;
layout(location = 5) in float vertexAttribute2;
layout(location = 6) in float vertexAttribute3;
layout(location = 7) in float vertexAttribute4;
layout(location = 8) in float vertexAttribute5;

const uint NUM_MULTI_ATTRIBUTES = 5;

out VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttributes[NUM_MULTI_ATTRIBUTES];
};

void main()
{
    linePosition = vertexPosition;
    lineNormal = vertexLineNormal;
    lineTangent = vertexLineTangent;
    lineAttributes = float[NUM_MULTI_ATTRIBUTES](vertexAttribute0, vertexAttribute1, vertexAttribute2,
                                                 vertexAttribute3, vertexAttribute4);
}

-- InstanceVertex

#version 430 core

//#include "VertexAttributeNames.glsl"

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexLineNormal;
layout(location = 2) in vec3 vertexLineTangent;
layout(location = 3) in vec4 multiVariable;
layout(location = 4) in vec2 variableDesc;

struct VarData
{
    float value;
};

struct LineDescData
{
    float startIndex;
};

struct VarDescData
{
//    float startIndex;
//    vec2 minMax;
//    float dummy;
    vec4 info;
};

struct LineVarDescData
{
    vec4 minMax;
};

layout (std430, binding = 2) buffer VariableArray
{
    float varArray[];
};

layout (std430, binding = 3) buffer LineDescArray
{
    float lineDescs[];
};

layout (std430, binding = 4) buffer VarDescArray
{
    VarDescData varDescs[];
};

layout (std430, binding = 5) buffer LineVarDescArray
{
    LineVarDescData lineVarDescs[];
};

out VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    int instanceID;
    vec4 lineVariable;
    vec2 lineMinMax;
    vec2 lineVariableDesc;
    int vertexID;
    uint lineSegID;
    uint varElemOffsetID;
};

//void main()
//{
//    linePosition = vertexPosition;
//    lineNormal = vertexLineNormal;
//    lineTangent = vertexLineTangent;
//    lineVariable = multiVariable;
////    lineVariable = vec4(multiVariable.x, multiVariable.y, multiVariable.z, multiVariableDesc.y);
//    lineVariableDesc = multiVariableDesc;
//    instanceID = gl_InstanceID;
//}

//#define DRAW_ROLLS 1
//#define CONSTANT_RADIUS 1

void main()
{
    uint varID = gl_InstanceID % 6;
    const uint lineID = int(variableDesc.y);
    const uint varElementID = uint(variableDesc.x);

    uint startIndex = uint(lineDescs[lineID]);
    VarDescData varDesc = varDescs[6 * lineID + varID];
    LineVarDescData lineVarDesc = lineVarDescs[6 * lineID + varID];
    const uint varOffset = uint(varDesc.info.r);
    const vec2 varMinMax = varDesc.info.gb;
    float value = varArray[startIndex + varOffset + varElementID];

    linePosition = vertexPosition;
    lineNormal = vertexLineNormal;
    lineTangent = vertexLineTangent;
    // For Rolls
#if defined (DRAW_ROLLS)
    lineVariable = multiVariable;
    lineMinMax = lineVarDescs[6 * lineID + uint(multiVariable.w)].minMax.rg;
#else
    // For STRIPES
//    if (multiVariable.w < 0) { lineVariable = vec4(0.0, 0.0, 0.0, -1.0); }
//    else { lineVariable = vec4(value, varMinMax.r, varMinMax.g, varID); }
    lineVariable = vec4(value, varMinMax.r, varMinMax.g, varID);
    lineMinMax = lineVarDesc.minMax.rg;
#endif
    lineVariableDesc = variableDesc;
    instanceID = gl_InstanceID;
    vertexID = gl_VertexID;
    lineSegID = lineID;
    varElemOffsetID = varElementID;
}

-- StarGeometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

uniform float radius = 0.001f;

const int NUM_MULTI_ATTRIBUTES = 5;
const float PI = 3.1415926;
const float MIN_RADIUS_PERCENTAGE = 0.2;
uniform vec2 minMaxCriterionValues[NUM_MULTI_ATTRIBUTES];

in VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttributes[NUM_MULTI_ATTRIBUTES];
} v_in[];

out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentPositionWorld;
out vec3 screenSpacePosition;
flat out float fragmentAttribute;
flat out int fragmentAttributeIndex;

float computeRadius(in float minRadius, in float maxRadius, in uint attributeIndex, in float attributeValue)
{
//    float minRadius = MIN_RADIUS_PERCENTAGE * maxRadius;
    float remainingRadius = maxRadius - minRadius;

    float t = max(0.0, min(1.0 ,(attributeValue - minMaxCriterionValues[attributeIndex].x)
    / (minMaxCriterionValues[attributeIndex].y - minMaxCriterionValues[attributeIndex].x)));

    return minRadius + t * remainingRadius;
}

void main()
{
    vec3 currentPoint = (mMatrix * vec4(v_in[0].linePosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(v_in[1].linePosition, 1.0)).xyz;

    vec3 normalCurrent = v_in[0].lineNormal;
    vec3 tangentCurrent = v_in[0].lineTangent;
    vec3 binormalCurrent = cross(tangentCurrent, normalCurrent);
    vec3 normalNext = v_in[1].lineNormal;
    vec3 tangentNext = v_in[1].lineTangent;
    vec3 binormalNext = cross(tangentNext, normalNext);

    vec3 tangent = normalize(nextPoint - currentPoint);

    mat3 tangentFrameMatrixCurrent = mat3(normalCurrent, binormalCurrent, tangentCurrent);
    mat3 tangentFrameMatrixNext = mat3(normalNext, binormalNext, tangentNext);

    const uint NUM_STAR_SEGMENTS = (NUM_MULTI_ATTRIBUTES) * 3;

    vec3 circlePointsCurrent[NUM_STAR_SEGMENTS + 1];
    vec3 circlePointsNext[NUM_STAR_SEGMENTS + 1];
    vec3 vertexNormalsCurrent[NUM_STAR_SEGMENTS + 1];
    vec3 vertexNormalsNext[NUM_STAR_SEGMENTS + 1];

    float innerRadius = radius * 0.2;

    const float thetaAll = 2.0 * PI / float(NUM_MULTI_ATTRIBUTES);
    const float thetaSmall = thetaAll * 0.25;
    const float thetaLarge = thetaAll - thetaSmall;

    const float tangetialFactorLarge = tan(thetaLarge / 2.0); // opposite / adjacent
    const float radialFactorLarge = cos(thetaLarge / 2.0); // adjacent / hypotenuse

    const float tangetialFactorSmall = tan(thetaSmall); // opposite / adjacent
    const float radialFactorSmall = cos(thetaSmall); // adjacent / hypotenuse

    const float tangetialFactorSmallHalf = tan(thetaSmall / 2.0); // opposite / adjacent
    const float radialFactorSmallHalf = cos(thetaSmall / 2.0); // adjacent / hypotenuse

    vec2 positionOuter = vec2(radius, 0.0);
    vec2 positionInner = vec2(innerRadius, 0.0);
    vec2 position = vec2(1.0, 0.0);

    float radiussesCurrent[NUM_MULTI_ATTRIBUTES];
    float radiussesNext[NUM_MULTI_ATTRIBUTES];

    for (int a = 0; a < NUM_MULTI_ATTRIBUTES; ++a)
    {
        radiussesCurrent[a] = computeRadius(innerRadius, radius, a, v_in[0].lineAttributes[a]);
        radiussesNext[a] = computeRadius(innerRadius, radius, a, v_in[1].lineAttributes[a]);
    }

    for (int i = 0; i < NUM_STAR_SEGMENTS + 1; i++)
    {
        int attrIndex = int(i / 3) % NUM_MULTI_ATTRIBUTES;

//        vec2 position = ((i + 1) % 3) == 0 ? positionInner : positionOuter;
//        if (i == 0)
//        {
//            position = positionOuter;
//        }

        float radiusCurrent = 1.0;
        float radiusNext = 1.0;

        if (i == 0 || !((i + 1) % 3 == 0)) {
            radiusCurrent = radiussesCurrent[attrIndex];
            radiusNext = radiussesNext[attrIndex];
        }
        else {
            radiusCurrent = innerRadius;
            radiusNext = innerRadius; }

        vec3 point2DCurrent = tangentFrameMatrixCurrent * vec3(position, 0.0) * radiusCurrent;
        vec3 point2DNext = tangentFrameMatrixNext * vec3(position, 0.0) * radiusNext;

//        if (i < NUM_STAR_SEGMENTS)
//        {
            circlePointsCurrent[i] = point2DCurrent.xyz + currentPoint;
            circlePointsNext[i] = point2DNext.xyz + nextPoint;
//        }

        // Compute Normal
        if (i == 0)
        {
//            vertexNormalsCurrent[i] = normalize(circlePointsCurrent[i] - currentPoint);
//            vertexNormalsNext[i] = normalize(circlePointsNext[i] - nextPoint);
        }
        else
        {
            vec3 tangentSideCurrent = normalize(circlePointsCurrent[i] - circlePointsCurrent[i - 1]);
            vec3 tangentSideNext = normalize(circlePointsNext[i] - circlePointsNext[i - 1]);

            vertexNormalsCurrent[i - 1] = normalize(cross(tangentSideCurrent, tangent));
            vertexNormalsNext[i - 1] = normalize(cross(tangentSideNext, tangent));

//            vertexNormalsCurrent[i] = normalize(cross(tangentSideCurrent, tangent));
//            vertexNormalsNext[i] = normalize(cross(tangentSideNext, tangent));

//            vertexNormalsCurrent[i] = normalize(currentPoint - circlePointsCurrent[i]);
//            vertexNormalsNext[i] = normalize(nextPoint - circlePointsNext[i]);
        }

        // Add the tangent vector and correct the position using the radial factor.
        vec2 circleTangentOuter = vec2(-position.y, position.x);
        vec2 circleTangentInner = vec2(-position.y, position.x);

        // thetaSmall / 2
          // thetaSmall
        if (i % 3 == 0)
        {
            position += tangetialFactorSmall * circleTangentOuter;
            position *= radialFactorSmall;
//            positionInner += tangetialFactorSmall * circleTangentInner;
//            positionInner *= radialFactorSmall;
        }
        // thetaLarge
        else
        {
            position += tangetialFactorLarge * circleTangentOuter;
            position *= radialFactorLarge;
//            positionInner += tangetialFactorLarge * circleTangentInner;
//            positionInner *= radialFactorLarge;
        }
    }

    // Emit the tube triangle vertices
    for (int i = 0; i < NUM_STAR_SEGMENTS; i++) {
        fragmentAttributeIndex = int((i + 1) / 3) % NUM_MULTI_ATTRIBUTES;

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[i], 1.0);
        fragmentNormal = vertexNormalsCurrent[i];
        fragmentTangent = tangent;
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        fragmentAttribute = v_in[0].lineAttributes[fragmentAttributeIndex];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[(i+1)%NUM_STAR_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsCurrent[i];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_STAR_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_STAR_SEGMENTS], 1.0)).xyz;
        fragmentTangent = tangent;
        fragmentAttribute = v_in[0].lineAttributes[fragmentAttributeIndex];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
        fragmentNormal = vertexNormalsNext[i];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        fragmentTangent = tangent;
        fragmentAttribute = v_in[1].lineAttributes[fragmentAttributeIndex];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[(i+1)%NUM_STAR_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsNext[i];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[(i+1)%NUM_STAR_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[(i+1)%NUM_STAR_SEGMENTS], 1.0)).xyz;
        fragmentAttribute = v_in[1].lineAttributes[fragmentAttributeIndex];
        fragmentTangent = tangent;
        EmitVertex();

        EndPrimitive();
    }


}


-- FibersGeometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

uniform float radius = 0.001f;

const uint NUM_MULTI_ATTRIBUTES = 5;
const uint CUR_ATTRIBUTE = 1;
const float MIN_RIBBON_WIDTH_PERCENTAGE = 0.33;
uniform vec2 minMaxCriterionValues[NUM_MULTI_ATTRIBUTES];

in VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttributes[NUM_MULTI_ATTRIBUTES];
} v_in[];

out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentPositionWorld;
out vec3 screenSpacePosition;
flat out float fragmentAttribute;
flat out int fragmentAttributeIndex;

float computeFactor(in uint attributeIndex, in float attributeValue)
{
    float t = max(0.0, min(1.0,(attributeValue - minMaxCriterionValues[attributeIndex].x)
    / (minMaxCriterionValues[attributeIndex].y - minMaxCriterionValues[attributeIndex].x)));

    return t;
}

void main()
{
    vec3 currentPoint = (mMatrix * vec4(v_in[0].linePosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(v_in[1].linePosition, 1.0)).xyz;

    vec3 tangent = nextPoint - currentPoint;
    vec3 normal = (inverse(vMatrix) * vec4(0, 0, -1, 0)).xyz;

    float diameter = radius * 2;
    float ribbonWidthPerVar = diameter * 0.25;
    vec3 up = normalize((inverse(vMatrix) * vec4(0, 1, 0, 0)).xyz);

    vec3 upRadius = up * radius;

    vec2 ribbonOffsets[] = vec2[](vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0), vec2(1.0, -1.0));

    vec3 initCurrentPosLowerBottom = currentPoint - upRadius * up;
    vec3 initNextPosLowerBottom = nextPoint - upRadius * up;

    const float totalWidth = diameter - ribbonWidthPerVar;
    vec3 upTotalRibbon = up * totalWidth;

    for (int i = 0; i < NUM_MULTI_ATTRIBUTES + 1; ++i)
    {
        float currentAttributeValue = -1;
        float nextAttributeValue = -1;

        vec3 currentPositionsLower;
        vec3 nextPositionsLower;

        float curWidth = ribbonWidthPerVar;

        if (i < NUM_MULTI_ATTRIBUTES)
        {
            currentAttributeValue = v_in[0].lineAttributes[i];
            nextAttributeValue = v_in[1].lineAttributes[i];

            float curFactor = computeFactor(i, currentAttributeValue);
            float nextFactor = computeFactor(i, nextAttributeValue);

            const float offset = 0.0001 * (i + 1);
            currentPositionsLower = initCurrentPosLowerBottom + upTotalRibbon * curFactor - normal * offset;
            nextPositionsLower = initNextPosLowerBottom + upTotalRibbon * nextFactor - normal * offset;
        }
        else
        {
            curWidth = diameter;
            currentPositionsLower = initCurrentPosLowerBottom;
            nextPositionsLower = initNextPosLowerBottom;
        }

//        vec3 currentPosLowerBottom = initCurrentPosLowerBottom + currentWidth * up;
//        //        vec3 currentPosCenter = currentPosLowerBottom + upRibbon / 2 - normal * ribbonWidth / 2.0;
//        vec3 nextPosLowerBottom = initNextPosLowerBottom + nextWidth * up;
        //        vec3 nexttPosCenter = nextPosLowerBottom + upRibbon / 2 - normal * ribbonWidth / 2.0;
        vec3 posOffsets[] = vec3[](currentPositionsLower, currentPositionsLower,
        nextPositionsLower, nextPositionsLower);
        float attributes[] = float[](currentAttributeValue, currentAttributeValue,
        nextAttributeValue, nextAttributeValue);
        float widths[] = float[](curWidth, curWidth,
        curWidth, curWidth);
        //        vec3 posCenters[] = vec3[](currentPosCenter, currentPosCenter, nexttPosCenter, nexttPosCenter);
        float ribbonOffsets[] = float[](1.0, 0.0, 1.0, 0.0);

        for (int j = 0; j < 4; ++j)
        {
            float attributeValue = attributes[j];

            float offset = ribbonOffsets[j];
            vec3 offsetPos = posOffsets[j];
            //            vec3 center = posCenters[j];

            float ribbonWidth = widths[j];
            vec3 upRibbon = up * ribbonWidth;

            vec3 pos = offsetPos + upRibbon * offset;
            vec3 center = offsetPos + upRibbon / 2.0 - normal * ribbonWidth / 4.0;
            //            vec3 center = centers[j];

            gl_Position = mvpMatrix * vec4(pos, 1.0);
            fragmentNormal = normalize(pos - center);
            fragmentTangent = tangent;
            fragmentPositionWorld = pos;
            screenSpacePosition = (vMatrix * mMatrix * vec4(pos, 1.0)).xyz;
            fragmentAttribute = attributeValue;
            fragmentAttributeIndex = (i < NUM_MULTI_ATTRIBUTES) ? i : -1;
            EmitVertex();
        }

        EndPrimitive();
    }
}

-- RibbonGeometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

uniform float radius = 0.001f;

const uint NUM_MULTI_ATTRIBUTES = 5;
const uint CUR_ATTRIBUTE = 1;
const float MIN_RIBBON_WIDTH_PERCENTAGE = 0.33;
uniform vec2 minMaxCriterionValues[NUM_MULTI_ATTRIBUTES];

in VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttributes[NUM_MULTI_ATTRIBUTES];
} v_in[];

out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentPositionWorld;
out vec3 screenSpacePosition;
flat out float fragmentAttribute;
flat out int fragmentAttributeIndex;

float computeRibbonWidth(in float maxWidth, in uint attributeIndex, in float attributeValue)
{
    float minWidth = MIN_RIBBON_WIDTH_PERCENTAGE * maxWidth;
    float remainingWidth = maxWidth - minWidth;

    float t = max(0.0, min(1.0,(attributeValue - minMaxCriterionValues[attributeIndex].x)
                        / (minMaxCriterionValues[attributeIndex].y - minMaxCriterionValues[attributeIndex].x)));

    return minWidth + t * remainingWidth;
}

void main()
{
    vec3 currentPoint = (mMatrix * vec4(v_in[0].linePosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(v_in[1].linePosition, 1.0)).xyz;

    vec3 tangent = nextPoint - currentPoint;
    vec3 normal = (inverse(vMatrix) * vec4(0, 0, -1, 0)).xyz;

    float diameter = radius * 2;
    float ribbonWidthPerVar = diameter / NUM_MULTI_ATTRIBUTES;
    vec3 up = normalize((inverse(vMatrix) * vec4(0, 1, 0, 0)).xyz);

    vec3 upRadius = up * radius;
    float currentWidth = 0;
    float nextWidth = 0;

    vec2 ribbonOffsets[] = vec2[](vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0), vec2(1.0, -1.0));

    vec3 initCurrentPosLowerBottom = currentPoint - upRadius * up;
    vec3 initNextPosLowerBottom = nextPoint - upRadius * up;

    for (int i = 0; i < NUM_MULTI_ATTRIBUTES + 1; ++i)
    {
        float currentAttributeValue = -1;
        float nextAttributeValue = -1;

        float currentRibbonWidth = diameter - currentWidth;
        float nextRibbonWidth = diameter - nextWidth;

        if (i < NUM_MULTI_ATTRIBUTES)
        {
            currentAttributeValue = v_in[0].lineAttributes[i];
            nextAttributeValue = v_in[1].lineAttributes[i];

            currentRibbonWidth = computeRibbonWidth(ribbonWidthPerVar, i, currentAttributeValue);
            nextRibbonWidth = computeRibbonWidth(ribbonWidthPerVar, i, nextAttributeValue);
        }

        vec3 currentPosLowerBottom = initCurrentPosLowerBottom + currentWidth * up;
//        vec3 currentPosCenter = currentPosLowerBottom + upRibbon / 2 - normal * ribbonWidth / 2.0;
        vec3 nextPosLowerBottom = initNextPosLowerBottom + nextWidth * up;
//        vec3 nexttPosCenter = nextPosLowerBottom + upRibbon / 2 - normal * ribbonWidth / 2.0;
        vec3 posOffsets[] = vec3[](currentPosLowerBottom, currentPosLowerBottom,
        nextPosLowerBottom, nextPosLowerBottom);
        float attributes[] = float[](currentAttributeValue, currentAttributeValue,
        nextAttributeValue, nextAttributeValue);
        float widths[] = float[](currentRibbonWidth, currentRibbonWidth,
        nextRibbonWidth, nextRibbonWidth);
//        vec3 posCenters[] = vec3[](currentPosCenter, currentPosCenter, nexttPosCenter, nexttPosCenter);
        float ribbonOffsets[] = float[](1.0, 0.0, 1.0, 0.0);

        // Test to selected centers of "fake" tube

//        vec3 currentCenter = currentPoint + normal * radius / 3.0;
//        vec3 nextCenter = nextPoint + normal * radius / 3.0;
//        vec3 centers[] = vec3[](currentCenter, currentCenter, nextCenter, nextCenter);

        for (int j = 0; j < 4; ++j)
        {
            float attributeValue = attributes[j];

            float offset = ribbonOffsets[j];
            vec3 offsetPos = posOffsets[j];
//            vec3 center = posCenters[j];

            float ribbonWidth = widths[j];
            vec3 upRibbon = up * ribbonWidth;

            vec3 pos = offsetPos + upRibbon * offset;
            vec3 center = offsetPos + upRibbon / 2.0 - normal * ribbonWidth / 4.0;
//            vec3 center = centers[j];

            gl_Position = mvpMatrix * vec4(pos, 1.0);
            fragmentNormal = normalize(pos - center);
            fragmentTangent = tangent;
            fragmentPositionWorld = pos;
            screenSpacePosition = (vMatrix * mMatrix * vec4(pos, 1.0)).xyz;
            fragmentAttribute = attributeValue;
            fragmentAttributeIndex = (i < NUM_MULTI_ATTRIBUTES) ? i : -1;
            EmitVertex();
        }

        EndPrimitive();

        currentWidth += currentRibbonWidth;
        nextWidth += nextRibbonWidth;
    }
}


-- TubeRollsGeometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

const uint NUM_MULTI_ATTRIBUTES = 5;
const uint NUM_SHOWN_ATTRIBUTES = 5;

uniform float radius = 0.001f;
uniform vec2 minMaxCriterionValues[NUM_MULTI_ATTRIBUTES];

in VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttributes[NUM_MULTI_ATTRIBUTES];
} v_in[];

out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentPositionWorld;
out vec3 screenSpacePosition;
flat out float fragmentAttribute;
flat out int fragmentAttributeIndex;

#define NUM_SEGMENTS 3

float computeRadius(in float minRadius, in float maxRadius, in uint attributeIndex, in float attributeValue)
{
    float remainingRadius = maxRadius - minRadius;

    float t = max(0.0, min(1.0,(attributeValue - minMaxCriterionValues[attributeIndex].x)
    / (minMaxCriterionValues[attributeIndex].y - minMaxCriterionValues[attributeIndex].x)));

    return minRadius + t * remainingRadius;
}

void main()
{
    vec3 currentPoint = (mMatrix * vec4(v_in[0].linePosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(v_in[1].linePosition, 1.0)).xyz;

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

    vec3 tangent = normalize(nextPoint - currentPoint);
    vec3 tangentTube = nextPoint - currentPoint;

    mat3 tangentFrameMatrixCurrent = mat3(normalCurrent, binormalCurrent, tangentCurrent);
    mat3 tangentFrameMatrixNext = mat3(normalNext, binormalNext, tangentNext);

    const float theta = 2.0 * 3.1415926 / float(NUM_SEGMENTS);
    const float tangetialFactor = tan(theta); // opposite / adjacent
    const float radialFactor = cos(theta); // adjacent / hypotenuse

    float maxTubeWidth = length(tangentTube) / float(NUM_SHOWN_ATTRIBUTES);
    float minTubeWidth = 0.25 * maxTubeWidth;
    float minRadiusVarTube = radius + 0.05 * radius;
    float maxRadiusVarTube = radius + 0.5 * radius;

    float curWidthOnTube = 0.0;

    for (int a = 0; a < NUM_SHOWN_ATTRIBUTES + 1; ++a)
    {
        float currentAttribute = v_in[0].lineAttributes[a];
        float nextAttribute = v_in[1].lineAttributes[a];

        float currentRadius = radius;
        float nextRadius = radius;
        float currentTubeWidth = 0;

        if (a < NUM_SHOWN_ATTRIBUTES)
        {
            currentRadius = computeRadius(minRadiusVarTube, maxRadiusVarTube, a, currentAttribute);
            nextRadius = computeRadius(minRadiusVarTube, maxRadiusVarTube, a, nextAttribute);

            currentTubeWidth = computeRadius(minTubeWidth, maxTubeWidth, a, currentAttribute);
        }
//        float nextTubeWidth = computeRadius(minRadiusVarTube, maxRadiusVarTube, a, nextAttribute);

        vec2 position = vec2(1.0, 0.0);

        for (int i = 0; i < NUM_SEGMENTS; i++) {
            vec3 tubeCurrentPoint = currentPoint;
            vec3 tubeNextPoint = nextPoint;

            if (a < NUM_SHOWN_ATTRIBUTES)
            {
                tubeCurrentPoint = currentPoint + curWidthOnTube * tangent;
                tubeNextPoint = tubeCurrentPoint + currentTubeWidth * tangent;
            }

            vec3 point2DCurrent = tangentFrameMatrixCurrent * vec3(position, 0.0) * currentRadius;
            vec3 point2DNext = tangentFrameMatrixNext * vec3(position, 0.0) * nextRadius;
            if (a < NUM_SHOWN_ATTRIBUTES)
            {
                point2DNext = tangentFrameMatrixCurrent * vec3(position, 0.0) * nextRadius;
            }
            circlePointsCurrent[i] = point2DCurrent.xyz + tubeCurrentPoint;
            circlePointsNext[i] = point2DNext.xyz + tubeNextPoint;
            vertexNormalsCurrent[i] = normalize(circlePointsCurrent[i] - tubeCurrentPoint);
            vertexNormalsNext[i] = normalize(circlePointsNext[i] - tubeNextPoint);

            // Add the tangent vector and correct the position using the radial factor.
            vec2 circleTangent = vec2(-position.y, position.x);
            position += tangetialFactor * circleTangent;
            position *= radialFactor;
        }

        curWidthOnTube += currentTubeWidth;

        // Emit the tube triangle vertices
        for (int i = 0; i < NUM_SEGMENTS; i++) {
            fragmentAttributeIndex = (a < NUM_SHOWN_ATTRIBUTES) ? a : -1;

            gl_Position = mvpMatrix * vec4(circlePointsCurrent[i], 1.0);
            fragmentNormal = vertexNormalsCurrent[i];
            fragmentTangent = tangent;
            fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
            fragmentAttribute = currentAttribute;
            EmitVertex();

            gl_Position = mvpMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0);
            fragmentNormal = vertexNormalsCurrent[(i+1)%NUM_SEGMENTS];
            fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
            fragmentTangent = tangent;
            fragmentAttribute = currentAttribute;
            EmitVertex();

            gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
            fragmentNormal = vertexNormalsNext[i];
            fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
            fragmentTangent = tangent;
            fragmentAttribute = nextAttribute;
            EmitVertex();

            gl_Position = mvpMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0);
            fragmentNormal = vertexNormalsNext[(i+1)%NUM_SEGMENTS];
            fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
            fragmentAttribute = nextAttribute;
            fragmentTangent = tangent;
            EmitVertex();

            EndPrimitive();
        }
    }
}



-- Geometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

uniform float radius = 0.001f;

const uint NUM_MULTI_ATTRIBUTES = 5;
const uint CUR_ATTRIBUTE = 1;

in VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineAttributes[NUM_MULTI_ATTRIBUTES];
} v_in[];

in int gl_PrimitiveIDIn;

out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentPositionWorld;
out vec3 screenSpacePosition;
flat out float fragmentAttribute;
flat out int fragmentAttributeIndex;

#define NUM_SEGMENTS 9

void main()
{
    vec3 currentPoint = (mMatrix * vec4(v_in[0].linePosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(v_in[1].linePosition, 1.0)).xyz;

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

    vec3 tangent = normalize(nextPoint - currentPoint);

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

    fragmentAttributeIndex = gl_PrimitiveIDIn;

    // Emit the tube triangle vertices
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        gl_Position = mvpMatrix * vec4(circlePointsCurrent[i], 1.0);
        fragmentNormal = vertexNormalsCurrent[i];
        fragmentTangent = tangent;
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        fragmentAttribute = v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsCurrent[(i+1)%NUM_SEGMENTS];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        fragmentTangent = tangent;
        fragmentAttribute = v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
        fragmentNormal = vertexNormalsNext[i];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        fragmentTangent = tangent;
        fragmentAttribute = v_in[1].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsNext[(i+1)%NUM_SEGMENTS];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        fragmentAttribute = v_in[1].lineAttributes[CUR_ATTRIBUTE];
        fragmentTangent = tangent;
        EmitVertex();

        EndPrimitive();
    }
}


-- InstanceGeometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

uniform float radius = 0.001f;

const uint NUM_MULTI_ATTRIBUTES = 5;
const uint CUR_ATTRIBUTE = 1;

in VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    int instanceID;
    vec4 lineVariable;
    vec2 lineMinMax;
    vec2 lineVariableDesc;
    int vertexID;
    uint lineSegID;
    uint varElemOffsetID;
} v_in[];

in int gl_PrimitiveIDIn;

out vec3 fragmentNormal;
out vec3 fragmentTangent;
out vec3 fragmentPositionWorld;
out vec3 screenSpacePosition;
flat out float fragmentAttribute;
flat out int fragmentAttributeIndex;
out vec2 texCoords;
flat out uint fragmentLineSegID;
flat out uint fragmentVarElemOffsetID;

#define NUM_SEGMENTS 3
#define MAX_NUM_CIRLCE_SEGMENTS 12
#define CONSTANT_RADIUS 1

void main()
{
    vec3 currentPoint = (mMatrix * vec4(v_in[0].linePosition, 1.0)).xyz;
    vec3 nextPoint = (mMatrix * vec4(v_in[1].linePosition, 1.0)).xyz;

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

    int instanceID = v_in[0].instanceID;
    int vertexID = v_in[0].vertexID;

    vec3 tangent = normalize(nextPoint - currentPoint);

    vec3 cameraPos = normalize((inverse(vMatrix) * vec4(0, 0, 0, 1)).xyz);
    vec3 cameraUp = normalize((inverse(vMatrix) * vec4(0, 1, 0, 0)).xyz);
    vec3 cameraRight = normalize((inverse(vMatrix) * vec4(-1, 0, 0, 0)).xyz);
    vec3 cameraDir = normalize((inverse(vMatrix) * vec4(0, 0, -1, 0)).xyz);
//    normalCurrent = cameraUp;
//    normalNext = cameraUp;
//
//    binormalCurrent = cross(tangentCurrent, cameraUp);
//    binormalNext = cross(tangentNext, cameraUp);
//
//    normalCurrent = cross(binormalCurrent, tangentCurrent);
//    normalNext = cross(binormalNext, tangentNext);

    mat3 tangentFrameMatrixCurrent = mat3(normalCurrent, binormalCurrent, tangentCurrent);
    mat3 tangentFrameMatrixNext = mat3(normalNext, binormalNext, tangentNext);

    float theta = 2.0 * 3.1415926 / float(MAX_NUM_CIRLCE_SEGMENTS);
    float tangetialFactor = tan(theta); // opposite / adjacent
    float radialFactor = cos(theta); // adjacent / hypotenuse

    //    fragmentAttributeIndex = gl_PrimitiveIDIn; //instanceID;//gl_PrimitiveIDIn;
    vec4 varInfo = v_in[0].lineVariable;

    fragmentAttributeIndex = int(varInfo.w);

    float minRadius = 0.5 * radius;
    float curRadius = minRadius;

    vec2 minMaxPerLine = v_in[0].lineMinMax;

    if (fragmentAttributeIndex > -1)
    {
        fragmentAttribute = (varInfo.x - varInfo.y) / (varInfo.z - varInfo.y);
        float interpolant = min(1.0, max(0.0, (varInfo.x - minMaxPerLine.x) / (minMaxPerLine.y - minMaxPerLine.x) * 0.9 + 0.1));

        curRadius = interpolant * (radius - minRadius) + minRadius;
    }
    else
    {
        fragmentAttribute = 0.0;
        fragmentAttributeIndex = -1;
//        curRadius = 0.4 * radius;
    }

//    vec2 position = vec2(radius * 0.5 * (instanceID + 1) * 0.5 * (gl_PrimitiveIDIn % MAX_NUM_CIRLCE_SEGMENTS + 1), 0.0);
#if defined (CONSTANT_RADIUS)
    curRadius = radius;
//    if (fragmentAttributeIndex < 0)
//    {
//        curRadius = 0.4 * radius;
//    }
#endif

    // Varying radius


//    curRadius = 0.5 * radius * (varInfo.x - minMaxPerLine.x) / (minMaxPerLine.y - minMaxPerLine.x) + 0.5 * radius;

//    vec3 posCurCam = normalize(tangentFrameMatrixCurrent * vec3(cameraUp.xy, 0));
//    vec3 posNextCam = normalize(tangentFrameMatrixNext * vec3(cameraUp.xy, 0));

//    ///////////////////////////////////////
//    // Implicit rotation
//    vec3 refVector = cameraUp;
//    vec2 posOnCircle = normalize(vec2(dot(normalize(normalCurrent), refVector), dot(normalize(binormalCurrent), refVector)));
//    if (abs(dot(normalize(tangentCurrent), cameraUp)) <= 0.02) {
//        refVector = cameraRight;
//        posOnCircle = normalize(vec2(dot(normalize(normalCurrent), refVector), dot(normalize(binormalCurrent), refVector)));
//        tangetialFactor = tan(-theta); // opposite / adjacent
//        radialFactor = cos(-theta); // adjacent / hypotenuse
//    }

//        refVector = cameraRight;
//        posOnCircle = normalize(vec2(dot(normalize(normalCurrent), refVector), dot(normalize(binormalCurrent), refVector)));
//    }
//
//    vec2 position = posOnCircle * curRadius;

    //    ///////////////////////////////////////



//    vec3 posInFrame = tangentFrameMatrixCurrent * vec3(posOnCircle, 0.0) * curRadius;
//    vec3 tempPosUp = currentPoint + posInFrame;
//    vec3 viewDir = normalize(cameraPos - curPoint);
//    vec3 tempPosDown = currentPoint - posInFrame;
//
//    vec3 tempViewPosUp = (vMatrix * vec4(tempPosUp, 1.0)).xyz;
//    vec3 tempViewPosDown = (vMatrix * vec4(tempPosDown, 1.0)).xyz;
//
//    if (tempViewPosDown.y > tempViewPosUp.y)
//    {
//        posOnCircle *= -1;
//    }




//    vec2 position = vec2(curRadius, 0.0);
//    vec2 position = posCurCam.xy * curRadius;

    ///////////////////////////////////////
    // Manual Translation
//    vec2 position = vec2(curRadius, 0.0);
//    float curSign = -2.0;
//    float numSteps = 400;
//
//    float thetaMove = 2.0 * 3.1415926 / numSteps;
//    float tangetialFactorMove = tan(thetaMove); // opposite / adjacent
//    float radialFactorMove = cos(thetaMove); // adjacent / hypotenuse
//
//    for (int i = 0; i < numSteps; i++) {
//        vec2 circleTangent = vec2(-position.y, position.x);
//        position += tangetialFactorMove * circleTangent;
//        position *= radialFactorMove;
//
//        vec3 point2DCurrent = tangentFrameMatrixCurrent * vec3(position, 0.0);
//        vec3 point2DNext = tangentFrameMatrixNext * vec3(position, 0.0);
//        vec3 curPoint = point2DCurrent.xyz + currentPoint;
//
//        vec3 curNormal = normalize(curPoint - currentPoint);
//
//        vec3 viewDir = normalize(cameraPos - curPoint);
//        float scalar = dot(viewDir, curNormal);
//
//        float signProd = sign(scalar);
////        if (abs(scalar) <= 0.001)
////        {
////            vec3 curBinormal = normalize(cross(tangentCurrent, curNormal));
////            scalar = dot(viewDir, curBinormal);
////            signProd = sign(scalar);
////        }
//
//
//
////        if (scalar >= 0)
////        {
////            signProd = dot(curNormal, cameraUp);
////        }
//
////        float signProd = sign(scalar);
//
//        if (curSign == -2.0 || signProd == curSign)
//        {
//            curSign = signProd;
//        }
//        else
//        {
//            float signNToCam = sign(dot(curNormal, cameraUp));
//
//            if (signNToCam <= 0.01 || curSign <= 0.01)
//            {
//                curSign = signProd;
//            }
//            else
//            {
//                break;
//            }
//
//        }
//    }

    ///////////////////////////////////////

    // adjust start position
    vec2 position = vec2(curRadius, 0.0);

    for (int i = 0; i < instanceID; i++) {
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }

    // offset position
    // For Rolled Stripes
//    float thetaOffset = 0;//2.0 * 3.1415926 / 15;
//    float tangetialFactorOffset = tan(thetaOffset); // opposite / adjacent
//    float radialFactorOffset = cos(thetaOffset); // adjacent / hypotenuse
//
//    for (int i = 0; i < vertexID; i++) {
//        vec2 circleTangent = vec2(-position.y, position.x);
//        position += tangetialFactorOffset * circleTangent;
//        position *= radialFactorOffset;
//    }


    // Test whether we can make use of texture coordinates
    vec3 circlePointsCurrentNDC[MAX_NUM_CIRLCE_SEGMENTS];
    vec3 circlePointsNextNDC[MAX_NUM_CIRLCE_SEGMENTS];
//    vec3 vertexNormalsCurrent[MAX_NUM_CIRLCE_SEGMENTS];
//    vec3 vertexNormalsNext[MAX_NUM_CIRLCE_SEGMENTS];

    vec2 tempPosition = vec2(curRadius, 0.0);

    // minX, maxX, minY, maxY
    vec2 bboxMin = vec2(10,10);
    vec2 bboxMax = vec2(-10,-10);

    vec4 pCurNDXCenter = mvpMatrix * vec4(currentPoint, 1);
    pCurNDXCenter.xyz /= pCurNDXCenter.w;
    vec4 pNextNDXCenter = mvpMatrix * vec4(nextPoint, 1);
    pNextNDXCenter.xyz /= pNextNDXCenter.w;

    vec2 ndcOrientation = normalize(pNextNDXCenter.xy - pCurNDXCenter.xy);
    vec2 ndcOrientNormal = vec2(-ndcOrientation.y, ndcOrientation.x);

//    for (int i = 0; i < MAX_NUM_CIRLCE_SEGMENTS; ++i)
//    {
//        vec3 point2DCurrent = tangentFrameMatrixCurrent * vec3(tempPosition, 0.0);
//        vec3 point2DNext = tangentFrameMatrixNext * vec3(tempPosition, 0.0);
//        vec3 pCur = point2DCurrent.xyz + currentPoint;
//        vec3 pNext = point2DNext.xyz + nextPoint;
//
//        vec4 pCurNDX = mvpMatrix * vec4(pCur, 1);
//        pCurNDX.xyz /= pCurNDX.w;
//        vec4 pNextNDX = mvpMatrix * vec4(pNext, 1);
//        pNextNDX.xyz /= pNextNDX.w;
//
//        vec2 circleTangent = vec2(-tempPosition.y, tempPosition.x);
//        tempPosition += tangetialFactor * circleTangent;
//        tempPosition *= radialFactor;
//
////        ndcOrientNormal += (pNextNDX.xy - pNextNDXCenter.xy);
//        ndcOrientNormal += (pCurNDX.xy - pCurNDXCenter.xy);
//    }

//    vec2 ndcOrientation = normalize(pNextNDX.xy - pCurNDX.xy);
    ndcOrientation = normalize(ndcOrientation);
    ndcOrientNormal = normalize(ndcOrientNormal);
    //    if (pNextNDX.x < pCurNDX.x) { ndcOrientation *= -1; }


    mat2 matrixNDC = inverse(mat2(ndcOrientation, ndcOrientNormal));

    tempPosition = vec2(curRadius, 0.0);

    for (int i = 0; i < MAX_NUM_CIRLCE_SEGMENTS; ++i)
    {
        vec3 point2DCurrent = tangentFrameMatrixCurrent * vec3(tempPosition, 0.0);
        vec3 point2DNext = tangentFrameMatrixNext * vec3(tempPosition, 0.0);
        vec3 pCur = point2DCurrent.xyz + currentPoint;
        vec3 pNext = point2DNext.xyz + nextPoint;

        vec4 pCurNDX = mvpMatrix * vec4(pCur, 1);
        pCurNDX.xyz /= pCurNDX.w;
        pCurNDX.xy = matrixNDC * pCurNDX.xy;
        vec4 pNextNDX = mvpMatrix * vec4(pNext, 1);
        pNextNDX.xyz /= pNextNDX.w;
        pNextNDX.xy = matrixNDC * pNextNDX.xy;

//        if (pCurNDX.y <= bboxY.x) { p00 = pCurNDX.xy; }
//        if (pCurNDX.x >= bboxX.y) { p11 = pCurNDX.xy; }

        bboxMin.x = min(pCurNDX.x, bboxMin.x);
        bboxMax.x = max(pCurNDX.x, bboxMax.x);
        bboxMin.y = min(pCurNDX.y, bboxMin.y);
        bboxMax.y = max(pCurNDX.y, bboxMax.y);

        bboxMin.x = min(pNextNDX.x, bboxMin.x);
        bboxMax.x = max(pNextNDX.x, bboxMax.x);
        bboxMin.y = min(pNextNDX.y, bboxMin.y);
        bboxMax.y = max(pNextNDX.y, bboxMax.y);

        circlePointsCurrentNDC[i] = pCurNDX.xyz;
        circlePointsNextNDC[i] = pNextNDX.xyz;

        vec2 circleTangent = vec2(-tempPosition.y, tempPosition.x);
        tempPosition += tangetialFactor * circleTangent;
        tempPosition *= radialFactor;
    }

    vec2 ndcRefPoint = vec2(bboxMin.x, bboxMax.y);
    vec2 ndcNormal = vec2(0, bboxMin.y - bboxMax.y);//vec2(bboxX.x, bboxY.x) - ndcRefPoint;
    vec2 ndcTangent = vec2(bboxMax.x - bboxMin.x, 0);//vec2(bboxX.y, bboxY.y) - ndcRefPoint;

//    if (ndcOrientation.y > ndcOrientation.x)
//    {
//        ndcRefPoint = vec2(bboxX.x, bboxY.x);
//        ndcNormal = vec2(bboxX.y, bboxY.x) - ndcRefPoint;
//        ndcTangent = vec2(bboxX.x, bboxY.y) - ndcRefPoint;
//    }

    vec2 circleTexCoordsCurrent[NUM_SEGMENTS];
    vec2 circleTexCoordsNext[NUM_SEGMENTS];

    theta /= float(NUM_SEGMENTS - 1);
    tangetialFactor = tan(theta); // opposite / adjacent
    radialFactor = cos(theta); // adjacent / hypotenuse

    for (int i = 0; i < NUM_SEGMENTS; i++) {
        vec3 point2DCurrent = tangentFrameMatrixCurrent * vec3(position, 0.0);
        vec3 point2DNext = tangentFrameMatrixNext * vec3(position, 0.0);
        circlePointsCurrent[i] = point2DCurrent.xyz + currentPoint;
        circlePointsNext[i] = point2DNext.xyz + nextPoint;
        vertexNormalsCurrent[i] = normalize(circlePointsCurrent[i] - currentPoint);
        vertexNormalsNext[i] = normalize(circlePointsNext[i] - nextPoint);

        vec4 pCurNDX = mvpMatrix * vec4(circlePointsCurrent[i], 1);
        pCurNDX.xyz /= pCurNDX.w;
        pCurNDX.xy = matrixNDC * pCurNDX.xy;
        vec4 pNextNDX = mvpMatrix * vec4(circlePointsNext[i], 1);
        pNextNDX.xyz /= pNextNDX.w;
        pNextNDX.xy = matrixNDC * pNextNDX.xy;

        vec2 localNDX = pCurNDX.xy - ndcRefPoint;
        vec2 texCoord = vec2(dot(localNDX, normalize(ndcTangent)) / length(ndcTangent),
                             dot(localNDX, normalize(ndcNormal)) / length(ndcNormal));
        circleTexCoordsCurrent[i] = texCoord;

        localNDX = pNextNDX.xy - ndcRefPoint;
        texCoord = vec2(dot(localNDX, normalize(ndcTangent)) / length(ndcTangent),
                        dot(localNDX, normalize(ndcNormal)) / length(ndcNormal));
        circleTexCoordsNext[i] = texCoord;

        // Add the tangent vector and correct the position using the radial factor.
        vec2 circleTangent = vec2(-position.y, position.x);
        position += tangetialFactor * circleTangent;
        position *= radialFactor;
    }

//    fragmentAttributeIndex = int(floor(circleTexCoordsCurrent[NUM_SEGMENTS - 1].y * 6.0));
//    fragmentAttribute = 1.0;

    fragmentLineSegID = v_in[0].lineSegID;
    fragmentVarElemOffsetID = v_in[0].varElemOffsetID;

    // Emit the tube triangle vertices
    for (int i = 0; i < NUM_SEGMENTS - 1; i++) {
        gl_Position = mvpMatrix * vec4(circlePointsCurrent[i], 1.0);
        fragmentNormal = vertexNormalsCurrent[i];
        fragmentTangent = tangent;
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        texCoords = circleTexCoordsCurrent[i];
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsCurrent[(i+1)%NUM_SEGMENTS];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        fragmentTangent = tangent;
        texCoords = circleTexCoordsCurrent[(i+1)%NUM_SEGMENTS];
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
        fragmentNormal = vertexNormalsNext[i];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        fragmentTangent = tangent;
        texCoords = circleTexCoordsNext[i];
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsNext[(i+1)%NUM_SEGMENTS];
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        fragmentTangent = tangent;
        texCoords = circleTexCoordsNext[(i+1)%NUM_SEGMENTS];
        EmitVertex();

        EndPrimitive();
    }

    // Render lids

    // 1) Lid fans from circle points to center
    for (int i = 0; i < NUM_SEGMENTS - 1; i++) {
        fragmentNormal = normalize(-tangent);

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[i], 1.0);
        //        fragmentNormal = vertexNormalsCurrent[i];
        fragmentTangent = normalize(circlePointsCurrent[i] - currentPoint);
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(currentPoint, 1.0);
        //        fragmentNormal = vertexNormalsCurrent[i];
//        fragmentTangent = tangent;
        fragmentPositionWorld = (mMatrix * vec4(currentPoint, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(currentPoint, 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[i + 1], 1.0);
//        fragmentNormal = vertexNormalsCurrent[i];
//        fragmentTangent = tangent;
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[i + 1], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i + 1], 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        EndPrimitive();
    }

    for (int i = 0; i < NUM_SEGMENTS - 1; i++) {
        fragmentNormal = normalize(tangent);

        gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);

        fragmentTangent = normalize(circlePointsNext[i] - nextPoint);;
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[i + 1], 1.0);
//        fragmentNormal = vertexNormalsCurrent[i];
//        fragmentTangent = tangent;
        fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i + 1], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i + 1], 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(nextPoint, 1.0);
        //        fragmentNormal = vertexNormalsCurrent[i];
//        fragmentTangent = tangent;
        fragmentPositionWorld = (mMatrix * vec4(nextPoint, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(nextPoint, 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        EndPrimitive();
    }
//
    // 2) Lids that are starting and closing our partial circle segment
    for (int i = 0; i < NUM_SEGMENTS; i += (NUM_SEGMENTS - 1)) {
        fragmentNormal = normalize(cross(vertexNormalsCurrent[i], tangent));

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[i], 1.0);
//        fragmentNormal = vertexNormalsCurrent[i];
        fragmentTangent = normalize(cross(tangent, fragmentNormal));
        fragmentPositionWorld = (mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[i], 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
        EmitVertex();

        if (i == 0)
        {

            gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
//            fragmentNormal = vertexNormalsCurrent[(i+1)%NUM_SEGMENTS];
            fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
//            fragmentTangent = tangent;
//            fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
            EmitVertex();

            gl_Position = mvpMatrix * vec4(currentPoint, 1.0);
//            fragmentNormal = vertexNormalsNext[i];
            fragmentPositionWorld = (mMatrix * vec4(currentPoint, 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(currentPoint, 1.0)).xyz;
//            fragmentTangent = tangent;
//            fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
            EmitVertex();
        }
        else
        {
            gl_Position = mvpMatrix * vec4(currentPoint, 1.0);
//            fragmentNormal = vertexNormalsNext[i];
            fragmentPositionWorld = (mMatrix * vec4(currentPoint, 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(currentPoint, 1.0)).xyz;
//            fragmentTangent = tangent;
//            fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
            EmitVertex();

            gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
//            fragmentNormal = vertexNormalsCurrent[(i+1)%NUM_SEGMENTS];
            fragmentPositionWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
            screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
//            fragmentTangent = tangent;
//            fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
            EmitVertex();
        }


        gl_Position = mvpMatrix * vec4(nextPoint, 1.0);
//        fragmentNormal = vertexNormalsNext[(i+1)%NUM_SEGMENTS];
        fragmentPositionWorld = (mMatrix * vec4(nextPoint, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(nextPoint, 1.0)).xyz;
//        fragmentAttribute = 0.0;//v_in[0].lineAttributes[CUR_ATTRIBUTE];
//        fragmentTangent = tangent;
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

#if defined(USE_PROGRAMMABLE_FETCH) || defined(BILLBOARD_LINES)
in float fragmentNormalFloat;
in vec3 normal0;
in vec3 normal1;
#else
in vec3 fragmentNormal;
#endif
in vec3 fragmentTangent;
in vec3 fragmentPositionWorld;
flat in float fragmentAttribute;
flat in int fragmentAttributeIndex;
in vec2 texCoords;
flat in uint fragmentLineSegID;
flat in uint fragmentVarElemOffsetID;

#ifdef DIRECT_BLIT_GATHER
out vec4 fragColor;
#endif


#include "AmbientOcclusion.glsl"
#include "Shadows.glsl"

const uint NUM_MULTI_ATTRIBUTES = 6;
const uint CUR_ATTRIBUTE = 1;

#define M_PI 3.14159265358979323846

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);
uniform vec3 cameraPosition; // world space
uniform float aoFactorGlobal = 1.0f;
uniform float shadowFactorGlobal = 1.0f;


uniform float minCriterionValue = 0.0;
uniform float maxCriterionValue = 1.0;
uniform vec2 minMaxCriterionValues[NUM_MULTI_ATTRIBUTES];
uniform bool transparencyMapping = true;

// Color of the object
uniform vec4 colorGlobal;

struct VarData
{
    float value;
};

struct LineDescData
{
    float startIndex;
};

struct VarDescData
{
//    float startIndex;
//    vec2 minMax;
//    float dummy;
    vec4 info;
};

struct LineVarDescData
{
    vec4 minMax;
};

layout (std430, binding = 2) buffer VariableArray
{
    float varArray[];
};

layout (std430, binding = 3) buffer LineDescArray
{
    float lineDescs[];
};

layout (std430, binding = 4) buffer VarDescArray
{
    VarDescData varDescs[];
};

layout (std430, binding = 5) buffer LineVarDescArray
{
    LineVarDescData lineVarDescs[];
};


// Transfer function color lookup table
uniform sampler1D transferFunctionTexture;

vec4 transferFunction(float attr, uint index)
{
    // Transfer to range [0,1]
    float posFloat = clamp((attr - minMaxCriterionValues[index].x)
    / (minMaxCriterionValues[index].y - minMaxCriterionValues[index].x), 0.0, 1.0);
    // Look up the color value
    return texture(transferFunctionTexture, posFloat);
}

vec3 saturate(in vec3 color, in float saturateFactor)
{
    color *= 0.1;

    float gray = 0.2989*color.r + 0.5870*color.g + 0.1140*color.b; //weights from CCIR 601 spec
    vec3 newColor;
    newColor.r = -gray * saturateFactor + color.r * (1 + saturateFactor);
    newColor.g = -gray * saturateFactor + color.g * (1 + saturateFactor);
    newColor.b = -gray * saturateFactor + color.b * (1 + saturateFactor);

    newColor.r = min(1.0, max(0.0, newColor.r));
    newColor.g = min(1.0, max(0.0, newColor.g));
    newColor.b = min(1.0, max(0.0, newColor.b));

    return newColor;
}

vec3 rgbToHSV(in vec3 color)
{
    float minValue = min(color.r, min(color.g, color.b));
    float maxValue = max(color.r, max(color.g, color.b));

    float C = maxValue - minValue;

    // 1) Compute hue H
    float H = 0;
    if (maxValue == color.r)
    {
        H = mod((color.g - color.b) / C, 6.0);
    }
    else if (maxValue == color.g)
    {
        H = (color.b - color.r) / C + 2;
    }
    else if (maxValue == color.b)
    {
        H = (color.r - color.g) / C + 4;
    }
    else { H = 0; }

    H *= 60; // hue is in degree

    // 2) Compute the value V
    float V = maxValue;

    // 3) Compute saturation S
    float S = 0;
    if (V == 0)
    {
        S = 0;
    }
    else
    {
//        S = C / V;
        S = C / (1 - abs(maxValue + minValue - 1));
    }

    return vec3(H, S, V);
}

// https://en.wikipedia.org/wiki/HSL_and_HSV
// https://de.wikipedia.org/wiki/HSV-Farbraum

vec3 hsvToRGB(in vec3 color)
{
    const float H = color.r;
    const float S = color.g;
    const float V = color.b;

    float h = H / 60.0;

    int hi = int(floor(h));
    float f = (h - float(hi));

    float p = V * (1.0 - S);
    float q = V * (1.0 - S * f);
    float t = V * (1.0 - S * (1.0 - f));

    if (hi == 1)
    {
        return vec3(q, V, p);
    }
    else if (hi == 2)
    {
        return vec3(p, V, t);
    }
    else if (hi == 3)
    {
        return vec3(p, q, V);
    }
    else if (hi == 4)
    {
        return vec3(t, p, V);
    }
    else if (hi == 5)
    {
        return vec3(V, p, q);
    }
    else
    {
        return vec3(V, t, p);
    }
}

vec3 linearRGBTosRGB(in vec3 color_sRGB)
{
//float factor = 1.0f / 2.2f;
//return glm::pow(color_sRGB, glm::vec3(factor));
// See https://en.wikipedia.org/wiki/SRGB
return mix(1.055f * pow(color_sRGB, vec3(1.0f / 2.4f)) - 0.055f, color_sRGB * 12.92f,
                        lessThanEqual(color_sRGB, vec3(0.0031308f)));
}

vec3 sRGBToLinearRGB(in vec3 color_LinearRGB)
{
//float factor = 2.2f;
//return glm::pow(color_LinearRGB, glm::vec3(factor));
// See https://en.wikipedia.org/wiki/SRGB
return mix(pow((color_LinearRGB + 0.055f) / 1.055f, vec3(2.4f)),
            color_LinearRGB / 12.92f, lessThanEqual(color_LinearRGB, vec3(0.04045f)));
}


// https://stats.stackexchange.com/questions/214877/is-there-a-formula-for-an-s-shaped-curve-with-domain-and-range-0-1
float sigmoid_zero_one(in float x)
{
    float beta = 2.2;

    float denom = 1.0 + pow((x / (1 - x)), beta);
    return 1.0 - 1.0 / denom;
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

    const int varID = int(floor(texCoords.y * 3.0));
    const uint lineID = fragmentLineSegID;
    const uint varElementID = fragmentVarElemOffsetID;

    uint startIndex = uint(lineDescs[lineID]);
    VarDescData varDesc = varDescs[6 * lineID + varID];
    LineVarDescData lineVarDesc = lineVarDescs[6 * lineID + varID];
    const uint varOffset = uint(varDesc.info.r);
    const vec2 varMinMax = varDesc.info.gb;
    float value = varArray[startIndex + varOffset + varElementID];

//    fragmentAttributeIndex = varID;
//    fragmentAttribute = (value - varMinMax.x) / (varMinMax.y - varMinMax.x);

    uint variableIndex = varID;
    float variableValue = (value - varMinMax.x) / (varMinMax.y - varMinMax.x);

//    uint variableIndex = int(fragmentAttributeIndex);
//    float variableValue = fragmentAttribute;


    vec4 colorAttribute = transferFunction(variableIndex % NUM_MULTI_ATTRIBUTES, CUR_ATTRIBUTE);
//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES == -1) { colorAttribute = vec4(0.4, 0.4, 0.4, 1); }
//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES == 0) { colorAttribute = vec4(0.7, 0, 0, 1); }
//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES == 1) { colorAttribute = vec4(0, 0.7, 0, 1); }
//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES == 2) { colorAttribute = vec4(0, 0, 0.7, 1); }
//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES == 3) { colorAttribute = vec4(0.7, 0, 0.7, 1); }
//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES == 4) { colorAttribute = vec4(0.7, 0.7, 0, 1); }
//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES == 5) { colorAttribute = vec4(0, 0.7, 0.7, 1); }

//    if (fragmentAttributeIndex == -1) { colorAttribute = vec4(0.6, 0.6, 0.6, 1); }
    if (variableIndex == -1) { colorAttribute = vec4(0.8, 0.8, 0.8, 1); }
//    colorAttribute = vec4(0.8, 0.8, 0.8, 1);
//    if (fragmentAttributeIndex == -1) { colorAttribute = vec4(247 / 255.0, 129 / 255.0, 191 / 255.0, 1); }

//    if (fragmentAttributeIndex == 0) { colorAttribute = vec4(0.7, 0, 0, 1); }
//    if (fragmentAttributeIndex == 1) { colorAttribute = vec4(0, 0.7, 0, 1); }
//    if (fragmentAttributeIndex == 2) { colorAttribute = vec4(0, 0, 0.7, 1); }
//    if (fragmentAttributeIndex == 3) { colorAttribute = vec4(1, 0, 1, 1); }
//    if (fragmentAttributeIndex == 4) { colorAttribute = vec4(1, 1, 0, 1); }
//    if (fragmentAttributeIndex == 5) { colorAttribute = vec4(0, 1, 1, 1); }

    // https://www.rapidtables.com/convert/color/rgb-to-hsv.html
    // https://colorbrewer2.org/#type=qualitative&scheme=Set1&n=9

    if (variableIndex == 0) { colorAttribute = vec4(227 / 255.0, 26 / 255.0, 28 / 255.0, 1); }
    else if (variableIndex == 1) { colorAttribute = vec4(51 / 255.0, 160 / 255.0, 44 / 255.0, 1); }
    else if (variableIndex == 2) { colorAttribute = vec4(31 / 255.0, 120 / 255.0, 180 / 255.0, 1); }
    else if (variableIndex == 5) { colorAttribute = vec4(152 / 255.0, 78 / 255.0, 163 / 255.0, 1); }
    else if (variableIndex == 4) { colorAttribute = vec4(255 / 255.0, 255 / 255.0, 51 / 255.0, 1); }
//    if (fragmentAttributeIndex == 4) { colorAttribute = vec4(255 / 255.0, 127 / 255.0, 0 / 255.0, 1); }
//    if (fragmentAttributeIndex == 5) { colorAttribute = vec4(177 / 255.0, 89 / 255.0, 40 / 255.0, 1); }
    else if (variableIndex == 3) { colorAttribute = vec4(255 / 255.0, 127 / 255.0, 0 / 255.0, 1); }


//    if (fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES != 0 && fragmentAttributeIndex % NUM_MULTI_ATTRIBUTES != 5)
//    {
//        colorAttribute.a = 0.1;
//    }

    if (variableIndex < 0)
    {
        colorAttribute.a = 1.0;
    }

    if (variableIndex >= 0)
    {
        colorAttribute.rgb = sRGBToLinearRGB(colorAttribute.rgb);

        vec3 hsvCol = rgbToHSV(colorAttribute.rgb);
        hsvCol.g = 1;
        hsvCol.g = 0.4 + 0.6 * sigmoid_zero_one(variableValue);
        colorAttribute.rgb = hsvCol.rgb;

        colorAttribute.rgb = hsvToRGB(colorAttribute.rgb);
        //        colorAttribute.rgb *= fragmentAttribute;
//        colorAttribute.rgb = saturate(colorAttribute.rgb, fragmentAttribute * 15);
    }


//    colorAttribute = vec4(floor(length(texCoords.y) * 4.0) / 10.0, 0, 0, 1);
//    colorAttribute = vec4(floor(texCoords.y * 4.0) / 4.0, 0, 0, 1);


    #if defined(USE_PROGRAMMABLE_FETCH) || defined(BILLBOARD_LINES)
    vec3 fragmentNormal;
    float interpolationFactor = fragmentNormalFloat;
    vec3 normalCos = normalize(normal0);
    vec3 normalSin = normalize(normal1);
    if (interpolationFactor < 0.0) {
        normalSin = -normalSin;
        interpolationFactor = -interpolationFactor;
    }
    float angle = interpolationFactor * M_PI / 2.0;
    fragmentNormal = cos(angle) * normalCos + sin(angle) * normalSin;
    #endif

    #if REFLECTION_MODEL == 0 // PSEUDO_PHONG_LIGHTING
    const vec3 lightColor = vec3(1,1,1);
    const vec3 ambientColor = colorAttribute.rgb;
    const vec3 diffuseColor = colorAttribute.rgb;

    const float kA = 0.1 * occlusionFactor * shadowFactor;
    const vec3 Ia = kA * ambientColor;
    const float kD = 0.85;
    const float kS = 0.05;
    const float s = 10;

    const vec3 n = normalize(fragmentNormal);
    const vec3 v = normalize(cameraPosition - fragmentPositionWorld);
//    const vec3 l = normalize(lightDirection);
    const vec3 l = normalize(v);
    const vec3 h = normalize(v + l);

    #ifdef CONVECTION_ROLLS
    vec3 t = normalize(cross(vec3(0, 0, 1), n));
    #else
    vec3 t = normalize(cross(vec3(0, 0, 1), n));
    #endif

    vec3 Id = kD * clamp(abs(dot(n, l)), 0.0, 1.0) * diffuseColor;
    vec3 Is = kS * pow(clamp(abs(dot(n, h)), 0.0, 1.0), s) * lightColor;

    float haloParameter = 0.5;
    float angle1 = abs( dot( v, n)) * 0.8;
    float angle2 = abs( dot( v, normalize(t))) * 0.2;
    float halo = min(1.0,mix(1.0f,angle1 + angle2,haloParameter));//((angle1)+(angle2)), haloParameter);

    vec3 hV = normalize(cross(t, v));
    vec3 vNew = normalize(cross(hV, t));

    float angle = pow(abs((dot(vNew, n))), 1.8); // 1.8 + 1.5
    float angleN = pow(abs((dot(v, n))), 1.4);
    float EPSILON = 0.8f;
    float coverage = 1.0 - smoothstep(1.0 - 2.0*EPSILON, 1.0, angle);

    vec3 colorShading = Ia + Id + Is;
    float haloNew = min(1.0, mix(1.0f, angle * 1 + angleN, 0.9)) * 0.9 + 0.1;
    colorShading *= (haloNew) * (haloNew);//clamp(halo, 0, 1) * clamp(halo, 0, 1);
//    colordShading = mix(colorShading, vec3(0, 0, 0), coverage);
    #elif REFLECTION_MODEL == 1 // COMBINED_SHADOW_MAP_AND_AO
    vec3 colorShading = vec3(occlusionFactor * shadowFactor);
    #elif REFLECTION_MODEL == 2 // LOCAL_SHADOW_MAP_OCCLUSION
    vec3 colorShading = vec3(shadowFactor);
    #elif REFLECTION_MODEL == 3 // AMBIENT_OCCLUSION_FACTOR
    vec3 colorShading = vec3(occlusionFactor);
    #elif REFLECTION_MODEL == 4 // NO_LIGHTING
    vec3 colorShading = colorAttribute.rgb;
    #endif
    vec4 color = vec4(colorShading, colorAttribute.a);

    //color.rgb = fragmentNormal;

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
