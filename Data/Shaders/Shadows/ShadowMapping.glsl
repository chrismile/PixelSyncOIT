uniform mat4 lightSpaceMatrix;

uniform sampler2D shadowMap;

float getShadowFactor(vec4 worldPosition)
{
    const float bias = 0.005;
    vec4 fragPosLight = lightSpaceMatrix * worldPosition;
    vec3 projCoords = fragPosLight.xyz / fragPosLight.w * 0.5 + 0.5;
    float shadowMapDepth = texture(shadowMap, projCoords.xy).x;
    float fragDepth = projCoords.z;
    float shadowFactor = fragDepth - bias > shadowMapDepth ? 0.0 : 1.0;
    return shadowFactor;
}
