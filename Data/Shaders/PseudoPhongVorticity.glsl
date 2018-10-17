-- Vertex

#version 430 core


layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexLineNormal;
layout(location = 2) in vec3 vertexLineTangent;
layout(location = 3) in float vertexVorticity;

out VertexData
{
    vec3 linePosition;
    vec3 lineNormal;
    vec3 lineTangent;
    float lineVorticity;
};

void main()
{
    linePosition = vertexPosition;
    lineNormal = vertexLineNormal;
    lineTangent = vertexLineTangent;
    lineVorticity = vertexVorticity;
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
    float lineVorticity;
} v_in[];

out vec3 fragmentNormal;
out vec3 fragmentPositonWorld;
out vec3 screenSpacePosition;
out float vorticity;

#define NUM_SEGMENTS 8

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
        vorticity = v_in[0].lineVorticity;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsCurrent[(i+1)%NUM_SEGMENTS];
        fragmentPositonWorld = (mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsCurrent[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        vorticity = v_in[0].lineVorticity;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[i], 1.0);
        fragmentNormal = vertexNormalsNext[i];
        fragmentPositonWorld = (mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[i], 1.0)).xyz;
        vorticity = v_in[1].lineVorticity;
        EmitVertex();

        gl_Position = mvpMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0);
        fragmentNormal = vertexNormalsNext[(i+1)%NUM_SEGMENTS];
        fragmentPositonWorld = (mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        screenSpacePosition = (vMatrix * mMatrix * vec4(circlePointsNext[(i+1)%NUM_SEGMENTS], 1.0)).xyz;
        vorticity = v_in[1].lineVorticity;
        EmitVertex();

        EndPrimitive();
    }
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition;

#ifndef DIRECT_BLIT_GATHER
#include OIT_GATHER_HEADER
#endif

in vec3 fragmentNormal;
in vec3 fragmentPositonWorld;
in float vorticity;

#ifdef DIRECT_BLIT_GATHER
out vec4 fragColor;
#endif


#ifdef USE_SSAO
uniform sampler2D ssaoTexture;
#endif

uniform vec3 lightDirection = vec3(1.0,0.0,0.0);

uniform float minVorticity;
uniform float maxVorticity;
uniform bool transparencyMapping = true;

// Color of the object
uniform vec4 colorGlobal;


// Transfer function color lookup table
uniform sampler1D transferFunctionTexture;

vec4 transferFunction(float attr)
{
    // Transfer to range [0,1]
    float posFloat = clamp((attr - minVorticity) / (maxVorticity - minVorticity), 0.0, 1.0);
    // Look up the color value
    return texture(transferFunctionTexture, posFloat);
}

void main()
{
#ifdef USE_SSAO
    // Read ambient occlusion factor from texture
    vec2 texCoord = vec2(gl_FragCoord.xy + vec2(0.5, 0.5))/textureSize(ssaoTexture, 0);
    float occlusionFactor = texture(ssaoTexture, texCoord).r;
#else
    // No ambient occlusion
    const float occlusionFactor = 1.0;
#endif

	// Use vorticity
	vec4 diffuseColorVorticity = transferFunction(vorticity);

    vec3 normal = fragmentNormal;
	if (length(normal) < 0.5) {
	    normal = vec3(1.0, 0.0, 0.0);
	}

	vec3 diffuseShadingVorticity = diffuseColorVorticity.rgb * clamp(dot(normal, lightDirection)/2.0
	        + 0.75 * occlusionFactor, 0.0, 1.0);
	vec4 color = vec4(diffuseShadingVorticity, diffuseColorVorticity.a * colorGlobal.a); // colorGlobal.a

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
