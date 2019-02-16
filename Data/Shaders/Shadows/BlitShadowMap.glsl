-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
out vec2 fragTexcoord;

void main()
{
    gl_Position = vec4(vertexPosition, 1.0);
    fragTexcoord = vertexTexcoord;
}


-- Fragment

#version 430 core

uniform sampler2D shadowMap;

in vec2 fragTexcoord;

out vec4 fragColor;

void main()
{
    fragColor = vec4(vec3(texture(shadowMap, fragTexcoord).x), 1.0);
    //fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
