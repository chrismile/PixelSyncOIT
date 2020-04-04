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
    int vLineID;// number of line
    int vElementID;// number of line element (original line vertex index)
    int vElementNextID; // number of next line element (original next line vertex index)
    float vElementInterpolant; // curve parameter t along curve between element and next element
};
//} GeomOut;



void main()
{
//    uint varID = gl_InstanceID % 6;
    const int lineID = int(variableDesc.y);
    const int elementID = int(variableDesc.x);

    vPosition = vertexPosition;
    vNormal = normalize(vertexLineNormal);
    vTangent = normalize(vertexLineTangent);

    vLineID = lineID;
    vElementID = elementID;

    vElementNextID = int(variableDesc.z);
    vElementInterpolant = variableDesc.w;
}

-- Geometry

#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 128) out;

#include "MultiVarGlobalVariables.glsl"

#define NUM_SEGMENTS 10

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

// Input from vertex buffer
in VertexData
{
    vec3 vPosition;// Position in world space
    vec3 vNormal;// Orientation normal along line in world space
    vec3 vTangent;// Tangent of line in world space
    int vLineID;// number of line
    int vElementID;// number of line element (original line vertex index)
    int vElementNextID; // number of next line element (original next line vertex index)
    float vElementInterpolant; // curve parameter t along curve between element and next element
} vertexOutput[];

// Output to fragments
out vec3 fragWorldPos;
out vec3 fragNormal;
out vec3 fragTangent;
out vec3 screenSpacePosition; // screen space position for depth in view space (to sort for buckets...)
out vec2 fragTexCoord;
// "Oriented Stripes"-specfic variables
flat out int fragElementID; // Actual per-line vertex index --> required for sampling from global buffer
flat out int fragElementNextID; // Actual next per-line vertex index --> for linear interpolation
flat out int fragLineID; // Line index --> required for sampling from global buffer
out float fragElementInterpolant; // current number of curve parameter t (in [0;1]) within one line segment

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

    // 4) Emit the tube triangle vertices and attributes to the fragment shader
    fragElementID = vertexOutput[0].vElementID;
    fragLineID = vertexOutput[0].vLineID;

    for (int i = 0; i < NUM_SEGMENTS; i++)
    {
        int iN = (i + 1) % NUM_SEGMENTS;

        vec3 segmentPointCurrent0 = circlePointsCurrent[i];
        vec3 segmentPointNext0 = circlePointsNext[i];
        vec3 segmentPointCurrent1 = circlePointsCurrent[iN];
        vec3 segmentPointNext1 = circlePointsNext[iN];

        fragElementNextID = vertexOutput[0].vElementNextID;
        fragElementInterpolant = vertexOutput[0].vElementInterpolant;

        gl_Position = mvpMatrix * vec4(segmentPointCurrent0, 1.0);
        fragNormal = vertexNormalsCurrent[i];
        fragTangent = tangentCurrent;
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsCurrent[i];
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent0, 1.0)).xyz;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointCurrent1, 1.0);
        fragNormal = vertexNormalsCurrent[iN];
        fragTangent = tangentCurrent;
        fragWorldPos = (mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointCurrent1, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsCurrent[iN];
        EmitVertex();

        if (vertexOutput[1].vElementInterpolant < vertexOutput[0].vElementInterpolant)
        {
            fragElementInterpolant = 1.0f;
            fragElementNextID = int(vertexOutput[0].vElementNextID);
        }
        else
        {
            fragElementInterpolant = vertexOutput[1].vElementInterpolant;
            fragElementNextID = int(vertexOutput[1].vElementNextID);
        }

        gl_Position = mvpMatrix * vec4(segmentPointNext0, 1.0);
        fragNormal = vertexNormalsNext[i];
        fragTangent = tangentNext;
        fragWorldPos = (mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext0, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsNext[i];
        EmitVertex();

        gl_Position = mvpMatrix * vec4(segmentPointNext1, 1.0);
        fragNormal = vertexNormalsNext[iN];
        fragTangent = tangentNext;
        fragWorldPos = (mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(segmentPointNext1, 1.0)).xyz;
        fragTexCoord = vertexTexCoordsNext[iN];
        EmitVertex();

        EndPrimitive();
    }
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition; // Required for transparency rendering techniques

in vec3 fragWorldPos;
in vec3 fragNormal;
in vec3 fragTangent;
in vec2 fragTexCoord;
flat in int fragElementID; // Actual per-line vertex index --> required for sampling from global buffer
flat in int fragElementNextID; // Actual next per-line vertex index --> for linear interpolation
flat in int fragLineID; // Line index --> required for sampling from global buffer
in float fragElementInterpolant; // current number of curve parameter t (in [0;1]) within one line segment

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
        S = C / V;
//        S = C / (1 - abs(maxValue + minValue - 1));
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


vec4 mapColor(in float value, uint index)
{
    if (index == 0)
    {
        return mix(vec4(vec3(253,219,199)/ 255.0, 1), vec4(vec3(178,24,43)/ 255.0, 1), value);
    }
    else if (index == 1)
    {
        return mix(vec4(vec3(209,229,240)/ 255.0, 1), vec4(vec3(33,102,172)/ 255.0, 1), value);
    }
    else if (index == 2)
    {
        return mix(vec4(vec3(217,240,211)/ 255.0, 1), vec4(vec3(27,120,55)/ 255.0, 1), value);
    }
    else if (index == 3)
    {
        return mix(vec4(vec3(216,218,235) / 255.0, 1), vec4(vec3(84,39,136) / 255.0, 1), value);
    }
    else if (index == 5)
    {
        return mix(vec4(vec3(254,224,182)/ 255.0, 1), vec4(vec3(179,88,6)/ 255.0, 1), value);
    }
    else if (index == 6)
    {
        return mix(vec4(vec3(199,234,229)/ 255.0, 1), vec4(vec3(1,102,94)/ 255.0, 1), value);
    }
    else if (index == 4)
    {
        return mix(vec4(vec3(253,224,239)/ 255.0, 1), vec4(vec3(197,27,125)/ 255.0, 1), value);
    }
}

void main()
{
//    vec4 surfaceColor = vec4(floor(fragTexCoord.y * 4.0) / 10.0, 0, 0, 1);
//    vec4 surfaceColor = vec4(fragTexCoord.xy, 0, 1);

    float variableValue;
    vec2 variableMinMax;

    float variableNextValue;
    vec2 variableNextMinMax;

    const float numVars = 4.0;
    const int varID = int(floor(fragTexCoord.y * numVars));
    float rest = fragTexCoord.y * numVars - float(varID);

    // Sample variables from buffers
    sampleVariableFromLineSSBO(fragLineID, varID, fragElementID, variableValue, variableMinMax);
    sampleVariableFromLineSSBO(fragLineID, varID, fragElementNextID, variableNextValue, variableNextMinMax);

    // Normalize values
    variableValue = (variableValue - variableMinMax.x) / (variableMinMax.y - variableMinMax.x);
    variableNextValue = (variableNextValue - variableNextMinMax.x) / (variableNextMinMax.y - variableNextMinMax.x);

    // Determine variable color
    vec4 surfaceColor = vec4(0.2, 0.2, 0.2, 1);
    if (varID == 0) { surfaceColor = vec4(vec3(228,26,28)/ 255.0, 1); } // red
    else if (varID == 1) { surfaceColor = vec4(vec3(55,126,184)/ 255.0, 1); } // blue
    else if (varID == 2) { surfaceColor = vec4(vec3(5,139,69)/ 255.0, 1); } // green
    else if (varID == 3) { surfaceColor = vec4(vec3(129,15,124)/ 255.0, 1); } // lila / purple
    else if (varID == 4) { surfaceColor = vec4(vec3(217,72,1)/ 255.0, 1); } // orange
    else if (varID == 5) { surfaceColor = vec4(vec3(231,41,138)/ 255.0, 1); } // pink

    surfaceColor.rgb = sRGBToLinearRGB(surfaceColor.rgb);
//    vec3 hsvCol = rgbToHSV(surfaceColor.rgb);
//    surfaceColor.rgb = hsvToRGB(hsvCol.rgb);
    vec3 hsvCol = rgbToHSV(surfaceColor.rgb);
    float curMapping = variableValue;
    float nextMapping = variableNextValue;
    float rate = mix(curMapping, nextMapping, fragElementInterpolant);
//
    hsvCol.g = hsvCol.g * (0.25 + 0.75 * rate);
    surfaceColor.rgb = hsvCol.rgb;
    surfaceColor.rgb = hsvToRGB(surfaceColor.rgb);

    float borderWidth = 0.15;
    float alphaBorder = 0.5;
    if (rest <= borderWidth || rest >= (1.0 - borderWidth))
    {
        if (rest > 0.5) { rest = 1.0 - rest; }

        surfaceColor.rgb = surfaceColor.rgb * (alphaBorder + (1 - alphaBorder) * rest / borderWidth);
    }

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
//        const vec3 l = normalize(lightDirection);
    const vec3 l = normalize(v);
    const vec3 h = normalize(v + l);
    vec3 t = normalize(fragTangent);
//    vec3 t = normalize(cross(vec3(0, 0, 1), n));


    vec3 Id = kD * clamp((dot(n, l)), 0.0, 1.0) * diffuseColor;
    vec3 Is = kS * pow(clamp((dot(n, h)), 0.0, 1.0), s) * lightColor;
    vec3 colorShading = Ia + Id + Is;

//    float haloParameter = 0.5;
//    float angle1 = abs( dot( v, n)) * 0.8;
//    float angle2 = abs( dot( v, normalize(t))) * 0.2;
//    float halo = min(1.0,mix(1.0f,angle1 + angle2,haloParameter));//((angle1)+(angle2)), haloParameter);

    vec3 hV = normalize(cross(t, v));
    vec3 vNew = normalize(cross(hV, t));

    float angle = pow(abs((dot(vNew, n))), 1.2); // 1.8 + 1.5
    float angleN = pow(abs((dot(v, n))), 1.2);
//    float EPSILON = 0.8f;
//    float coverage = 1.0 - smoothstep(1.0 - 2.0*EPSILON, 1.0, angle);

    float haloNew = min(1.0, mix(1.0f, angle + angleN, 0.9)) * 0.9 + 0.1;
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