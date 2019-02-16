-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#include "DepthComplexityHeader.glsl"

// The color to shade the framents with
uniform vec4 color;

// The number of fragments necessary to reach the maximal color opacity
uniform uint numFragmentsMaxColor;

out vec4 fragColor;

void main()
{
    uint x = uint(gl_FragCoord.x);
    uint y = uint(gl_FragCoord.y);
    uint numFragments = numFragmentsBuffer[addrGen(uvec2(x,y))];

    float percentage = clamp(float(numFragments)/float(numFragmentsMaxColor), 0.0, 1.0)*color.a;
    vec4 color = vec4(color.rgb, percentage);

    fragColor = color;
}
