#if !defined NUM_SEGMENTS
    #define NUM_SEGMENTS 9
#endif

void createTubeSegments(inout vec3 positions[NUM_SEGMENTS], inout vec3 normals[NUM_SEGMENTS],
                        in vec3 center, in vec3 normal, in vec3 tangent, in float curRadius)
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