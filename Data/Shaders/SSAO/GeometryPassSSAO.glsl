-- Vertex

#version 430 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormal;

out vec3 fragPosition;
out vec3 fragNormal;

void main()
{
    mat4 modelViewMatrix = vMatrix * mMatrix;
    mat3 invModelViewMatrix = transpose(inverse(mat3(modelViewMatrix)));
    fragPosition = (modelViewMatrix * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = invModelViewMatrix * vertexNormal;
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}

-- Fragment

#version 430 core

in vec3 fragPosition;
in vec3 fragNormal;

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;

void main()
{
    // View position
    gPosition = fragPosition;

    // Normalized (-space) normals
    gNormal = normalize(fragNormal);
}
